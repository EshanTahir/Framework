#include "renderer2d.h"
#include "Core/log.h"

#include <cmath>
#include <cstring>

namespace
{
    const char* kVS = R"(#version 330 core
layout(location=0) in vec2 a_pos;
layout(location=1) in vec2 a_uv;
layout(location=2) in vec4 a_col;
uniform mat4 u_proj;
out vec2 v_uv;
out vec4 v_col;
void main(){
  v_uv=a_uv; v_col=a_col;
  gl_Position = u_proj * vec4(a_pos,0.0,1.0);
})";

    const char* kFS = R"(#version 330 core
in vec2 v_uv; in vec4 v_col;
uniform sampler2D u_tex;
uniform sampler2D u_pal;
uniform float u_use_pal;
uniform vec2 u_pal_size;
out vec4 frag;
void main(){
  vec4 src = texture(u_tex, v_uv);
  if (u_use_pal < 0.5 || src.a < 0.004) {
    frag = src * v_col;
    return;
  }
  vec2 xy = floor(src.rg * 255.0 + 0.5);
  vec2 pal_uv = (xy + 0.5) / max(u_pal_size, vec2(1.0));
  vec4 pal = texture(u_pal, pal_uv);
  frag = vec4(pal.rgb, pal.a * src.a) * v_col;
})";

    const char* kLineVS = R"(#version 330 core
layout(location=0) in vec2 a_pos;
layout(location=1) in vec4 a_col;
uniform mat4 u_proj;
out vec4 v_col;
void main(){ v_col=a_col; gl_Position=u_proj*vec4(a_pos,0.0,1.0); })";

    const char* kLineFS = R"(#version 330 core
in vec4 v_col; out vec4 frag; void main(){ frag=v_col; })";

    void Ortho(float* out, float l, float r, float b, float t)
    {
        std::memset(out, 0, sizeof(float) * 16);
        out[0] = 2.f / (r - l);
        out[5] = 2.f / (t - b);
        out[10] = -1.f;
        out[12] = -(r + l) / (r - l);
        out[13] = -(t + b) / (t - b);
        out[15] = 1.f;
    }
}

Renderer2D& Renderer2D::GetInstance()
{
    static Renderer2D i;
    return i;
}

GLuint Renderer2D::Compile(const char* vs_src, const char* fs_src)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, nullptr);
    glCompileShader(vs);
    GLint ok = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(vs, 512, nullptr, log);
        FW_CORE_ERROR("VS compile: {0}", log);
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(fs, 512, nullptr, log);
        FW_CORE_ERROR("FS compile: {0}", log);
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        FW_CORE_ERROR("Link: {0}", log);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

bool Renderer2D::Init()
{
    m_shader = Compile(kVS, kFS);
    m_u_proj = glGetUniformLocation(m_shader, "u_proj");
    m_u_tex = glGetUniformLocation(m_shader, "u_tex");
    m_u_pal = glGetUniformLocation(m_shader, "u_pal");
    m_u_use_pal = glGetUniformLocation(m_shader, "u_use_pal");
    m_u_pal_size = glGetUniformLocation(m_shader, "u_pal_size");

    m_line_shader = Compile(kLineVS, kLineFS);
    m_line_u_proj = glGetUniformLocation(m_line_shader, "u_proj");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, kMaxQuads * kVertsPerQuad * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(4 * sizeof(float)));
    glBindVertexArray(0);

    glGenVertexArrays(1, &m_line_vao);
    glGenBuffers(1, &m_line_vbo);
    glBindVertexArray(m_line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_line_vbo);
    glBufferData(GL_ARRAY_BUFFER, 2 * sizeof(Vertex), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(4 * sizeof(float)));
    glBindVertexArray(0);

    m_batch.reserve(kMaxQuads * kVertsPerQuad);

    unsigned char px[4] = {255, 255, 255, 255};
    glGenTextures(1, &m_white_tex);
    glBindTexture(GL_TEXTURE_2D, m_white_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindTexture(GL_TEXTURE_2D, 0);

    FW_CORE_INFO("Renderer2D initialized");
    return true;
}

unsigned int Renderer2D::WhiteTexture()
{
    return m_white_tex;
}

void Renderer2D::DrawSolidQuad(float x, float y, float w, float h,
                               float angle_rad, float r, float g, float b, float a,
                               bool world_space)
{
    DrawQuad(m_white_tex, x, y, w, h, 0, 0, 1, 1, angle_rad, r, g, b, a, world_space);
}

void Renderer2D::DrawPoly4(float ax, float ay, float bx, float by,
                           float cx, float cy, float dx, float dy,
                           float r, float g, float b, float a)
{
    if (m_white_tex == 0)
        return;
    if (m_current_tex != m_white_tex || m_current_pal != 0
        || (int)m_batch.size() >= kMaxQuads * kVertsPerQuad)
    {
        FlushInternal();
        m_current_tex = m_white_tex;
        m_current_pal = 0;
        m_current_pal_w = 1;
        m_current_pal_h = 1;
    }
    // two tris: A-B-C and A-C-D
    m_batch.push_back({ax, ay, 0.f, 0.f, r, g, b, a});
    m_batch.push_back({bx, by, 1.f, 0.f, r, g, b, a});
    m_batch.push_back({cx, cy, 1.f, 1.f, r, g, b, a});
    m_batch.push_back({ax, ay, 0.f, 0.f, r, g, b, a});
    m_batch.push_back({cx, cy, 1.f, 1.f, r, g, b, a});
    m_batch.push_back({dx, dy, 0.f, 1.f, r, g, b, a});
}

void Renderer2D::Shutdown()
{
    Flush();
    if (m_white_tex) glDeleteTextures(1, &m_white_tex);
    m_white_tex = 0;
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_line_vbo) glDeleteBuffers(1, &m_line_vbo);
    if (m_line_vao) glDeleteVertexArrays(1, &m_line_vao);
    if (m_shader) glDeleteProgram(m_shader);
    if (m_line_shader) glDeleteProgram(m_line_shader);
    m_vbo = m_vao = m_shader = 0;
}

void Renderer2D::SetCamera(const Camera2D& cam) { m_camera = cam; }
void Renderer2D::SetScreenSize(int w, int h) { m_width = w; m_height = h; }
void Renderer2D::ResetStats() { m_draw_calls = 0; m_quads_drawn = 0; }

void Renderer2D::BeginFrame(float r, float g, float b, float a)
{
    ResetStats();
    glViewport(0, 0, m_width, m_height);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    m_batch.clear();
    m_current_tex = 0;
    m_current_pal = 0;
}

void Renderer2D::BeginBatch()
{
    ResetStats();
    m_batch.clear();
    m_current_tex = 0;
    m_current_pal = 0;
}

void Renderer2D::EndFrame()
{
    Flush();
}

void Renderer2D::SetOrtho(GLint loc, bool world)
{
    float mat[16];
    if (!world)
    {
        Ortho(mat, 0.f, (float)m_width, (float)m_height, 0.f);
    }
    else
    {
        float zoom = m_camera.zoom <= 0.01f ? 0.01f : m_camera.zoom;
        float half_w = (m_width / kWorldPixels) * 0.5f / zoom;
        float half_h = (m_height / kWorldPixels) * 0.5f / zoom;
        float cx = SnapWorldPixel(m_camera.x + m_camera.shake_x, zoom);
        float cy = SnapWorldPixel(m_camera.y + m_camera.shake_y, zoom);
        Ortho(mat, cx - half_w, cx + half_w, cy + half_h, cy - half_h);

        // translate-only camera; sprite angle is separate
        (void)m_camera.angle;
    }
    glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
}

void Renderer2D::FlushInternal()
{
    if (m_batch.empty() || m_current_tex == 0)
    {
        m_batch.clear();
        return;
    }

    glUseProgram(m_shader);
    SetOrtho(m_u_proj, true);
    glUniform1i(m_u_tex, 0);
    glUniform1i(m_u_pal, 1);
    const bool use_pal = m_current_pal != 0;
    glUniform1f(m_u_use_pal, use_pal ? 1.f : 0.f);
    float pw = (float)(m_current_pal_w > 0 ? m_current_pal_w : 16);
    float ph = (float)(m_current_pal_h > 0 ? m_current_pal_h : 16);
    glUniform2f(m_u_pal_size, pw, ph);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, use_pal ? m_current_pal : m_white_tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_current_tex);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, m_batch.size() * sizeof(Vertex), m_batch.data());
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_batch.size());
    glBindVertexArray(0);

    m_draw_calls++;
    m_quads_drawn += (int)m_batch.size() / 6;
    m_batch.clear();
}

void Renderer2D::Flush()
{
    FlushInternal();
}

void Renderer2D::DrawQuad(unsigned int texture,
                          float x, float y, float w, float h,
                          float u0, float v0, float u1, float v1,
                          float angle_rad, float r, float g, float b, float a,
                          bool world_space, bool flip_h, bool flip_v,
                          unsigned int palette, int pal_w, int pal_h)
{
    if (texture == 0)
        return;

    // Screen-space path: convert to world batch by using a separate flush with screen ortho -
    // for simplicity, screen-space draws flush immediately with screen matrix.
    if (!world_space)
    {
        FlushInternal();
        if (flip_h) std::swap(u0, u1);
        if (flip_v) std::swap(v0, v1);
        float hw = w * 0.5f, hh = h * 0.5f;
        float c = std::cos(angle_rad), s = std::sin(angle_rad);
        auto rot = [&](float px, float py, float& ox, float& oy) {
            ox = x + px * c - py * s;
            oy = y + px * s + py * c;
        };
        float x0, y0, x1, y1, x2, y2, x3, y3;
        rot(-hw, -hh, x0, y0);
        rot( hw, -hh, x1, y1);
        rot( hw,  hh, x2, y2);
        rot(-hw,  hh, x3, y3);
        Vertex verts[6] = {
            {x0,y0,u0,v0,r,g,b,a},{x1,y1,u1,v0,r,g,b,a},{x2,y2,u1,v1,r,g,b,a},
            {x0,y0,u0,v0,r,g,b,a},{x2,y2,u1,v1,r,g,b,a},{x3,y3,u0,v1,r,g,b,a},
        };
        glUseProgram(m_shader);
        SetOrtho(m_u_proj, false);
        glUniform1i(m_u_tex, 0);
        glUniform1i(m_u_pal, 1);
        glUniform1f(m_u_use_pal, 0.f);
        glUniform2f(m_u_pal_size, 1.f, 1.f);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_white_tex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        m_draw_calls++;
        m_quads_drawn++;
        return;
    }

    if (texture != m_current_tex || palette != m_current_pal
        || (int)m_batch.size() >= kMaxQuads * kVertsPerQuad)
    {
        FlushInternal();
        m_current_tex = texture;
        m_current_pal = palette;
        m_current_pal_w = pal_w > 0 ? pal_w : 16;
        m_current_pal_h = pal_h > 0 ? pal_h : 16;
    }

    if (flip_h) std::swap(u0, u1);
    if (flip_v) std::swap(v0, v1);

    float hw = w * 0.5f, hh = h * 0.5f;
    // Subtract camera angle so world rotates with camera
    float ang = angle_rad - m_camera.angle;
    float c = std::cos(ang), s = std::sin(ang);
    auto rot = [&](float px, float py, float& ox, float& oy) {
        ox = x + px * c - py * s;
        oy = y + px * s + py * c;
    };
    float x0, y0, x1, y1, x2, y2, x3, y3;
    rot(-hw, -hh, x0, y0);
    rot( hw, -hh, x1, y1);
    rot( hw,  hh, x2, y2);
    rot(-hw,  hh, x3, y3);

    m_batch.push_back({x0,y0,u0,v0,r,g,b,a});
    m_batch.push_back({x1,y1,u1,v0,r,g,b,a});
    m_batch.push_back({x2,y2,u1,v1,r,g,b,a});
    m_batch.push_back({x0,y0,u0,v0,r,g,b,a});
    m_batch.push_back({x2,y2,u1,v1,r,g,b,a});
    m_batch.push_back({x3,y3,u0,v1,r,g,b,a});
}

void Renderer2D::DrawLine(float x0, float y0, float x1, float y1,
                          float r, float g, float b, float a, bool world_space)
{
    FlushInternal();
    Vertex verts[2] = {
        {x0, y0, 0, 0, r, g, b, a},
        {x1, y1, 0, 0, r, g, b, a},
    };
    glUseProgram(m_line_shader);
    SetOrtho(m_line_u_proj, world_space);
    glBindVertexArray(m_line_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_line_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
    m_draw_calls++;
}
