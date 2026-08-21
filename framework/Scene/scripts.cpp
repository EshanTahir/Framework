#include "systems.h"
#include "Script/scriptEngine.h"
#include "Scene/components.h"

#include <vector>

namespace
{
    void TickOne(entt::registry& registry, entt::entity entity, float delta_time)
    {
        auto& script = registry.get<ScriptComponent>(entity);
        script.self = static_cast<uint32_t>(entity);

        if (script.language == ScriptLanguage::Luau)
        {
            ScriptEngine::GetInstance().Tick(registry, entity, script, delta_time);
            return;
        }

        if (script.entering_scene)
        {
            script.OnEnter();
            script.entering_scene = false;
        }
        else if (script.exiting_scene)
        {
            script.OnExit();
            script.exiting_scene = false;
        }
        else
        {
            script.Update(delta_time);
        }
    }

    // Director scripts (tag "level") run after pawns so same-tick signals are seen.
    bool IsLevelDirector(entt::registry& registry, entt::entity entity)
    {
        if (!registry.has<TagComponent>(entity))
            return false;
        return registry.get<TagComponent>(entity).tag == "level";
    }
}

void Scripts::Run(entt::registry& registry, float delta_time)
{
    auto view = registry.view<ScriptComponent>();
    std::vector<entt::entity> deferred;
    deferred.reserve(4);

    for (auto entity : view)
    {
        if (IsLevelDirector(registry, entity))
        {
            deferred.push_back(entity);
            continue;
        }
        TickOne(registry, entity, delta_time);
    }
    for (auto entity : deferred)
        TickOne(registry, entity, delta_time);
}

void Scripts::RunPostPhys(entt::registry& registry, float delta_time)
{
    auto view = registry.view<ScriptComponent>();
    std::vector<entt::entity> deferred;
    deferred.reserve(4);

    for (auto entity : view)
    {
        if (IsLevelDirector(registry, entity))
        {
            deferred.push_back(entity);
            continue;
        }
        auto& script = registry.get<ScriptComponent>(entity);
        if (script.language == ScriptLanguage::Luau)
            ScriptEngine::GetInstance().TickPostPhys(registry, entity, script, delta_time);
    }
    for (auto entity : deferred)
    {
        auto& script = registry.get<ScriptComponent>(entity);
        if (script.language == ScriptLanguage::Luau)
            ScriptEngine::GetInstance().TickPostPhys(registry, entity, script, delta_time);
    }
}

void Scripts::RunTagged(entt::registry& registry, const char* tag, float delta_time)
{
    if (!tag || !tag[0])
        return;
    auto view = registry.view<ScriptComponent, TagComponent>();
    for (auto entity : view)
    {
        if (registry.get<TagComponent>(entity).tag != tag)
            continue;
        TickOne(registry, entity, delta_time);
        return;
    }
}
