#pragma once

#include "inputFrame.h"
#include "inputHistory.h"
#include "rollbackStateStore.h"

#include <cstdint>
#include <functional>
#include <vector>

// predict remotes, save ring, resim on mispredict.

class MatchRollback
{
public:
    static MatchRollback& GetInstance();

    void Reset();
    void SetStore(IRollbackStateStore* store);
    IRollbackStateStore* Store() const { return m_store; }
    bool Active() const { return m_store != nullptr; }

    void SetWindow(int ticks);
    void SetMaxResim(int ticks);
    void SetSlotCount(int n);
    void SetLocalSlot(int slot);
    // Bitmask of slots that are remote (predicted until real input arrives).
    void SetRemoteSlotMask(uint32_t mask);
    uint32_t RemoteSlotMask() const { return m_remote_mask; }
    uint32_t ActiveSlotMask() const;

    InputHistory& History() { return m_history; }
    const InputHistory& History() const { return m_history; }

    // Optional: incomplete resim → request panic blob for this tick.
    std::function<void(uint64_t /*tick*/)> on_resim_exhausted;

    uint64_t ConfirmedTick() const { return m_confirmed; }
    uint64_t CurrentTick() const { return m_current; }

    // Predict missing remotes, install all slot frames into InputSampler, Save.
    void BeginLocalTick(uint64_t tick);
    // After game SimStep: record local input as real; advance confirmed.
    void AfterLocalTick(uint64_t tick);
    // Convenience: Begin → store->SimStep → After.
    void RunTick(uint64_t tick, float dt);

    // Peer / harness injects real remote frames. May trigger Load+resim.
    void OnRemoteInputs(const InputFrame* frames, size_t count);

    // Optional: called when local input should be sent (game/net wires this).
    std::function<void(const InputFrame&)> on_send_local;

    // Panic: game blob for checksum / full resync.
    uint64_t HashSavedTick(uint64_t tick) const;
    bool GetSavedBlob(uint64_t tick, std::vector<uint8_t>& out) const;
    bool ApplyPanicBlob(uint64_t tick, const uint8_t* data, size_t len);

private:
    MatchRollback() = default;

    struct Saved
    {
        uint64_t tick = 0;
        std::vector<uint8_t> bytes;
        bool has = false;
    };

    void InstallFrames(uint64_t tick);
    InputFrame PredictSlot(uint64_t tick, int slot);
    void RecomputeConfirmed();
    void ResimFrom(uint64_t from_tick);
    int SavedIndex(uint64_t tick) const;

    IRollbackStateStore* m_store = nullptr;
    InputHistory m_history;
    int m_max_resim = 8;
    int m_local_slot = 0;
    int m_slot_count = 2;
    uint32_t m_remote_mask = 0;
    uint64_t m_current = 0;
    uint64_t m_confirmed = 0;
    float m_last_dt = 1.f / 60.f;
    std::vector<Saved> m_saves; // size = window
};
