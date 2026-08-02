#include "rollbackSession.h"
#include "inputSampler.h"
#include "Core/log.h"

#include <cstdint>
#include <cstring>
#include <limits>

MatchRollback& MatchRollback::GetInstance()
{
    static MatchRollback instance;
    return instance;
}

void MatchRollback::Reset()
{
    m_history.Reset();
    m_history.SetSlotCount(m_slot_count);
    m_saves.assign((size_t)m_history.Window(), Saved{});
    m_current = 0;
    m_confirmed = 0;
}

void MatchRollback::SetStore(IRollbackStateStore* store)
{
    m_store = store;
    Reset();
}

void MatchRollback::SetWindow(int ticks)
{
    m_history.SetWindow(ticks);
    m_saves.assign((size_t)m_history.Window(), Saved{});
}

void MatchRollback::SetMaxResim(int ticks)
{
    m_max_resim = ticks > 0 ? ticks : 1;
}

void MatchRollback::SetSlotCount(int n)
{
    m_slot_count = n;
    m_history.SetSlotCount(n);
}

void MatchRollback::SetLocalSlot(int slot)
{
    m_local_slot = slot;
}

void MatchRollback::SetRemoteSlotMask(uint32_t mask)
{
    m_remote_mask = mask;
}

uint32_t MatchRollback::ActiveSlotMask() const
{
    uint32_t m = 0;
    for (int s = 0; s < m_slot_count; ++s)
        m |= (1u << s);
    return m;
}

int MatchRollback::SavedIndex(uint64_t tick) const
{
    const int w = m_history.Window();
    return w > 0 ? (int)(tick % (uint64_t)w) : 0;
}

InputFrame MatchRollback::PredictSlot(uint64_t tick, int slot)
{
    InputFrame f{};
    f.tick = tick;
    f.slot = slot;
    InputFrame last{};
    if (m_history.LastReal(slot, last))
    {
        f = last;
        f.tick = tick;
        f.slot = slot;
        f.bits &= ~InputBits::EdgeMask;
    }
    return f;
}

void MatchRollback::InstallFrames(uint64_t tick)
{
    auto& sampler = InputSampler::GetInstance();
    for (int s = 0; s < m_slot_count; ++s)
    {
        InputFrame f{};
        bool pred = false;
        if (!m_history.Get(tick, s, f, &pred))
        {
            if ((m_remote_mask & (1u << s)) != 0 || s != m_local_slot)
            {
                f = PredictSlot(tick, s);
                m_history.Store(tick, s, f, true);
            }
            else
            {
                // Local: take live sampler frame if already sampled, else empty.
                f = sampler.Frame(s);
                f.tick = tick;
                f.slot = s;
                m_history.Store(tick, s, f, false);
            }
        }
        sampler.SetRemoteFrame(s, f);
    }
}

void MatchRollback::BeginLocalTick(uint64_t tick)
{
    if (!m_store)
        return;
    m_current = tick;

    // local slot from sampler (caller samples first)
    {
        InputFrame local = InputSampler::GetInstance().Frame(m_local_slot);
        local.tick = tick;
        local.slot = m_local_slot;
        m_history.Store(tick, m_local_slot, local, false);
    }

    for (int s = 0; s < m_slot_count; ++s)
    {
        if (s == m_local_slot)
            continue;
        if ((m_remote_mask & (1u << s)) == 0)
            continue;
        if (!m_history.Has(tick, s))
        {
            InputFrame pred = PredictSlot(tick, s);
            m_history.Store(tick, s, pred, true);
        }
    }

    InstallFrames(tick);

    std::vector<uint8_t> blob;
    m_store->Save(tick, blob);
    Saved& sv = m_saves[(size_t)SavedIndex(tick)];
    sv.tick = tick;
    sv.bytes = std::move(blob);
    sv.has = true;
}

void MatchRollback::AfterLocalTick(uint64_t tick)
{
    if (!m_store)
        return;

    InputFrame local{};
    if (m_history.Get(tick, m_local_slot, local, nullptr))
    {
        m_history.Store(tick, m_local_slot, local, false);
        if (on_send_local)
            on_send_local(local);
    }

    RecomputeConfirmed();
}

void MatchRollback::RunTick(uint64_t tick, float dt)
{
    if (!m_store)
        return;
    m_last_dt = dt;
    BeginLocalTick(tick);
    m_store->SimStep(tick, dt);
    AfterLocalTick(tick);
}

void MatchRollback::RecomputeConfirmed()
{
    m_confirmed = m_history.ConfirmedTick(m_current + 1, ActiveSlotMask());
}

void MatchRollback::ResimFrom(uint64_t from_tick)
{
    if (!m_store)
        return;

    int idx = SavedIndex(from_tick);
    Saved& sv = m_saves[(size_t)idx];
    if (!sv.has || sv.tick != from_tick)
    {
        FW_CORE_WARN("MatchRollback: no save for tick {0}", from_tick);
        return;
    }
    if (!m_store->Load(from_tick, sv.bytes.data(), sv.bytes.size()))
    {
        FW_CORE_WARN("MatchRollback: Load failed tick {0}", from_tick);
        return;
    }

    const uint64_t end = m_current;
    int steps = 0;
    uint64_t last_done = from_tick;
    const int budget = m_max_resim > 0 ? m_max_resim : 1;
    for (uint64_t t = from_tick; t <= end; ++t)
    {
        if (steps >= budget && t < end)
        {
            FW_CORE_WARN("MatchRollback: max resim hit at {0} (want {1})", t, end);
            m_current = last_done;
            RecomputeConfirmed();
            if (on_resim_exhausted)
                on_resim_exhausted(from_tick);
            return;
        }
        InstallFrames(t);
        std::vector<uint8_t> blob;
        m_store->Save(t, blob);
        Saved& s2 = m_saves[(size_t)SavedIndex(t)];
        s2.tick = t;
        s2.bytes = std::move(blob);
        s2.has = true;

        m_store->SimStep(t, m_last_dt);
        last_done = t;
        ++steps;
    }
    RecomputeConfirmed();
}

void MatchRollback::OnRemoteInputs(const InputFrame* frames, size_t count)
{
    if (!m_store || !frames || count == 0)
        return;

    uint64_t earliest_mis = std::numeric_limits<uint64_t>::max();
    for (size_t i = 0; i < count; ++i)
    {
        const InputFrame& in = frames[i];
        int slot = in.slot;
        if (slot < 0 || slot >= m_slot_count)
            continue;
        if ((m_remote_mask & (1u << slot)) == 0 && slot != m_local_slot)
            continue;

        InputFrame prev{};
        bool was_pred = false;
        const bool had = m_history.Get(in.tick, slot, prev, &was_pred);
        m_history.Store(in.tick, slot, in, false);

        if (had)
        {
            const bool bits_diff = prev.bits != in.bits;
            const bool axis_diff = prev.axis_x != in.axis_x || prev.axis_y != in.axis_y;
            if (was_pred && (bits_diff || axis_diff))
            {
                if (in.tick < earliest_mis)
                    earliest_mis = in.tick;
            }
            else if (!was_pred && (bits_diff || axis_diff))
            {
                // Conflicting real input - still resim
                if (in.tick < earliest_mis)
                    earliest_mis = in.tick;
            }
        }
    }

    RecomputeConfirmed();

    if (earliest_mis != std::numeric_limits<uint64_t>::max() && earliest_mis <= m_current)
        ResimFrom(earliest_mis);
}

uint64_t MatchRollback::HashSavedTick(uint64_t tick) const
{
    int idx = SavedIndex(tick);
    const Saved& sv = m_saves[(size_t)idx];
    if (!sv.has || sv.tick != tick)
        return 0;
    // FNV-1a 64
    uint64_t h = 14695981039346656037ULL;
    for (uint8_t b : sv.bytes)
    {
        h ^= b;
        h *= 1099511628211ULL;
    }
    h ^= tick;
    return h;
}

bool MatchRollback::GetSavedBlob(uint64_t tick, std::vector<uint8_t>& out) const
{
    int idx = SavedIndex(tick);
    const Saved& sv = m_saves[(size_t)idx];
    if (!sv.has || sv.tick != tick)
        return false;
    out = sv.bytes;
    return true;
}

bool MatchRollback::ApplyPanicBlob(uint64_t tick, const uint8_t* data, size_t len)
{
    if (!m_store || !data)
        return false;
    const uint64_t catch_to = m_current > tick ? m_current : tick;
    if (!m_store->Load(tick, data, len))
        return false;
    Saved& sv = m_saves[(size_t)SavedIndex(tick)];
    sv.tick = tick;
    sv.bytes.assign(data, data + len);
    sv.has = true;
    m_current = catch_to;
    if (catch_to > tick)
        ResimFrom(tick);
    else
        RecomputeConfirmed();
    return true;
}
