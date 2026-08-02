#include "matchEvents.h"
#include "matchNetModel.h"
#include "matchSession.h"
#include "rollbackSession.h"

#include <cstring>

MatchEvents& MatchEvents::GetInstance()
{
    static MatchEvents instance;
    return instance;
}

void MatchEvents::Emit(uint8_t kind, const char* name, float volume, int slot, float x, float y,
                       uint64_t tick, uint8_t flags)
{
    MatchEventWire ev{};
    ev.kind = kind;
    ev.flags = flags;
    ev.slot = slot;
    ev.volume = volume;
    ev.x = x;
    ev.y = y;
    if (name)
        std::strncpy(ev.name, name, sizeof(ev.name) - 1);

    const bool gate = MatchSession::GetInstance().NetModel() == MatchNetModel::Rollback
        && MatchRollback::GetInstance().Active();

    if (!gate || tick == 0)
    {
        m_out.push_back(ev);
        m_in.push_back(ev);
        return;
    }

    Pending p;
    p.ev = ev;
    p.tick = tick;
    p.outbound = true;
    m_pending.push_back(p);
    // Local preview held until confirm (avoids double SFX on resim).
}

void MatchEvents::PushInbound(const MatchEventWire& ev, uint64_t tick)
{
    const bool gate = MatchSession::GetInstance().NetModel() == MatchNetModel::Rollback
        && MatchRollback::GetInstance().Active();
    if (!gate || tick == 0)
    {
        m_in.push_back(ev);
        return;
    }
    Pending p;
    p.ev = ev;
    p.tick = tick;
    p.outbound = false;
    m_pending.push_back(p);
}

void MatchEvents::ReleaseConfirmed(uint64_t confirmed_tick)
{
    std::vector<Pending> keep;
    for (auto& p : m_pending)
    {
        if (p.tick <= confirmed_tick)
        {
            m_in.push_back(p.ev);
            if (p.outbound)
                m_out.push_back(p.ev);
        }
        else
            keep.push_back(p);
    }
    m_pending.swap(keep);
}

std::vector<MatchEventWire> MatchEvents::TakeOutbound()
{
    std::vector<MatchEventWire> out;
    out.swap(m_out);
    return out;
}

std::vector<MatchEventWire> MatchEvents::TakeInbound()
{
    std::vector<MatchEventWire> out;
    out.swap(m_in);
    return out;
}

void MatchEvents::Clear()
{
    m_out.clear();
    m_in.clear();
    m_pending.clear();
}
