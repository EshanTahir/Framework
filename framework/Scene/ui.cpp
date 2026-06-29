#include "systems.h"
#include "Core/assetCache.h"
#include "Core/glContext.h"
#include "Core/log.h"
#include "Render/gl.h"
#include "Render/renderer2d.h"
#include "sceneHandler.h"

#include "box2d.h"
#include "SDL.h"
#include "SDL_ttf.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

void Ui::ProcessInput(entt::registry& registry)
{
    UpdateButtons(registry);
    HandleButtons(registry);
}

void Ui::Run(entt::registry& registry, float delta_time)
{
    (void)delta_time;
    DrawOverlays(registry);
    DrawChrome(registry);
}

void Ui::DrawOverlays(entt::registry& registry)
{
    DrawButtons(registry, true);
}

void Ui::DrawChrome(entt::registry& registry)
{
    DrawButtons(registry, false);
}

void Ui::UpdateButtons(entt::registry& registry)
{
    auto view = registry.view<ButtonComponent>();
    for (auto e : view)
    {
        auto& b = registry.get<ButtonComponent>(e);
        Rect::Update(b.rect, b.rect.x, b.rect.y);
    }
}

void Ui::HandleButtons(entt::registry& registry)
{
    int mx, my;
    Uint32 mouse = SDL_GetMouseState(&mx, &my);
    bool left_down = (mouse & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

    auto view = registry.view<ButtonComponent>();
    for (auto e : view)
    {
        auto& b = registry.get<ButtonComponent>(e);
        if (!b.active)
        {
            b.hover = false;
            b.clicked_down = false;
            // Keep sticky clicked_up so a press that raced deactivation still reaches scripts.
            b.pressed = false;
            continue;
        }

        bool inside = mx >= b.rect.left && mx <= b.rect.right && my >= b.rect.top && my <= b.rect.bottom;
        b.hover = inside;
        b.clicked_down = false;
        // clicked_up is sticky until ui_clicked() consumes it (survives 0-sim display frames).

        if (inside && input_event.MouseButtonDownThisFrame(b.click_button))
        {
            b.clicked_down = true;
            b.pressed = true;
        }
        if (b.pressed && input_event.MouseButtonUpThisFrame(b.click_button))
        {
            if (inside)
                b.clicked_up = true;
            b.pressed = false;
        }
        // Release without a poll edge (focus loss / missed up) - drop press, no click.
        if (!left_down && b.click_button == SDL_BUTTON_LEFT)
            b.pressed = false;
    }
}

void Ui::DrawButtons(entt::registry& registry, bool overlays_only)
{
    auto& r2d = Renderer2D::GetInstance();
    auto& cache = AssetCache::GetInstance();
    auto view = registry.view<ButtonComponent>();
    for (auto e : view)
    {
        auto& b = registry.get<ButtonComponent>(e);
        if (!b.active)
            continue;
        if (b.overlay != overlays_only)
            continue;

        const ColorComponent* fill = &b.color;
        const ColorComponent* tcol = &b.text_color;
        // Overlays keep a flat fill (no hover wash / accent).
        if (!b.overlay)
        {
            if (b.pressed)
            {
                fill = &b.selected_color;
                tcol = &b.selected_text_color;
            }
            else if (b.hover)
            {
                fill = &b.hover_color;
                tcol = &b.hover_text_color;
            }
        }

        float x = b.rect.x;
        float y = b.rect.y;
        float w = (float)b.rect.w;
        float h = (float)b.rect.h;
        float cx = x + w * 0.5f;
        float cy = y + h * 0.5f;

        if (b.outline)
        {
            float ow = b.outline_width;
            float or_ = b.outline_color.primary.r / 255.f;
            float og = b.outline_color.primary.g / 255.f;
            float ob = b.outline_color.primary.b / 255.f;
            float oa = b.outline_color.primary.a / 255.f;
            r2d.DrawSolidQuad(cx, cy, w + ow * 2.f, h + ow * 2.f, 0, or_, og, ob, oa, false);
        }

        float fr = fill->primary.r / 255.f;
        float fg = fill->primary.g / 255.f;
        float fb = fill->primary.b / 255.f;
        float fa = fill->primary.a / 255.f;
        if (fa > 0.01f)
            r2d.DrawSolidQuad(cx, cy, w, h, 0, fr, fg, fb, fa, false);

        // Violet left accent on interactive chrome only (not full-screen dim).
        if (b.accent && !b.overlay && (b.hover || b.pressed))
        {
            const float aw = 4.f;
            r2d.DrawSolidQuad(x + aw * 0.5f, cy, aw, h, 0, 0.71f, 0.48f, 0.91f, 1.f, false);
        }

        if (b.text.empty() || !b.font_location)
            continue;

        const std::string key = std::string(b.font_location) + "|" +
            std::to_string(b.font_size) + "|" + (b.aa_font ? "1|" : "0|") + b.text;
        if (b.cached_tex == 0 || b.cache_key != key)
        {
            if (b.cached_tex)
            {
                GLuint old = b.cached_tex;
                glDeleteTextures(1, &old);
                b.cached_tex = 0;
            }

            TTF_Font* font = cache.GetFont(b.font_location, b.font_size);
            if (!font)
                continue;

            SDL_Color white{255, 255, 255, 255};
            SDL_Surface* surf = b.aa_font
                ? TTF_RenderUTF8_Blended(font, b.text.c_str(), white)
                : TTF_RenderUTF8_Solid(font, b.text.c_str(), white);
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
            const unsigned char* src = static_cast<const unsigned char*>(rgba->pixels);
            for (int row = 0; row < rgba->h; ++row)
                std::memcpy(packed.data() + (size_t)row * (size_t)rgba->w * 4,
                            src + row * rgba->pitch,
                            (size_t)rgba->w * 4);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, packed.data());
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

            b.cached_tex = id;
            b.cached_w = rgba->w;
            b.cached_h = rgba->h;
            b.cache_key = key;
            SDL_FreeSurface(rgba);
        }

        float tw = (float)b.cached_w;
        float th = (float)b.cached_h;
        float tr = tcol->primary.r / 255.f;
        float tg = tcol->primary.g / 255.f;
        float tb = tcol->primary.b / 255.f;
        float ta = tcol->primary.a / 255.f;
        float tx = cx;
        if (b.text_align == 1)
            tx = x + 28.f + tw * 0.5f;
        r2d.DrawQuad(b.cached_tex, tx, cy, tw, th, 0, 0, 1, 1, 0, tr, tg, tb, ta, false);
        r2d.Flush();
    }
}

