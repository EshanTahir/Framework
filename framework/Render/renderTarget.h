#pragma once

#include "Render/gl.h"

// Color-only FBO used by the presentation pipeline (scene / bloom / aperture).
class RenderTarget
{
public:
    bool Create(int w, int h, bool linear_filter = false);
    void Destroy();
    void Resize(int w, int h, bool linear_filter = false);

    void Bind() const;
    static void BindDefault(int screen_w, int screen_h);

    GLuint Fbo() const { return m_fbo; }
    GLuint Color() const { return m_color; }
    int Width() const { return m_w; }
    int Height() const { return m_h; }
    bool Valid() const { return m_fbo != 0 && m_color != 0; }

private:
    GLuint m_fbo = 0;
    GLuint m_color = 0;
    int m_w = 0;
    int m_h = 0;
    bool m_linear = false;
};
