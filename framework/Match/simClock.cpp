#include "simClock.h"

#include <algorithm>

SimClock& SimClock::GetInstance()
{
    static SimClock instance;
    return instance;
}

void SimClock::SetHz(float hz)
{
    if (hz < 1.f)
        hz = 1.f;
    if (hz > 240.f)
        hz = 240.f;
    m_sim_dt = 1.f / hz;
}

void SimClock::SetMaxStepsPerFrame(int n)
{
    m_max_steps = std::max(1, std::min(n, 16));
}

void SimClock::Reset()
{
    m_accum = 0.f;
    m_tick = 0;
    m_paused = false;
}

int SimClock::BeginFrame(float real_dt)
{
    if (m_paused)
    {
        m_accum = 0.f;
        return 0;
    }
    if (real_dt < 0.f)
        real_dt = 0.f;
    // Cap accumulation so a long hitch does not explode (same idea as app dt clamp).
    m_accum += real_dt;
    const float max_accum = m_sim_dt * (float)m_max_steps;
    if (m_accum > max_accum)
        m_accum = max_accum;

    int steps = 0;
    while (m_accum >= m_sim_dt && steps < m_max_steps)
    {
        m_accum -= m_sim_dt;
        ++steps;
    }
    return steps;
}

float SimClock::Alpha() const
{
    if (m_sim_dt <= 1e-8f)
        return 1.f;
    float a = m_accum / m_sim_dt;
    if (a < 0.f)
        a = 0.f;
    if (a > 1.f)
        a = 1.f;
    return a;
}

void SimClock::SoftSyncToRemote(uint64_t remote_tick)
{
    if (remote_tick == 0)
        return;
    // If we're more than a few ticks behind/ahead, snap; else ignore small skew.
    const int64_t delta = (int64_t)remote_tick - (int64_t)m_tick;
    if (delta > 8)
        m_tick = remote_tick - 2; // stay slightly behind host
    else if (delta < -8)
        m_tick = remote_tick;
}
