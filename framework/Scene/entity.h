#pragma once
#include "scene.h"

#include "entt.hpp"

class Entity
{
public:
    Entity(entt::entity handle, Scene* scene);

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
        return scene->registry.emplace<T>(entity_handle, std::forward<Args>(args)...);
    }

    template<typename T>
    T& GetComponent()
    {
        return scene->registry.get<T>(entity_handle);
    }

    template<typename T>
    bool HasComponent()
    {
        return scene->registry.has<T>(entity_handle);
    }

    template<typename T>
    void RemoveComponent()
    {
        return scene->registry.remove<T>(entity_handle);
    }

    entt::entity GetHandle()
    {
        return entity_handle;
    }

private:
    entt::entity entity_handle = entt::null;
    Scene* scene = NULL;
};
