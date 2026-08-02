#pragma once

#include <cstdint>
#include <vector>

// game blob save/load/sim for MatchNetModel::Rollback.

struct IRollbackStateStore
{
    virtual ~IRollbackStateStore() = default;

    // Capture sim state *before* stepping tick (or equivalent restore point).
    virtual void Save(uint64_t tick, std::vector<uint8_t>& out) = 0;

    // Restore state saved for `tick`, then caller will resim from tick forward.
    virtual bool Load(uint64_t tick, const uint8_t* data, size_t len) = 0;

    // One fixed sim step at `tick` using InputSampler frames already installed.
    virtual void SimStep(uint64_t tick, float dt) = 0;
};
