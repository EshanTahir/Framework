#pragma once

#include "Render/gl.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

struct Camera2D
{
    float x = 0, y = 0, angle = 0, zoom = 1;
    float shake_x = 0, shake_y = 0;
};

// At zoom 1, 1 world unit = this many *render* pixels. 16px art @ ppu 2 = 1 tile.
// Screen px / texel = zoom * kWorldPixels / 16. Zoom 1.5 → 3 px/texel (even).
inline constexpr float kWorldPixels = 32.f;

inline float SnapWorldPixel(float v, float zoom = 1.f)
{
    const float z = zoom <= 0.01f ? 0.01f : zoom;
    const float px = kWorldPixels * z;
    return std::round(v * px) / px;
}

class Renderer2D
{
public:
    static Renderer2D& GetInstance();

    bool Init();
    void Shutdown();

    void BeginFrame(float r, float g, float b, float a = 1.f);
    // Reset batch/stats without clearing - use when already bound to an RT.
    void BeginBatch();
    void EndFrame();

    void SetCamera(const Camera2D& cam);
    void SetScreenSize(int w, int h);

    // Screen-space or world-space textured quad. If world_space, applies camera.
    void DrawQuad(unsigned int texture,
                  float x, float y, float w, float h,
                  float u0, float v0, float u1, float v1,
                  float angle_rad, float r, float g, float b, float a,
                  bool world_space = true, bool flip_h = false, bool flip_v = false,
                  unsigned int palette = 0, int pal_w = 16, int pal_h = 16);

    void DrawLine(float x0, float y0, float x1, float y1,
                  float r, float g, float b, float a, bool world_space = true);

    void Flush();

    // 1x1 white texture for debug/solid-color quads
    unsigned int WhiteTexture();
    void DrawSolidQuad(float x, float y, float w, float h,
                       float angle_rad, float r, float g, float b, float a,
                       bool world_space = true);

    int DrawCalls() const { return m_draw_calls; }
    int QuadCount() const { return m_quads_drawn; }
    int ScreenWidth() const { return m_width; }
    int ScreenHeight() const { return m_height; }
    void ResetStats();

private:
    Renderer2D() = default;
    GLuint m_white_tex = 0;

    struct Vertex
    {
        float x, y, u, v, r, g, b, a;
    };

    static constexpr int kMaxQuads = 10000;
    static constexpr int kVertsPerQuad = 6;

    GLuint m_vao = 0, m_vbo = 0;
    GLuint m_shader = 0;
    GLuint m_line_vao = 0, m_line_vbo = 0;
    GLuint m_line_shader = 0;

    GLint m_u_proj = -1;
    GLint m_u_tex = -1;
    GLint m_u_pal = -1;
    GLint m_u_use_pal = -1;
    GLint m_u_pal_size = -1;
    GLint m_line_u_proj = -1;

    std::vector<Vertex> m_batch;
    unsigned int m_current_tex = 0;
    unsigned int m_current_pal = 0;
    int m_current_pal_w = 16;
    int m_current_pal_h = 16;
    Camera2D m_camera;
    int m_width = 1280, m_height = 720;
    int m_draw_calls = 0, m_quads_drawn = 0;

    GLuint Compile(const char* vs, const char* fs);
    void SetOrtho(GLint loc, bool world);
    void FlushInternal();
};
