#pragma once

#include "Match/inputFrame.h"

#include <cstdint>
#include <string>

// Host-side input sanity checks (anti-cheat lite). Kick after strikes.

namespace NetInputGuard
{
    constexpr uint32_t kAllowedBits = InputBits::Left | InputBits::Right | InputBits::Up | InputBits::Down
        | InputBits::Btn0Held | InputBits::Btn0Pressed | InputBits::Btn1Held | InputBits::Btn1Pressed
        | InputBits::Btn2Pressed | InputBits::Btn3Held | InputBits::Reserved0 | InputBits::Reserved1;

    struct ClientState
    {
        int strikes = 0;
        uint64_t last_tick = 0;
        int inputs_this_sec = 0;
        uint64_t sec_bucket_ms = 0;
    };

    // Returns false if input should be dropped. Sets *kick if peer should be removed.
    bool Accept(ClientState& st, const InputFrame& f, int assigned_slot, uint64_t host_tick,
                uint64_t now_ms, bool* kick, std::string* reason);
}
