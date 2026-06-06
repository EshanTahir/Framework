#include "application.h"

Application::Application(std::function<void()> AppStart, std::function<void(float)> AppUpdate, std::function<void()> AppEnd)
: AppStart(AppStart), AppUpdate(AppUpdate), AppEnd(AppEnd)
{
    AppStart();
}

Application::~Application()
{
    AppEnd();
    window.CleanUp();
    SDL_Quit();
    TTF_Quit();
}

void Application::Run()
{
    while (m_running)
    {
        ticks = SDL_GetTicks();
        delta_time = (ticks - get_ticks_last_frame) / 1000.0;
        get_ticks_last_frame = ticks;

        event_handler.pollEvents();
        app_event.PollEvents();
        input_event.PollEvents();

        if (app_event.AppQuit())
            m_running = false;

        AppUpdate(delta_time);
        window.OnUpdate();
    }
}
