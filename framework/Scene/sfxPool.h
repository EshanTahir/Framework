#pragma once

#include "entt.hpp"

class Scene;

// Reuse ephemeral one-shot SoundComponent entities instead of Create/Destroy per play.
namespace SfxPool
{
    // path must remain valid (caller usually keeps a static string pool).
    // Local / UI: full volume, centered pan.
    void Play(entt::registry& registry, Scene& scene, const char* path, float volume, int bus);

    // World spatial (fake 2D): distance gain + stereo pan from Audio listener.
    // Culled when SpatialGain == 0. Not FMOD 3D - see Audio notes.
    void PlayAt(entt::registry& registry, Scene& scene, const char* path, float x, float y,
                float volume, int bus);
}
