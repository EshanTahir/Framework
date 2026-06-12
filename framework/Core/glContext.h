#pragma once

#include "SDL.h"

class GlContext
{
public:
    static GlContext& GetInstance();

    bool Init(SDL_Window* window);
    void MakeCurrent();
    void Swap();
    void SetVSync(bool enabled);
    void Resize(int w, int h);

    int Width() const { return m_width; }
    int Height() const { return m_height; }
    SDL_GLContext Native() const { return m_context; }
    bool Ready() const { return m_context != nullptr; }

private:
    GlContext() = default;

    SDL_Window* m_window = nullptr;
    SDL_GLContext m_context = nullptr;
    int m_width = 1280;
    int m_height = 720;
};
