#pragma once

#include "matchAuthority.h"
#include "matchNetModel.h"
#include "inputFrame.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// authority, seed, content hash, per-tick sampling.

class MatchSession
{
public:
    static MatchSession& GetInstance();

    void ResetForLocal();
    void SetAuthority(MatchAuthority a);
    MatchAuthority Authority() const { return m_authority; }
    bool IsHost() const { return MatchAuthorityIsHost(m_authority); }
    bool IsClient() const { return m_authority == MatchAuthority::NetClient; }
    bool IsDedicated() const { return m_authority == MatchAuthority::DedicatedHost; }
    bool IsNetServer() const { return MatchAuthorityIsNetServer(m_authority); }

    void SetNetModel(MatchNetModel m) { m_net_model = m; }
    MatchNetModel NetModel() const { return m_net_model; }

    void SetSeed(uint64_t seed);
    uint64_t Seed() const { return m_seed; }

    void SetContentPaths(std::vector<std::string> paths);
    void RecomputeContentHash();
    uint64_t ContentHashValue() const { return m_content_hash; }
    std::string ContentHashHex() const;

    void BeginSimTick(uint64_t tick, bool resample_devices = true);
    void RecordSimInputs();

    void ApplyRemoteInput(const InputFrame& frame);
    void ApplyRemoteInputQueue(const InputFrame* frames, size_t count);

    int LocalControlledSlot() const { return m_local_slot; }
    void SetLocalControlledSlot(int slot) { m_local_slot = slot; }

    void RequestForceSnap(int slot);
    bool ConsumeForceSnap(int slot);

    // Input tape helpers (ghost slot + file I/O). Not a session "mode".
    bool ReplaySave(const std::string& path) const;
    bool ReplayLoad(const std::string& path, int source_slot = 0);
    bool ReplayActive() const;
    void ReplayStop();
    int ReplayGhostSlot() const { return m_replay_ghost_slot; }

private:
    MatchSession() = default;
    MatchAuthority m_authority = MatchAuthority::OfflineHost;
    MatchNetModel m_net_model = MatchNetModel::HostSnapshot;
    uint64_t m_seed = 1;
    uint64_t m_content_hash = 0;
    std::vector<std::string> m_content_paths;
    int m_local_slot = 0;
    std::array<bool, 4> m_force_snap{};
    int m_replay_ghost_slot = -1;
};
