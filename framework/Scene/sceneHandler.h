#pragma once

#include "Core/log.h"
#include "scene.h"

#include <functional>
#include <map>
#include <string>

class SceneHandler
{
public:
    static SceneHandler& GetInstance();

    void AddScene(std::string scene_name);
    // Runs on every activation of that scene, before Initialize: rebuild
    // per-match entities (players) while nothing is iterating the registry.
    void SetActivateHook(std::string scene_name, std::function<void(Scene&)> hook);
    // Null if missing - never inserts via operator[].
    Scene* GetScene(std::string scene_name);

    Scene* GetActiveScene();
    // Immediate switch (startup / after Flush). Prefer RequestActiveScene from scripts.
    void SetActiveScene(std::string scene_name);
    // Queue a switch; applied by FlushPendingScene between frames.
    void RequestActiveScene(std::string scene_name);
    // Apply any queued RequestActiveScene. Safe outside Scene::Update.
    void FlushPendingScene();

private:
    SceneHandler() = default;
    ~SceneHandler();

    Scene* active_scene = NULL;
    std::string pending_scene;
    bool has_pending = false;

    std::map<std::string, Scene*> scenes;
    std::map<std::string, std::function<void(Scene&)>> activate_hooks;
};
