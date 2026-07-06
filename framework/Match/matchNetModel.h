#pragma once

// How this process advances shared sim over the network.
enum class MatchNetModel : int
{
    HostSnapshot = 0, // default: host sim + client soft reconcile
    Rollback = 1,     // peer inputs + local predict/resim (opt-in games)
};
