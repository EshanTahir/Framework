#pragma once

#include "components.h"
#include "Events/inputEvent.h"
#include "Render/renderer2d.h"
#include "box2d.h"

#include "entt.hpp"
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
    void RunPostPhys(entt::registry& registry, float delta_time);
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
    void DrawScreenHud(entt::registry& registry) { (void)registry; }
    SDL_Window* window = nullptr;
    bool draw_debug_colliders = false;
    bool draw_world_textures = true;
    float clear_r = 0.06f, clear_g = 0.07f, clear_b = 0.12f, clear_a = 1.f;
};


class Sound : public System
{
public:
    void Run(entt::registry& registry, float delta_time) override;
};


class Ui : public System
{
public:
    void ProcessInput(entt::registry& registry);
    void Run(entt::registry& registry, float delta_time) override;
    void DrawOverlays(entt::registry& registry);
    void DrawChrome(entt::registry& registry);

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
