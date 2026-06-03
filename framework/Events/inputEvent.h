#pragma once
#include "SDL.h"
#include "keys.h"

#include <string>
#include <vector>

#include "event.h"

class EventHandler;

// Virtual pad buttons for analog triggers (beyond SDL_CONTROLLER_BUTTON_MAX).
inline constexpr int PAD_TRIGGERLEFT = 100;
inline constexpr int PAD_TRIGGERRIGHT = 101;

class InputEvent
{
public:
    static InputEvent& GetInstance();

    void PollEvents();

    int GetControllerCount() const;
    bool IsControllerConnected(int controllerIndex) const;

    // Slot indices shift when a pad is unplugged; instance ids never do.
    int GetControllerInstanceId(int controllerIndex) const;
    int GetControllerIndexFromInstance(int instanceId) const;

    // True if this pad produced any input this frame: button, trigger, d-pad or stick.
    bool AnyControllerInputThisFrame(int controllerIndex) const;

    bool IsControllerButtonPressed(int controllerIndex, int button) const;
    bool ControllerButtonDownThisFrame(int controllerIndex, int button) const;
    bool ControllerButtonUpThisFrame(int controllerIndex, int button) const;

    // Sticks: -1..1 with deadzone applied (0 inside deadzone).
    // Triggers: 0..1 (no deadzone zeroing; use trigger threshold for digital).
    float GetControllerAxis(int controllerIndex, int axis) const;

    // Rumble. Amplitudes are 0..1; duration is milliseconds.
    // low_freq = heavy motor, high_freq = light motor. Global scale multiplies both.
    bool HasControllerRumble(int controllerIndex) const;
    bool RumbleController(int controllerIndex, float low_freq, float high_freq, Uint32 duration_ms);
    void StopControllerRumble(int controllerIndex);

    float GetRumbleScale() const { return rumble_scale; }
    void SetRumbleScale(float scale);

    bool IsKeyPressed(SDL_Keycode key) const;
    bool KeyDownThisFrame(SDL_Keycode key) const;
    bool KeyUpThisFrame(SDL_Keycode key) const;

    bool IsKeyPressed(Key key) const { return IsKeyPressed((SDL_Keycode)KeyToScancode(key)); }
    bool KeyDownThisFrame(Key key) const { return KeyDownThisFrame((SDL_Keycode)KeyToScancode(key)); }
    bool KeyUpThisFrame(Key key) const { return KeyUpThisFrame((SDL_Keycode)KeyToScancode(key)); }

    bool IsMouseButtonPressed(Uint8 button) const;
    bool MouseButtonDownThisFrame(Uint8 button) const;
    bool MouseButtonUpThisFrame(Uint8 button) const;

    bool IsMouseButtonPressed(MouseButton button) const { return IsMouseButtonPressed((Uint8)button); }
    bool MouseButtonDownThisFrame(MouseButton button) const { return MouseButtonDownThisFrame((Uint8)button); }
    bool MouseButtonUpThisFrame(MouseButton button) const { return MouseButtonUpThisFrame((Uint8)button); }

    // SDL text composition (JOIN CODE / future chat). Start while field focused.
    void StartTextInput();
    void StopTextInput();
    bool TextInputActive() const { return m_text_active; }
    // UTF-8 chars composed this frame (key-repeat TEXTINPUT suppressed).
    const std::string& TextCharsThisFrame() const { return m_text_chars; }

private:
    friend class EventHandler;

    InputEvent();
    ~InputEvent();

    void ClearInput();
    void SampleControllerAxes();

    void OpenGameControllers();
    void CloseGameControllers();

    void HandleKeyDown(SDL_Scancode key);
    void HandleKeyUp(SDL_Scancode key);
    void HandleMouseButtonDown(Uint8 button);
    void HandleMouseButtonUp(Uint8 button);
    void HandleControllerAdded(int joystickDeviceIndex);
    void HandleControllerRemoved(SDL_JoystickID instanceId);
    void HandleControllerButtonDown(SDL_JoystickID instanceId, SDL_GameControllerButton button);
    void HandleControllerButtonUp(SDL_JoystickID instanceId, SDL_GameControllerButton button);

    int IndexFromInstance(SDL_JoystickID instanceId) const;

    struct ControllerState
    {
        SDL_GameController* handle = nullptr;
        SDL_JoystickID instance_id = -1;

        std::vector<bool> buttons;
        std::vector<bool> buttons_down;
        std::vector<bool> buttons_up;
        std::vector<Uint32> button_down_times;
        std::vector<Uint32> button_up_times;

        float axes[SDL_CONTROLLER_AXIS_MAX]{};

        // Digital trigger state (LT=0, RT=1)
        bool triggers[2]{};
        bool triggers_down[2]{};
        bool triggers_up[2]{};

        ControllerState();
    };
    std::vector<ControllerState> controllers;

    std::vector<bool> keys;
    std::vector<bool> keys_down;
    std::vector<bool> keys_up;
    std::vector<Uint32> key_down_times;
    std::vector<Uint32> key_up_times;

    std::vector<bool> mouse_buttons;
    std::vector<bool> mouse_buttons_down;
    std::vector<bool> mouse_buttons_up;
    std::vector<Uint32> mouse_button_down_times;
    std::vector<Uint32> mouse_button_up_times;

    bool m_text_active = false;
    bool m_text_suppress_repeat = false;
    std::string m_text_chars;

    static constexpr float kStickDeadzone = 0.25f;
    static constexpr float kTriggerThreshold = 0.30f;

    float rumble_scale = 1.f;
};
