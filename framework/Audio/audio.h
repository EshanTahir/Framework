#pragma once
#include "fmod.h"

enum class AudioBus
{
    Master = 0,
    Music,
    Sfx,
    Ui,
};

class Audio
{
public:
    static Audio& GetInstance();
    void Init();
    void Shutdown();

    FMOD_SYSTEM* audio_system = nullptr;

    FMOD_CHANNELGROUP* Group(AudioBus bus) const;
    void SetBusVolume(AudioBus bus, float linear01);
    float GetBusVolume(AudioBus bus) const;

    // Play a cached one-shot/loop on a bus; returns channel or nullptr.
    // pan: -1 left ... 0 center ... +1 right (2D stereo; not FMOD 3D).
    FMOD_CHANNEL* Play(const char* path, AudioBus bus, float volume, bool looping, float pan = 0.f);

    // Fake-2D spatial: listener at camera, distance gain + stereo pan.
    // full FMOD 3D deferred.
    void SetListener(float x, float y);
    void GetListener(float& x, float& y) const;
    void SetSpatialRanges(float near_dist, float far_dist);
    // Linear falloff: 1 at <= near, 0 at >= far. Returns 0 when culled.
    float SpatialGain(float world_x, float world_y) const;
    // Soft stereo pan from listener→source (-1..1).
    float SpatialPan(float world_x, float world_y) const;
    float SpatialNear() const { return m_spatial_near; }
    float SpatialFar() const { return m_spatial_far; }

private:
    Audio() = default;
    ~Audio();

    FMOD_CHANNELGROUP* m_master = nullptr;
    FMOD_CHANNELGROUP* m_music = nullptr;
    FMOD_CHANNELGROUP* m_sfx = nullptr;
    FMOD_CHANNELGROUP* m_ui = nullptr;

    float m_listener_x = 0.f;
    float m_listener_y = 0.f;
    // World units = entity/Box2D xy. Scale near/far against your camera view half.
    // Full volume on-screen-ish; fade out past ~1.5× view. FMOD 3D still later.
    float m_spatial_near = 12.f;
    float m_spatial_far = 36.f;
};
