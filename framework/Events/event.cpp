#include "event.h"

EventHandler::EventHandler()
{
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
    {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        // Handle error
    }
}

EventHandler& EventHandler::GetInstance()
{
    static EventHandler instance;
    return instance;
}

void EventHandler::pollEvents()
{
    application_events.clear();
    input_events.clear();

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_WINDOWEVENT:
                switch (event.window.event)
                {
                    case SDL_WINDOWEVENT_CLOSE:
                        application_events.push_back(event);
                        break;
                    case SDL_WINDOWEVENT_RESIZED:
                        application_events.push_back(event);
                        break;
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        application_events.push_back(event);
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        application_events.push_back(event);
                        break;
                    case SDL_WINDOWEVENT_MINIMIZED:
                        application_events.push_back(event);
                        break;
                    case SDL_WINDOWEVENT_MAXIMIZED:
                        application_events.push_back(event);
                        break;
                    case SDL_WINDOWEVENT_MOVED:
                        application_events.push_back(event);
                        break;
                }
                break;
            case SDL_QUIT:
                application_events.push_back(event);
                break;


            case SDL_KEYDOWN:
                input_events.push_back(event);
                break;
            case SDL_KEYUP:
                input_events.push_back(event);
                break;
            case SDL_MOUSEBUTTONDOWN:
                input_events.push_back(event);
                break;
            case SDL_MOUSEBUTTONUP:
                input_events.push_back(event);
                break;
            case SDL_CONTROLLERDEVICEADDED:
                input_events.push_back(event);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                input_events.push_back(event);
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                input_events.push_back(event);
                break;
            case SDL_CONTROLLERBUTTONUP:
                input_events.push_back(event);
                break;
            default:
                break;
        }
    }
}