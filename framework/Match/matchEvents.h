#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// one-shot net events; sticky KV lives in MatchChannels.

namespace MatchEventKind
{
    constexpr uint8_t Sound = 1; // name = asset path
    constexpr uint8_t Cue = 2;   // name = opaque cue id (VFX / UI / telegraph)
}

namespace MatchEventFlags
{
    constexpr uint8_t None = 0;
    constexpr uint8_t Spatial = 1; // play with x,y (even if both 0)
}

#pragma pack(push, 1)
struct MatchEventWire
{
    uint8_t kind = MatchEventKind::Cue;
    uint8_t flags = MatchEventFlags::None;
    int32_t slot = -1;
    float volume = 1.f;
    float x = 0.f;
    float y = 0.f;
    // Long pack paths (Helton Yan…); keep same-build host+client.
    char name[160]{};
};
#pragma pack(pop)

class MatchEvents
{
public:
    static MatchEvents& GetInstance();

    // Offline / host local: queue for local consumers. Host also marks for net send.
    // `tick` = sim tick for confirm gating (0 = release immediately).
    void Emit(uint8_t kind, const char* name, float volume = 1.f, int slot = -1,
              float x = 0.f, float y = 0.f, uint64_t tick = 0, uint8_t flags = 0);

    // Host: take events ready to Broadcast as NetMsg::Event (confirm-gated).
    std::vector<MatchEventWire> TakeOutbound();

    // Client / local: inbound for scripts + auto sound (confirm-gated).
    void PushInbound(const MatchEventWire& ev, uint64_t tick = 0);
    std::vector<MatchEventWire> TakeInbound();

    // Release pending events with tick <= confirmed into inbound/outbound ready queues.
    void ReleaseConfirmed(uint64_t confirmed_tick);

    void Clear();

private:
    struct Pending
    {
        MatchEventWire ev{};
        uint64_t tick = 0;
        bool outbound = false;
    };

    MatchEvents() = default;
    std::vector<MatchEventWire> m_out;
    std::vector<MatchEventWire> m_in;
    std::vector<Pending> m_pending;
};
