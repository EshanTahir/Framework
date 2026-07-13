#ifdef FW_HAS_LUAU

#include "scriptInternal.h"
#include "Audio/audio.h"
#include "Audio/audioDebug.h"
#include "Audio/musicPlayer.h"
#include "Scene/sceneHandler.h"
#include "Scene/sfxPool.h"
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

int l_play_sound(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene || !path || !g_registry)
        return 0;
    static std::unordered_map<std::string, std::string> paths;
    auto& stored = paths[path] = path;
    float vol = (float)luaL_optnumber(L, 2, 1.0);
    int bus = BusNameToId(luaL_optstring(L, 3, "sfx"));
    SfxPool::Play(*g_registry, *scene, stored.c_str(), vol, bus);
    return 0;
}

int l_play_sound_at(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene || !path || !g_registry)
        return 0;
    static std::unordered_map<std::string, std::string> paths;
    auto& stored = paths[path] = path;
    float vol = (float)luaL_optnumber(L, 4, 1.0);
    int bus = BusNameToId(luaL_optstring(L, 5, "sfx"));
    SfxPool::PlayAt(*g_registry, *scene, stored.c_str(), x, y, vol, bus);
    return 0;
}

int l_audio_debug_hud(lua_State* L)
{
    char buf[2048];
    AudioDebug::FormatHud(buf, sizeof(buf));
    lua_pushstring(L, buf);
    return 1;
}

int l_audio_spatial_bypass(lua_State* L)
{
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1))
        AudioDebug::SetBypassSpatial(lua_toboolean(L, 1) != 0);
    lua_pushboolean(L, AudioDebug::BypassSpatial() ? 1 : 0);
    return 1;
}

int l_audio_listener(lua_State* L)
{
    float x = 0.f, y = 0.f;
    Audio::GetInstance().GetListener(x, y);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    return 2;
}

int l_audio_spatial_gain(lua_State* L)
{
    float x = (float)luaL_checknumber(L, 1);
    float y = (float)luaL_checknumber(L, 2);
    // Refresh listener from camera if possible (same as PlayAt).
    if (g_registry)
    {
        entt::entity tagged = entt::null;
        auto view = g_registry->view<CameraComponent, TransformComponent, TagComponent>();
        for (auto e : view)
        {
            if (g_registry->get<TagComponent>(e).tag == "camera")
            {
                tagged = e;
                break;
            }
        }
        if (tagged != entt::null)
        {
            auto& t = g_registry->get<TransformComponent>(tagged);
            Audio::GetInstance().SetListener(t.x, t.y);
        }
    }
    lua_pushnumber(L, Audio::GetInstance().SpatialGain(x, y));
    return 1;
}

int l_music_play(lua_State* L)
{
    const char* path = luaL_checkstring(L, 1);
    float fade = (float)luaL_optnumber(L, 2, 1.6);
    lua_pushboolean(L, MusicPlayer::GetInstance().Play(path, fade) ? 1 : 0);
    return 1;
}

int l_music_stop(lua_State* L)
{
    float fade = (float)luaL_optnumber(L, 1, 1.0);
    MusicPlayer::GetInstance().Stop(fade);
    return 0;
}

int l_music_current(lua_State* L)
{
    lua_pushstring(L, MusicPlayer::GetInstance().CurrentPath().c_str());
    return 1;
}

int l_music_playing(lua_State* L)
{
    lua_pushboolean(L, MusicPlayer::GetInstance().IsPlaying() ? 1 : 0);
    return 1;
}

void RegisterAudioBindsImpl(lua_State* L)
{
#define BIND(name, fn) lua_pushcfunction(L, fn, #name); lua_setfield(L, -2, #name)
    BIND(play_sound, l_play_sound);
    BIND(play_sound_at, l_play_sound_at);
    BIND(audio_debug_hud, l_audio_debug_hud);
    BIND(audio_spatial_bypass, l_audio_spatial_bypass);
    BIND(audio_listener, l_audio_listener);
    BIND(audio_spatial_gain, l_audio_spatial_gain);
    BIND(music_play, l_music_play);
    BIND(music_stop, l_music_stop);
    BIND(music_current, l_music_current);
    BIND(music_playing, l_music_playing);
#undef BIND
}

} // namespace

void ScriptInternal::RegisterAudioBinds(lua_State* L)
{
    RegisterAudioBindsImpl(L);
}

#endif // FW_HAS_LUAU
