#include "window.h"
#include "glContext.h"

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
    GlContext::GetInstance().Swap();
}

void Window::CleanUp()
{
    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
}
