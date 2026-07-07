#include "inputSampler.h"
#include "inputRecorder.h"
#include "Events/inputEvent.h"
#include "Events/keys.h"

#include "SDL.h"

InputSampler& InputSampler::GetInstance()
{
    static InputSampler instance;
    return instance;
}

const InputFrame& InputSampler::Frame(int slot) const
{
    static InputFrame empty;
    if (slot < 0 || slot >= kMaxSlots)
        return empty;
    return m_frames[slot];
}

InputFrame& InputSampler::MutableFrame(int slot)
{
    if (slot < 0 || slot >= kMaxSlots)
        return m_frames[0];
    return m_frames[slot];
}

void InputSampler::SetRemoteFrame(int slot, const InputFrame& frame)
{
    if (slot < 0 || slot >= kMaxSlots)
        return;
    m_frames[slot] = frame;
    m_frames[slot].slot = slot;
    m_last_remote[slot] = m_frames[slot];
    m_has_remote[slot] = true;
}

void InputSampler::ApplyRemoteQueue(const InputFrame* frames, size_t count)
{
    if (!frames || count == 0)
        return;

    struct Acc
    {
        InputFrame latest{};
        uint32_t edges = 0;
        bool any = false;
    };
    Acc acc[kMaxSlots]{};

    for (size_t i = 0; i < count; ++i)
    {
        const InputFrame& f = frames[i];
        int slot = f.slot;
        if (slot < 0 || slot >= kMaxSlots)
            continue;
        Acc& a = acc[slot];
        a.edges |= (f.bits & InputBits::EdgeMask);
        a.latest = f;
        a.any = true;
    }

    for (int slot = 0; slot < kMaxSlots; ++slot)
    {
        if (!acc[slot].any)
            continue;
        InputFrame out = acc[slot].latest;
        out.slot = slot;
        out.bits = (out.bits & ~InputBits::EdgeMask) | acc[slot].edges;
        SetRemoteFrame(slot, out);
    }
}

void InputSampler::ClearPressEdges()
{
    for (auto& f : m_frames)
        f.bits &= ~InputBits::EdgeMask;
}

void InputSampler::LatchSlotEdges(int slot, const MatchSlot& ms)
{
    if (slot < 0 || slot >= kMaxSlots)
        return;
    auto& in = InputEvent::GetInstance();
    uint32_t& pend = m_pending_edges[slot];
    const InputBinds& b = m_binds;

    if (ms.device == MatchDevice::Keyboard)
    {
        if (b.btn0_key >= 0 && in.KeyDownThisFrame((Key)b.btn0_key))
            pend |= InputBits::Btn0Pressed;
        if (b.btn0_key_alt >= 0 && in.KeyDownThisFrame((Key)b.btn0_key_alt))
            pend |= InputBits::Btn0Pressed;
        if (b.btn1_key >= 0 && in.KeyDownThisFrame((Key)b.btn1_key))
            pend |= InputBits::Btn1Pressed;
        if (b.btn1_key_alt >= 0 && in.KeyDownThisFrame((Key)b.btn1_key_alt))
            pend |= InputBits::Btn1Pressed;
        if (b.btn2_key >= 0 && in.KeyDownThisFrame((Key)b.btn2_key))
            pend |= InputBits::Btn2Pressed;
    }
    else if (ms.device == MatchDevice::Pad)
    {
        int idx = ms.pad_index;
        if (ms.pad_instance >= 0)
        {
            int mapped = in.GetControllerIndexFromInstance(ms.pad_instance);
            if (mapped >= 0)
                idx = mapped;
        }
        if (idx < 0)
            return;
        if (b.btn0_pad >= 0 && in.ControllerButtonDownThisFrame(idx, b.btn0_pad))
            pend |= InputBits::Btn0Pressed;
        if (b.btn1_pad >= 0 && in.ControllerButtonDownThisFrame(idx, b.btn1_pad))
            pend |= InputBits::Btn1Pressed;
        if (b.btn1_trigger.axis >= 0
            && in.ControllerButtonDownThisFrame(idx, PAD_TRIGGERLEFT))
            pend |= InputBits::Btn1Pressed;
        if (b.btn2_pad >= 0 && in.ControllerButtonDownThisFrame(idx, b.btn2_pad))
            pend |= InputBits::Btn2Pressed;
        if (b.btn2_pad_alt >= 0 && in.ControllerButtonDownThisFrame(idx, b.btn2_pad_alt))
            pend |= InputBits::Btn2Pressed;
    }
}

void InputSampler::LatchDisplayEdges()
{
    auto& mc = MatchConfig::GetInstance();
    for (int i = 0; i < mc.Count() && i < kMaxSlots; ++i)
    {
        const MatchSlot* s = mc.Slot(i);
        if (!s || s->device == MatchDevice::Net || s->device == MatchDevice::Replay)
            continue;
        LatchSlotEdges(i, *s);
    }
}

void InputSampler::SampleLocal(int slot, const MatchSlot& ms, uint64_t tick)
{
    InputFrame& f = m_frames[slot];
    f = {};
    f.tick = tick;
    f.slot = slot;
    auto& in = InputEvent::GetInstance();
    const InputBinds& b = m_binds;

    if (ms.device == MatchDevice::Keyboard)
    {
        if (in.IsKeyPressed(Key::A) || in.IsKeyPressed(Key::Left))
            f.bits |= InputBits::Left;
        if (in.IsKeyPressed(Key::D) || in.IsKeyPressed(Key::Right))
            f.bits |= InputBits::Right;
        if (in.IsKeyPressed(Key::W) || in.IsKeyPressed(Key::Up))
            f.bits |= InputBits::Up;
        if (in.IsKeyPressed(Key::S) || in.IsKeyPressed(Key::Down))
            f.bits |= InputBits::Down;

        auto key_held = [&](int k) { return k >= 0 && in.IsKeyPressed((Key)k); };
        auto key_down = [&](int k) { return k >= 0 && in.KeyDownThisFrame((Key)k); };

        if (key_held(b.btn0_key) || key_held(b.btn0_key_alt))
            f.bits |= InputBits::Btn0Held;
        if (key_down(b.btn0_key) || key_down(b.btn0_key_alt))
            f.bits |= InputBits::Btn0Pressed;
        if (key_held(b.btn1_key) || key_held(b.btn1_key_alt))
            f.bits |= InputBits::Btn1Held;
        if (key_down(b.btn1_key) || key_down(b.btn1_key_alt))
            f.bits |= InputBits::Btn1Pressed;
        if (key_down(b.btn2_key))
            f.bits |= InputBits::Btn2Pressed;
        if (key_held(b.btn3_key))
            f.bits |= InputBits::Btn3Held;
        if (key_held(b.btn4_key))
            f.bits |= InputBits::Btn4Held;
        if (key_held(b.btn5_key))
            f.bits |= InputBits::Btn5Held;
    }
    else if (ms.device == MatchDevice::Pad)
    {
        int idx = ms.pad_index;
        if (ms.pad_instance >= 0)
        {
            int mapped = in.GetControllerIndexFromInstance(ms.pad_instance);
            if (mapped >= 0)
                idx = mapped;
        }
        if (idx < 0)
            return;

        float ax = in.GetControllerAxis(idx, SDL_CONTROLLER_AXIS_LEFTX);
        float ay = in.GetControllerAxis(idx, SDL_CONTROLLER_AXIS_LEFTY);
        f.axis_x = ax;
        f.axis_y = ay;
        if (ax < -0.35f || in.IsControllerButtonPressed(idx, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            f.bits |= InputBits::Left;
        if (ax > 0.35f || in.IsControllerButtonPressed(idx, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            f.bits |= InputBits::Right;
        if (ay < -0.35f || in.IsControllerButtonPressed(idx, SDL_CONTROLLER_BUTTON_DPAD_UP))
            f.bits |= InputBits::Up;
        if (ay > 0.35f || in.IsControllerButtonPressed(idx, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            f.bits |= InputBits::Down;

        if (b.btn0_pad >= 0)
        {
            if (in.IsControllerButtonPressed(idx, b.btn0_pad))
                f.bits |= InputBits::Btn0Held;
            if (in.ControllerButtonDownThisFrame(idx, b.btn0_pad))
                f.bits |= InputBits::Btn0Pressed;
        }
        if (b.btn1_pad >= 0)
        {
            if (in.IsControllerButtonPressed(idx, b.btn1_pad))
                f.bits |= InputBits::Btn1Held;
            if (in.ControllerButtonDownThisFrame(idx, b.btn1_pad))
                f.bits |= InputBits::Btn1Pressed;
        }
        if (b.btn1_trigger.axis >= 0)
        {
            float t = in.GetControllerAxis(idx, b.btn1_trigger.axis);
            if (t > b.btn1_trigger.threshold || in.IsControllerButtonPressed(idx, PAD_TRIGGERLEFT))
                f.bits |= InputBits::Btn1Held;
            if (in.ControllerButtonDownThisFrame(idx, PAD_TRIGGERLEFT))
                f.bits |= InputBits::Btn1Pressed;
        }
        if (b.btn2_pad >= 0 && in.ControllerButtonDownThisFrame(idx, b.btn2_pad))
            f.bits |= InputBits::Btn2Pressed;
        if (b.btn2_pad_alt >= 0 && in.ControllerButtonDownThisFrame(idx, b.btn2_pad_alt))
            f.bits |= InputBits::Btn2Pressed;
        if (b.btn3_pad >= 0 && in.IsControllerButtonPressed(idx, b.btn3_pad))
            f.bits |= InputBits::Btn3Held;
        if (b.btn3_pad_alt >= 0 && in.IsControllerButtonPressed(idx, b.btn3_pad_alt))
            f.bits |= InputBits::Btn3Held;
        if (b.btn3_trigger.axis >= 0)
        {
            float t = in.GetControllerAxis(idx, b.btn3_trigger.axis);
            if (t > b.btn3_trigger.threshold)
                f.bits |= InputBits::Btn3Held;
        }
        if (b.btn4_pad >= 0 && in.IsControllerButtonPressed(idx, b.btn4_pad))
            f.bits |= InputBits::Btn4Held;
        if (b.btn5_pad >= 0 && in.IsControllerButtonPressed(idx, b.btn5_pad))
            f.bits |= InputBits::Btn5Held;
    }

    f.bits |= m_pending_edges[slot];
    m_pending_edges[slot] = 0;
}

void InputSampler::BeginTick(uint64_t tick, bool resample_devices)
{
    auto& mc = MatchConfig::GetInstance();
    if (!resample_devices)
    {
        ClearPressEdges();
        for (int i = 0; i < kMaxSlots; ++i)
            m_frames[i].tick = tick;
        return;
    }

    for (int i = 0; i < kMaxSlots; ++i)
        m_frames[i] = {};

    for (int i = 0; i < mc.Count() && i < kMaxSlots; ++i)
    {
        const MatchSlot* s = mc.Slot(i);
        if (!s)
            continue;
        if (s->device == MatchDevice::Replay)
        {
            InputFrame fr = InputPlayback::GetInstance().FrameAtTick(tick);
            fr.tick = tick;
            fr.slot = i;
            m_frames[i] = fr;
            continue;
        }
        if (s->device == MatchDevice::Net)
        {
            if (s->vacant)
            {
                m_frames[i] = {};
                m_frames[i].tick = tick;
                m_frames[i].slot = i;
                m_has_remote[i] = false;
                continue;
            }
            if (m_has_remote[i])
            {
                m_frames[i] = m_last_remote[i];
                m_frames[i].tick = tick;
                m_frames[i].slot = i;
                m_last_remote[i].bits &= ~InputBits::EdgeMask;
            }
            else
            {
                m_frames[i].tick = tick;
                m_frames[i].slot = i;
            }
            continue;
        }
        SampleLocal(i, *s, tick);
    }
}
