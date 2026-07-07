#pragma once

#include <cstdint>

// per-slot input bits for one sim tick (opaque actions via SetBinds).

namespace InputBits
{
    constexpr uint32_t Left = 1u << 0;
    constexpr uint32_t Right = 1u << 1;
    constexpr uint32_t Up = 1u << 2;
    constexpr uint32_t Down = 1u << 3;
    constexpr uint32_t Btn0Held = 1u << 4;
    constexpr uint32_t Btn0Pressed = 1u << 5;
    constexpr uint32_t Btn1Held = 1u << 6;
    constexpr uint32_t Btn1Pressed = 1u << 7;
    constexpr uint32_t Btn2Pressed = 1u << 8;
    constexpr uint32_t Btn3Held = 1u << 9;
    constexpr uint32_t Reserved0 = 1u << 10; // Btn4Held (grab)
    constexpr uint32_t Reserved1 = 1u << 11; // Btn5Held (stomp)
    constexpr uint32_t Btn4Held = Reserved0;
    constexpr uint32_t Btn5Held = Reserved1;

    // One-shot edges stripped on catch-up sim steps.
    constexpr uint32_t EdgeMask = Btn0Pressed | Btn1Pressed | Btn2Pressed;
}

struct InputFrame
{
    uint64_t tick = 0;
    int32_t slot = 0;
    uint32_t bits = 0;
    float axis_x = 0.f;
    float axis_y = 0.f;

    bool Has(uint32_t bit) const { return (bits & bit) != 0; }
};
