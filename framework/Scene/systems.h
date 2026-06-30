#pragma once

#include "components.h"
#include "Audio/audio.h"
#include "Events/inputEvent.h"
#include "Render/renderer2d.h"

#include "box2d.h"
#include "entt.hpp"
#include "fmod.h"
#include "SDL.h"

#include <vector>

class System
{
public:
    virtual void Run(entt::registry& registry, float delta_time) = 0;
};

class Scripts : public System
{
public:
    void Run(entt::registry& registry, float delta_time) override;
    // after physics.Step; optional on_post_phys(dt).
    void RunPostPhys(entt::registry& registry, float delta_time);
    // Tick a single scripted entity by tag (pause UI while sim is frozen).
    void RunTagged(entt::registry& registry, const char* tag, float delta_time);
};

class ContactBuffer
{
public:
    struct Event
    {
        uint32_t a = 0, b = 0;
        bool begin = true;
        bool sensor = false;
    };
    std::vector<Event> events;
    void Clear() { events.clear(); }
};

class Physics
{
public:
    void Initialize(entt::registry& registry, b2World& world);
    void UnInitialize(entt::registry& registry, b2World& world);
    void Run(entt::registry& registry, b2World& world, float delta_time);
    void BuildTileCollision(entt::registry& registry, b2World& world);

    // Body + fixtures for one entity. Used on scene init and for late spawns.
    void InitializeBody(entt::registry& registry, b2World& world, entt::entity entity);

    bool gravity_on = true;
    ContactBuffer contacts;

private:
    int velocity_iterations = 8;
    int position_iterations = 3;
    class Listener;
    Listener* listener = nullptr;
};

class Rect
{
public:
    static void Update(RectComponent& rect, float x, float y);
    static void SetTop(entt::registry& registry, RectComponent& rect, int y);
    static void SetLeft(entt::registry& registry, RectComponent& rect, int x);
    static void SetBottom(entt::registry& registry, RectComponent& rect, int y);
    static void SetRight(entt::registry& registry, RectComponent& rect, int x);
    static void SetTopLeft(entt::registry& registry, RectComponent& rect, int x, int y);
    static void SetBottomLeft(entt::registry& registry, RectComponent& rect, int x, int y);
    static void SetTopRight(entt::registry& registry, RectComponent& rect, int x, int y);
    static void SetBottomRight(entt::registry& registry, RectComponent& rect, int x, int y);
    static void SetMidTop(entt::registry& registry, RectComponent& rect, int x, int y);
    static void SetMidLeft(entt::registry& registry, RectComponent& rect, int x, int y);
    static void SetMidBottom(entt::registry& registry, RectComponent& rect, int x, int y);
    static void SetMidRight(entt::registry& registry, RectComponent& rect, int x, int y);
    static void SetCenter(entt::registry& registry, RectComponent& rect, int x, int y);
    static bool CollideRect(RectComponent& rect, RectComponent& other_rect);
};

class Renderer : public System
{
public:
    void Run(entt::registry& registry, float delta_time) override;
    void CalculateCameras(entt::registry& registry);
    void CutSprites(entt::registry& registry);
    void DrawColliders(entt::registry& registry);
    void DrawTileMaps(entt::registry& registry);
    void DrawSprites(entt::registry& registry);
    void DrawSpriteGrids(entt::registry& registry);
    void DrawAnimations(entt::registry& registry, float delta_time);
    void DrawText(entt::registry& registry);
    // screen HUD / pause labels after overlays.
    void DrawScreenHud(entt::registry& registry);
    void DrawParticles(entt::registry& registry);
    void DrawDebugShapes(entt::registry& registry);
    void DrawLightVolumes(entt::registry& registry);
    void DrawLightShafts(entt::registry& registry, float delta_time);
    void DrawApertureFrames(entt::registry& registry);
    void DrawEmissiveMask(entt::registry& registry);

    // Last-frame sprite submit stats (reset each world pass).
    int sprites_seen = 0;
    int sprites_drawn = 0;
    int sprites_culled = 0;
    int atlas_pages = 0;

    SDL_Window* window = nullptr;
    bool draw_debug_colliders = false;
    // When false: skip sprites / sheets / tilemaps; force DebugDraw boxes (classic look).
    bool draw_world_textures = true;
    // clear / sky color (0-1; scripts may pass 0-255).
    float clear_r = 0.06f;
    float clear_g = 0.07f;
    float clear_b = 0.12f;
    float clear_a = 1.f;

private:
    void DrawWorldPass(entt::registry& registry, float delta_time, const Camera2D& master, bool parallax_on);
    void RenderApertures(entt::registry& registry, float delta_time, const Camera2D& master);
    void UpdateViewCull(const Camera2D& master);
    static int EntityBand(entt::registry& registry, entt::entity e);
    static float EntityParallax(entt::registry& registry, entt::entity e);
    bool BandMatch(entt::registry& registry, entt::entity e) const;
    bool InView(float x, float y, float half_w, float half_h) const;

    entt::entity active_camera = entt::null;
    int m_band_filter = 0;
    bool m_band_filter_on = false;
    bool m_drawing_aperture = false;
    bool m_screen_text_only = false;
    bool m_world_text_only = false;

    // World AABB of the gameplay camera (+ margin), for sprite cull.
    float m_view_x0 = 0, m_view_y0 = 0, m_view_x1 = 0, m_view_y1 = 0;
    bool m_view_cull_on = false;
};

class Sound : public System
{
public:
    void Run(entt::registry& registry, float delta_time) override;

private:
    FMOD_SYSTEM* audio = Audio::GetInstance().audio_system;
};

class Ui : public System
{
public:
    // mouse hit-test / click latch; once per display frame before scripts.
    void ProcessInput(entt::registry& registry);
    // Draw only (hover/pressed already latched by ProcessInput).
    void Run(entt::registry& registry, float delta_time) override;
    // Overlay panels (pause dim) under HUD text; interactive buttons above.
    void DrawOverlays(entt::registry& registry);
    void DrawChrome(entt::registry& registry);
    void UpdateButtons(entt::registry& registry);
    void HandleButtons(entt::registry& registry);
    void DrawButtons(entt::registry& registry, bool overlays_only);

private:
    InputEvent& input_event = InputEvent::GetInstance();
};

class Particles : public System
{
public:
    void Run(entt::registry& registry, float delta_time) override;
};

class DebugHud : public System
{
public:
    void Run(entt::registry& registry, float delta_time) override;
};
