#pragma once

#include "Render/renderTarget.h"
#include "Render/renderer2d.h"

// Fullscreen post passes: blit, threshold, blur, vignette. Owns bloom ping-pong RTs.
class PresentPipeline
{
public:
    static PresentPipeline& GetInstance();

    bool Init();
    void Shutdown();

    // resize scene + bloom targets to match window.
    void EnsureSize(int w, int h);

    RenderTarget& Scene() { return m_scene; }
    RenderTarget& Bright() { return m_bright; }
    RenderTarget& BloomA() { return m_bloom_a; }
    RenderTarget& BloomB() { return m_bloom_b; }
    RenderTarget& Aperture() { return m_aperture; }

    int WorldWidth() const { return m_w; }
    int WorldHeight() const { return m_h; }

    // quality: 0=off, 1=low (1/4), 2=high (1/2). reduce_bloom forces off.
    void ApplyBloom(int quality, bool reduce_bloom);
    // Blur contents already drawn into Bright() and additive-composite onto Scene().
    // Soft perimeter glow (screen-space blur ≈ even distance around silhouette).
    void CompositeEmissiveGlow(float intensity = 1.f, int quality = 1);
    void ApplyVignette(float strength, float r, float g, float b);
    // Nearest (or linear) fullscreen blit of the scene RT into the window.
    void BlitToScreen(int screen_w, int screen_h, bool nearest);

    void BlitTexture(GLuint tex, float r = 1, float g = 1, float b = 1, float a = 1,
                     bool additive = false);

private:
    PresentPipeline() = default;

    GLuint Compile(const char* vs, const char* fs);
    void DrawFullscreen(GLuint program, GLuint tex0, GLuint tex1 = 0);

    RenderTarget m_scene;
    RenderTarget m_bright;
    RenderTarget m_bloom_a;
    RenderTarget m_bloom_b;
    RenderTarget m_aperture;

    GLuint m_blit = 0;
    GLuint m_threshold = 0;
    GLuint m_blur = 0;
    GLuint m_vignette = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;

    int m_w = 0;
    int m_h = 0;
    bool m_ready = false;
};
