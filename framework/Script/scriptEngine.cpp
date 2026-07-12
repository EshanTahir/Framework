#include "scriptEngine.h"
#include "scriptInternal.h"
#include "Audio/audioDebug.h"
#include "Audio/musicPlayer.h"
#include "Core/assetCache.h"
#include "Core/log.h"
#include "Core/paths.h"
#include "Events/appEvent.h"
#include "Events/inputEvent.h"
#include "Events/keys.h"
#include "Match/simClock.h"
#include "Render/worldAtlas.h"
#include "Scene/sceneHandler.h"
#include "Scene/systems.h"

#include "SDL.h"
#include "box2d.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef FW_HAS_LUAU
#include "lua.h"
#include "lualib.h"
#include "luacode.h"
#endif


namespace fs = std::filesystem;

using ScriptInternal::g_registry;
using ScriptInternal::g_script_events;
using ScriptInternal::g_ropes;
using ScriptInternal::g_sources;
using ScriptInternal::g_runtimes;
using ScriptInternal::RopeHandle;
using ScriptInternal::ScriptSource;
using ScriptInternal::ScriptRuntime;
using ScriptInternal::BodyOf;
using ScriptInternal::LiveRope;
using ScriptInternal::DropRopeFor;
using ScriptInternal::SyncBodyWorldPos;
using ScriptInternal::EntityFromId;
using ScriptInternal::FindByTag;
using ScriptInternal::FileMtime;
using ScriptInternal::LoadSourceFile;
using ScriptInternal::MarkPathDirty;
using ScriptInternal::RefreshSource;
using ScriptInternal::PlaySoundLocal;
using ScriptInternal::PlaySoundLocalAt;
using ScriptInternal::BusNameToId;
#ifdef FW_HAS_LUAU
using ScriptInternal::g_lua;
using ScriptInternal::g_luaExtraBinds;
using ScriptInternal::ResolveArg;
using ScriptInternal::PumpMatchEvents;
#endif
using ScriptInternal::g_ui_default_font;
using ScriptInternal::UiPaintDefaults;

#ifdef FW_HAS_LUAU
namespace
{
static lua_State* EnsureSharedLuau()
{
    if (g_lua)
        return g_lua;
    g_lua = luaL_newstate();
    luaL_openlibs(g_lua);
    // Best-practice sandbox: drop debug (upvalues/hooks), freeze stdlibs + globals.
    // Luau's os lib is already time-only (no execute/getenv). No separate io lib.
    lua_pushnil(g_lua);
    lua_setglobal(g_lua, "debug");
    ScriptInternal::ScriptPushEngineTable(g_lua);
    luaL_sandbox(g_lua);
    FW_CORE_INFO("Luau sandbox active (debug stripped, libs readonly, safeenv)");
    return g_lua;
}

int NewInstanceEnv(lua_State* L)
{
    lua_newtable(L); // env
    lua_newtable(L); // meta
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    return lua_ref(L, -1); // env stays on the stack
}

bool PushInstanceEnv(lua_State* L, const ScriptRuntime& rt)
{
    if (rt.env_ref < 0)
        return false;
    lua_getref(L, rt.env_ref);
    if (!lua_istable(L, -1))
    {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

void CallInstanceHook(lua_State* L, const ScriptRuntime& rt, const char* name,
                      const std::string& chunk_name, float* dt)
{
    if (!PushInstanceEnv(L, rt))
        return;
    lua_getfield(L, -1, name);
    if (!lua_isfunction(L, -1))
    {
        lua_pop(L, 2);
        return;
    }
    int args = 0;
    if (dt)
    {
        lua_pushnumber(L, *dt);
        args = 1;
    }
    if (lua_pcall(L, args, 0, 0) != 0)
    {
        FW_CORE_ERROR("Luau {0} ({1}): {2}", name, chunk_name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_pop(L, 1); // env
}

bool EnsureLuauInstance(ScriptRuntime& rt, ScriptComponent& script,
                        const std::string& chunk_name, uint32_t self_id)
{
    lua_State* L = EnsureSharedLuau();
    if (!L)
        return false;
    if (!rt.need_reexec && rt.env_ref >= 0)
        return true;

    size_t bc_size = 0;
    char* bytecode = luau_compile(rt.source.c_str(), rt.source.size(), nullptr, &bc_size);
    if (!bytecode)
    {
        FW_CORE_ERROR("Luau compile failed: {0}", chunk_name);
        return false;
    }

    const int load_ok = luau_load(L, chunk_name.c_str(), bytecode, bc_size, 0);
    free(bytecode);
    if (load_ok != 0)
    {
        FW_CORE_ERROR("Luau load failed ({0}): {1}", chunk_name, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    if (rt.env_ref >= 0)
    {
        lua_unref(L, rt.env_ref);
        rt.env_ref = -1;
    }

    const int env_ref = NewInstanceEnv(L); // env on stack, closure below it
    lua_pushinteger(L, (int)self_id);
    lua_setfield(L, -2, "self_id");
    if (lua_setfenv(L, -2) == 0)
    {
        FW_CORE_ERROR("Luau setfenv failed: {0}", chunk_name);
        lua_pop(L, 1);
        lua_unref(L, env_ref);
        return false;
    }

    if (lua_pcall(L, 0, 0, 0) != 0)
    {
        FW_CORE_ERROR("Luau init error ({0}): {1}", chunk_name, lua_tostring(L, -1));
        lua_pop(L, 1);
        lua_unref(L, env_ref);
        return false;
    }
    rt.env_ref = env_ref;

    // First boot → on_enter; 1 / file change → on_reload (keeps game state)
    const bool booted = script.booted;
    script.booted = true;
    rt.need_reexec = false;

    // A missing on_reload is deliberate opt-out: don't fall back to on_enter mid-session.
    CallInstanceHook(L, rt, booted ? "on_reload" : "on_enter", chunk_name, nullptr);

    FW_CORE_INFO("Luau (re)loaded {0} [entity {1}]", chunk_name, self_id);
    return true;
}

} // namespace
#endif

ScriptEngine& ScriptEngine::GetInstance()
{
    static ScriptEngine i;
    return i;
}

entt::registry* ScriptEngine::ActiveRegistry() { return g_registry; }
void ScriptEngine::SetActiveRegistry(entt::registry* reg) { g_registry = reg; }

void ScriptEngine::SetLuaExtraBinds(LuaExtraBindsFn fn)
{
#ifdef FW_HAS_LUAU
    g_luaExtraBinds = fn;
#else
    (void)fn;
#endif
}

void ScriptEngine::Init()
{
    m_ready = true;
    FW_CORE_INFO("ScriptEngine ready (hot-reload={0})", hot_reload_enabled);
}

void ScriptEngine::Shutdown()
{
#ifdef FW_HAS_LUAU
    if (g_lua)
    {
        lua_close(g_lua);
        g_lua = nullptr;
    }
    for (auto& [_, rt] : g_runtimes)
        rt.env_ref = -1;
#endif
    g_runtimes.clear();
    g_sources.clear();
    m_ready = false;
}

void ScriptEngine::BeginFrame(float dt)
{
    (void)dt;
    m_reload_all = false;
    // 1 = Luau hot-reload + texture/atlas refresh (was F2).
    if (InputEvent::GetInstance().KeyDownThisFrame((SDL_Keycode)SDL_SCANCODE_1))
    {
        m_reload_all = true;
        AssetCache::GetInstance().ReloadTextures();
        WorldAtlas::GetInstance().Clear();
        WorldAtlas::GetInstance().EnsureContentPaths();
        FW_CORE_INFO("1: hot-reload scripts + world textures");
    }
#ifdef FW_HAS_LUAU
    PumpMatchEvents();
#endif
}

void ScriptEngine::DropEntity(uint32_t entity_id)
{
    auto it = g_runtimes.find(entity_id);
    if (it == g_runtimes.end())
        return;
#ifdef FW_HAS_LUAU
    if (g_lua && it->second.env_ref >= 0)
        lua_unref(g_lua, it->second.env_ref);
    it->second.env_ref = -1;
#endif
    DropRopeFor(entity_id);
    g_runtimes.erase(it);
}

void ScriptEngine::Tick(entt::registry& registry, entt::entity entity, ScriptComponent& script, float dt)
{
    SetActiveRegistry(&registry);

    if (script.path.empty())
    {
        if (script.entering_scene)
        {
            script.OnEnter();
            script.entering_scene = false;
        }
        else
            script.Update(dt);
        return;
    }

    std::string resolved = Paths::Resolve(script.path);
    const uint32_t eid = (uint32_t)entity;
    auto& rt = g_runtimes[eid];

    if (!RefreshSource(resolved, m_reload_all && hot_reload_enabled))
    {
        if (!script.loaded)
            FW_CORE_ERROR("Script not found: {0}", resolved);
        script.loaded = true;
        script.entering_scene = false;
        return;
    }

    auto& src = g_sources[resolved];
    if (rt.path != resolved || rt.mtime != src.mtime || rt.source.empty())
    {
        rt.path = resolved;
        rt.source = src.source;
        rt.mtime = src.mtime;
        rt.need_reexec = true;
        if (script.loaded && hot_reload_enabled)
            FW_CORE_INFO("Hot-reload: {0} (entity {1})", resolved, eid);
    }
    script.loaded = true;
    script.entering_scene = false;

#ifdef FW_HAS_LUAU
    if (script.language == ScriptLanguage::Luau)
    {
        if (rt.source.empty())
            return;
        if (!EnsureLuauInstance(rt, script, script.path, eid))
            return;

        lua_State* L = g_lua;
        if (!PushInstanceEnv(L, rt))
            return;
        lua_pushinteger(L, (int)eid);
        lua_setfield(L, -2, "self_id");
        lua_pop(L, 1);

        CallInstanceHook(L, rt, "update", script.path, &dt);
        return;
    }
#endif


    if (script.entering_scene)
    {
        script.OnEnter();
        script.entering_scene = false;
    }
    else
        script.Update(dt);
}

void ScriptEngine::TickPostPhys(entt::registry& registry, entt::entity entity, ScriptComponent& script,
                                float dt)
{
    SetActiveRegistry(&registry);
    if (script.path.empty())
        return;

    const uint32_t eid = (uint32_t)entity;
    auto it = g_runtimes.find(eid);
    if (it == g_runtimes.end())
        return;
    auto& rt = it->second;

#ifdef FW_HAS_LUAU
    if (script.language == ScriptLanguage::Luau)
    {
        if (!g_lua || rt.env_ref < 0)
            return;
        lua_State* L = g_lua;
        if (!PushInstanceEnv(L, rt))
            return;
        lua_pushinteger(L, (int)eid);
        lua_setfield(L, -2, "self_id");
        lua_pop(L, 1);
        CallInstanceHook(L, rt, "on_post_phys", script.path, &dt);
        return;
    }
#endif

}


