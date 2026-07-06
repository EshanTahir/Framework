#include "matchEvents.h"
#include "matchNetModel.h"
#include "matchSession.h"

#include <cstring>

MatchEvents& MatchEvents::GetInstance()
{
    static MatchEvents instance;
    return instance;
}

void MatchEvents::Emit(uint8_t kind, const char* name, float volume, int slot, float x, float y,
                       uint64_t tick, uint8_t flags)
{
    (void)tick;
    MatchEventWire ev{};
    ev.kind = kind;
    ev.flags = flags;
    ev.slot = slot;
    ev.volume = volume;
    ev.x = x;
    ev.y = y;
    if (name)
        std::strncpy(ev.name, name, sizeof(ev.name) - 1);
    m_out.push_back(ev);
    m_in.push_back(ev);
}

void MatchEvents::PushInbound(const MatchEventWire& ev, uint64_t tick)
{
    (void)tick;
    m_in.push_back(ev);
}

void MatchEvents::ReleaseConfirmed(uint64_t) {}

std::vector<MatchEventWire> MatchEvents::TakeOutbound()
{
    std::vector<MatchEventWire> out;
    out.swap(m_out);
    return out;
}

std::vector<MatchEventWire> MatchEvents::TakeInbound()
{
    std::vector<MatchEventWire> in;
    in.swap(m_in);
    return in;
}

void MatchEvents::Clear()
{
    m_out.clear();
    m_in.clear();
    m_pending.clear();
}
