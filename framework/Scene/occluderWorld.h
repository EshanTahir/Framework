#pragma once

#include "entt.hpp"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

struct OccluderSegment
{
    float ax = 0, ay = 0, bx = 0, by = 0;
    float nx = 0, ny = 0; // outward unit normal (Y-down: floor top n=(0,-1))
    uint32_t solid_id = 0;
    bool dynamic = false;
};

enum class OccludeMode
{
    None,
    FullBox,
    TopEdgeOnly
};

class OccluderWorld
{
public:
    static OccluderWorld& Get();

    void RebuildStatics(entt::registry& reg);
    void ClearDynamics();
    void UpsertDynamicBox(uint32_t id, float cx, float cy, float hw, float hh,
                          bool one_way_top_only);
    // L6: movers / crushers on-screen, cap 32
    void UpdateDynamics(entt::registry& reg, float view_min_x, float view_min_y,
                        float view_max_x, float view_max_y);

    void QueryAABB(float minx, float miny, float maxx, float maxy,
                   std::vector<const OccluderSegment*>& out) const;

    const std::vector<OccluderSegment>& Statics() const { return m_static; }
    const std::vector<OccluderSegment>& Dynamics() const { return m_dynamic; }

    bool debug_draw = false;

private:
    OccluderWorld() = default;

    void BuildSpatialHash();
    void EmitPathB(entt::registry& reg);

    std::vector<OccluderSegment> m_static;
    std::vector<OccluderSegment> m_dynamic;

    // spatial hash: cell 4 wu → segment indices into m_static
    struct CellKey
    {
        int x = 0, y = 0;
        bool operator==(const CellKey& o) const { return x == o.x && y == o.y; }
    };
    struct CellHash
    {
        size_t operator()(const CellKey& k) const
        {
            return (size_t)k.x * 73856093u ^ (size_t)k.y * 19349663u;
        }
    };
    std::unordered_map<CellKey, std::vector<size_t>, CellHash> m_hash;
};

OccludeMode ClassifyOccluder(entt::registry& reg, entt::entity e);
void EmitBoxEdges(std::vector<OccluderSegment>& out,
                  float cx, float cy, float hw, float hh,
                  uint32_t id, bool dyn, OccludeMode mode);
void CancelSharedFaces(std::vector<OccluderSegment>& segs);
