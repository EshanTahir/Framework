#ifdef FW_HAS_LUAU

#include "scriptInternal.h"
#include "Events/inputEvent.h"
#include "Events/keys.h"
#include "SDL.h"
#include "lua.h"
#include "lualib.h"

using ScriptInternal::g_registry;
using ScriptInternal::g_ropes;
using ScriptInternal::g_sources;
using ScriptInternal::g_runtimes;
using ScriptInternal::g_script_events;
using ScriptInternal::BodyOf;
using ScriptInternal::LiveRope;
using ScriptInternal::DropRopeFor;
using ScriptInternal::SyncBodyWorldPos;
using ScriptInternal::EntityFromId;
using ScriptInternal::FindByTag;
using ScriptInternal::ResolveArg;
using ScriptInternal::ResolveEntityArg;
using ScriptInternal::BusNameToId;
using ScriptInternal::RefreshSource;
using ScriptInternal::RopeHandle;

namespace {

int l_is_key_down(lua_State* L)
{
    int sc = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, InputEvent::GetInstance().IsKeyPressed((SDL_Keycode)sc));
    return 1;
}

int l_key_down_this_frame(lua_State* L)
{
    int sc = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, InputEvent::GetInstance().KeyDownThisFrame((SDL_Keycode)sc));
    return 1;
}

int l_controller_count(lua_State* L)
{
    lua_pushinteger(L, InputEvent::GetInstance().GetControllerCount());
    return 1;
}

int l_is_controller_down(lua_State* L)
{
    int idx = (int)luaL_optinteger(L, 1, 0);
    int btn = (int)luaL_checkinteger(L, 2);
    lua_pushboolean(L, InputEvent::GetInstance().IsControllerButtonPressed(idx, btn));
    return 1;
}

int l_controller_down(lua_State* L)
{
    int idx = (int)luaL_optinteger(L, 1, 0);
    int btn = (int)luaL_checkinteger(L, 2);
    lua_pushboolean(L, InputEvent::GetInstance().ControllerButtonDownThisFrame(idx, btn));
    return 1;
}

int l_controller_up(lua_State* L)
{
    int idx = (int)luaL_optinteger(L, 1, 0);
    int btn = (int)luaL_checkinteger(L, 2);
    lua_pushboolean(L, InputEvent::GetInstance().ControllerButtonUpThisFrame(idx, btn));
    return 1;
}

int l_get_axis(lua_State* L)
{
    int idx = (int)luaL_optinteger(L, 1, 0);
    int axis = (int)luaL_checkinteger(L, 2);
    lua_pushnumber(L, InputEvent::GetInstance().GetControllerAxis(idx, axis));
    return 1;
}

int l_rumble(lua_State* L)
{
    int idx = (int)luaL_optinteger(L, 1, 0);
    float low = (float)luaL_checknumber(L, 2);
    float high = (float)luaL_checknumber(L, 3);
    int ms = (int)luaL_optinteger(L, 4, 120);
    if (ms < 0) ms = 0;
    lua_pushboolean(L, InputEvent::GetInstance().RumbleController(idx, low, high, (Uint32)ms));
    return 1;
}

int l_stop_rumble(lua_State* L)
{
    int idx = (int)luaL_optinteger(L, 1, 0);
    InputEvent::GetInstance().StopControllerRumble(idx);
    return 0;
}

int l_has_rumble(lua_State* L)
{
    int idx = (int)luaL_optinteger(L, 1, 0);
    lua_pushboolean(L, InputEvent::GetInstance().HasControllerRumble(idx));
    return 1;
}

int l_set_rumble_scale(lua_State* L)
{
    float s = (float)luaL_checknumber(L, 1);
    InputEvent::GetInstance().SetRumbleScale(s);
    return 0;
}

int l_get_rumble_scale(lua_State* L)
{
    lua_pushnumber(L, InputEvent::GetInstance().GetRumbleScale());
    return 1;
}

int l_get_mouse(lua_State* L)
{
    int x = 0, y = 0;
    SDL_GetMouseState(&x, &y);
    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    return 2;
}

int l_is_mouse_down(lua_State* L)
{
    int button = (int)luaL_optinteger(L, 1, 1); // 1=left
    Uint8 sdl = SDL_BUTTON_LEFT;
    if (button == 2) sdl = SDL_BUTTON_MIDDLE;
    if (button == 3) sdl = SDL_BUTTON_RIGHT;
    lua_pushboolean(L, InputEvent::GetInstance().IsMouseButtonPressed(sdl));
    return 1;
}

int l_mouse_down(lua_State* L)
{
    int button = (int)luaL_optinteger(L, 1, 1);
    Uint8 sdl = SDL_BUTTON_LEFT;
    if (button == 2) sdl = SDL_BUTTON_MIDDLE;
    if (button == 3) sdl = SDL_BUTTON_RIGHT;
    lua_pushboolean(L, InputEvent::GetInstance().MouseButtonDownThisFrame(sdl));
    return 1;
}

int l_any_key_down(lua_State* L)
{
    // First newly pressed key scancode this frame, or nil.
    auto& in = InputEvent::GetInstance();
    for (int sc = 0; sc < SDL_NUM_SCANCODES; ++sc)
    {
        if (in.KeyDownThisFrame((SDL_Keycode)sc))
        {
            lua_pushinteger(L, sc);
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

int l_text_input_start(lua_State* L)
{
    (void)L;
    InputEvent::GetInstance().StartTextInput();
    return 0;
}

int l_text_input_stop(lua_State* L)
{
    (void)L;
    InputEvent::GetInstance().StopTextInput();
    return 0;
}

int l_text_input_active(lua_State* L)
{
    lua_pushboolean(L, InputEvent::GetInstance().TextInputActive());
    return 1;
}

int l_text_input_chars(lua_State* L)
{
    const std::string& s = InputEvent::GetInstance().TextCharsThisFrame();
    lua_pushlstring(L, s.data(), s.size());
    return 1;
}

void RegisterKeyConstants(lua_State* L)
{
    auto set = [&](const char* name, int sc) {
        lua_pushinteger(L, sc);
        lua_setfield(L, -2, name);
    };
    set("KEY_A", SDL_SCANCODE_A);
    set("KEY_D", SDL_SCANCODE_D);
    set("KEY_W", SDL_SCANCODE_W);
    set("KEY_S", SDL_SCANCODE_S);
    set("KEY_LEFT", SDL_SCANCODE_LEFT);
    set("KEY_RIGHT", SDL_SCANCODE_RIGHT);
    set("KEY_UP", SDL_SCANCODE_UP);
    set("KEY_DOWN", SDL_SCANCODE_DOWN);
    set("KEY_SPACE", SDL_SCANCODE_SPACE);
    set("KEY_R", SDL_SCANCODE_R);
    set("KEY_F2", SDL_SCANCODE_F2);
    set("KEY_1", SDL_SCANCODE_1);
    set("KEY_ESCAPE", SDL_SCANCODE_ESCAPE);
    set("KEY_LSHIFT", SDL_SCANCODE_LSHIFT);
    set("KEY_RSHIFT", SDL_SCANCODE_RSHIFT);
    set("KEY_LCTRL", SDL_SCANCODE_LCTRL);
    set("KEY_E", SDL_SCANCODE_E);
    set("KEY_Q", SDL_SCANCODE_Q);
    set("KEY_K", SDL_SCANCODE_K);
    set("KEY_O", SDL_SCANCODE_O);
    set("KEY_P", SDL_SCANCODE_P);
    set("KEY_L", SDL_SCANCODE_L);
    set("KEY_0", SDL_SCANCODE_0);
    set("KEY_6", SDL_SCANCODE_6);
    set("KEY_7", SDL_SCANCODE_7);
    set("KEY_8", SDL_SCANCODE_8);
    set("KEY_9", SDL_SCANCODE_9);
    set("MOUSE_LEFT", 1);
    set("MOUSE_MIDDLE", 2);
    set("MOUSE_RIGHT", 3);

    // SDL_GameControllerButton
    set("PAD_A", SDL_CONTROLLER_BUTTON_A);
    set("PAD_B", SDL_CONTROLLER_BUTTON_B);
    set("PAD_X", SDL_CONTROLLER_BUTTON_X);
    set("PAD_Y", SDL_CONTROLLER_BUTTON_Y);
    set("PAD_BACK", SDL_CONTROLLER_BUTTON_BACK);
    set("PAD_GUIDE", SDL_CONTROLLER_BUTTON_GUIDE);
    set("PAD_START", SDL_CONTROLLER_BUTTON_START);
    set("PAD_LEFTSTICK", SDL_CONTROLLER_BUTTON_LEFTSTICK);
    set("PAD_RIGHTSTICK", SDL_CONTROLLER_BUTTON_RIGHTSTICK);
    set("PAD_LEFTSHOULDER", SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    set("PAD_RIGHTSHOULDER", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    set("PAD_DPAD_UP", SDL_CONTROLLER_BUTTON_DPAD_UP);
    set("PAD_DPAD_DOWN", SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    set("PAD_DPAD_LEFT", SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    set("PAD_DPAD_RIGHT", SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    // Virtual digital triggers (axis threshold in InputEvent)
    set("PAD_LT", PAD_TRIGGERLEFT);
    set("PAD_RT", PAD_TRIGGERRIGHT);

    // SDL_GameControllerAxis
    set("AXIS_LEFTX", SDL_CONTROLLER_AXIS_LEFTX);
    set("AXIS_LEFTY", SDL_CONTROLLER_AXIS_LEFTY);
    set("AXIS_RIGHTX", SDL_CONTROLLER_AXIS_RIGHTX);
    set("AXIS_RIGHTY", SDL_CONTROLLER_AXIS_RIGHTY);
    set("AXIS_TRIGGERLEFT", SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    set("AXIS_TRIGGERRIGHT", SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
}

void RegisterInputBindsImpl(lua_State* L)
{
#define BIND(name, fn) lua_pushcfunction(L, fn, #name); lua_setfield(L, -2, #name)
    BIND(is_key_down, l_is_key_down);
    BIND(key_down, l_key_down_this_frame);
    BIND(controller_count, l_controller_count);
    BIND(is_controller_down, l_is_controller_down);
    BIND(controller_down, l_controller_down);
    BIND(controller_up, l_controller_up);
    BIND(get_axis, l_get_axis);
    BIND(rumble, l_rumble);
    BIND(stop_rumble, l_stop_rumble);
    BIND(has_rumble, l_has_rumble);
    BIND(set_rumble_scale, l_set_rumble_scale);
    BIND(get_rumble_scale, l_get_rumble_scale);
    BIND(get_mouse, l_get_mouse);
    BIND(is_mouse_down, l_is_mouse_down);
    BIND(mouse_down, l_mouse_down);
    BIND(any_key_down, l_any_key_down);
    BIND(text_input_start, l_text_input_start);
    BIND(text_input_stop, l_text_input_stop);
    BIND(text_input_active, l_text_input_active);
    BIND(text_input_chars, l_text_input_chars);
#undef BIND
}

void RegisterKeyConstantsImpl(lua_State* L)
{
    RegisterKeyConstants(L);
}

} // namespace

void ScriptInternal::RegisterInputBinds(lua_State* L)
{
    RegisterInputBindsImpl(L);
}

void ScriptInternal::RegisterKeyConstants(lua_State* L)
{
    RegisterKeyConstantsImpl(L);
}

#endif // FW_HAS_LUAU
