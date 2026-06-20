#include "systems.h"
#include "Scene/components.h"
#include <vector>

namespace
{
    void TickOne(entt::registry& registry, entt::entity entity, float delta_time)
    {
        auto& script = registry.get<ScriptComponent>(entity);
        script.self = static_cast<uint32_t>(entity);
        if (script.entering_scene) { script.OnEnter(); script.entering_scene = false; }
        else if (script.exiting_scene) { script.OnExit(); script.exiting_scene = false; }
        else { script.Update(delta_time); }
    }
}

void Scripts::Run(entt::registry& registry, float delta_time)
{
    for (auto entity : registry.view<ScriptComponent>())
        TickOne(registry, entity, delta_time);
}

void Scripts::RunPostPhys(entt::registry& registry, float delta_time)
{
    (void)registry; (void)delta_time;
}

void Scripts::RunTagged(entt::registry& registry, const char* tag, float delta_time)
{
    if (!tag || !tag[0]) return;
    auto view = registry.view<ScriptComponent, TagComponent>();
    for (auto entity : view)
    {
        if (registry.get<TagComponent>(entity).tag != tag) continue;
        TickOne(registry, entity, delta_time);
        return;
    }
}
