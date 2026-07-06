#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Survives scene switches. Menu writes slots; active gameplay scene reads them on spawn.
enum class MatchDevice : int
{
    Keyboard = 0,
    Pad = 1,
    Net = 2,    // remote peer; InputSampler leaves empty, filled from net
    Replay = 3, // ghost; InputSampler feeds from InputPlayback
};

struct MatchSlot
{
    MatchDevice device = MatchDevice::Keyboard;
    int pad_index = -1;
    // SDL joystick instance id: survives other pads being unplugged, unlike pad_index.
    int pad_instance = -1;
    uint8_t r = 80, g = 180, b = 255, a = 255;
    bool vacant = false; // disconnected net peer placeholder (lobby); do not spawn
};

class MatchConfig
{
public:
    static MatchConfig& GetInstance();

    void Clear();
    bool AddKeyboard();
    bool AddPad(int pad_index, int pad_instance = -1);
    bool HasKeyboard() const;
    bool HasPad(int pad_index) const;
    bool HasPadInstance(int pad_instance) const;

    int Count() const { return (int)slots.size(); }
    const MatchSlot* Slot(int index) const;
    MatchSlot* Slot(int index);

    // Default single-keyboard lobby when gameplay starts with no joins (dev / F2).
    void EnsureDefaultSolo();

    bool AddNetPeer(); // remote player placeholder slot; returns false if full
    // Returns assigned slot index, or -1 on failure. Reuses vacant Net slots first.
    int AddOrReuseNetPeer();
    // Ghost / replay pawn. Returns slot index or -1 if full.
    int AddReplay();
    bool RemoveSlot(int index); // erase slot and recolor; shifts higher indices down
    void FreeNetPeer(int index); // mark Net slot vacant without shifting (optional)

    // Product may override the 4-slot RGB palette used by ColorForSlot.
    static void SetDefaultPalette(const uint8_t rgb[4][3]);
    static void ColorForSlot(int index, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a);

    static constexpr int MaxSlots() { return kMaxSlots; }

private:
    MatchConfig() = default;
    std::vector<MatchSlot> slots;
    static constexpr int kMaxSlots = 4;
};
