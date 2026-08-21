#include "presentPipeline.h"
#include "Core/log.h"

#include <algorithm>
#include <cstring>

namespace
{
    const char* kFsVS = R"(#version 330 core
out vec2 v_uv;
void main(){
  // Fullscreen triangle
  float x = float((gl_VertexID & 1) * 4 - 1);
  float y = float((gl_VertexID & 2) * 2 - 1);
  v_uv = vec2((x + 1.0) * 0.5, (y + 1.0) * 0.5);
  gl_Position = vec4(x, y, 0.0, 1.0);
})";

    const char* kBlitFS = R"(#version 330 core
in vec2 v_uv; uniform sampler2D u_tex; uniform vec4 u_tint; out vec4 frag;
void main(){ frag = texture(u_tex, v_uv) * u_tint; })";

    const char* kThresholdFS = R"(#version 330 core
in vec2 v_uv; uniform sampler2D u_tex; uniform float u_knee; out vec4 frag;
void main(){
  vec4 c = texture(u_tex, v_uv);
  float lum = dot(c.rgb, vec3(0.2126, 0.7152, 0.0722));
  float m = max(0.0, lum - u_knee);
  frag = vec4(c.rgb * (m / max(lum, 1e-4)), 1.0);
})";

    const char* kBlurFS = R"(#version 330 core
in vec2 v_uv; uniform sampler2D u_tex; uniform vec2 u_dir; out vec4 frag;
void main(){
  vec2 t = u_dir;
  vec4 s = texture(u_tex, v_uv) * 0.227027;
  s += texture(u_tex, v_uv + t * 1.384615) * 0.316216;
  s += texture(u_tex, v_uv - t * 1.384615) * 0.316216;
  s += texture(u_tex, v_uv + t * 3.230769) * 0.070270;
  s += texture(u_tex, v_uv - t * 3.230769) * 0.070270;
  frag = s;
})";

    const char* kVignetteFS = R"(#version 330 core
in vec2 v_uv; uniform sampler2D u_tex; uniform float u_strength; uniform vec3 u_color; out vec4 frag;
void main(){
  vec4 c = texture(u_tex, v_uv);
  vec2 d = v_uv - vec2(0.5);
  float v = smoothstep(0.85, 0.15, dot(d, d) * 2.2);
  float f = mix(1.0, v, clamp(u_strength, 0.0, 1.0));
  frag = vec4(mix(u_color, c.rgb, f), c.a);
})";

    const char* kLightingFS = R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_scene;
uniform sampler2D u_light;
uniform vec3 u_fill;
uniform float u_strength;
uniform vec2 u_maskSize;
out vec4 frag;
void main(){
  vec4 scene = texture(u_scene, v_uv);
  vec2 uvL = (floor(v_uv * u_maskSize) + 0.5) / u_maskSize;
  float vis = texture(u_light, uvL).r;
  vis = mix(1.0, vis, clamp(u_strength, 0.0, 1.0));
  vec3 lit = mix(scene.rgb * u_fill, scene.rgb, vis);
  frag = vec4(lit, scene.a);
})";

    GLuint CompileProgram(const char* vs_src, const char* fs_src)
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
            FW_CORE_ERROR("Present VS: {0}", log);
        }
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fs_src, nullptr);
        glCompileShader(fs);
        glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[512];
            glGetShaderInfoLog(fs, 512, nullptr, log);
            FW_CORE_ERROR("Present FS: {0}", log);
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
            FW_CORE_ERROR("Present link: {0}", log);
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        return prog;
    }
}

PresentPipeline& PresentPipeline::GetInstance()
{
    static PresentPipeline i;
    return i;
}

GLuint PresentPipeline::Compile(const char* vs, const char* fs)
{
    return CompileProgram(vs, fs);
}

bool PresentPipeline::Init()
{
    if (m_ready)
        return true;
    m_blit = CompileProgram(kFsVS, kBlitFS);
    m_threshold = CompileProgram(kFsVS, kThresholdFS);
    m_blur = CompileProgram(kFsVS, kBlurFS);
    m_vignette = CompileProgram(kFsVS, kVignetteFS);
    m_lighting = CompileProgram(kFsVS, kLightingFS);

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    // No VBO - vertex shader uses gl_VertexID
    glBindVertexArray(0);

    m_ready = m_blit && m_threshold && m_blur && m_vignette && m_lighting && m_vao;
    if (m_ready)
        FW_CORE_INFO("PresentPipeline initialized");
    return m_ready;
}

void PresentPipeline::Shutdown()
{
    m_scene.Destroy();
    m_bright.Destroy();
    m_bloom_a.Destroy();
    m_bloom_b.Destroy();
    m_aperture.Destroy();
    m_light_accum.Destroy();
    m_light_scratch.Destroy();
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_blit)
        glDeleteProgram(m_blit);
    if (m_threshold)
        glDeleteProgram(m_threshold);
    if (m_blur)
        glDeleteProgram(m_blur);
    if (m_vignette)
        glDeleteProgram(m_vignette);
    if (m_lighting)
        glDeleteProgram(m_lighting);
    m_vao = m_blit = m_threshold = m_blur = m_vignette = m_lighting = 0;
    m_w = m_h = 0;
    m_ready = false;
}

void PresentPipeline::EnsureSize(int w, int h)
{
    if (!m_ready)
        Init();
    if (w <= 0 || h <= 0)
        return;
    if (w == m_w && h == m_h && m_scene.Valid() && m_light_accum.Valid())
        return;
    m_w = w;
    m_h = h;
    m_scene.Resize(w, h, false);
    m_bright.Resize(w, h, false);
    // Bloom ping-pong at half res (high path); low path uses these too then downsamples via viewport
    m_bloom_a.Resize(std::max(1, w / 2), std::max(1, h / 2), true);
    m_bloom_b.Resize(std::max(1, w / 2), std::max(1, h / 2), true);
    m_aperture.Resize(std::max(64, w / 4), std::max(64, h / 4), false);
    // light masks: half-res, nearest (pixel-snap upsample in composite)
    m_light_accum.Resize(std::max(1, w / 2), std::max(1, h / 2), false);
    m_light_scratch.Resize(std::max(1, w / 2), std::max(1, h / 2), false);
}

void PresentPipeline::DrawFullscreen(GLuint program, GLuint tex0, GLuint tex1)
{
    glDisable(GL_BLEND);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex0);
    GLint loc = glGetUniformLocation(program, "u_tex");
    if (loc >= 0)
        glUniform1i(loc, 0);
    if (tex1)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, tex1);
    }
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void PresentPipeline::BlitTexture(GLuint tex, float r, float g, float b, float a, bool additive)
{
    if (!tex || !m_blit)
        return;
    if (additive)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    glUseProgram(m_blit);
    glUniform4f(glGetUniformLocation(m_blit, "u_tint"), r, g, b, a);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(m_blit, "u_tex"), 0);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void PresentPipeline::ApplyBloom(int quality, bool reduce_bloom)
{
    if (reduce_bloom || quality <= 0 || !m_scene.Valid())
        return;

    const int div = (quality >= 2) ? 2 : 4;
    const int bw = std::max(1, m_w / div);
    const int bh = std::max(1, m_h / div);
    m_bloom_a.Resize(bw, bh, true);
    m_bloom_b.Resize(bw, bh, true);

    // Threshold scene → bloom A
    m_bloom_a.Bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(m_threshold);
    glUniform1f(glGetUniformLocation(m_threshold, "u_knee"), 0.92f);
    DrawFullscreen(m_threshold, m_scene.Color());

    const int passes = (quality >= 2) ? 2 : 1;
    for (int i = 0; i < passes; ++i)
    {
        m_bloom_b.Bind();
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(m_blur);
        glUniform2f(glGetUniformLocation(m_blur, "u_dir"), 1.f / (float)bw, 0.f);
        DrawFullscreen(m_blur, m_bloom_a.Color());

        m_bloom_a.Bind();
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(m_blur);
        glUniform2f(glGetUniformLocation(m_blur, "u_dir"), 0.f, 1.f / (float)bh);
        DrawFullscreen(m_blur, m_bloom_b.Color());
    }

    m_scene.Bind();
    BlitTexture(m_bloom_a.Color(), 1.f, 1.f, 1.f, 1.f, true);
}

void PresentPipeline::CompositeEmissiveGlow(float intensity, int quality)
{
    if (intensity <= 0.001f || quality <= 0 || !m_scene.Valid() || !m_bright.Valid())
        return;

    const int div = (quality >= 2) ? 2 : 3;
    const int bw = std::max(1, m_w / div);
    const int bh = std::max(1, m_h / div);
    m_bloom_a.Resize(bw, bh, true);
    m_bloom_b.Resize(bw, bh, true);

    // Downsample bright mask → bloom A
    m_bloom_a.Bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    BlitTexture(m_bright.Color());

    const int passes = (quality >= 2) ? 3 : 2;
    for (int i = 0; i < passes; ++i)
    {
        m_bloom_b.Bind();
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(m_blur);
        glUniform2f(glGetUniformLocation(m_blur, "u_dir"), 1.25f / (float)bw, 0.f);
        DrawFullscreen(m_blur, m_bloom_a.Color());

        m_bloom_a.Bind();
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(m_blur);
        glUniform2f(glGetUniformLocation(m_blur, "u_dir"), 0.f, 1.25f / (float)bh);
        DrawFullscreen(m_blur, m_bloom_b.Color());
    }

    m_scene.Bind();
    const float s = std::clamp(intensity, 0.f, 2.f);
    BlitTexture(m_bloom_a.Color(), s, s, s, 1.f, true);
}

void PresentPipeline::ApplyVignette(float strength, float r, float g, float b)
{
    if (strength <= 0.001f || !m_scene.Valid() || !m_bright.Valid())
        return;
    // Ping through bright RT
    m_bright.Bind();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(m_vignette);
    glUniform1f(glGetUniformLocation(m_vignette, "u_strength"), strength);
    glUniform3f(glGetUniformLocation(m_vignette, "u_color"), r, g, b);
    DrawFullscreen(m_vignette, m_scene.Color());

    m_scene.Bind();
    BlitTexture(m_bright.Color());
}

void PresentPipeline::SetLightFill(float r, float g, float b, float strength)
{
    m_fill_r = r;
    m_fill_g = g;
    m_fill_b = b;
    m_fill_strength = strength;
}

void PresentPipeline::GetLightFill(float& r, float& g, float& b, float& strength) const
{
    r = m_fill_r;
    g = m_fill_g;
    b = m_fill_b;
    strength = m_fill_strength;
}

void PresentPipeline::CompositeLighting()
{
    if (m_fill_strength <= 0.f || !m_scene.Valid() || !m_light_accum.Valid()
        || !m_bright.Valid() || !m_lighting)
        return;

    // ping through Bright (full-res), blit back to Scene - same as vignette
    m_bright.Bind();
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_BLEND);
    glUseProgram(m_lighting);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_scene.Color());
    glUniform1i(glGetUniformLocation(m_lighting, "u_scene"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_light_accum.Color());
    glUniform1i(glGetUniformLocation(m_lighting, "u_light"), 1);
    glUniform3f(glGetUniformLocation(m_lighting, "u_fill"), m_fill_r, m_fill_g, m_fill_b);
    glUniform1f(glGetUniformLocation(m_lighting, "u_strength"), m_fill_strength);
    glUniform2f(glGetUniformLocation(m_lighting, "u_maskSize"),
                (float)m_light_accum.Width(), (float)m_light_accum.Height());
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_scene.Bind();
    BlitTexture(m_bright.Color());
}

void PresentPipeline::BlitToScreen(int screen_w, int screen_h, bool nearest)
{
    RenderTarget::BindDefault(screen_w, screen_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!m_scene.Valid())
        return;
    glBindTexture(GL_TEXTURE_2D, m_scene.Color());
    GLint filter = nearest ? (GLint)GL_NEAREST : (GLint)GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    BlitTexture(m_scene.Color());
}
