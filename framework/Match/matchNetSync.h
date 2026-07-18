#pragma once

#include "entt.hpp"

// Host snapshots / client soft-reconcile. Thresholds are tunable from game code.

namespace MatchNetSync
{
    struct Config
    {
        float hard_err = 1.25f;
        float soft_err = 0.06f;
        float blend = 0.35f;
        float hard_dvy = 8.f;
    };

    Config& GetConfig();

    void AfterSim(entt::registry& registry, bool send_local_input);
}
