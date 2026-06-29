#include "Scene/systems.h"
#include "Core/log.h"
#include "Render/renderer2d.h"
#include "Scene/sceneHandler.h"

#include "box2d.h"
#include "SDL.h"

void DebugHud::Run(entt::registry& registry, float delta_time)
{
    (void)delta_time;
    static float acc = 0;
    static int frames = 0;
    static float fps = 0;
    static Uint32 wall_t0 = 0;
    static int wall_frames = 0;
    static float wall_fps = 0.f;
    acc += delta_time;
    frames++;
    if (acc >= 0.5f)
    {
        fps = frames / acc;
        frames = 0;
        acc = 0;
    }
    const Uint32 now = SDL_GetTicks();
    if (wall_t0 == 0)
        wall_t0 = now;
    ++wall_frames;
    const uint32_t wall_dt = now - wall_t0;
    if (wall_dt >= 500)
    {
        wall_fps = wall_frames * 1000.f / (float)wall_dt;
        wall_frames = 0;
        wall_t0 = now;
    }

    auto view = registry.view<DebugHudComponent>();
    for (auto e : view)
    {
        auto& hud = registry.get<DebugHudComponent>(e);
        if (!hud.show)
            continue;
        // Stats available via Renderer2D; logged occasionally
        static float log_t = 0;
        log_t += delta_time;
        if (log_t > 2.f)
        {
            log_t = 0;
            auto& r = Renderer2D::GetInstance();
            Scene* scene = SceneHandler::GetInstance().GetActiveScene();
            int seen = 0, drawn = 0, culled = 0, pages = 0;
            if (scene)
            {
                seen = scene->renderer.sprites_seen;
                drawn = scene->renderer.sprites_drawn;
                culled = scene->renderer.sprites_culled;
                pages = scene->renderer.atlas_pages;
            }
            int bodies_on = 0, bodies_all = 0, contacts = 0;
            if (scene && scene->world)
            {
                contacts = scene->world->GetContactCount();
                bodies_all = scene->world->GetBodyCount();
                for (b2Body* b = scene->world->GetBodyList(); b; b = b->GetNext())
                {
                    if (b->IsEnabled())
                        ++bodies_on;
                }
            }
            FW_CORE_INFO(
                "FPS={0:.1f} wall={1:.1f} draws={2} quads={3} spr={4}/{5} cull={6} atlas={7} bodies={8}/{9} contacts={10}",
                fps, wall_fps, r.DrawCalls(), r.QuadCount(), drawn, seen, culled, pages,
                bodies_on, bodies_all, contacts);
        }
    }
}
