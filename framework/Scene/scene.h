#pragma once

#include "components.h"
#include "systems.h"
#include "Core/window.h"
#include "Core/log.h"

#include "entt.hpp"

#include <string>
#include <unordered_map>
#include <vector>

class Entity;
class Scene
{
public:
    Scene();
    ~Scene();

    Entity CreateEntity(const std::string& tag);
    Entity GetEntity(const std::string& tag);
    void DestroyEntity(const std::string& tag);
    void DestroyEntity(entt::entity handle);

    // All live entity tags (for depth layer parking, validators, etc.).
    std::vector<std::string> ListTags() const;

    // Destroy every entity (scripts, bodies, textures). Used to reload a scene fresh.
    void ClearAllEntities();

    // real_dt = display frame time; runs 0..N fixed sim steps then one render pass.
    void Update(float real_dt);

    // Contact begin/end from the last physics step (valid until next Physics::Run).
    ContactBuffer& Contacts() { return physics.contacts; }
    const ContactBuffer& Contacts() const { return physics.contacts; }

    Renderer renderer = Renderer();

    b2World* world;
private:
    friend class Entity;
    friend class SceneHandler;

    entt::registry registry;

    std::unordered_map<std::string, entt::entity> tag_entity;

    void Initialize();
    void UnInitialize();
    void FixedUpdate(float sim_dt, bool resample_input);
    void FrameUpdate(float real_dt);

    Scripts scripts = Scripts();
    Physics physics = Physics();
    Particles particles = Particles();
    Sound sound = Sound();
    Ui ui = Ui();
    DebugHud debug_hud = DebugHud();
};