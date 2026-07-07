#pragma once

#include "inputFrame.h"
#include "Match/matchConfig.h"
#include "Events/keys.h"

#include <array>
#include <cstddef>
#include <cstdint>

// Optional pad trigger-as-button (axis > threshold).
struct InputPadTrigger
{
    int axis = -1; // SDL_CONTROLLER_AXIS_* or -1
    float threshold = 0.45f;
};

// Game installs binds once at startup. Defaults leave face buttons unbound
// until SetBinds is called (movement WASD/stick still works).
struct InputBinds
{
    // Keyboard (Key enum as int). -1 = unused.
    int btn0_key = -1;
    int btn0_key_alt = -1;
    int btn1_key = -1;
    int btn1_key_alt = -1;
    int btn2_key = -1;
    int btn3_key = -1;
    int btn4_key = -1;
    int btn5_key = -1;

    // Pad buttons (SDL_CONTROLLER_BUTTON_*). -1 = unused.
    int btn0_pad = -1;
    int btn1_pad = -1;
    int btn2_pad = -1;
    int btn2_pad_alt = -1;
    int btn3_pad = -1;
    int btn3_pad_alt = -1;
    int btn4_pad = -1;
    int btn5_pad = -1;

    InputPadTrigger btn1_trigger{}; // optional held/edge from trigger axis
    InputPadTrigger btn3_trigger{}; // optional held from trigger axis
};

class InputSampler
{
public:
    static InputSampler& GetInstance();

    void SetBinds(const InputBinds& binds) { m_binds = binds; }
    const InputBinds& Binds() const { return m_binds; }

    void LatchDisplayEdges();
    void BeginTick(uint64_t tick, bool resample_devices = true);
    const InputFrame& Frame(int slot) const;
    InputFrame& MutableFrame(int slot);

    void SetRemoteFrame(int slot, const InputFrame& frame);
    void ApplyRemoteQueue(const InputFrame* frames, size_t count);
    void ClearPressEdges();

    static constexpr int kMaxSlots = 4;

private:
    InputSampler() = default;
    void SampleLocal(int slot, const MatchSlot& ms, uint64_t tick);
    void LatchSlotEdges(int slot, const MatchSlot& ms);

    InputBinds m_binds{};
    std::array<InputFrame, kMaxSlots> m_frames{};
    std::array<InputFrame, kMaxSlots> m_last_remote{};
    std::array<bool, kMaxSlots> m_has_remote{};
    std::array<uint32_t, kMaxSlots> m_pending_edges{};
};
