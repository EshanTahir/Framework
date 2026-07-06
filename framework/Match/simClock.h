#pragma once

#include <cstdint>

// fixed-timestep accumulator; same sim_dt on every machine.

class SimClock
{
public:
    static SimClock& GetInstance();

    void SetHz(float hz); // default 60
    float SimDt() const { return m_sim_dt; }
    uint64_t Tick() const { return m_tick; }

    // Call once per display frame with real seconds (already clamped by app).
    // Returns how many sim steps to run this frame (0..max_steps).
    int BeginFrame(float real_dt);

    // Fraction of the next sim step elapsed (0..1) - use for render lerp.
    float Alpha() const;

    void AdvanceTick() { ++m_tick; }

    // Client soft-sync: nudge local tick toward host snapshot tick (no hard jump).
    void SoftSyncToRemote(uint64_t remote_tick);

    void Reset();

    void SetPaused(bool paused) { m_paused = paused; }
    bool Paused() const { return m_paused; }

    int MaxStepsPerFrame() const { return m_max_steps; }
    void SetMaxStepsPerFrame(int n);

private:
    SimClock() = default;
    float m_sim_dt = 1.f / 60.f;
    float m_accum = 0.f;
    int m_max_steps = 5;
    uint64_t m_tick = 0;
    bool m_paused = false;
};
