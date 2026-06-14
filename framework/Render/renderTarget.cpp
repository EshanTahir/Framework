#include "renderTarget.h"
#include "Core/log.h"

bool RenderTarget::Create(int w, int h, bool linear_filter)
{
    if (w <= 0 || h <= 0)
        return false;
    Destroy();
    m_w = w;
    m_h = h;
    m_linear = linear_filter;

    glGenTextures(1, &m_color);
    glBindTexture(GL_TEXTURE_2D, m_color);
    GLint filter = linear_filter ? (GLint)GL_LINEAR : (GLint)GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        FW_CORE_ERROR("RenderTarget incomplete ({0}x{1}) status={2}", w, h, (int)status);
        Destroy();
        return false;
    }
    return true;
}

void RenderTarget::Destroy()
{
    if (m_fbo)
    {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_color)
    {
        glDeleteTextures(1, &m_color);
        m_color = 0;
    }
    m_w = m_h = 0;
}

void RenderTarget::Resize(int w, int h, bool linear_filter)
{
    if (w == m_w && h == m_h && Valid() && linear_filter == m_linear)
        return;
    Create(w, h, linear_filter);
}

void RenderTarget::Bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_w, m_h);
}

void RenderTarget::BindDefault(int screen_w, int screen_h)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screen_w, screen_h);
}
