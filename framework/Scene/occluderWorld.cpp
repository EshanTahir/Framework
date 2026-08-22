#include "occluderWorld.h"
#include "components.h"
#include "Core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>

namespace
{
    constexpr float kCell = 4.f;
    constexpr float kParked = 9000.f;

    bool StartsWith(std::string_view t, std::string_view p)
    {
        return t.size() >= p.size() && t.compare(0, p.size(), p) == 0;
    }

    bool IsClosedDoorPiece(std::string_view tag)
    {
        if (!StartsWith(tag, "door_"))
            return false;
        if (StartsWith(tag, "door_mute_") || StartsWith(tag, "door_portal_")
            || StartsWith(tag, "door_grav_"))
            return false;
        return tag.find("_top") != std::string_view::npos
            || tag.find("_bot") != std::string_view::npos
            || tag.find("_L") != std::string_view::npos
            || tag.find("_R") != std::string_view::npos;
    }

    bool IsParked(const TransformComponent& T)
    {
        return std::fabs(T.x) >= kParked || std::fabs(T.y) >= kParked;
    }

    bool IsDynamicTag(std::string_view tag)
    {
        return StartsWith(tag, "move_")
            || StartsWith(tag, "elev_")
            || StartsWith(tag, "fallplat_")
            || StartsWith(tag, "crusher_")
            || StartsWith(tag, "vanish_")
            || StartsWith(tag, "launch_block_")
            || StartsWith(tag, "bounce_cloud_")
            || StartsWith(tag, "killbrick_");
    }

    void EmitEdge(std::vector<OccluderSegment>& out,
                  float ax, float ay, float bx, float by,
                  float nx, float ny, uint32_t id, bool dyn)
    {
        float dx = bx - ax, dy = by - ay;
        if (dx * dx + dy * dy < 1e-8f)
            return;
        out.push_back(OccluderSegment{ax, ay, bx, by, nx, ny, id, dyn});
    }

    void EmitPolygonEdges(std::vector<OccluderSegment>& out,
                          float tx, float ty,
                          const std::vector<float>& local_xy,
                          uint32_t id)
    {
        const int n = (int)(local_xy.size() / 2);
        if (n < 3)
            return;
        for (int i = 0; i < n; ++i)
        {
            int j = (i + 1) % n;
            float ax = tx + local_xy[i * 2];
            float ay = ty + local_xy[i * 2 + 1];
            float bx = tx + local_xy[j * 2];
            float by = ty + local_xy[j * 2 + 1];
            float ex = bx - ax, ey = by - ay;
            float len = std::sqrt(ex * ex + ey * ey);
            if (len < 1e-6f)
                continue;
            // CCW local verts, Y-down: rotate edge 90 deg CW for outward
            float nx = ey / len;
            float ny = -ex / len;
            EmitEdge(out, ax, ay, bx, by, nx, ny, id, false);
        }
    }

    int CellOf(float v)
    {
        return (int)std::floor(v / kCell);
    }
}

OccluderWorld& OccluderWorld::Get()
{
    static OccluderWorld i;
    return i;
}

OccludeMode ClassifyOccluder(entt::registry& reg, entt::entity e)
{
    if (!reg.valid(e) || !reg.has<TagComponent>(e) || !reg.has<Collider2DComponent>(e)
        || !reg.has<TransformComponent>(e))
        return OccludeMode::None;

    const auto& tag = reg.get<TagComponent>(e).tag;
    const auto& col = reg.get<Collider2DComponent>(e);
    const auto& T = reg.get<TransformComponent>(e);

    if (IsParked(T))
        return OccludeMode::None;

    // NEVER: sensors / climb volumes / fluids / spikes-as-hull / player / mute
    if (col.sensor)
        return OccludeMode::None;
    if (StartsWith(tag, "wallrun_"))
        return OccludeMode::None;
    if (StartsWith(tag, "mute_"))
        return OccludeMode::None;
    if (StartsWith(tag, "door_mute_"))
        return OccludeMode::None;
    if (StartsWith(tag, "ladder_") && !StartsWith(tag, "ladder_side_"))
        return OccludeMode::None;
    if (StartsWith(tag, "pole_") || StartsWith(tag, "ice_") || StartsWith(tag, "tar_")
        || StartsWith(tag, "water_") || StartsWith(tag, "poison_"))
        return OccludeMode::None;
    if (StartsWith(tag, "door_portal_") || StartsWith(tag, "door_grav_"))
        return OccludeMode::None;
    if (StartsWith(tag, "spike_"))
        return OccludeMode::None;
    if (StartsWith(tag, "player") || StartsWith(tag, "coin_")
        || StartsWith(tag, "laser_") || StartsWith(tag, "mine_"))
        return OccludeMode::None;

    // L1 static YES
    if (StartsWith(tag, "solid_"))
        return OccludeMode::FullBox;
    if (StartsWith(tag, "depth1_solid_"))
        return OccludeMode::FullBox;
    if (StartsWith(tag, "ladder_side_"))
        return OccludeMode::FullBox;
    if (IsClosedDoorPiece(tag))
        return OccludeMode::FullBox;

    // one-way top only - L1 static bake skips these (L6 dynamics)
    if (col.one_way && (StartsWith(tag, "move_") || StartsWith(tag, "elev_")
                        || StartsWith(tag, "fallplat_")))
        return OccludeMode::TopEdgeOnly;

    return OccludeMode::None;
}

void EmitBoxEdges(std::vector<OccluderSegment>& out,
                  float cx, float cy, float hw, float hh,
                  uint32_t id, bool dyn, OccludeMode mode)
{
    // Y-down: T = cy-hh is visually up the screen
    float L = cx - hw, R = cx + hw, T = cy - hh, B = cy + hh;
    if (mode == OccludeMode::TopEdgeOnly)
    {
        EmitEdge(out, L, T, R, T, 0.f, -1.f, id, dyn);
        return;
    }
    EmitEdge(out, L, T, L, B, -1.f, 0.f, id, dyn);
    EmitEdge(out, L, B, R, B, 0.f, +1.f, id, dyn);
    EmitEdge(out, R, B, R, T, +1.f, 0.f, id, dyn);
    EmitEdge(out, R, T, L, T, 0.f, -1.f, id, dyn);
}

void CancelSharedFaces(std::vector<OccluderSegment>& segs)
{
    struct Key
    {
        int64_t x0, y0, x1, y1;
        bool operator==(const Key& o) const
        {
            return x0 == o.x0 && y0 == o.y0 && x1 == o.x1 && y1 == o.y1;
        }
    };
    struct Hash
    {
        size_t operator()(const Key& k) const
        {
            size_t h = (size_t)k.x0 * 1315423911u;
            h ^= (size_t)k.y0 + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= (size_t)k.x1 + 0x9e3779b9 + (h << 6) + (h >> 2);
            h ^= (size_t)k.y1 + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

    auto quant = [](float v) -> int64_t {
        return (int64_t)std::llround((double)v * 1000.0);
    };

    struct Acc
    {
        int sign = 0;
        OccluderSegment rep{};
    };
    std::unordered_map<Key, Acc, Hash> map;
    map.reserve(segs.size());

    for (const auto& s : segs)
    {
        int64_t ax = quant(s.ax), ay = quant(s.ay), bx = quant(s.bx), by = quant(s.by);
        if (ax > bx || (ax == bx && ay > by))
        {
            std::swap(ax, bx);
            std::swap(ay, by);
        }
        Key key{ax, ay, bx, by};
        int sign = 0;
        if (std::fabs(s.nx) >= std::fabs(s.ny))
            sign = (s.nx >= 0.f) ? 1 : -1;
        else
            sign = (s.ny >= 0.f) ? 1 : -1;

        auto& acc = map[key];
        if (acc.sign == 0)
            acc.rep = s;
        acc.sign += sign;
        if (acc.sign > 0)
            acc.rep = s;
    }

    std::vector<OccluderSegment> kept;
    kept.reserve(map.size());
    for (auto& kv : map)
    {
        if (kv.second.sign == 0)
            continue;
        kept.push_back(kv.second.rep);
        auto& s = kept.back();
        if (std::fabs(s.nx) >= std::fabs(s.ny))
        {
            s.nx = (kv.second.sign > 0) ? 1.f : -1.f;
            s.ny = 0.f;
        }
        else
        {
            s.nx = 0.f;
            s.ny = (kv.second.sign > 0) ? 1.f : -1.f;
        }
    }
    segs.swap(kept);
}

void OccluderWorld::BuildSpatialHash()
{
    m_hash.clear();
    for (size_t i = 0; i < m_static.size(); ++i)
    {
        const auto& s = m_static[i];
        int x0 = CellOf(std::min(s.ax, s.bx));
        int x1 = CellOf(std::max(s.ax, s.bx));
        int y0 = CellOf(std::min(s.ay, s.by));
        int y1 = CellOf(std::max(s.ay, s.by));
        for (int cx = x0; cx <= x1; ++cx)
            for (int cy = y0; cy <= y1; ++cy)
                m_hash[{cx, cy}].push_back(i);
    }
}

void OccluderWorld::EmitPathB(entt::registry& reg)
{
    auto view = reg.view<TransformComponent, LightOccluderComponent>();
    for (auto e : view)
    {
        auto& lo = view.get<LightOccluderComponent>(e);
        if (!lo.enabled)
            continue;
        const auto& T = view.get<TransformComponent>(e);
        if (IsParked(T))
            continue;

        uint32_t id = lo.solid_id ? lo.solid_id : (uint32_t)entt::to_integral(e);
        switch (lo.source)
        {
        case LightOccluderComponent::Source::Box:
        {
            OccludeMode mode = lo.one_way_top_only ? OccludeMode::TopEdgeOnly
                                                   : OccludeMode::FullBox;
            EmitBoxEdges(m_static, T.x, T.y, lo.half_w, lo.half_h, id, false, mode);
            break;
        }
        case LightOccluderComponent::Source::Polygon:
            EmitPolygonEdges(m_static, T.x, T.y, lo.local_xy, id);
            break;
        case LightOccluderComponent::Source::FromCollider:
            // emitted in Path A when physics present
            break;
        case LightOccluderComponent::Source::SpriteSilhouette:
            // Path C: outline not baked here yet; use local_xy if authored
            if (lo.local_xy.size() >= 6)
                EmitPolygonEdges(m_static, T.x, T.y, lo.local_xy, id);
            break;
        case LightOccluderComponent::Source::SpriteAlphaMap:
            // Path D: no segments - occlusion map pass (L4.5 stub)
            break;
        }
    }
}

void OccluderWorld::RebuildStatics(entt::registry& reg)
{
    m_static.clear();
    m_hash.clear();

    auto view = reg.view<TransformComponent, BoxShapeComponent,
                         Collider2DComponent, TagComponent>();
    for (auto e : view)
    {
        OccludeMode mode = ClassifyOccluder(reg, e);
        if (mode == OccludeMode::None)
            continue;
        // L1 static path: skip one-way movers (they are L6)
        if (mode == OccludeMode::TopEdgeOnly)
            continue;

        // Path D opt-in: skip Path A so texture holes are not sealed by AABB
        if (reg.has<LightOccluderComponent>(e))
        {
            const auto& lo = reg.get<LightOccluderComponent>(e);
            if (lo.enabled && lo.casts_shadow_from_sprite)
            {
                // avoid dual A+D seal; Path D draws later (stub)
                continue;
            }
            if (lo.enabled && lo.source != LightOccluderComponent::Source::FromCollider
                && lo.source != LightOccluderComponent::Source::SpriteAlphaMap
                && lo.source != LightOccluderComponent::Source::SpriteSilhouette)
            {
                // authored B box/poly replaces physics edges for light
                continue;
            }
        }

        const auto& T = reg.get<TransformComponent>(e);
        const auto& B = reg.get<BoxShapeComponent>(e);
        uint32_t id = (uint32_t)entt::to_integral(e);
        float hw = B.width * 0.5f;
        float hh = B.height * 0.5f;
        EmitBoxEdges(m_static, T.x, T.y, hw, hh, id, false, OccludeMode::FullBox);
    }

    EmitPathB(reg);
    CancelSharedFaces(m_static);
    static bool overflow_logged = false;
    if (m_static.size() > 2048 && !overflow_logged)
    {
        overflow_logged = true;
        FW_CORE_WARN("OccluderWorld: static segments {} > 2048", (int)m_static.size());
    }
    BuildSpatialHash();
}

void OccluderWorld::ClearDynamics()
{
    m_dynamic.clear();
}

void OccluderWorld::UpsertDynamicBox(uint32_t id, float cx, float cy, float hw, float hh,
                                     bool one_way_top_only)
{
    OccludeMode mode = one_way_top_only ? OccludeMode::TopEdgeOnly : OccludeMode::FullBox;
    EmitBoxEdges(m_dynamic, cx, cy, hw, hh, id, true, mode);
}

void OccluderWorld::UpdateDynamics(entt::registry& reg, float view_min_x, float view_min_y,
                                   float view_max_x, float view_max_y)
{
    ClearDynamics();
    int count = 0;
    auto view = reg.view<TransformComponent, BoxShapeComponent,
                         Collider2DComponent, TagComponent>();
    for (auto e : view)
    {
        if (count >= 32)
            break;
        const auto& tag = reg.get<TagComponent>(e).tag;
        if (!IsDynamicTag(tag))
            continue;
        const auto& T = reg.get<TransformComponent>(e);
        if (IsParked(T))
            continue;
        const auto& B = reg.get<BoxShapeComponent>(e);
        float hw = B.width * 0.5f;
        float hh = B.height * 0.5f;
        // on-screen only
        if (T.x + hw < view_min_x || T.x - hw > view_max_x
            || T.y + hh < view_min_y || T.y - hh > view_max_y)
            continue;
        const auto& col = reg.get<Collider2DComponent>(e);
        if (col.sensor)
            continue;
        UpsertDynamicBox((uint32_t)entt::to_integral(e), T.x, T.y, hw, hh, col.one_way);
        ++count;
    }
}

void OccluderWorld::QueryAABB(float minx, float miny, float maxx, float maxy,
                              std::vector<const OccluderSegment*>& out) const
{
    out.clear();
    std::vector<size_t> seen_idx;
    seen_idx.reserve(64);

    int x0 = CellOf(minx), x1 = CellOf(maxx);
    int y0 = CellOf(miny), y1 = CellOf(maxy);
    for (int cx = x0; cx <= x1; ++cx)
    {
        for (int cy = y0; cy <= y1; ++cy)
        {
            auto it = m_hash.find({cx, cy});
            if (it == m_hash.end())
                continue;
            for (size_t idx : it->second)
            {
                if (idx >= m_static.size())
                    continue;
                if (std::find(seen_idx.begin(), seen_idx.end(), idx) != seen_idx.end())
                    continue;
                const auto& s = m_static[idx];
                float sx0 = std::min(s.ax, s.bx), sx1 = std::max(s.ax, s.bx);
                float sy0 = std::min(s.ay, s.by), sy1 = std::max(s.ay, s.by);
                if (sx1 < minx || sx0 > maxx || sy1 < miny || sy0 > maxy)
                    continue;
                seen_idx.push_back(idx);
                out.push_back(&s);
            }
        }
    }

    for (const auto& s : m_dynamic)
    {
        float sx0 = std::min(s.ax, s.bx), sx1 = std::max(s.ax, s.bx);
        float sy0 = std::min(s.ay, s.by), sy1 = std::max(s.ay, s.by);
        if (sx1 < minx || sx0 > maxx || sy1 < miny || sy0 > maxy)
            continue;
        out.push_back(&s);
    }
}
