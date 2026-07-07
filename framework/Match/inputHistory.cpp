#include "inputHistory.h"

void InputHistory::Reset()
{
    for (int s = 0; s < kMaxSlots; ++s)
    {
        m_cells[s].assign((size_t)m_window, Cell{});
        m_has_last_real[s] = false;
        m_last_real[s] = {};
    }
}

void InputHistory::SetWindow(int ticks)
{
    if (ticks < 2)
        ticks = 2;
    if (ticks > 128)
        ticks = 128;
    m_window = ticks;
    Reset();
}

void InputHistory::SetSlotCount(int n)
{
    if (n < 1)
        n = 1;
    if (n > kMaxSlots)
        n = kMaxSlots;
    m_slots = n;
}

void InputHistory::Store(uint64_t tick, int slot, const InputFrame& frame, bool predicted)
{
    if (slot < 0 || slot >= m_slots || slot >= kMaxSlots)
        return;
    Cell& c = m_cells[slot][(size_t)Index(tick)];
    c.frame = frame;
    c.frame.tick = tick;
    c.frame.slot = slot;
    c.has = true;
    c.predicted = predicted;
    c.tick = tick;
    if (!predicted)
    {
        m_last_real[slot] = c.frame;
        m_has_last_real[slot] = true;
    }
}

bool InputHistory::Get(uint64_t tick, int slot, InputFrame& out, bool* predicted) const
{
    if (slot < 0 || slot >= m_slots || slot >= kMaxSlots)
        return false;
    const Cell& c = m_cells[slot][(size_t)Index(tick)];
    if (!c.has || c.tick != tick)
        return false;
    out = c.frame;
    if (predicted)
        *predicted = c.predicted;
    return true;
}

bool InputHistory::Has(uint64_t tick, int slot) const
{
    InputFrame tmp;
    return Get(tick, slot, tmp, nullptr);
}

bool InputHistory::IsPredicted(uint64_t tick, int slot) const
{
    bool pred = true;
    InputFrame tmp;
    if (!Get(tick, slot, tmp, &pred))
        return true;
    return pred;
}

bool InputHistory::LastReal(int slot, InputFrame& out) const
{
    if (slot < 0 || slot >= kMaxSlots || !m_has_last_real[slot])
        return false;
    out = m_last_real[slot];
    return true;
}

uint64_t InputHistory::ConfirmedTick(uint64_t current_tick, uint32_t active_slot_mask) const
{
    if (current_tick == 0)
        return 0;
    // Walk backward from current_tick-1 (current may still be in progress).
    uint64_t max_check = current_tick > 0 ? current_tick - 1 : 0;
    for (uint64_t t = max_check;; --t)
    {
        bool ok = true;
        for (int s = 0; s < m_slots; ++s)
        {
            if ((active_slot_mask & (1u << s)) == 0)
                continue;
            bool pred = true;
            InputFrame f;
            if (!Get(t, s, f, &pred) || pred)
            {
                ok = false;
                break;
            }
        }
        if (ok)
            return t;
        if (t == 0)
            break;
        // Don't walk forever outside window
        if (max_check >= (uint64_t)m_window && t + (uint64_t)m_window <= max_check)
            break;
    }
    return 0;
}
