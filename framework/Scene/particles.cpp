#include "systems.h"
#include "Core/assetCache.h"
#include "Core/glContext.h"
#include "Core/log.h"
#include "Render/gl.h"
#include "Render/renderer2d.h"
#include "sceneHandler.h"

#include "box2d.h"
#include "SDL.h"
#include "SDL_ttf.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

void Particles::Run(entt::registry& registry, float delta_time)
{
    auto view = registry.view<ParticleEmitterComponent, ParticleBufferComponent, TransformComponent>();
    for (auto e : view)
    {
        auto& em = registry.get<ParticleEmitterComponent>(e);
        auto& buf = registry.get<ParticleBufferComponent>(e);
        auto& t = registry.get<TransformComponent>(e);

        auto spawn_one = [&]() {
            float ang = em.angle + ((rand() % 1000) / 1000.f) * em.spread - em.spread * 0.5f;
            Particle p;
            p.x = t.x + em.offset_x;
            p.y = t.y + em.offset_y;
            if (std::fabs(em.offset_y) > 0.01f && !em.volume_spawn && registry.has<BoxShapeComponent>(e))
                p.y = t.y + registry.get<BoxShapeComponent>(e).height * 0.5f;
            if (em.volume_spawn && registry.has<BoxShapeComponent>(e))
            {
                auto& box = registry.get<BoxShapeComponent>(e);
                p.x = t.x + ((rand() % 1000) / 1000.f - 0.5f) * box.width * 0.92f;
                p.y = t.y + ((rand() % 1000) / 1000.f - 0.5f) * box.height * 0.92f;
            }
            p.vx = std::cos(ang) * em.speed;
            p.vy = std::sin(ang) * em.speed;
            // Foot dust: extra sideways scatter, less vertical streak
            if (std::fabs(em.offset_y) > 0.01f && !em.volume_spawn)
            {
                p.vx += ((rand() % 1000) / 1000.f - 0.5f) * 1.6f;
                p.vy *= 0.45f;
            }
            p.life = em.life;
            p.max_life = em.life;
            p.size = em.size;
            buf.particles.push_back(p);
        };

        if (em.burst > 0)
        {
            for (int i = 0; i < em.burst; ++i)
                spawn_one();
            em.burst = 0;
        }

        if (em.emitting)
        {
            em.accum += em.rate * delta_time;
            while (em.accum >= 1.f)
            {
                em.accum -= 1.f;
                spawn_one();
            }
        }

        for (size_t i = 0; i < buf.particles.size();)
        {
            auto& p = buf.particles[i];
            p.life -= delta_time;
            p.x += p.vx * delta_time;
            p.y += p.vy * delta_time;
            if (p.life <= 0)
                buf.particles.erase(buf.particles.begin() + (long long)i);
            else
                ++i;
        }
    }
}

