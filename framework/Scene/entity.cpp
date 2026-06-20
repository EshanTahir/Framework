#include "entity.h"

#include "entt.hpp"

Entity::Entity(entt::entity handle, Scene* og_scene)
:entity_handle(handle), scene(og_scene)
{}