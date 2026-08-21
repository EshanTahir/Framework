#include "appEvent.h"

AppEvent::AppEvent()
{
    app_events.resize(8);
}

AppEvent& AppEvent::GetInstance()
{
    static AppEvent instance;
    return instance;
}


void AppEvent::PollEvents()
{
    // Clear transient flags each frame (quit sticks until handled)
    for (size_t i = 0; i < app_events.size(); ++i)
    {
        if (i != app_quit)
            app_events[i] = false;
    }

    std::vector<SDL_Event> events = EventHandler::GetInstance().application_events;
    for (SDL_Event event: events)
    {
        switch (event.type)
        {
            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                    case SDL_WINDOWEVENT_CLOSE:
                        app_events[window_close] = true;
                        break;
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        app_events[window_resize] = true;
                        break;
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        app_events[window_lost_focus] = true;
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        app_events[window_focus] = true;
                        break;
                    case SDL_WINDOWEVENT_MINIMIZED:
                        app_events[window_minimize] = true;
                        break;
                    case SDL_WINDOWEVENT_MAXIMIZED:
                        app_events[window_maximize] = true;
                        break;
                    case SDL_WINDOWEVENT_MOVED:
                        app_events[window_move] = true;
                        break;
                    default:
                        break;
                }
                break;
            case SDL_QUIT:
                app_events[app_quit] = true;
                break;
        }
    }
}

bool AppEvent::AppQuit()
{
    return app_events[app_quit];
}

void AppEvent::RequestQuit()
{
    app_events[app_quit] = true;
}

bool AppEvent::WindowClose()
{
    return app_events[window_close];
}

bool AppEvent::WindowResize()
{
    return app_events[window_resize];
}

bool AppEvent::WindowLostFocus()
{
    return app_events[window_lost_focus];
}

bool AppEvent::WindowFocus()
{
    return app_events[window_focus];
}

bool AppEvent::WindowMinimized()
{
    return app_events[window_minimize];
}

bool AppEvent::WindowMaximized()
{
    return app_events[window_maximize];
}

bool AppEvent::WindowMoved()
{
    return app_events[window_move];
}