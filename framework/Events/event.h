#pragma once

#include "SDL.h"

#include <iostream>
#include <vector>

class EventHandler
{
public:
    static EventHandler& GetInstance();

    void pollEvents();
private:
    friend class AppEvent;
    friend class InputEvent;

    EventHandler();
    ~EventHandler() = default;

    std::vector<SDL_Event> application_events;
    std::vector<SDL_Event> input_events;
};