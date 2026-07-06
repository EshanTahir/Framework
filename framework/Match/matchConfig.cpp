#include "matchConfig.h"
#include "Match/matchSession.h"

namespace
{
    // Neutral defaults until the game calls SetDefaultPalette.
    uint8_t s_palette[4][3] = {
        {200, 200, 200},
        {180, 180, 180},
        {160, 160, 160},
        {140, 140, 140},
    };
}

MatchConfig& MatchConfig::GetInstance()
{
    static MatchConfig instance;
    return instance;
}

void MatchConfig::Clear()
{
    slots.clear();
}

bool MatchConfig::HasKeyboard() const
{
    for (const auto& s : slots)
        if (s.device == MatchDevice::Keyboard)
            return true;
    return false;
}

bool MatchConfig::HasPad(int pad_index) const
{
    for (const auto& s : slots)
        if (s.device == MatchDevice::Pad && s.pad_index == pad_index)
            return true;
    return false;
}

bool MatchConfig::AddKeyboard()
{
    if ((int)slots.size() >= kMaxSlots || HasKeyboard())
        return false;
    MatchSlot s;
    s.device = MatchDevice::Keyboard;
    s.pad_index = -1;
    ColorForSlot((int)slots.size(), s.r, s.g, s.b, s.a);
    slots.push_back(s);
    return true;
}

bool MatchConfig::HasPadInstance(int pad_instance) const
{
    if (pad_instance < 0)
        return false;
    for (const auto& s : slots)
        if (s.device == MatchDevice::Pad && s.pad_instance == pad_instance)
            return true;
    return false;
}

bool MatchConfig::AddPad(int pad_index, int pad_instance)
{
    if ((int)slots.size() >= kMaxSlots || pad_index < 0 || HasPad(pad_index)
        || HasPadInstance(pad_instance))
        return false;
    MatchSlot s;
    s.device = MatchDevice::Pad;
    s.pad_index = pad_index;
    s.pad_instance = pad_instance;
    ColorForSlot((int)slots.size(), s.r, s.g, s.b, s.a);
    slots.push_back(s);
    return true;
}

const MatchSlot* MatchConfig::Slot(int index) const
{
    if (index < 0 || index >= (int)slots.size())
        return nullptr;
    return &slots[index];
}

MatchSlot* MatchConfig::Slot(int index)
{
    if (index < 0 || index >= (int)slots.size())
        return nullptr;
    return &slots[index];
}

void MatchConfig::EnsureDefaultSolo()
{
    if (slots.empty() && !MatchSession::GetInstance().IsDedicated())
        AddKeyboard();
}

bool MatchConfig::AddNetPeer()
{
    return AddOrReuseNetPeer() >= 0;
}

int MatchConfig::AddReplay()
{
    if ((int)slots.size() >= kMaxSlots)
        return -1;
    MatchSlot s;
    s.device = MatchDevice::Replay;
    s.pad_index = -1;
    s.pad_instance = -1;
    s.vacant = false;
    ColorForSlot((int)slots.size(), s.r, s.g, s.b, s.a);
    // Ghosts read slightly translucent
    s.a = 160;
    slots.push_back(s);
    return (int)slots.size() - 1;
}

int MatchConfig::AddOrReuseNetPeer()
{
    for (int i = 0; i < (int)slots.size(); ++i)
    {
        if (slots[i].device == MatchDevice::Net && slots[i].vacant)
        {
            slots[i].vacant = false;
            ColorForSlot(i, slots[i].r, slots[i].g, slots[i].b, slots[i].a);
            return i;
        }
    }
    if ((int)slots.size() >= kMaxSlots)
        return -1;
    MatchSlot s;
    s.device = MatchDevice::Net;
    s.pad_index = -1;
    s.pad_instance = -1;
    s.vacant = false;
    ColorForSlot((int)slots.size(), s.r, s.g, s.b, s.a);
    slots.push_back(s);
    return (int)slots.size() - 1;
}

bool MatchConfig::RemoveSlot(int index)
{
    if (index < 0 || index >= (int)slots.size())
        return false;
    slots.erase(slots.begin() + index);
    for (int i = 0; i < (int)slots.size(); ++i)
        ColorForSlot(i, slots[i].r, slots[i].g, slots[i].b, slots[i].a);
    return true;
}

void MatchConfig::FreeNetPeer(int index)
{
    if (index < 0 || index >= (int)slots.size())
        return;
    if (slots[index].device == MatchDevice::Net)
        slots[index].vacant = true;
}

void MatchConfig::SetDefaultPalette(const uint8_t rgb[4][3])
{
    if (!rgb)
        return;
    for (int i = 0; i < 4; ++i)
    {
        s_palette[i][0] = rgb[i][0];
        s_palette[i][1] = rgb[i][1];
        s_palette[i][2] = rgb[i][2];
    }
}

void MatchConfig::ColorForSlot(int index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a)
{
    const int i = index < 0 ? 0 : (index % 4);
    r = s_palette[i][0];
    g = s_palette[i][1];
    b = s_palette[i][2];
    a = 255;
}
