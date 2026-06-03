#include "inputEvent.h"
#include "event.h"

#include <algorithm>
#include <cmath>

InputEvent::InputEvent()
{
    keys.resize(SDL_NUM_SCANCODES, false);
    keys_down.resize(SDL_NUM_SCANCODES, false);
    keys_up.resize(SDL_NUM_SCANCODES, false);
    key_down_times.resize(SDL_NUM_SCANCODES, 0);
    key_up_times.resize(SDL_NUM_SCANCODES, 1);

    mouse_buttons.resize(5, false);
    mouse_buttons_down.resize(5, false);
    mouse_buttons_up.resize(5, false);
    mouse_button_down_times.resize(5, 0);
    mouse_button_up_times.resize(5, 1);

    OpenGameControllers();
}

InputEvent::~InputEvent()
{
    StopTextInput();
    CloseGameControllers();
}

InputEvent& InputEvent::GetInstance()
{
    static InputEvent instance;
    return instance;
}

InputEvent::ControllerState::ControllerState()
{
    buttons.resize(SDL_CONTROLLER_BUTTON_MAX, false);
    buttons_down.resize(SDL_CONTROLLER_BUTTON_MAX, false);
    buttons_up.resize(SDL_CONTROLLER_BUTTON_MAX, false);
    button_down_times.resize(SDL_CONTROLLER_BUTTON_MAX, 0);
    button_up_times.resize(SDL_CONTROLLER_BUTTON_MAX, 1);
}

void InputEvent::ClearInput()
{
    for (auto& controller : controllers)
    {
        std::fill(controller.buttons_down.begin(), controller.buttons_down.end(), false);
        std::fill(controller.buttons_up.begin(), controller.buttons_up.end(), false);
        controller.triggers_down[0] = controller.triggers_down[1] = false;
        controller.triggers_up[0] = controller.triggers_up[1] = false;
    }

    std::fill(keys_down.begin(), keys_down.end(), false);
    std::fill(keys_up.begin(), keys_up.end(), false);

    std::fill(mouse_buttons_down.begin(), mouse_buttons_down.end(), false);
    std::fill(mouse_buttons_up.begin(), mouse_buttons_up.end(), false);

    m_text_chars.clear();
    m_text_suppress_repeat = false;
}

void InputEvent::PollEvents()
{
    ClearInput();

    std::vector<SDL_Event> events = EventHandler::GetInstance().input_events;
    for (SDL_Event event : events)
    {
        switch (event.type)
        {
            case SDL_KEYDOWN:
                // KEYDOWN(repeat) precedes TEXTINPUT for held keys - suppress spam for codes.
                m_text_suppress_repeat = event.key.repeat != 0;
                if (!event.key.repeat)
                    HandleKeyDown(event.key.keysym.scancode);
                else
                {
                    // Keep held state true; do not edge-trigger again.
                    keys[event.key.keysym.scancode] = true;
                }
                break;
            case SDL_KEYUP:
                HandleKeyUp(event.key.keysym.scancode);
                break;
            case SDL_TEXTINPUT:
                if (m_text_active && !m_text_suppress_repeat && event.text.text[0])
                    m_text_chars.append(event.text.text);
                break;
            case SDL_MOUSEBUTTONDOWN:
                HandleMouseButtonDown(event.button.button);
                break;
            case SDL_MOUSEBUTTONUP:
                HandleMouseButtonUp(event.button.button);
                break;
            case SDL_CONTROLLERDEVICEADDED:
                // which = joystick device index
                HandleControllerAdded(event.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                // which = joystick instance id
                HandleControllerRemoved(event.cdevice.which);
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                HandleControllerButtonDown(event.cbutton.which,
                    static_cast<SDL_GameControllerButton>(event.cbutton.button));
                break;
            case SDL_CONTROLLERBUTTONUP:
                HandleControllerButtonUp(event.cbutton.which,
                    static_cast<SDL_GameControllerButton>(event.cbutton.button));
                break;
            default:
                break;
        }
    }

    SampleControllerAxes();
}

void InputEvent::StartTextInput()
{
    if (!m_text_active)
    {
        SDL_StartTextInput();
        m_text_active = true;
    }
    m_text_chars.clear();
}

void InputEvent::StopTextInput()
{
    if (m_text_active)
    {
        SDL_StopTextInput();
        m_text_active = false;
    }
    m_text_chars.clear();
    m_text_suppress_repeat = false;
}

void InputEvent::SampleControllerAxes()
{
    for (auto& c : controllers)
    {
        if (!c.handle)
            continue;

        for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; ++a)
        {
            const Sint16 raw = SDL_GameControllerGetAxis(c.handle, static_cast<SDL_GameControllerAxis>(a));
            float v = raw / 32767.f;
            if (v > 1.f) v = 1.f;
            if (v < -1.f) v = -1.f;

            const bool stick = a == SDL_CONTROLLER_AXIS_LEFTX || a == SDL_CONTROLLER_AXIS_LEFTY
                || a == SDL_CONTROLLER_AXIS_RIGHTX || a == SDL_CONTROLLER_AXIS_RIGHTY;
            if (stick && std::fabs(v) < kStickDeadzone)
                v = 0.f;
            // Triggers are 0..1 in practice (SDL reports 0..32767)
            if (a == SDL_CONTROLLER_AXIS_TRIGGERLEFT || a == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
            {
                if (v < 0.f) v = 0.f;
            }
            c.axes[a] = v;
        }

        const float lt = c.axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT];
        const float rt = c.axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT];
        const bool lt_now = lt >= kTriggerThreshold;
        const bool rt_now = rt >= kTriggerThreshold;

        if (lt_now && !c.triggers[0])
            c.triggers_down[0] = true;
        if (!lt_now && c.triggers[0])
            c.triggers_up[0] = true;
        c.triggers[0] = lt_now;

        if (rt_now && !c.triggers[1])
            c.triggers_down[1] = true;
        if (!rt_now && c.triggers[1])
            c.triggers_up[1] = true;
        c.triggers[1] = rt_now;
    }
}

void InputEvent::OpenGameControllers()
{
    const int n = SDL_NumJoysticks();
    for (int i = 0; i < n; ++i)
        HandleControllerAdded(i);
}

void InputEvent::CloseGameControllers()
{
    for (auto& c : controllers)
    {
        if (c.handle)
        {
            SDL_GameControllerClose(c.handle);
            c.handle = nullptr;
        }
    }
    controllers.clear();
}

int InputEvent::IndexFromInstance(SDL_JoystickID instanceId) const
{
    for (int i = 0; i < (int)controllers.size(); ++i)
    {
        if (controllers[i].instance_id == instanceId)
            return i;
    }
    return -1;
}

int InputEvent::GetControllerCount() const
{
    return (int)controllers.size();
}

bool InputEvent::IsControllerConnected(int controllerIndex) const
{
    return controllerIndex >= 0
        && controllerIndex < (int)controllers.size()
        && controllers[controllerIndex].handle != nullptr;
}

int InputEvent::GetControllerInstanceId(int controllerIndex) const
{
    if (!IsControllerConnected(controllerIndex))
        return -1;
    return (int)controllers[controllerIndex].instance_id;
}

int InputEvent::GetControllerIndexFromInstance(int instanceId) const
{
    return IndexFromInstance((SDL_JoystickID)instanceId);
}

bool InputEvent::AnyControllerInputThisFrame(int controllerIndex) const
{
    if (!IsControllerConnected(controllerIndex))
        return false;
    const auto& c = controllers[controllerIndex];
    for (bool down : c.buttons_down)
    {
        if (down)
            return true;
    }
    if (c.triggers_down[0] || c.triggers_down[1])
        return true;
    // Sticks have no press edge, so treat a decisive push as input.
    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; ++a)
    {
        const bool stick = a == SDL_CONTROLLER_AXIS_LEFTX || a == SDL_CONTROLLER_AXIS_LEFTY
            || a == SDL_CONTROLLER_AXIS_RIGHTX || a == SDL_CONTROLLER_AXIS_RIGHTY;
        if (stick && std::fabs(c.axes[a]) > 0.5f)
            return true;
    }
    return false;
}

bool InputEvent::IsControllerButtonPressed(int controllerIndex, int button) const
{
    if (!IsControllerConnected(controllerIndex))
        return false;
    const auto& c = controllers[controllerIndex];
    if (button == PAD_TRIGGERLEFT)
        return c.triggers[0];
    if (button == PAD_TRIGGERRIGHT)
        return c.triggers[1];
    if (button < 0 || button >= (int)c.buttons.size())
        return false;
    return c.buttons[button];
}

bool InputEvent::ControllerButtonDownThisFrame(int controllerIndex, int button) const
{
    if (!IsControllerConnected(controllerIndex))
        return false;
    const auto& c = controllers[controllerIndex];
    if (button == PAD_TRIGGERLEFT)
        return c.triggers_down[0];
    if (button == PAD_TRIGGERRIGHT)
        return c.triggers_down[1];
    if (button < 0 || button >= (int)c.buttons_down.size())
        return false;
    return c.buttons_down[button];
}

bool InputEvent::ControllerButtonUpThisFrame(int controllerIndex, int button) const
{
    if (!IsControllerConnected(controllerIndex))
        return false;
    const auto& c = controllers[controllerIndex];
    if (button == PAD_TRIGGERLEFT)
        return c.triggers_up[0];
    if (button == PAD_TRIGGERRIGHT)
        return c.triggers_up[1];
    if (button < 0 || button >= (int)c.buttons_up.size())
        return false;
    return c.buttons_up[button];
}

float InputEvent::GetControllerAxis(int controllerIndex, int axis) const
{
    if (!IsControllerConnected(controllerIndex))
        return 0.f;
    if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX)
        return 0.f;
    return controllers[controllerIndex].axes[axis];
}

void InputEvent::SetRumbleScale(float scale)
{
    if (scale < 0.f) scale = 0.f;
    if (scale > 1.f) scale = 1.f;
    rumble_scale = scale;
    if (rumble_scale <= 0.f)
    {
        for (int i = 0; i < (int)controllers.size(); ++i)
            StopControllerRumble(i);
    }
}

bool InputEvent::HasControllerRumble(int controllerIndex) const
{
    if (!IsControllerConnected(controllerIndex))
        return false;
    return SDL_GameControllerHasRumble(controllers[controllerIndex].handle) == SDL_TRUE;
}

bool InputEvent::RumbleController(int controllerIndex, float low_freq, float high_freq, Uint32 duration_ms)
{
    if (!IsControllerConnected(controllerIndex) || rumble_scale <= 0.f)
        return false;
    if (low_freq < 0.f) low_freq = 0.f;
    if (high_freq < 0.f) high_freq = 0.f;
    if (low_freq > 1.f) low_freq = 1.f;
    if (high_freq > 1.f) high_freq = 1.f;

    low_freq *= rumble_scale;
    high_freq *= rumble_scale;
    if (low_freq <= 0.f && high_freq <= 0.f)
        return StopControllerRumble(controllerIndex), true;

    const Uint16 low = (Uint16)(low_freq * 0xFFFF + 0.5f);
    const Uint16 high = (Uint16)(high_freq * 0xFFFF + 0.5f);
    return SDL_GameControllerRumble(controllers[controllerIndex].handle, low, high, duration_ms) == 0;
}

void InputEvent::StopControllerRumble(int controllerIndex)
{
    if (!IsControllerConnected(controllerIndex))
        return;
    SDL_GameControllerRumble(controllers[controllerIndex].handle, 0, 0, 0);
}

bool InputEvent::IsKeyPressed(SDL_Keycode key) const
{
    if (key < 0 || key >= (SDL_Keycode)keys.size())
        return false;
    return keys[key];
}

bool InputEvent::KeyDownThisFrame(SDL_Keycode key) const
{
    if (key < 0 || key >= (SDL_Keycode)keys_down.size())
        return false;
    return keys_down[key];
}

bool InputEvent::KeyUpThisFrame(SDL_Keycode key) const
{
    if (key < 0 || key >= (SDL_Keycode)keys_up.size())
        return false;
    return keys_up[key];
}

bool InputEvent::IsMouseButtonPressed(Uint8 button) const
{
    if (button >= mouse_buttons.size())
        return false;
    return mouse_buttons[button];
}

bool InputEvent::MouseButtonDownThisFrame(Uint8 button) const
{
    if (button >= mouse_buttons_down.size())
        return false;
    return mouse_buttons_down[button];
}

bool InputEvent::MouseButtonUpThisFrame(Uint8 button) const
{
    if (button >= mouse_buttons_up.size())
        return false;
    return mouse_buttons_up[button];
}

void InputEvent::HandleKeyDown(SDL_Scancode key)
{
    // Non-repeat only (repeats filtered in PollEvents). ClearInput wiped keys_down.
    keys[key] = true;
    keys_down[key] = true;
    key_down_times[key] = SDL_GetTicks();
}

void InputEvent::HandleKeyUp(SDL_Scancode key)
{
    keys[key] = false;
    keys_up[key] = true;
    key_up_times[key] = SDL_GetTicks();
}

void InputEvent::HandleMouseButtonDown(Uint8 button)
{
    if (button >= mouse_buttons.size())
        return;
    mouse_buttons[button] = true;
    if (mouse_button_down_times[button] < SDL_GetTicks() && mouse_button_up_times[button] < SDL_GetTicks()
        && mouse_button_down_times[button] < mouse_button_up_times[button])
    {
        mouse_button_down_times[button] = SDL_GetTicks();
        mouse_buttons_down[button] = true;
    }
}

void InputEvent::HandleMouseButtonUp(Uint8 button)
{
    if (button >= mouse_buttons.size())
        return;
    mouse_buttons[button] = false;
    mouse_buttons_up[button] = true;
    mouse_button_up_times[button] = SDL_GetTicks();
}

void InputEvent::HandleControllerButtonDown(SDL_JoystickID instanceId, SDL_GameControllerButton button)
{
    const int idx = IndexFromInstance(instanceId);
    if (idx < 0 || button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX)
        return;
    auto& c = controllers[idx];
    c.buttons[button] = true;
    if (c.button_down_times[button] < SDL_GetTicks() && c.button_up_times[button] < SDL_GetTicks()
        && c.button_down_times[button] < c.button_up_times[button])
    {
        c.button_down_times[button] = SDL_GetTicks();
        c.buttons_down[button] = true;
    }
}

void InputEvent::HandleControllerButtonUp(SDL_JoystickID instanceId, SDL_GameControllerButton button)
{
    const int idx = IndexFromInstance(instanceId);
    if (idx < 0 || button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX)
        return;
    auto& c = controllers[idx];
    c.buttons[button] = false;
    c.buttons_up[button] = true;
    c.button_up_times[button] = SDL_GetTicks();
}

void InputEvent::HandleControllerAdded(int joystickDeviceIndex)
{
    if (!SDL_IsGameController(joystickDeviceIndex))
        return;

    SDL_GameController* pad = SDL_GameControllerOpen(joystickDeviceIndex);
    if (!pad)
        return;

    SDL_Joystick* joy = SDL_GameControllerGetJoystick(pad);
    const SDL_JoystickID id = joy ? SDL_JoystickInstanceID(joy) : -1;
    if (id >= 0 && IndexFromInstance(id) >= 0)
    {
        // Already tracked (duplicate add)
        SDL_GameControllerClose(pad);
        return;
    }

    ControllerState state;
    state.handle = pad;
    state.instance_id = id;
    controllers.push_back(std::move(state));
}

void InputEvent::HandleControllerRemoved(SDL_JoystickID instanceId)
{
    const int idx = IndexFromInstance(instanceId);
    if (idx < 0)
        return;
    if (controllers[idx].handle)
    {
        SDL_GameControllerRumble(controllers[idx].handle, 0, 0, 0);
        SDL_GameControllerClose(controllers[idx].handle);
    }
    controllers.erase(controllers.begin() + idx);
}
