#include "entity.h"
#include "scene.h"
#include "Script/scriptEngine.h"
#include "Match/simClock.h"
#include "Match/matchSession.h"
#include "Match/matchConfig.h"
#include "Render/gl.h"
#include "Core/log.h"

#include <algorithm>
#include <vector>

Scene::Scene()
{
    b2Vec2 gravity(0.0f, 9.81f);
    world = new b2World(gravity);
}

Scene::~Scene()
{
    delete world;
}

Entity Scene::CreateEntity(const std::string& tag)
{
    Entity entity(registry.create(), this);
    entity.AddComponent<TagComponent>(tag);
    tag_entity[tag] = entity.GetHandle();
    return entity;
}

Entity Scene::GetEntity(const std::string& tag)
{
    if (tag_entity.find(tag) != tag_entity.end())
        return Entity(tag_entity[tag], this);
    return Entity(entt::null, this);
}

std::vector<std::string> Scene::ListTags() const
{
    std::vector<std::string> out;
    out.reserve(tag_entity.size());
    for (const auto& kv : tag_entity)
    {
        if (registry.valid(kv.second))
            out.push_back(kv.first);
    }
    return out;
}

void Scene::DestroyEntity(entt::entity handle)
{
    if (handle == entt::null || !registry.valid(handle))
        return;

    if (registry.has<ScriptComponent>(handle))
        ScriptEngine::GetInstance().DropEntity((uint32_t)handle);

    if (registry.has<RigidBody2DComponent>(handle))
    {
        auto& rb = registry.get<RigidBody2DComponent>(handle);
        if (rb.body && world)
        {
            world->DestroyBody(rb.body);
            rb.body = nullptr;
            rb.initialized = false;
        }
    }
    if (registry.has<TagComponent>(handle))
    {
        const std::string& tag = registry.get<TagComponent>(handle).tag;
        auto it = tag_entity.find(tag);
        if (it != tag_entity.end() && it->second == handle)
            tag_entity.erase(it);
    }
    registry.destroy(handle);
}

void Scene::DestroyEntity(const std::string& tag)
{
    auto it = tag_entity.find(tag);
    if (it == tag_entity.end())
        return;
    DestroyEntity(it->second);
}

void Scene::ClearAllEntities()
{
    std::vector<entt::entity> handles;
    for (auto& kv : tag_entity)
    {
        if (registry.valid(kv.second))
            handles.push_back(kv.second);
    }
    registry.each([&](entt::entity e) {
        if (registry.valid(e))
            handles.push_back(e);
    });
    std::sort(handles.begin(), handles.end());
    handles.erase(std::unique(handles.begin(), handles.end()), handles.end());
    for (entt::entity e : handles)
        DestroyEntity(e);
    tag_entity.clear();
}

void Scene::FixedUpdate(float sim_dt, bool resample_input)
{
    (void)resample_input;
    auto view = registry.view<TransformComponent>();
    for (auto e : view)
    {
        auto& t = registry.get<TransformComponent>(e);
        t.ix = t.x; t.iy = t.y; t.ia = t.a; t.interp_primed = true;
    }
    auto& session = MatchSession::GetInstance();
    auto& clock = SimClock::GetInstance();
    session.BeginSimTick(clock.Tick(), resample_input);
    session.RecordSimInputs();
    scripts.Run(registry, sim_dt);
    physics.Run(registry, *world, sim_dt);
    scripts.RunPostPhys(registry, sim_dt);
    clock.AdvanceTick();
}

void Scene::FrameUpdate(float real_dt)
{
    particles.Run(registry, real_dt);
    renderer.Run(registry, real_dt);
    ui.DrawOverlays(registry);
    renderer.DrawScreenHud(registry);
    ui.DrawChrome(registry);
    sound.Run(registry, real_dt);
    debug_hud.Run(registry, real_dt);
}

void Scene::Update(float real_dt)
{
    if (renderer.window == nullptr)
        renderer.window = Window::GetInstance().GetWindow();
    ui.ProcessInput(registry);
    ScriptEngine::GetInstance().BeginFrame(real_dt);
    auto& clock = SimClock::GetInstance();
    const int steps = clock.BeginFrame(real_dt);
    const float sim_dt = clock.SimDt();
    if (clock.Paused())
        scripts.RunTagged(registry, "pause_ui", real_dt);
    else
    {
        for (int i = 0; i < steps; ++i)
            FixedUpdate(sim_dt, i == 0);
    }
    FrameUpdate(real_dt);
}

void Scene::Initialize()
{
    auto view = registry.view<ScriptComponent>();
    for (auto e : view)
    {
        auto& sc = registry.get<ScriptComponent>(e);
        sc.entering_scene = true;
        sc.booted = false;
        ScriptEngine::GetInstance().DropEntity((uint32_t)e);
    }
    physics.Initialize(registry, *world);
}

void Scene::UnInitialize()
{
    physics.UnInitialize(registry, *world);
}
