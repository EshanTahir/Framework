#pragma once

#include "inputFrame.h"

#include <array>
#include <cstdint>
#include <vector>

// Ring of InputFrame per slot × tick for rollback prediction / confirm.

class InputHistory
{
public:
    static constexpr int kMaxSlots = 4;
    static constexpr int kDefaultWindow = 16;

    void Reset();
    void SetWindow(int ticks);
    int Window() const { return m_window; }

    void SetSlotCount(int n);
    int SlotCount() const { return m_slots; }

    // Store real (confirmed from device/peer) or predicted frame.
    void Store(uint64_t tick, int slot, const InputFrame& frame, bool predicted);
    bool Get(uint64_t tick, int slot, InputFrame& out, bool* predicted = nullptr) const;
    bool Has(uint64_t tick, int slot) const;
    bool IsPredicted(uint64_t tick, int slot) const;

    // Highest tick where every active slot has a non-predicted frame (or no slots).
    uint64_t ConfirmedTick(uint64_t current_tick, uint32_t active_slot_mask) const;

    // Last real frame for slot (for prediction).
    bool LastReal(int slot, InputFrame& out) const;

private:
    struct Cell
    {
        InputFrame frame{};
        bool has = false;
        bool predicted = false;
        uint64_t tick = 0;
    };

    int Index(uint64_t tick) const { return (int)(tick % (uint64_t)m_window); }

    int m_window = kDefaultWindow;
    int m_slots = kMaxSlots;
    // [slot][window]
    std::array<std::vector<Cell>, kMaxSlots> m_cells{};
    std::array<InputFrame, kMaxSlots> m_last_real{};
    std::array<bool, kMaxSlots> m_has_last_real{};
};
