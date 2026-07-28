#ifdef FW_HAS_LUAU

#include "scriptInternal.h"
#include "Core/glContext.h"
#include "Core/log.h"
#include "Core/paths.h"
#include "Events/appEvent.h"
#include "Match/simClock.h"
#include "Scene/sceneHandler.h"
#include "SDL.h"
#include "lua.h"
#include "lualib.h"
#include <cmath>

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

int l_log(lua_State* L)
{
    const char* s = luaL_checkstring(L, 1);
    FW_CORE_INFO("[luau] {0}", s);
    return 0;
}

int l_log_warn(lua_State* L)
{
    FW_CORE_WARN("[luau] {0}", luaL_checkstring(L, 1));
    return 0;
}

int l_log_error(lua_State* L)
{
    FW_CORE_ERROR("[luau] {0}", luaL_checkstring(L, 1));
    return 0;
}

int l_get_entity(lua_State* L)
{
    const char* tag = luaL_checkstring(L, 1);
    auto e = FindByTag(tag);
    if (e == entt::null)
        lua_pushnil(L);
    else
        lua_pushinteger(L, (int)(uint32_t)e);
    return 1;
}

int l_list_tags(lua_State* L)
{
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    lua_newtable(L);
    if (!scene)
        return 1;
    const auto tags = scene->ListTags();
    int i = 1;
    for (const auto& tag : tags)
    {
        lua_pushstring(L, tag.c_str());
        lua_rawseti(L, -2, i++);
    }
    return 1;
}

int l_entity_exists(lua_State* L)
{
    auto e = ResolveEntityArg(L, 1);
    lua_pushboolean(L, g_registry && g_registry->valid(e));
    return 1;
}

int l_get_tag(lua_State* L)
{
    uint32_t id = (uint32_t)luaL_checkinteger(L, 1);
    if (!g_registry) { lua_pushstring(L, ""); return 1; }
    auto e = EntityFromId(id);
    if (!g_registry->valid(e) || !g_registry->has<TagComponent>(e))
    { lua_pushstring(L, ""); return 1; }
    lua_pushstring(L, g_registry->get<TagComponent>(e).tag.c_str());
    return 1;
}

int l_set_enabled_debug(lua_State* L)
{
    // no-op placeholder / could toggle renderer flag via scene later
    (void)L;
    return 0;
}

int l_get_time(lua_State* L)
{
    lua_pushnumber(L, SDL_GetTicks() / 1000.0);
    return 1;
}

int l_distance(lua_State* L)
{
    float x1 = (float)luaL_checknumber(L, 1);
    float y1 = (float)luaL_checknumber(L, 2);
    float x2 = (float)luaL_checknumber(L, 3);
    float y2 = (float)luaL_checknumber(L, 4);
    float dx = x2 - x1, dy = y2 - y1;
    lua_pushnumber(L, std::sqrt(dx * dx + dy * dy));
    return 1;
}

int l_lerp(lua_State* L)
{
    float a = (float)luaL_checknumber(L, 1);
    float b = (float)luaL_checknumber(L, 2);
    float t = (float)luaL_checknumber(L, 3);
    lua_pushnumber(L, a + (b - a) * t);
    return 1;
}

int l_set_active_scene(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    // deferred; never tear down mid Scene::Update.
    SceneHandler::GetInstance().RequestActiveScene(name);
    return 0;
}

int l_request_quit(lua_State* L)
{
    (void)L;
    AppEvent::GetInstance().RequestQuit();
    return 0;
}

int l_set_paused(lua_State* L)
{
    SimClock::GetInstance().SetPaused(lua_toboolean(L, 1) != 0);
    return 0;
}

int l_is_paused(lua_State* L)
{
    lua_pushboolean(L, SimClock::GetInstance().Paused() ? 1 : 0);
    return 1;
}

int l_get_screen_size(lua_State* L)
{
    auto& gl = GlContext::GetInstance();
    lua_pushinteger(L, gl.Width());
    lua_pushinteger(L, gl.Height());
    return 2;
}

int l_reload_scripts(lua_State* L)
{
    (void)L;
    for (auto& [path, _] : g_sources)
        RefreshSource(path, true);
    for (auto& [_, rt] : g_runtimes)
        rt.need_reexec = true;
    return 0;
}

int l_user_data_path(lua_State* L)
{
    lua_pushstring(L, Paths::GetUserDataRoot().c_str());
    return 1;
}

void RegisterMiscBindsImpl(lua_State* L)
{
#define BIND(name, fn) lua_pushcfunction(L, fn, #name); lua_setfield(L, -2, #name)
    BIND(log, l_log);
    BIND(warn, l_log_warn);
    BIND(error, l_log_error);
    BIND(get_entity, l_get_entity);
    BIND(list_tags, l_list_tags);
    BIND(entity_exists, l_entity_exists);
    BIND(get_tag, l_get_tag);
    BIND(get_time, l_get_time);
    BIND(distance, l_distance);
    BIND(lerp, l_lerp);
    BIND(set_active_scene, l_set_active_scene);
    BIND(request_quit, l_request_quit);
    BIND(set_paused, l_set_paused);
    BIND(is_paused, l_is_paused);
    BIND(get_screen_size, l_get_screen_size);
    BIND(reload_scripts, l_reload_scripts);
    BIND(user_data_path, l_user_data_path);
#undef BIND
}

} // namespace

void ScriptInternal::RegisterMiscBinds(lua_State* L)
{
    RegisterMiscBindsImpl(L);
}

#endif // FW_HAS_LUAU
