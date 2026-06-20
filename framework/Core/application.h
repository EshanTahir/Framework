#pragma once

#include "log.h"
#include "Events/appEvent.h"
#include "Events/event.h"
#include "Events/inputEvent.h"
#include "scene/sceneHandler.h"
#include "window.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <functional>

class Application
{
public:
    Application(std::function<void()> AppStart, std::function<void(float)> AppUpdate, std::function<void()> AppEnd);
    virtual ~Application();
    void Run();
    float delta_time;

private:
    std::function<void()> AppStart;
    std::function<void(float)> AppUpdate;
    std::function<void()> AppEnd;
    EventHandler& event_handler = EventHandler::GetInstance();
    AppEvent& app_event = AppEvent::GetInstance();
    InputEvent& input_event = InputEvent::GetInstance();
    bool m_running = true;
    float get_ticks_last_frame = 0;
    float ticks;
    SceneHandler& scene_handler = SceneHandler::GetInstance();
    Window& window = Window::GetInstance();
};
