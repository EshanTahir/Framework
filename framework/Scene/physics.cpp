#include "systems.h"
void Physics::Initialize(entt::registry& registry, b2World& world) { (void)registry; (void)world; }
void Physics::UnInitialize(entt::registry& registry, b2World& world) { (void)registry; (void)world; }
void Physics::Run(entt::registry& registry, b2World& world, float delta_time) { (void)registry; (void)world; (void)delta_time; }
void Physics::BuildTileCollision(entt::registry& registry, b2World& world) { (void)registry; (void)world; }
void Physics::InitializeBody(entt::registry& registry, b2World& world, entt::entity entity) { (void)registry; (void)world; (void)entity; }
