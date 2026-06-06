#include "window.h"

Window& Window::GetInstance()
{
    static Window instance;
    return instance;
}

void Window::SetWindow(SDL_Window* p_window)
{
    window = p_window;
}

SDL_Window* Window::GetWindow()
{
    return window;
}

void Window::OnUpdate()
{
    if (window)
        SDL_GL_SwapWindow(window);
}

void Window::CleanUp()
{
    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
}
