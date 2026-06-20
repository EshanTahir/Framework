#include "sceneHandler.h"

SceneHandler::~SceneHandler()
{
    for (auto& map_entry : scenes)
    {
        delete map_entry.second;
    }
}

SceneHandler& SceneHandler::GetInstance()
{
    static SceneHandler instance;
    return instance;
}

void SceneHandler::AddScene(std::string scene_name)
{
    Scene* scene = new Scene();
    scenes[scene_name] = scene;

    if (active_scene == NULL)
    {
        active_scene = scene;
    }
}

void SceneHandler::SetActivateHook(std::string scene_name, std::function<void(Scene&)> hook)
{
    activate_hooks[std::move(scene_name)] = std::move(hook);
}

Scene* SceneHandler::GetScene(std::string scene_name)
{
    auto it = scenes.find(scene_name);
    if (it == scenes.end())
        return nullptr;
    return it->second;
}

Scene* SceneHandler::GetActiveScene()
{
    return active_scene;
}

void SceneHandler::RequestActiveScene(std::string scene_name)
{
    if (scenes.find(scene_name) == scenes.end())
    {
        FW_CORE_ERROR("RequestActiveScene: unknown scene '{0}'", scene_name);
        return;
    }
    pending_scene = std::move(scene_name);
    has_pending = true;
}

void SceneHandler::FlushPendingScene()
{
    if (!has_pending)
        return;
    has_pending = false;
    const std::string name = pending_scene;
    pending_scene.clear();
    SetActiveScene(name);
}

void SceneHandler::SetActiveScene(std::string scene_name)
{
    auto it = scenes.find(scene_name);
    if (it == scenes.end() || !it->second)
    {
        FW_CORE_ERROR("SetActiveScene: unknown scene '{0}'", scene_name);
        return;
    }

    if (active_scene)
        active_scene->UnInitialize();

    active_scene = it->second;
    if (auto hook = activate_hooks.find(scene_name); hook != activate_hooks.end() && hook->second)
        hook->second(*active_scene);
    active_scene->Initialize();
    FW_CORE_INFO("Scene is now {0}", scene_name);
}
