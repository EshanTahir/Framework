#include "sfxPool.h"
#include "Scene/entity.h"
#include "Scene/scene.h"
#include "Scene/components.h"
#include "Audio/audio.h"
#include "Audio/audioDebug.h"

#include "SDL.h"

#include <cstdio>
#include <cstring>

namespace
{
    constexpr int kMaxPooledOneshotsPlaying = 24;

    void SyncListenerFromCamera(entt::registry& registry)
    {
        entt::entity tagged = entt::null;
        entt::entity best = entt::null;
        int best_depth = -999999;
        auto view = registry.view<CameraComponent, TransformComponent, TagComponent>();
        for (auto e : view)
        {
            const std::string& tag = registry.get<TagComponent>(e).tag;
            if (tag == "camera")
                tagged = e;
            auto& cam = registry.get<CameraComponent>(e);
            if (cam.depth < 0)
                continue;
            if (cam.depth >= best_depth)
            {
                best_depth = cam.depth;
                best = e;
            }
        }
        entt::entity use = tagged != entt::null ? tagged : best;
        if (use == entt::null)
            return;
        auto& t = registry.get<TransformComponent>(use);
        Audio::GetInstance().SetListener(t.x, t.y);
    }

    int CountPlayingOneshots(entt::registry& registry)
    {
        int n = 0;
        auto view = registry.view<SoundComponent, TagComponent>();
        for (auto e : view)
        {
            const std::string& tag = registry.get<TagComponent>(e).tag;
            if (tag.rfind("_sfx_", 0) != 0)
                continue;
            auto& s = registry.get<SoundComponent>(e);
            if (!s.oneshot || s.is_looping)
                continue;
            if (s.play)
            {
                ++n;
                continue;
            }
            if (!s.channel)
                continue;
            FMOD_BOOL playing = 0;
            FMOD_Channel_IsPlaying(s.channel, &playing);
            if (playing)
                ++n;
        }
        return n;
    }

    bool AssignOrCreate(entt::registry& registry, Scene& scene, const char* path, float volume,
                        float pan, int bus)
    {
        if (!path || !path[0])
            return false;

        if (CountPlayingOneshots(registry) >= kMaxPooledOneshotsPlaying)
        {
            float lx = 0, ly = 0;
            Audio::GetInstance().GetListener(lx, ly);
            AudioDebug::Record('V', path, 0, 0, lx, ly, 0, volume, "voice_cap");
            return false;
        }

        auto view = registry.view<SoundComponent, TagComponent>();
        for (auto e : view)
        {
            const std::string& tag = registry.get<TagComponent>(e).tag;
            if (tag.rfind("_sfx_", 0) != 0)
                continue;
            auto& s = registry.get<SoundComponent>(e);
            if (!s.oneshot || s.is_looping)
                continue;
            FMOD_BOOL playing = 0;
            if (s.channel)
                FMOD_Channel_IsPlaying(s.channel, &playing);
            if (s.play || playing)
                continue;

            s.sound_location = path;
            s.channel = nullptr;
            s.play = true;
            s.oneshot = true;
            s.is_looping = false;
            s.volume = volume;
            s.pan = pan;
            s.bus = bus;
            return true;
        }

        char tag[64];
        std::snprintf(tag, sizeof(tag), "_sfx_%u", (unsigned)SDL_GetTicks());
        Entity ent = scene.CreateEntity(tag);
        ent.AddComponent<SoundComponent>();
        auto& s = ent.GetComponent<SoundComponent>();
        s.sound_location = path;
        s.play = true;
        s.oneshot = true;
        s.volume = volume;
        s.pan = pan;
        s.bus = bus;
        return true;
    }
}

void SfxPool::Play(entt::registry& registry, Scene& scene, const char* path, float volume, int bus)
{
    float lx = 0, ly = 0;
    Audio::GetInstance().GetListener(lx, ly);
    if (AssignOrCreate(registry, scene, path, volume, 0.f, bus))
        AudioDebug::Record('L', path, lx, ly, lx, ly, 1.f, volume, "local");
}

void SfxPool::PlayAt(entt::registry& registry, Scene& scene, const char* path, float x, float y,
                     float volume, int bus)
{
    SyncListenerFromCamera(registry);
    Audio& audio = Audio::GetInstance();
    float lx = 0, ly = 0;
    audio.GetListener(lx, ly);
    float gain = audio.SpatialGain(x, y);
    float pan = audio.SpatialPan(x, y);
    if (AudioDebug::BypassSpatial())
    {
        gain = 1.f;
        pan = 0.f;
    }
    if (gain <= 0.001f)
    {
        AudioDebug::Record('C', path, x, y, lx, ly, gain, volume, "culled");
        return;
    }
    if (AssignOrCreate(registry, scene, path, volume * gain, pan, bus))
        AudioDebug::Record('S', path, x, y, lx, ly, gain, volume * gain, "spatial");
}
