#include "systems.h"
#include "Audio/audio.h"
#include "Audio/musicPlayer.h"

namespace
{
    AudioBus BusFromComponent(int bus)
    {
        switch (bus)
        {
        case 0:
            return AudioBus::Master;
        case 2:
            return AudioBus::Music;
        case 3:
            return AudioBus::Ui;
        case 1:
        default:
            return AudioBus::Sfx;
        }
    }

    void UpdateAudioListener(entt::registry& registry)
    {
        // Prefer gameplay camera tag; skip aperture view cams (depth < 0).
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
}

void Sound::Run(entt::registry& registry, float delta_time)
{
    FMOD_SYSTEM* sys = Audio::GetInstance().audio_system;
    if (!sys)
        return;
    MusicPlayer::GetInstance().Update(delta_time);
    UpdateAudioListener(registry);
    auto view = registry.view<SoundComponent>();
    FMOD_System_Update(sys);

    for (auto entity : view)
    {
        auto& sound = registry.get<SoundComponent>(entity);
        FMOD_BOOL is_playing = 0;
        if (sound.channel)
            FMOD_Channel_IsPlaying(sound.channel, &is_playing);

        if ((sound.play || is_playing) && sound.channel)
        {
            FMOD_Channel_SetVolume(sound.channel, sound.volume);
            FMOD_Channel_SetPitch(sound.channel, sound.pitch);
            FMOD_Channel_SetPan(sound.channel, sound.pan);
        }

        if (sound.play && !is_playing && sound.sound_location)
        {
            sound.channel = Audio::GetInstance().Play(sound.sound_location, BusFromComponent(sound.bus),
                                                      sound.volume, sound.is_looping, sound.pan);
            if (!sound.is_looping)
                sound.play = false;
        }

        // Pooled oneshots: clear channel when done so SfxPool can reuse the entity.
        if (sound.oneshot && !sound.is_looping && !sound.play && sound.channel)
        {
            FMOD_BOOL still = 0;
            FMOD_Channel_IsPlaying(sound.channel, &still);
            if (!still)
                sound.channel = nullptr;
        }
    }
}
