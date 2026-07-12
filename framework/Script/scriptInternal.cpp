#include "scriptInternal.h"

#include "Audio/audio.h"
#include "Core/log.h"
#include "Match/matchEvents.h"
#include "Scene/entity.h"
#include "Scene/sceneHandler.h"
#include "Scene/sfxPool.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef FW_HAS_LUAU
#include "lua.h"
#include "lualib.h"
#endif

namespace fs = std::filesystem;

namespace ScriptInternal
{

entt::registry* g_registry = nullptr;
std::vector<MatchEventWire> g_script_events;
std::unordered_map<uint32_t, RopeHandle> g_ropes;
std::unordered_map<std::string, ScriptSource> g_sources;
std::unordered_map<uint32_t, ScriptRuntime> g_runtimes;

#ifdef FW_HAS_LUAU
lua_State* g_lua = nullptr;
ScriptEngine::LuaExtraBindsFn g_luaExtraBinds = nullptr;
#endif

void SyncBodyWorldPos(b2Body* body, float x, float y)
{
    if (!body)
        return;
    body->SetTransform(b2Vec2(x, y), body->GetAngle());
    const bool parked = IsParkedWorldPos(x, y);
    if (parked)
        body->SetLinearVelocity(b2Vec2(0.f, 0.f));
    body->SetEnabled(!parked);
}

b2Body* BodyOf(entt::entity e)
{
    if (!g_registry || e == entt::null || !g_registry->valid(e)) return nullptr;
    if (!g_registry->has<RigidBody2DComponent>(e)) return nullptr;
    return g_registry->get<RigidBody2DComponent>(e).body;
}

b2DistanceJoint* LiveRope(uint32_t owner)
{
    auto it = g_ropes.find(owner);
    if (it == g_ropes.end() || !it->second.joint)
        return nullptr;
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene || !scene->world || scene->world != it->second.world)
    {
        g_ropes.erase(it);
        return nullptr;
    }
    for (b2Joint* j = scene->world->GetJointList(); j; j = j->GetNext())
        if (j == it->second.joint)
            return it->second.joint;
    g_ropes.erase(it);
    return nullptr;
}

void DropRopeFor(uint32_t owner)
{
    if (b2DistanceJoint* rope = LiveRope(owner))
        g_ropes[owner].world->DestroyJoint(rope);
    g_ropes.erase(owner);
}

std::uintmax_t FileMtime(const std::string& path)
{
    std::error_code ec;
    auto t = fs::last_write_time(path, ec);
    if (ec)
        return 0;
    return static_cast<std::uintmax_t>(t.time_since_epoch().count());
}

bool LoadSourceFile(ScriptSource& src, const std::string& resolved)
{
    std::ifstream in(resolved);
    if (!in)
        return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    src.source = ss.str();
    src.mtime = FileMtime(resolved);
    src.dirty = true;
    return true;
}

void MarkPathDirty(const std::string& resolved)
{
    for (auto& [_, rt] : g_runtimes)
    {
        if (rt.path == resolved)
            rt.need_reexec = true;
    }
}

bool RefreshSource(const std::string& resolved, bool force)
{
    auto& src = g_sources[resolved];
    auto m = FileMtime(resolved);
    if (force || src.source.empty() || (m != 0 && m != src.mtime))
    {
        if (!LoadSourceFile(src, resolved))
            return false;
        MarkPathDirty(resolved);
    }
    return !src.source.empty();
}

entt::entity EntityFromId(uint32_t id)
{
    return static_cast<entt::entity>(id);
}

entt::entity FindByTag(const char* tag)
{
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene || !tag)
        return entt::null;
    return scene->GetEntity(tag).GetHandle();
}

int BusNameToId(const char* name)
{
    if (!name)
        return 1;
    if (std::strcmp(name, "master") == 0)
        return 0;
    if (std::strcmp(name, "music") == 0)
        return 2;
    if (std::strcmp(name, "ui") == 0)
        return 3;
    return 1; // sfx
}

void PlaySoundLocal(const char* path, float volume)
{
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene || !path || !path[0] || !g_registry)
        return;
    static std::unordered_map<std::string, std::string> paths;
    auto& stored = paths[path] = path;
    SfxPool::Play(*g_registry, *scene, stored.c_str(), volume, 1);
}

void PlaySoundLocalAt(const char* path, float volume, float x, float y)
{
    Scene* scene = SceneHandler::GetInstance().GetActiveScene();
    if (!scene || !path || !path[0] || !g_registry)
        return;
    static std::unordered_map<std::string, std::string> paths;
    auto& stored = paths[path] = path;
    SfxPool::PlayAt(*g_registry, *scene, stored.c_str(), x, y, volume, 1);
}

std::string g_ui_default_font;

void UiPaintDefaults(ButtonComponent& b)
{
    // Console left-rail: quiet idle bar, warm hover, no chunky outline.
    b.color.primary = {12, 14, 18, 0};
    b.hover_color.primary = {28, 32, 40, 210};
    b.selected_color.primary = {36, 40, 48, 240};
    b.text_color.primary = {210, 214, 220, 255};
    b.hover_text_color.primary = {255, 255, 255, 255};
    b.selected_text_color.primary = {236, 228, 255, 255};
    b.outline_color.primary = {0, 0, 0, 0};
    b.outline = false;
    b.outline_width = 0.f;
    b.text_align = 1;
    b.overlay = false;
    b.accent = true;
    b.font_location = g_ui_default_font.empty() ? nullptr : g_ui_default_font.c_str();
    b.font_size = 30;
    b.aa_font = true;
    b.active = true;
}

#ifdef FW_HAS_LUAU
entt::entity ResolveArg(lua_State* L, int idx)
{
    if (lua_isstring(L, idx) && !lua_isnumber(L, idx))
        return FindByTag(lua_tostring(L, idx));
    return EntityFromId((uint32_t)luaL_checkinteger(L, idx));
}

entt::entity ResolveEntityArg(lua_State* L, int idx)
{
    return ResolveArg(L, idx);
}

void PumpMatchEvents()
{
    auto inbound = MatchEvents::GetInstance().TakeInbound();
    for (const MatchEventWire& ev : inbound)
    {
        if (ev.kind == MatchEventKind::Sound)
        {
            if (ev.flags & MatchEventFlags::Spatial)
                PlaySoundLocalAt(ev.name, ev.volume, ev.x, ev.y);
            else
                PlaySoundLocal(ev.name, ev.volume);
        }
        g_script_events.push_back(ev);
    }
}


void ScriptPushEngineTable(lua_State* L)
{
    lua_newtable(L);
    RegisterInputBinds(L);
    RegisterPhysicsBinds(L);
    RegisterRenderBinds(L);
    RegisterAudioBinds(L);
    RegisterMatchBinds(L);
    RegisterUiBinds(L);
    RegisterMiscBinds(L);
    if (g_luaExtraBinds)
        g_luaExtraBinds(L);
    RegisterKeyConstants(L);
    lua_setglobal(L, "engine");
}
#endif

} // namespace ScriptInternal
