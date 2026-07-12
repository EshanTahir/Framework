#pragma once

#include "scriptEngine.h"
#include "Scene/components.h"
#include "Match/matchEvents.h"

#include "box2d.h"
#include "entt.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct lua_State;


// Shared ScriptEngine state + helpers used by Luau bind TUs and runtime.
namespace ScriptInternal
{

extern entt::registry* g_registry;
extern std::vector<MatchEventWire> g_script_events;

struct RopeHandle
{
    b2World* world = nullptr;
    b2DistanceJoint* joint = nullptr;
};
extern std::unordered_map<uint32_t, RopeHandle> g_ropes;

struct ScriptSource
{
    std::string source;
    std::uintmax_t mtime = 0;
    bool dirty = true;
};

struct ScriptRuntime
{
    std::string path;
    std::string source;
    std::uintmax_t mtime = 0;
    bool need_reexec = true;
#ifdef FW_HAS_LUAU
    int env_ref = -1;
#endif
};

extern std::unordered_map<std::string, ScriptSource> g_sources;
extern std::unordered_map<uint32_t, ScriptRuntime> g_runtimes;

#ifdef FW_HAS_LUAU
extern lua_State* g_lua;
extern ScriptEngine::LuaExtraBindsFn g_luaExtraBinds;
#endif

void SyncBodyWorldPos(b2Body* body, float x, float y);
b2Body* BodyOf(entt::entity e);
b2DistanceJoint* LiveRope(uint32_t owner);
void DropRopeFor(uint32_t owner);

std::uintmax_t FileMtime(const std::string& path);
bool LoadSourceFile(ScriptSource& src, const std::string& resolved);
void MarkPathDirty(const std::string& resolved);
bool RefreshSource(const std::string& resolved, bool force);

entt::entity EntityFromId(uint32_t id);
entt::entity FindByTag(const char* tag);

int BusNameToId(const char* name);
void PlaySoundLocal(const char* path, float volume);
void PlaySoundLocalAt(const char* path, float volume, float x, float y);

// Shared with Luau UI binds.
extern std::string g_ui_default_font;
void UiPaintDefaults(ButtonComponent& b);

#ifdef FW_HAS_LUAU
entt::entity ResolveArg(lua_State* L, int idx);
entt::entity ResolveEntityArg(lua_State* L, int idx);
void PumpMatchEvents();

void RegisterInputBinds(lua_State* L);
void RegisterKeyConstants(lua_State* L);
void RegisterPhysicsBinds(lua_State* L);
void RegisterRenderBinds(lua_State* L);
void RegisterAudioBinds(lua_State* L);
void RegisterMatchBinds(lua_State* L);
void RegisterUiBinds(lua_State* L);
void RegisterMiscBinds(lua_State* L);

// Builds global `engine` table (domain binds + extra + key constants).
void ScriptPushEngineTable(lua_State* L);
#endif

} // namespace ScriptInternal
