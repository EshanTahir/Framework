#include "glContext.h"
#include "Render/gl.h"
#include "log.h"

GlContext& GlContext::GetInstance()
{
    static GlContext instance;
    return instance;
}

bool GlContext::Init(SDL_Window* window)
{
    m_window = window;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    m_context = SDL_GL_CreateContext(window);
    if (!m_context)
    {
        FW_CORE_CRITICAL("SDL_GL_CreateContext failed: {0}", SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(window, m_context);
    if (!FW_LoadGL())
    {
        FW_CORE_CRITICAL("Failed to load OpenGL functions");
        return false;
    }

    SDL_GetWindowSize(window, &m_width, &m_height);
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    SetVSync(true);
    FW_CORE_INFO("OpenGL 3.3 context ready ({0}x{1})", m_width, m_height);
    return true;
}

void GlContext::MakeCurrent()
{
    if (m_window && m_context)
        SDL_GL_MakeCurrent(m_window, m_context);
}

void GlContext::Swap()
{
    if (m_window)
        SDL_GL_SwapWindow(m_window);
}

void GlContext::SetVSync(bool enabled)
{
    SDL_GL_SetSwapInterval(enabled ? 1 : 0);
}

void GlContext::Resize(int w, int h)
{
    m_width = w;
    m_height = h;
    glViewport(0, 0, w, h);
}
