#pragma once

#include <SDL.h>

class Window
{
public:
    static Window& GetInstance();

    void OnUpdate();
    void CleanUp();

    void SetWindow(SDL_Window* p_window);
    SDL_Window* GetWindow();

private:
    Window() = default;
    ~Window() = default;

    SDL_Window* window = nullptr;
};
