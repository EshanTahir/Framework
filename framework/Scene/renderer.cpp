#include "systems.h"
#include "occluderWorld.h"
#include "Core/assetCache.h"
#include "Core/gameSettings.h"
#include "Core/glContext.h"
#include "Core/log.h"
#include "Match/simClock.h"
#include "Render/presentPipeline.h"
#include "Render/renderer2d.h"
#include "Render/worldAtlas.h"

#ifdef DrawText
#undef DrawText
#endif

#include "SDL.h"
#include "SDL_ttf.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    struct DrawXform
    {
        float x, y, a;
    };

    DrawXform DrawPose(const TransformComponent& t, float alpha)
    {
        if (!t.interp_primed)
            return {t.x, t.y, t.a};
        return {
            t.ix + (t.x - t.ix) * alpha,
            t.iy + (t.y - t.iy) * alpha,
            t.ia + (t.a - t.ia) * alpha,
        };
    }

    Camera2D ParallaxCam(const Camera2D& master, float parallax, bool enabled)
    {
        Camera2D c = master;
        if (!enabled)
            return c;
        // parallax 0 = locked to gameplay cam; 1 = sky (cam effectively origin)
        const float p = std::clamp(parallax, 0.f, 1.f);
        c.x = master.x * (1.f - p);
        c.y = master.y * (1.f - p);
        return c;
    }
}

int Renderer::EntityBand(entt::registry& registry, entt::entity e)
{
    if (registry.valid(e) && registry.has<SortBandComponent>(e))
        return registry.get<SortBandComponent>(e).band;
    return 0;
}

float Renderer::EntityParallax(entt::registry& registry, entt::entity e)
{
    if (registry.valid(e) && registry.has<SortBandComponent>(e))
        return registry.get<SortBandComponent>(e).parallax;
    return 0.f;
}

bool Renderer::BandMatch(entt::registry& registry, entt::entity e) const
{
    if (!m_band_filter_on)
        return true;
    return EntityBand(registry, e) == m_band_filter;
}

void Renderer::UpdateViewCull(const Camera2D& master)
{
    auto& r2d = Renderer2D::GetInstance();
    const float zoom = master.zoom <= 0.01f ? 0.01f : master.zoom;
    const float half_w = (r2d.ScreenWidth() / kWorldPixels) * 0.5f / zoom;
    const float half_h = (r2d.ScreenHeight() / kWorldPixels) * 0.5f / zoom;
    constexpr float kMargin = 2.f;
    const float cx = SnapWorldPixel(master.x + master.shake_x, zoom);
    const float cy = SnapWorldPixel(master.y + master.shake_y, zoom);
    m_view_x0 = cx - half_w - kMargin;
    m_view_x1 = cx + half_w + kMargin;
    m_view_y0 = cy - half_h - kMargin;
    m_view_y1 = cy + half_h + kMargin;
    m_view_cull_on = true;
}

bool Renderer::InView(float x, float y, float half_w, float half_h) const
{
    if (!m_view_cull_on)
        return true;
    return !(x + half_w < m_view_x0 || x - half_w > m_view_x1
          || y + half_h < m_view_y0 || y - half_h > m_view_y1);
}

void Renderer::CalculateCameras(entt::registry& registry)
{
    active_camera = entt::null;
    int best = -999999;
    auto view = registry.view<CameraComponent, TransformComponent>();
    for (auto e : view)
    {
        // Skip aperture view cams (depth reserved negative for nested)
        if (registry.has<ApertureComponent>(e))
            continue;
        auto& cam = registry.get<CameraComponent>(e);
        if (cam.depth >= best)
        {
            best = cam.depth;
            active_camera = e;
        }
    }
}

void Renderer::CutSprites(entt::registry& registry)
{
    auto view = registry.view<SpriteSheetComponent>();
    for (auto e : view)
    {
        auto& sheet = registry.get<SpriteSheetComponent>(e);
        if (!sheet.image_location)
            continue;
        if (sheet.cut_image == sheet.image_location
            && sheet.cut_rows == sheet.rows
            && sheet.cut_columns == sheet.columns
            && !sheet.sprites.empty())
            continue;

        auto& tex = AssetCache::GetInstance().GetTexture(sheet.image_location);
        if (!tex.surface || sheet.columns <= 0 || sheet.rows <= 0)
            continue;
        int tw = tex.width / sheet.columns;
        int th = tex.height / sheet.rows;
        sheet.sprites.clear();
        for (int y = 0; y < sheet.rows; ++y)
            for (int x = 0; x < sheet.columns; ++x)
                sheet.sprites.push_back(SDL_Rect{x * tw, y * th, tw, th});
        sheet.cut_image = sheet.image_location;
        sheet.cut_rows = sheet.rows;
        sheet.cut_columns = sheet.columns;
    }
}

void Renderer::DrawWorldPass(entt::registry& registry, float delta_time, const Camera2D& master, bool parallax_on)
{
    auto& r2d = Renderer2D::GetInstance();
    UpdateViewCull(master);
    sprites_seen = 0;
    sprites_drawn = 0;
    sprites_culled = 0;
    atlas_pages = WorldAtlas::GetInstance().PageCount();

    std::vector<int> bands;
    bands.push_back(0);
    {
        auto view = registry.view<SortBandComponent>();
        for (auto e : view)
        {
            int b = registry.get<SortBandComponent>(e).band;
            if (std::find(bands.begin(), bands.end(), b) == bands.end())
                bands.push_back(b);
        }
    }
    std::sort(bands.begin(), bands.end());

    for (int band : bands)
    {
        m_band_filter = band;
        m_band_filter_on = true;

        // Use max parallax among entities in this band (typically uniform per band)
        float para = 0.f;
        {
            auto view = registry.view<SortBandComponent>();
            for (auto e : view)
            {
                auto& sb = registry.get<SortBandComponent>(e);
                if (sb.band == band)
                    para = std::max(para, sb.parallax);
            }
        }
        r2d.SetCamera(ParallaxCam(master, para, parallax_on));

        if (draw_world_textures)
        {
            DrawTileMaps(registry);
            DrawSprites(registry);
            DrawSpriteGrids(registry);
            DrawAnimations(registry, delta_time);
        }
        // Textures on: filled debug tint only (unskinned solids). Skip
        // per-edge DrawLine - each line flushed a draw and smashed the white batch.
        DrawDebugShapes(registry);
        DrawParticles(registry);
        m_world_text_only = true;
        m_screen_text_only = false;
        DrawText(registry);
        m_world_text_only = false;
    }
    m_band_filter_on = false;
    r2d.SetCamera(master);
}

void Renderer::Run(entt::registry& registry, float delta_time)
{
    auto& gl = GlContext::GetInstance();
    auto& r2d = Renderer2D::GetInstance();
    auto& present = PresentPipeline::GetInstance();
    auto& settings = GameSettings::GetInstance();

    // keep RT / blit viewport matched to the real window (maximize, OS fullscreen, drag-resize)
    gl.SyncFromWindow();
    const int sw = gl.Width();
    const int sh = gl.Height();
    r2d.SetScreenSize(sw, sh);
    present.EnsureSize(sw, sh);

    CalculateCameras(registry);
    CutSprites(registry);

    const float alpha = SimClock::GetInstance().Alpha();
    Camera2D master{};
    if (active_camera != entt::null)
    {
        auto& c = registry.get<CameraComponent>(active_camera);
        auto& t = registry.get<TransformComponent>(active_camera);
        DrawXform d = DrawPose(t, alpha);
        master.x = d.x;
        master.y = d.y;
        master.angle = d.a;
        master.zoom = c.zoom;
        if (c.shake_time > 0)
        {
            c.shake_time -= delta_time;
            float shake_scale = settings.ShakeScale();
            if (settings.ReduceFlash())
                shake_scale *= 0.35f;
            master.shake_x = ((rand() % 1000) / 1000.f - 0.5f) * c.shake_mag * shake_scale;
            master.shake_y = ((rand() % 1000) / 1000.f - 0.5f) * c.shake_mag * shake_scale;
        }
    }

    present.Scene().Bind();
    glClearColor(clear_r, clear_g, clear_b, clear_a);
    glClear(GL_COLOR_BUFFER_BIT);
    r2d.BeginBatch();

    DrawWorldPass(registry, delta_time, master, settings.ParallaxEnabled());

    r2d.Flush();
    r2d.EndFrame();

    // key 9 (draw_world_textures off): skip LightAccum + Composite
    // ReduceFlash: keep masks; dim volumes/shafts later; skip Bright+bloom below
    const bool do_lighting = draw_world_textures && present.LightFillStrength() > 0.f;
    if (do_lighting)
    {
        OccluderWorld::Get().UpdateDynamics(registry, m_view_x0, m_view_y0, m_view_x1, m_view_y1);
        BuildLightAccum(registry, master);
        present.CompositeLighting();
    }

    present.Scene().Bind();
    r2d.BeginBatch();
    r2d.SetScreenSize(sw, sh);
    r2d.SetCamera(master);

    DrawLightVolumes(registry);
    DrawLightShafts(registry, delta_time);

    if (settings.AperturesEnabled() && !m_drawing_aperture)
        RenderApertures(registry, delta_time, master);

    if (OccluderWorld::Get().debug_draw)
        DrawOccluderDebug(registry);

    if (draw_debug_colliders)
    {
        r2d.SetCamera(master);
        DrawColliders(registry);
    }

    r2d.Flush();
    r2d.EndFrame();

    // Selective soft glow only if something actually has EmissiveComponent.
    bool any_emissive = false;
    {
        auto ev = registry.view<EmissiveComponent>();
        for (auto e : ev)
        {
            (void)e;
            any_emissive = true;
            break;
        }
    }
    if (any_emissive && !settings.ReduceFlash())
    {
        present.Bright().Bind();
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        r2d.BeginBatch();
        r2d.SetScreenSize(sw, sh);
        r2d.SetCamera(master);
        DrawEmissiveMask(registry);
        r2d.Flush();
        present.CompositeEmissiveGlow(1.0f, 2);
    }

    int bloom_q = settings.BloomQuality();
    if (settings.ReduceFlash())
        bloom_q = 0;
    present.ApplyBloom(bloom_q, settings.ReduceFlash());
    // Vignette disabled: circular edge band; when re-enabled must stay on scene RT only
    // (HUD / UI draw after BlitToScreen below - never into the vignette pass).
    if (settings.Vignette() > 0.001f)
        present.ApplyVignette(settings.Vignette(), clear_r * 0.5f, clear_g * 0.5f, clear_b * 0.5f);

    // Blit scene → backbuffer. Screen HUD text is drawn later (after UI overlays).
    present.BlitToScreen(sw, sh, true);
}

void Renderer::DrawScreenHud(entt::registry& registry)
{
    auto& r2d = Renderer2D::GetInstance();
    auto& gl = GlContext::GetInstance();
    r2d.SetScreenSize(gl.Width(), gl.Height());
    m_screen_text_only = true;
    m_world_text_only = false;
    DrawText(registry);
    m_screen_text_only = false;
    r2d.Flush();
}

void Renderer::RenderApertures(entt::registry& registry, float delta_time, const Camera2D& master)
{
    auto& present = PresentPipeline::GetInstance();
    auto& r2d = Renderer2D::GetInstance();
    auto& settings = GameSettings::GetInstance();
    const float alpha = SimClock::GetInstance().Alpha();

    struct Cand
    {
        entt::entity e;
        float dist2;
    };
    std::vector<Cand> cands;
    auto view = registry.view<ApertureComponent, TransformComponent>();
    for (auto e : view)
    {
        auto& ap = registry.get<ApertureComponent>(e);
        if (!ap.active || ap.view_cam == 0)
            continue;
        auto& t = registry.get<TransformComponent>(e);
        DrawXform d = DrawPose(t, alpha);
        float dx = d.x - master.x;
        float dy = d.y - master.y;
        cands.push_back({e, dx * dx + dy * dy});
    }
    std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.dist2 < b.dist2; });
    const int max_n = settings.MaxApertures();
    if ((int)cands.size() > max_n)
        cands.resize(max_n);

    m_drawing_aperture = true;
    for (auto& c : cands)
    {
        auto& ap = registry.get<ApertureComponent>(c.e);
        auto cam_e = static_cast<entt::entity>(ap.view_cam);
        if (!registry.valid(cam_e) || !registry.has<CameraComponent>(cam_e) || !registry.has<TransformComponent>(cam_e))
            continue;

        Camera2D nested{};
        auto& cc = registry.get<CameraComponent>(cam_e);
        auto& ct = registry.get<TransformComponent>(cam_e);
        DrawXform cd = DrawPose(ct, alpha);
        nested.x = cd.x;
        nested.y = cd.y;
        nested.angle = cd.a;
        nested.zoom = cc.zoom;

        present.Aperture().Bind();
        glClearColor(clear_r, clear_g, clear_b, clear_a);
        glClear(GL_COLOR_BUFFER_BIT);
        // Temporary screen size for nested ortho
        r2d.SetScreenSize(present.Aperture().Width(), present.Aperture().Height());
        DrawWorldPass(registry, 0.f, nested, false); // no parallax inside postcard
        r2d.Flush();

        // Composite into scene as a quad
        present.Scene().Bind();
        r2d.SetScreenSize(present.Scene().Width(), present.Scene().Height());
        r2d.SetCamera(master);
        auto& t = registry.get<TransformComponent>(c.e);
        DrawXform d = DrawPose(t, alpha);
        r2d.DrawQuad(present.Aperture().Color(), d.x, d.y, ap.half_w * 2.f, ap.half_h * 2.f,
                     0, 0, 1, 1, d.a, 1, 1, 1, 1, true);
        r2d.Flush();
    }
    m_drawing_aperture = false;
    r2d.SetScreenSize(present.Scene().Width(), present.Scene().Height());
    r2d.SetCamera(master);
}

void Renderer::DrawEmissiveMask(entt::registry& registry)
{
    // Exact silhouettes into Bright RT - blur later expands evenly in screen space.
    // Use authored color (hue) × strength (intensity) without crushing to white.
    auto& r2d = Renderer2D::GetInstance();
    const float alpha = SimClock::GetInstance().Alpha();

    auto boxes = registry.view<EmissiveComponent, TransformComponent, BoxShapeComponent, ColorComponent>();
    for (auto e : boxes)
    {
        auto& em = registry.get<EmissiveComponent>(e);
        auto& t = registry.get<TransformComponent>(e);
        auto& box = registry.get<BoxShapeComponent>(e);
        auto& color = registry.get<ColorComponent>(e);
        DrawXform d = DrawPose(t, alpha);
        float strength = std::clamp(em.strength, 0.2f, 2.5f);
        float r = color.primary.r / 255.f;
        float g = color.primary.g / 255.f;
        float b = color.primary.b / 255.f;
        // Keep hue; boost contribution via alpha so soft glow tracks intensity + color change.
        float a = std::clamp((color.primary.a / 255.f) * (0.35f + 0.4f * strength), 0.08f, 1.f);
        r2d.DrawSolidQuad(d.x, d.y, box.width, box.height, d.a, r, g, b, a);
    }
}

void Renderer::EnsureRadialTex()
{
    if (m_radial_tex)
        return;
    constexpr int N = 128;
    std::vector<unsigned char> px((size_t)N * N * 4);
    for (int y = 0; y < N; ++y)
    {
        for (int x = 0; x < N; ++x)
        {
            float u = (x + 0.5f) / (float)N;
            float v = (y + 0.5f) / (float)N;
            float dx = u - 0.5f, dy = v - 0.5f;
            float d = std::sqrt(dx * dx + dy * dy) * 2.f;
            float a = std::clamp(1.f - d, 0.f, 1.f);
            a = a * a;
            unsigned char c = (unsigned char)(a * 255.f + 0.5f);
            size_t i = ((size_t)y * N + (size_t)x) * 4;
            px[i] = px[i + 1] = px[i + 2] = px[i + 3] = c;
        }
    }
    glGenTextures(1, &m_radial_tex);
    glBindTexture(GL_TEXTURE_2D, m_radial_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, N, N, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::DrawSpotDarkWedges(Renderer2D& r2d, float lx, float ly, float R,
                                  float aim, float outer)
{
    // after radial: subtract two large wedges covering the dark side of the cone
    constexpr float kTwoPi = 6.28318530718f;
    if (outer >= kTwoPi - 1e-3f)
        return;
    float BIG = R * 4.f;
    float start = aim + outer * 0.5f;
    float span = kTwoPi - outer;
    for (int i = 0; i < 2; ++i)
    {
        float t0 = start + span * (i * 0.5f);
        float t1 = start + span * ((i + 1) * 0.5f);
        float mid = (t0 + t1) * 0.5f;
        float ax = lx + std::cos(t0) * BIG;
        float ay = ly + std::sin(t0) * BIG;
        float mx = lx + std::cos(mid) * BIG * 1.25f;
        float my = ly + std::sin(mid) * BIG * 1.25f;
        float bx = lx + std::cos(t1) * BIG;
        float by = ly + std::sin(t1) * BIG;
        r2d.DrawPoly4(lx, ly, ax, ay, mx, my, bx, by, 1.f, 1.f, 1.f, 1.f);
    }
}

bool SegmentFacesLight(const OccluderSegment& s, float lx, float ly)
{
    float mx = (s.ax + s.bx) * 0.5f;
    float my = (s.ay + s.by) * 0.5f;
    float dx = mx - lx, dy = my - ly;
    return (dx * s.nx + dy * s.ny) > 0.f;
}

void Renderer::RenderOneLightCutout(const PointLightComponent& L, float lx, float ly,
                                    Renderer2D& r2d)
{
    float R = L.radius;
    float BIG = R * 4.f;
    auto& occ = OccluderWorld::Get();

    // 1) add radial
    glBlendFunc(GL_ONE, GL_ONE);
    float ir = L.r * L.intensity;
    float ig = L.g * L.intensity;
    float ib = L.b * L.intensity;
    r2d.DrawQuad(m_radial_tex, lx, ly, 2.f * R, 2.f * R, 0.f, 0.f, 1.f, 1.f, 0.f, ir, ig, ib, 1.f);
    r2d.Flush();

    // 2) subtract shadow quads + spot wedges
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    if (L.outer < 6.283185f - 1e-3f)
        DrawSpotDarkWedges(r2d, lx, ly, R, L.aim, L.outer);

    std::vector<const OccluderSegment*> segs;
    occ.QueryAABB(lx - R, ly - R, lx + R, ly + R, segs);
    for (const OccluderSegment* sp : segs)
    {
        const auto& s = *sp;
        if (!SegmentFacesLight(s, lx, ly))
            continue;
        float lax = s.ax - lx, lay = s.ay - ly;
        float lbx = s.bx - lx, lby = s.by - ly;
        float la = std::sqrt(lax * lax + lay * lay);
        float lb = std::sqrt(lbx * lbx + lby * lby);
        if (la < 1e-4f || lb < 1e-4f)
            continue;
        float a2x = s.ax + (lax / la) * BIG;
        float a2y = s.ay + (lay / la) * BIG;
        float b2x = s.bx + (lbx / lb) * BIG;
        float b2y = s.by + (lby / lb) * BIG;
        float dax = a2x - s.ax, day = a2y - s.ay;
        float dbx = b2x - s.bx, dby = b2y - s.by;
        if (dax * dax + day * day < 1e-8f || dbx * dbx + dby * dby < 1e-8f)
            continue;
        r2d.DrawPoly4(s.ax, s.ay, s.bx, s.by, b2x, b2y, a2x, a2y, 1.f, 1.f, 1.f, 1.f);
    }
    r2d.Flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::ApplySunShadows(entt::registry& registry, Renderer2D& r2d)
{
    auto view = registry.view<DirectionalLightComponent>();
    DirectionalLightComponent* sun = nullptr;
    for (auto e : view)
    {
        auto& d = registry.get<DirectionalLightComponent>(e);
        if (d.enabled)
        {
            sun = &d;
            break;
        }
    }
    if (!sun)
        return;

    float dx = sun->dx, dy = sun->dy;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1e-5f)
        return;
    dx /= len;
    dy /= len;
    float BIG = 80.f;
    float crush = std::clamp(sun->fill, 0.f, 1.f);

    // base outdoor vis then subtract parallel extrusions
    // (accum already cleared to 1 when sun path is taken)

    auto& occ = OccluderWorld::Get();
    std::vector<const OccluderSegment*> segs;
    occ.QueryAABB(m_view_x0 - 2.f, m_view_y0 - 2.f, m_view_x1 + 2.f, m_view_y1 + 2.f, segs);

    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    for (const OccluderSegment* sp : segs)
    {
        const auto& s = *sp;
        // facing -sun
        if ((s.nx * (-dx) + s.ny * (-dy)) <= 0.f)
            continue;
        float a2x = s.ax + dx * BIG;
        float a2y = s.ay + dy * BIG;
        float b2x = s.bx + dx * BIG;
        float b2y = s.by + dy * BIG;
        r2d.DrawPoly4(s.ax, s.ay, s.bx, s.by, b2x, b2y, a2x, a2y, crush, crush, crush, 1.f);
    }
    r2d.Flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::BuildLightAccum(entt::registry& registry, const Camera2D& cam)
{
    EnsureRadialTex();
    auto& pipe = PresentPipeline::GetInstance();
    auto& accum = pipe.LightAccum();
    auto& scratch = pipe.LightScratch();
    if (!accum.Valid() || !scratch.Valid())
        return;

    auto& r2d = Renderer2D::GetInstance();
    const int sw = pipe.SceneWidth();
    const int sh = pipe.SceneHeight();

    // L4.5 stub: Path D occlusion map not implemented; log once if tagged casters exist
    {
        static bool logged_path_d = false;
        if (!logged_path_d)
        {
            auto locc = registry.view<LightOccluderComponent>();
            for (auto e : locc)
            {
                const auto& lo = registry.get<LightOccluderComponent>(e);
                if (lo.enabled && lo.casts_shadow_from_sprite)
                {
                    logged_path_d = true;
                    FW_CORE_INFO("BuildOcclusionMapForLight: Path D stub (no-op); "
                                 "casts_shadow_from_sprite skips Path A only");
                    break;
                }
            }
        }
    }

    bool any_sun = false;
    {
        auto dv = registry.view<DirectionalLightComponent>();
        for (auto e : dv)
        {
            if (registry.get<DirectionalLightComponent>(e).enabled)
            {
                any_sun = true;
                break;
            }
        }
    }

    accum.Bind();
    if (any_sun)
        glClearColor(1.f, 1.f, 1.f, 1.f);
    else
        glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    r2d.BeginBatch();
    r2d.SetCamera(cam);
    r2d.SetScreenSize(sw, sh); // FULL ortho even on half-res FBO

    if (any_sun)
        ApplySunShadows(registry, r2d);

    constexpr int kMaxShadowLights = 8;
    int count = 0;
    auto view = registry.view<PointLightComponent, TransformComponent>();
    for (auto e : view)
    {
        auto& L = registry.get<PointLightComponent>(e);
        if (!L.enabled || L.additive || !L.casts_shadow)
            continue;
        auto& T = registry.get<TransformComponent>(e);
        if (!InView(T.x, T.y, L.radius, L.radius))
            continue;
        if (++count > kMaxShadowLights)
            break;

        // cutout into scratch
        scratch.Bind();
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        r2d.BeginBatch();
        r2d.SetCamera(cam);
        r2d.SetScreenSize(sw, sh);
        RenderOneLightCutout(L, T.x, T.y, r2d);
        r2d.Flush();
        r2d.EndFrame();

        // ADD scratch into accum - never multiply masks
        accum.Bind();
        pipe.BlitTexture(scratch.Color(), 1.f, 1.f, 1.f, 1.f, true);
    }
}

void Renderer::DrawOccluderDebug(entt::registry& registry)
{
    (void)registry;
    auto& r2d = Renderer2D::GetInstance();
    auto& occ = OccluderWorld::Get();
    r2d.Flush();
    for (const auto& s : occ.Statics())
    {
        r2d.DrawLine(s.ax, s.ay, s.bx, s.by, 1.f, 0.f, 1.f, 1.f);
        float mx = (s.ax + s.bx) * 0.5f;
        float my = (s.ay + s.by) * 0.5f;
        r2d.DrawLine(mx, my, mx + s.nx * 0.25f, my + s.ny * 0.25f, 1.f, 0.4f, 1.f, 1.f);
    }
    for (const auto& s : occ.Dynamics())
    {
        r2d.DrawLine(s.ax, s.ay, s.bx, s.by, 1.f, 0.2f, 0.8f, 1.f);
        float mx = (s.ax + s.bx) * 0.5f;
        float my = (s.ay + s.by) * 0.5f;
        r2d.DrawLine(mx, my, mx + s.nx * 0.25f, my + s.ny * 0.25f, 1.f, 0.5f, 1.f, 1.f);
    }
}

void Renderer::DrawLightVolumes(entt::registry& registry)
{
    auto& r2d = Renderer2D::GetInstance();
    const float alpha = SimClock::GetInstance().Alpha();
    auto view = registry.view<LightVolumeComponent, TransformComponent>();
    std::vector<entt::entity> order;
    for (auto e : view)
        order.push_back(e);
    std::sort(order.begin(), order.end(), [&](entt::entity a, entt::entity b) {
        return registry.get<LightVolumeComponent>(a).priority < registry.get<LightVolumeComponent>(b).priority;
    });

    for (auto e : order)
    {
        auto& lv = registry.get<LightVolumeComponent>(e);
        auto& t = registry.get<TransformComponent>(e);
        DrawXform d = DrawPose(t, alpha);
        r2d.Flush();
        if (lv.multiply)
        {
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            float ir = std::clamp(lv.r, 0.02f, 1.f);
            float ig = std::clamp(lv.g, 0.02f, 1.f);
            float ib = std::clamp(lv.b, 0.02f, 1.f);
            r2d.DrawSolidQuad(d.x, d.y, lv.half_w * 2.f, lv.half_h * 2.f, d.a, ir, ig, ib, 1.f);
        }
        else
        {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            float a = std::clamp(lv.intensity, 0.f, 1.f);
            if (GameSettings::GetInstance().ReduceFlash())
                a *= 0.4f;
            r2d.DrawSolidQuad(d.x, d.y, lv.half_w * 2.f, lv.half_h * 2.f, d.a, lv.r, lv.g, lv.b, a);
        }
        r2d.Flush();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}

void Renderer::DrawLightShafts(entt::registry& registry, float delta_time)
{
    auto& r2d = Renderer2D::GetInstance();
    const float alpha = SimClock::GetInstance().Alpha();
    auto view = registry.view<LightShaftComponent, TransformComponent>();
    r2d.Flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (auto e : view)
    {
        auto& sh = registry.get<LightShaftComponent>(e);
        auto& t = registry.get<TransformComponent>(e);
        DrawXform d = DrawPose(t, alpha);
        sh.scroll += delta_time;
        float a = sh.a;
        if (GameSettings::GetInstance().ReduceFlash())
            a *= 0.35f;
        // Beam centered along angle from origin
        float cx = d.x + std::cos(d.a) * sh.length * 0.5f;
        float cy = d.y + std::sin(d.a) * sh.length * 0.5f;
        float pulse = 0.85f + 0.15f * std::sin(sh.scroll * 1.7f);
        r2d.DrawSolidQuad(cx, cy, sh.length, sh.width, d.a, sh.r, sh.g, sh.b, a * pulse);
    }
    r2d.Flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::DrawApertureFrames(entt::registry& registry)
{
    (void)registry;
}

void Renderer::DrawDebugShapes(entt::registry& registry)
{
    auto& r2d = Renderer2D::GetInstance();
    const float alpha = SimClock::GetInstance().Alpha();

    auto view = registry.view<DebugDrawComponent, ColorComponent, TransformComponent, BoxShapeComponent>();
    std::vector<entt::entity> order;
    order.reserve(32);
    for (auto e : view)
    {
        if (!BandMatch(registry, e))
            continue;
        auto& dbg = registry.get<DebugDrawComponent>(e);
        auto& color = registry.get<ColorComponent>(e);
        if (color.primary.a <= 0)
            continue;
        // Texture mode never strokes outlines (DrawLine = flush + 1 draw per edge).
        if (draw_world_textures && !dbg.filled)
            continue;
        order.push_back(e);
    }
    std::sort(order.begin(), order.end(), [&](entt::entity a, entt::entity b) {
        return registry.get<DebugDrawComponent>(a).sort < registry.get<DebugDrawComponent>(b).sort;
    });

    for (auto e : order)
    {
        auto& dbg = registry.get<DebugDrawComponent>(e);
        auto& color = registry.get<ColorComponent>(e);
        auto& t = registry.get<TransformComponent>(e);
        DrawXform d = DrawPose(t, alpha);
        auto& box = registry.get<BoxShapeComponent>(e);
        float r = color.primary.r / 255.f, g = color.primary.g / 255.f, b = color.primary.b / 255.f, a = color.primary.a / 255.f;
        // strength drives soft-glow mask only (not RGB)
        if (a <= 0.001f)
            continue;
        if (!InView(d.x, d.y, box.width * 0.5f, box.height * 0.5f))
            continue;
        // Boxes-only mode: show filled quads even when scripts hid DebugDraw under sprites.
        const bool force_boxes = !draw_world_textures;
        if (dbg.filled || force_boxes)
            r2d.DrawSolidQuad(d.x, d.y, box.width, box.height, d.a, r, g, b, a);
        // Outlines: boxes-only. Texture mode + DrawLine = one glDraw per edge.
        if (dbg.outline && force_boxes)
        {
            float hw = box.width * 0.5f, hh = box.height * 0.5f;
            float c = std::cos(d.a), s = std::sin(d.a);
            auto corner = [&](float lx, float ly, float& ox, float& oy) {
                ox = d.x + lx * c - ly * s;
                oy = d.y + lx * s + ly * c;
            };
            float x0, y0, x1, y1, x2, y2, x3, y3;
            corner(-hw, -hh, x0, y0);
            corner( hw, -hh, x1, y1);
            corner( hw,  hh, x2, y2);
            corner(-hw,  hh, x3, y3);
            r2d.DrawLine(x0, y0, x1, y1, r, g, b, 1);
            r2d.DrawLine(x1, y1, x2, y2, r, g, b, 1);
            r2d.DrawLine(x2, y2, x3, y3, r, g, b, 1);
            r2d.DrawLine(x3, y3, x0, y0, r, g, b, 1);
        }
    }

    auto circles = registry.view<DebugDrawComponent, ColorComponent, TransformComponent, CircleShapeComponent>();
    for (auto e : circles)
    {
        if (!BandMatch(registry, e))
            continue;
        auto& color = registry.get<ColorComponent>(e);
        auto& t = registry.get<TransformComponent>(e);
        DrawXform d = DrawPose(t, alpha);
        auto& c = registry.get<CircleShapeComponent>(e);
        float r = color.primary.r / 255.f, g = color.primary.g / 255.f, b = color.primary.b / 255.f, a = color.primary.a / 255.f;
        if (a <= 0.001f)
            continue;
        if (!InView(d.x, d.y, c.radius, c.radius))
            continue;
        r2d.DrawSolidQuad(d.x, d.y, c.radius * 2, c.radius * 2, d.a, r, g, b, a);
    }
}

void Renderer::DrawSprites(entt::registry& registry)
{
    auto& r2d = Renderer2D::GetInstance();
    auto& atlas = WorldAtlas::GetInstance();
    const float alpha = SimClock::GetInstance().Alpha();

    struct Item
    {
        entt::entity e;
        unsigned int tex = 0;
        int sort_key = 0;
        float x = 0, y = 0, w = 0, h = 0, ang = 0;
        float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
        bool flip_h = false, flip_v = false;
    };
    std::vector<Item> items;
    items.reserve(64);

    auto view = registry.view<SpriteComponent, TransformComponent>();
    for (auto e : view)
    {
        if (!BandMatch(registry, e))
            continue;
        auto& spr = registry.get<SpriteComponent>(e);
        if (!spr.image_location)
            continue;
        ++sprites_seen;

        auto& t = registry.get<TransformComponent>(e);
        DrawXform d = DrawPose(t, alpha);

        // Frustum first (before atlas / GetTexture). World tiles are ~1×1; sky is parallax-exempt.
        const float para = EntityParallax(registry, e);
        if (para < 0.99f && !InView(d.x, d.y, 1.f, 1.f))
        {
            ++sprites_culled;
            continue;
        }

        unsigned int tex = 0;
        float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
        float w = 0.f, h = 0.f;

        WorldAtlasEntry ae;
        if (atlas.TryGet(spr.image_location, ae))
        {
            tex = ae.gl_id;
            u0 = ae.u0; v0 = ae.v0; u1 = ae.u1; v1 = ae.v1;
            w = (ae.px_w / 32.f) * spr.ppu;
            h = (ae.px_h / 32.f) * spr.ppu;
        }
        else
        {
            tex = AssetCache::GetInstance().GetGLTexture(spr.image_location);
            auto& texinfo = AssetCache::GetInstance().GetTexture(spr.image_location);
            w = (texinfo.width / 32.f) * spr.ppu;
            h = (texinfo.height / 32.f) * spr.ppu;
        }
        if (!tex || w <= 0.f || h <= 0.f)
            continue;

        // Tight cull now that we know the real quad (large sheets / sky already exempt above).
        if (para < 0.99f && !InView(d.x, d.y, w * 0.5f, h * 0.5f))
        {
            ++sprites_culled;
            continue;
        }

        Item it;
        it.e = e;
        it.tex = tex;
        it.sort_key = spr.sort_key;
        it.x = d.x;
        it.y = d.y;
        it.w = w;
        it.h = h;
        it.ang = d.a;
        it.u0 = u0; it.v0 = v0; it.u1 = u1; it.v1 = v1;
        it.flip_h = spr.flip_horizontal;
        it.flip_v = spr.flip_vertical;
        items.push_back(it);
    }

    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.tex != b.tex)
            return a.tex < b.tex;
        if (a.sort_key != b.sort_key)
            return a.sort_key < b.sort_key;
        return (uint32_t)a.e < (uint32_t)b.e;
    });

    for (const auto& it : items)
    {
        r2d.DrawQuad(it.tex, it.x, it.y, it.w, it.h, it.u0, it.v0, it.u1, it.v1, it.ang,
                     1, 1, 1, 1, true, it.flip_h, it.flip_v);
        ++sprites_drawn;
    }
}

void Renderer::DrawSpriteGrids(entt::registry& registry)
{
    auto& r2d = Renderer2D::GetInstance();
    auto& atlas = WorldAtlas::GetInstance();
    const float alpha = SimClock::GetInstance().Alpha();

    struct Cell
    {
        unsigned int tex = 0;
        int sort_key = 0;
        float x = 0, y = 0, w = 0, h = 0;
        float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
        uint32_t order = 0;
    };
    std::vector<Cell> cells;
    cells.reserve(512);

    auto view = registry.view<SpriteGridComponent, TransformComponent>();
    for (auto e : view)
    {
        if (!BandMatch(registry, e))
            continue;
        auto& grid = registry.get<SpriteGridComponent>(e);
        if (!grid.visible || !grid.body_path || grid.cols <= 0 || grid.rows <= 0)
            continue;

        auto& t = registry.get<TransformComponent>(e);
        DrawXform d = DrawPose(t, alpha);
        const float cell = grid.cell > 0.01f ? grid.cell : 1.f;
        const float gw = grid.cols * cell;
        const float gh = grid.rows * cell;
        ++sprites_seen;
        if (!InView(d.x, d.y, gw * 0.5f, gh * 0.5f))
        {
            ++sprites_culled;
            continue;
        }

        const float left = d.x - gw * 0.5f;
        const float top = d.y - gh * 0.5f;

        auto resolve = [&](const char* path, WorldAtlasEntry& ae, unsigned int& tex, float& w, float& h,
                           float& u0, float& v0, float& u1, float& v1) -> bool {
            if (!path)
                return false;
            if (atlas.TryGet(path, ae))
            {
                tex = ae.gl_id;
                u0 = ae.u0; v0 = ae.v0; u1 = ae.u1; v1 = ae.v1;
                w = (ae.px_w / 32.f) * grid.ppu;
                h = (ae.px_h / 32.f) * grid.ppu;
                return tex != 0;
            }
            tex = AssetCache::GetInstance().GetGLTexture(path);
            auto& info = AssetCache::GetInstance().GetTexture(path);
            w = (info.width / 32.f) * grid.ppu;
            h = (info.height / 32.f) * grid.ppu;
            u0 = 0; v0 = 0; u1 = 1; v1 = 1;
            return tex != 0;
        };

        WorldAtlasEntry body_ae, top_ae;
        unsigned int body_tex = 0, top_tex = 0;
        float bw = 0, bh = 0, tw = 0, th = 0;
        float bu0 = 0, bv0 = 0, bu1 = 1, bv1 = 1;
        float tu0 = 0, tv0 = 0, tu1 = 1, tv1 = 1;
        if (!resolve(grid.body_path, body_ae, body_tex, bw, bh, bu0, bv0, bu1, bv1))
            continue;
        const bool sheet = grid.sheet_cols > 0 && grid.sheet_rows > 0;
        const bool has_top = !sheet && grid.top_path != nullptr
            && resolve(grid.top_path, top_ae, top_tex, tw, th, tu0, tv0, tu1, tv1);

        int sheet_px_w = 0, sheet_px_h = 0;
        if (sheet)
        {
            if (body_ae.px_w > 0 && body_ae.px_h > 0)
            {
                sheet_px_w = body_ae.px_w;
                sheet_px_h = body_ae.px_h;
            }
            else
            {
                auto& info = AssetCache::GetInstance().GetTexture(grid.body_path);
                sheet_px_w = info.width;
                sheet_px_h = info.height;
            }
        }

        auto sheet_uv = [&](int frame, float& u0, float& v0, float& u1, float& v1, float& cw, float& ch) {
            const int sc = grid.sheet_cols;
            const int sr = grid.sheet_rows;
            const int cell_w = sheet_px_w / sc;
            const int cell_h = sheet_px_h / sr;
            const int n = sc * sr;
            if (cell_w <= 0 || cell_h <= 0 || frame < 0 || frame >= n)
                return false;
            const int sx = (frame % sc) * cell_w;
            const int sy = (frame / sc) * cell_h;
            cw = (cell_w / 32.f) * grid.ppu;
            ch = (cell_h / 32.f) * grid.ppu;
            if (body_ae.px_w > 0 && body_ae.px_h > 0)
            {
                const float du = body_ae.u1 - body_ae.u0;
                const float dv = body_ae.v1 - body_ae.v0;
                u0 = body_ae.u0 + (sx / (float)body_ae.px_w) * du;
                v0 = body_ae.v0 + (sy / (float)body_ae.px_h) * dv;
                u1 = body_ae.u0 + ((sx + cell_w) / (float)body_ae.px_w) * du;
                v1 = body_ae.v0 + ((sy + cell_h) / (float)body_ae.px_h) * dv;
            }
            else
            {
                u0 = sx / (float)sheet_px_w;
                v0 = sy / (float)sheet_px_h;
                u1 = (sx + cell_w) / (float)sheet_px_w;
                v1 = (sy + cell_h) / (float)sheet_px_h;
            }
            return true;
        };

        for (int iy = 0, i = 0; iy < grid.rows; ++iy)
        {
            for (int ix = 0; ix < grid.cols; ++ix, ++i)
            {
                const float cx = left + (ix + 0.5f) * cell;
                const float cy = top + (iy + 0.5f) * cell;
                Cell c;
                c.tex = body_tex;
                c.sort_key = grid.sort_key;
                c.x = cx;
                c.y = cy;
                c.order = (uint32_t)e;
                if (sheet)
                {
                    int frame = 0;
                    if (!grid.frames.empty())
                    {
                        if (i >= (int)grid.frames.size())
                            continue;
                        frame = grid.frames[i];
                        if (frame < 0)
                            continue;
                    }
                    if (!sheet_uv(frame, c.u0, c.v0, c.u1, c.v1, c.w, c.h))
                        continue;
                }
                else
                {
                    const bool use_top = has_top && iy == 0;
                    c.tex = use_top ? top_tex : body_tex;
                    c.w = use_top ? tw : bw;
                    c.h = use_top ? th : bh;
                    if (use_top)
                    {
                        c.u0 = tu0; c.v0 = tv0; c.u1 = tu1; c.v1 = tv1;
                    }
                    else
                    {
                        c.u0 = bu0; c.v0 = bv0; c.u1 = bu1; c.v1 = bv1;
                    }
                }
                if (!InView(c.x, c.y, c.w * 0.5f, c.h * 0.5f))
                {
                    ++sprites_culled;
                    continue;
                }
                cells.push_back(c);
            }
        }
    }

    std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) {
        if (a.tex != b.tex)
            return a.tex < b.tex;
        if (a.sort_key != b.sort_key)
            return a.sort_key < b.sort_key;
        return a.order < b.order;
    });

    for (const auto& c : cells)
    {
        r2d.DrawQuad(c.tex, c.x, c.y, c.w, c.h, c.u0, c.v0, c.u1, c.v1, 0, 1, 1, 1, 1, true);
        ++sprites_drawn;
    }
}

void Renderer::DrawAnimations(entt::registry& registry, float delta_time)
{
    // Same submit shape as DrawSprites: atlas when packed, camera cull, texture-sort.
    auto& r2d = Renderer2D::GetInstance();
    auto& atlas = WorldAtlas::GetInstance();
    const float alpha = SimClock::GetInstance().Alpha();

    struct Item
    {
        entt::entity e;
        unsigned int tex = 0;
        unsigned int pal = 0;
        int pal_w = 16, pal_h = 16;
        float x = 0, y = 0, w = 0, h = 0, ang = 0;
        float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
        float cr = 1, cg = 1, cb = 1, ca = 1;
        bool flip_h = false, flip_v = false;
    };
    std::vector<Item> items;
    items.reserve(64);

    auto view = registry.view<AnimationComponent, SpriteSheetComponent, TransformComponent>();
    for (auto e : view)
    {
        if (!BandMatch(registry, e))
            continue;
        auto& anim = registry.get<AnimationComponent>(e);
        auto& sheet = registry.get<SpriteSheetComponent>(e);
        if (!anim.enabled || !sheet.visible || !sheet.image_location || sheet.sprites.empty())
            continue;

        auto& t = registry.get<TransformComponent>(e);
        DrawXform d = DrawPose(t, alpha);
        ++sprites_seen;

        float draw_x = d.x + sheet.offset_x;
        float draw_y = d.y + sheet.offset_y;
        const SDL_Rect& src0 = sheet.sprites[0];
        const float cull_w = (src0.w / 32.f) * sheet.ppu;
        const float cull_h = (src0.h / 32.f) * sheet.ppu;
        const float para = EntityParallax(registry, e);
        if (para < 0.99f && !InView(draw_x, draw_y, cull_w * 0.5f, cull_h * 0.5f))
        {
            ++sprites_culled;
            continue;
        }

        const int n = (int)sheet.sprites.size();
        int start = anim.frame_start;
        if (start < 0)
            start = 0;
        if (start >= n)
            start = n - 1;
        int end = anim.frame_end < 0 ? (n - 1) : anim.frame_end;
        if (end < start)
            end = start;
        if (end >= n)
            end = n - 1;
        const int span = end - start + 1;

        if (anim.current_frame < (float)start || anim.current_frame > (float)end + 0.999f)
            anim.current_frame = (float)start;

        if (anim.play && delta_time > 0.f && span > 0 && !SimClock::GetInstance().Paused())
        {
            anim.current_frame += anim.frame_rate * delta_time;
            if (anim.current_frame > (float)end + 0.999f)
            {
                if (anim.looping)
                    anim.current_frame = (float)start + std::fmod(anim.current_frame - (float)start, (float)span);
                else
                {
                    anim.current_frame = (float)end;
                    anim.play = false;
                }
            }
        }

        int fi = (int)anim.current_frame;
        if (fi < start)
            fi = start;
        if (fi > end)
            fi = end;

        const SDL_Rect& src = sheet.sprites[fi];
        unsigned int tex = 0;
        float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
        float w = 0.f, h = 0.f;

        WorldAtlasEntry ae;
        if (atlas.TryGet(sheet.image_location, ae) && ae.px_w > 0 && ae.px_h > 0)
        {
            tex = ae.gl_id;
            const float du = ae.u1 - ae.u0;
            const float dv = ae.v1 - ae.v0;
            u0 = ae.u0 + (src.x / (float)ae.px_w) * du;
            v0 = ae.v0 + (src.y / (float)ae.px_h) * dv;
            u1 = ae.u0 + ((src.x + src.w) / (float)ae.px_w) * du;
            v1 = ae.v0 + ((src.y + src.h) / (float)ae.px_h) * dv;
            w = (src.w / 32.f) * sheet.ppu;
            h = (src.h / 32.f) * sheet.ppu;
        }
        else
        {
            auto& texinfo = AssetCache::GetInstance().GetTexture(sheet.image_location);
            tex = texinfo.gl_id;
            if (texinfo.width > 0 && texinfo.height > 0)
            {
                u0 = (float)src.x / texinfo.width;
                v0 = (float)src.y / texinfo.height;
                u1 = (float)(src.x + src.w) / texinfo.width;
                v1 = (float)(src.y + src.h) / texinfo.height;
            }
            w = (src.w / 32.f) * sheet.ppu;
            h = (src.h / 32.f) * sheet.ppu;
        }
        if (!tex || w <= 0.f || h <= 0.f)
            continue;

        if (sheet.align_feet && registry.has<BoxShapeComponent>(e))
        {
            const float bh = registry.get<BoxShapeComponent>(e).height;
            draw_y += (bh - h) * 0.5f;
        }

        Item it;
        it.e = e;
        it.tex = tex;
        it.pal = 0;
        it.pal_w = 16;
        it.pal_h = 16;
        if (sheet.palette_location && sheet.palette_location[0])
        {
            auto& paltex = AssetCache::GetInstance().GetTexture(sheet.palette_location);
            it.pal = paltex.gl_id;
            if (paltex.width > 0)
                it.pal_w = paltex.width;
            if (paltex.height > 0)
                it.pal_h = paltex.height;
        }
        it.x = draw_x;
        it.y = draw_y;
        it.w = w;
        it.h = h;
        it.ang = d.a + sheet.angle_offset;
        it.u0 = u0; it.v0 = v0; it.u1 = u1; it.v1 = v1;
        it.flip_h = sheet.flip_horizontal;
        it.flip_v = sheet.flip_vertical;
        if (registry.has<ColorComponent>(e))
        {
            auto& col = registry.get<ColorComponent>(e);
            it.cr = col.primary.r / 255.f;
            it.cg = col.primary.g / 255.f;
            it.cb = col.primary.b / 255.f;
            it.ca = col.primary.a / 255.f;
        }
        items.push_back(it);
    }

    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.tex != b.tex)
            return a.tex < b.tex;
        if (a.pal != b.pal)
            return a.pal < b.pal;
        return (uint32_t)a.e < (uint32_t)b.e;
    });

    for (const auto& it : items)
    {
        r2d.DrawQuad(it.tex, it.x, it.y, it.w, it.h, it.u0, it.v0, it.u1, it.v1, it.ang,
                     it.cr, it.cg, it.cb, it.ca, true, it.flip_h, it.flip_v, it.pal, it.pal_w, it.pal_h);
        ++sprites_drawn;
    }
}

void Renderer::DrawTileMaps(entt::registry& registry)
{
    auto& r2d = Renderer2D::GetInstance();
    const float alpha = SimClock::GetInstance().Alpha();
    auto view = registry.view<TileMapComponent, SpriteSheetComponent, TransformComponent>();
    for (auto e : view)
    {
        if (!BandMatch(registry, e))
            continue;
        auto& tile = registry.get<TileMapComponent>(e);
        auto& sheet = registry.get<SpriteSheetComponent>(e);
        auto& t = registry.get<TransformComponent>(e);
        DrawXform d = DrawPose(t, alpha);
        if (!tile.tile_map_location || !sheet.image_location || sheet.sprites.empty())
            continue;

        const auto& grid = AssetCache::GetInstance().GetJson(tile.tile_map_location);
        if (!grid.is_array())
            continue;

        auto& tex = AssetCache::GetInstance().GetTexture(sheet.image_location);
        unsigned int id = tex.gl_id;
        if (!id || tex.width <= 0 || tex.height <= 0)
            continue;

        int y = 0;
        for (auto& row : grid)
        {
            int x = 0;
            for (auto& cell : row)
            {
                int element = cell.get<int>();
                if (element < 0 || element >= (int)sheet.sprites.size())
                {
                    x++;
                    continue;
                }
                SDL_Rect src = sheet.sprites[element];
                float u0 = (float)src.x / tex.width;
                float v0 = (float)src.y / tex.height;
                float u1 = (float)(src.x + src.w) / tex.width;
                float v1 = (float)(src.y + src.h) / tex.height;
                float w = (src.w / 32.f) * sheet.ppu;
                float h = (src.h / 32.f) * sheet.ppu;
                r2d.DrawQuad(id, d.x + x, d.y + y, w, h, u0, v0, u1, v1, 0, 1, 1, 1, 1);
                x++;
            }
            y++;
        }
    }
}

void Renderer::DrawText(entt::registry& registry)
{
    auto& r2d = Renderer2D::GetInstance();
    r2d.Flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    auto view = registry.view<TextComponent, FontComponent, ColorComponent, TransformComponent>();
    for (auto e : view)
    {
        auto& text = registry.get<TextComponent>(e);
        if (m_screen_text_only && !text.screen_space)
            continue;
        if (m_world_text_only && text.screen_space)
            continue;
        if (m_world_text_only && !BandMatch(registry, e))
            continue;

        auto& fontc = registry.get<FontComponent>(e);
        auto& color = registry.get<ColorComponent>(e);
        auto& t = registry.get<TransformComponent>(e);
        if (!fontc.font_location || text.text.empty())
            continue;

        const std::string key = std::string(fontc.font_location) + "|" +
            std::to_string(fontc.size) + "|" + (fontc.aa ? "1|" : "0|") + text.text;
        if (text.cached_tex == 0 || text.cache_key != key)
        {
            if (text.cached_tex)
            {
                GLuint old = text.cached_tex;
                glDeleteTextures(1, &old);
                text.cached_tex = 0;
            }

            TTF_Font* font = AssetCache::GetInstance().GetFont(fontc.font_location, fontc.size);
            if (!font)
                continue;

            SDL_Color white{255, 255, 255, 255};
            // UTF-8: TTF_RenderText_* is Latin-1 and mojibakes multi-byte glyphs (e.g. · → Â·).
            SDL_Surface* surf = fontc.aa
                ? TTF_RenderUTF8_Blended(font, text.text.c_str(), white)
                : TTF_RenderUTF8_Solid(font, text.text.c_str(), white);
            if (!surf)
                continue;

            SDL_Surface* rgba = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(surf);
            if (!rgba)
                continue;

            GLuint id = 0;
            glGenTextures(1, &id);
            glBindTexture(GL_TEXTURE_2D, id);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            std::vector<unsigned char> packed((size_t)rgba->w * (size_t)rgba->h * 4);
            const int src_pitch = rgba->pitch;
            const unsigned char* src = static_cast<const unsigned char*>(rgba->pixels);
            for (int row = 0; row < rgba->h; ++row)
                std::memcpy(packed.data() + (size_t)row * (size_t)rgba->w * 4,
                            src + row * src_pitch,
                            (size_t)rgba->w * 4);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, packed.data());
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

            text.cached_tex = id;
            text.cached_w = rgba->w;
            text.cached_h = rgba->h;
            text.cache_key = key;
            SDL_FreeSurface(rgba);
        }

        float r = color.primary.r / 255.f, g = color.primary.g / 255.f, b = color.primary.b / 255.f, a = color.primary.a / 255.f;
        DrawXform d = text.screen_space ? DrawXform{t.x, t.y, t.a} : DrawPose(t, SimClock::GetInstance().Alpha());
        if (text.screen_space)
        {
            float w = (float)text.cached_w;
            float h = (float)text.cached_h;
            r2d.DrawQuad(text.cached_tex, d.x + w * 0.5f, d.y + h * 0.5f, w, h, 0, 0, 1, 1, 0, r, g, b, a, false);
        }
        else
        {
            float w = text.cached_w / 32.f;
            float h = text.cached_h / 32.f;
            r2d.DrawQuad(text.cached_tex, d.x + w * 0.5f, d.y + h * 0.5f, w, h, 0, 0, 1, 1, 0, r, g, b, a);
        }
        r2d.Flush();
    }
}

void Renderer::DrawColliders(entt::registry& registry)
{
    auto& r2d = Renderer2D::GetInstance();
    auto view = registry.view<RigidBody2DComponent, TransformComponent>();
    for (auto e : view)
    {
        auto& rb = registry.get<RigidBody2DComponent>(e);
        if (!rb.body)
            continue;
        for (b2Fixture* f = rb.body->GetFixtureList(); f; f = f->GetNext())
        {
            if (f->IsSensor() || f->GetType() != b2Shape::e_polygon)
                continue;
            auto* poly = static_cast<b2PolygonShape*>(f->GetShape());
            const b2Transform& xf = rb.body->GetTransform();
            for (int i = 0; i < poly->m_count; ++i)
            {
                b2Vec2 a = b2Mul(xf, poly->m_vertices[i]);
                b2Vec2 b = b2Mul(xf, poly->m_vertices[(i + 1) % poly->m_count]);
                r2d.DrawLine(a.x, a.y, b.x, b.y, 0.2f, 1.f, 0.35f, 1.f);
            }
        }
    }
}

void Renderer::DrawParticles(entt::registry& registry)
{
    auto view = registry.view<ParticleEmitterComponent, ParticleBufferComponent, TransformComponent>();
    bool any = false;
    for (auto e : view)
    {
        if (!BandMatch(registry, e))
            continue;
        if (registry.get<ParticleBufferComponent>(e).particles.empty())
            continue;
        any = true;
        break;
    }
    if (!any)
        return;

    auto& r2d = Renderer2D::GetInstance();
    r2d.Flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (auto e : view)
    {
        if (!BandMatch(registry, e))
            continue;
        auto& em = registry.get<ParticleEmitterComponent>(e);
        auto& buf = registry.get<ParticleBufferComponent>(e);
        if (buf.particles.empty())
            continue;
        unsigned int id = em.image_location
            ? AssetCache::GetInstance().GetGLTexture(em.image_location)
            : r2d.WhiteTexture();
        for (auto& p : buf.particles)
        {
            float life_a = p.life / p.max_life;
            float s = p.size * (0.6f + 0.8f * life_a);
            r2d.DrawQuad(id, p.x, p.y, s, s, 0, 0, 1, 1, 0,
                         em.r, em.g, em.b, em.a * life_a);
        }
    }
    r2d.Flush();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
