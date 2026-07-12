#pragma once

#include "Scene/components.h"
#include "entt.hpp"

#include <string>

struct lua_State;

class ScriptEngine
{
public:
    static ScriptEngine& GetInstance();

    void Init();
    void Shutdown();

    // Call once per frame before ticking scripts (1 / mtime hot-reload)
    void BeginFrame(float dt);

    void Tick(entt::registry& registry, entt::entity entity, ScriptComponent& script, float dt);
    // Calls optional on_post_phys(dt) after physics (Luau). No-op if missing.
    void TickPostPhys(entt::registry& registry, entt::entity entity, ScriptComponent& script, float dt);
    void DropEntity(uint32_t entity_id);

    static entt::registry* ActiveRegistry();
    static void SetActiveRegistry(entt::registry* reg);

    // Game registers product Luau APIs. Called while `engine` table is on stack.
    using LuaExtraBindsFn = void (*)(lua_State* L);
    static void SetLuaExtraBinds(LuaExtraBindsFn fn);

    bool hot_reload_enabled = true;

private:
    ScriptEngine() = default;
    bool m_ready = false;
    bool m_reload_all = false;
};
