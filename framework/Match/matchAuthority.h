#pragma once

// Who is allowed to mutate shared match / world state.
enum class MatchAuthority : int
{
    OfflineHost = 0,   // local / couch: this process is authority
    NetHost = 1,       // listen server with a local human
    NetClient = 2,     // applies snapshots; limited local prediction
    DedicatedHost = 3, // same net role as NetHost; no local human expected
};

inline bool MatchAuthorityIsHost(MatchAuthority a)
{
    return a == MatchAuthority::OfflineHost
        || a == MatchAuthority::NetHost
        || a == MatchAuthority::DedicatedHost;
}

inline bool MatchAuthorityIsNetServer(MatchAuthority a)
{
    return a == MatchAuthority::NetHost || a == MatchAuthority::DedicatedHost;
}
