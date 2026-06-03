#pragma once

#include "event.h"

#include "SDL.h"

#include <iostream>
#include <vector>

class EventHandler;

class AppEvent
{
public:
    static AppEvent& GetInstance();

    void PollEvents();

    bool AppQuit();
    void RequestQuit();
    bool WindowClose();
    bool WindowResize();
    bool WindowLostFocus();
    bool WindowFocus();
    bool WindowMinimized();
    bool WindowMaximized();
    bool WindowMoved();

private:
    friend class EventHandler;

    AppEvent();
    ~AppEvent() = default;

    enum appEvents
    {
        app_quit,
        window_close,
        window_resize,
        window_lost_focus,
        window_focus,
        window_minimize,
        window_maximize,
        window_move,
    };

    std::vector<bool> app_events;
};