#include "Audio/musicPlayer.h"
#include "Audio/audio.h"
#include "Core/paths.h"
#include "Core/log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

MusicPlayer& MusicPlayer::GetInstance()
{
    static MusicPlayer instance;
    return instance;
}

MusicPlayer::~MusicPlayer()
{
    ReleaseSlot(m_slots[0]);
    ReleaseSlot(m_slots[1]);
}

void MusicPlayer::ReleaseSlot(Slot& s)
{
    if (s.channel)
    {
        FMOD_Channel_Stop(s.channel);
        s.channel = nullptr;
    }
    if (s.sound)
    {
        FMOD_Sound_Release(s.sound);
        s.sound = nullptr;
    }
    s.vol = 0.f;
    s.target = 0.f;
    s.fade_speed = 0.f;
    s.path.clear();
}

MusicPlayer::Slot* MusicPlayer::FreeSlot()
{
    for (int i = 0; i < 2; ++i)
    {
        if (i == m_active)
            continue;
        return &m_slots[i];
    }
    // Both busy - steal the quieter / non-active by force-releasing the other.
    const int other = (m_active == 0) ? 1 : 0;
    ReleaseSlot(m_slots[other]);
    return &m_slots[other];
}

MusicPlayer::Slot* MusicPlayer::ActiveSlot()
{
    if (m_active < 0 || m_active > 1)
        return nullptr;
    return &m_slots[m_active];
}

void MusicPlayer::ApplyVolume(Slot& s)
{
    if (s.channel)
        FMOD_Channel_SetVolume(s.channel, s.vol);
}

bool MusicPlayer::IsPlaying() const
{
    if (m_active < 0)
        return false;
    const Slot& s = m_slots[m_active];
    if (!s.channel)
        return false;
    FMOD_BOOL playing = 0;
    FMOD_Channel_IsPlaying(s.channel, &playing);
    return playing != 0;
}

bool MusicPlayer::Play(const char* path, float fade_sec)
{
    if (!path || !path[0])
        return false;

    auto& audio = Audio::GetInstance();
    if (!audio.audio_system)
        return false;

    // Already on this bed - keep it (no restart).
    if (m_current_path == path && IsPlaying())
        return true;

    if (fade_sec < 0.05f)
        fade_sec = 0.05f;

    std::string resolved = Paths::Resolve(path);
    FMOD_SOUND* sound = nullptr;
    // Stream long tracks; no loop - music_bed picks the next bed when one ends.
    FMOD_RESULT r = FMOD_System_CreateSound(audio.audio_system, resolved.c_str(),
                                            FMOD_CREATESTREAM, nullptr, &sound);
    if (r != FMOD_OK || !sound)
    {
        FW_CORE_ERROR("Music load failed '{0}' (FMOD {1})", resolved, (int)r);
        return false;
    }

    Slot* next = FreeSlot();
    ReleaseSlot(*next);

    FMOD_CHANNEL* channel = nullptr;
    FMOD_CHANNELGROUP* group = audio.Group(AudioBus::Music);
    FMOD_System_PlaySound(audio.audio_system, sound, group, true, &channel);
    if (!channel)
    {
        FMOD_Sound_Release(sound);
        FW_CORE_ERROR("Music play failed '{0}'", resolved);
        return false;
    }

    next->sound = sound;
    next->channel = channel;
    next->path = path;
    next->vol = 0.f;
    next->target = 1.f;
    next->fade_speed = 1.f / fade_sec;
    FMOD_Channel_SetVolume(channel, 0.f);
    FMOD_Channel_SetPaused(channel, false);

    // Fade out whatever was active.
    if (Slot* cur = ActiveSlot())
    {
        if (cur != next)
        {
            cur->target = 0.f;
            cur->fade_speed = 1.f / fade_sec;
        }
    }

    m_active = (next == &m_slots[0]) ? 0 : 1;
    m_current_path = path;
    FW_CORE_INFO("Music → {0}", path);
    return true;
}

void MusicPlayer::Stop(float fade_sec)
{
    if (fade_sec < 0.05f)
        fade_sec = 0.05f;
    for (int i = 0; i < 2; ++i)
    {
        Slot& s = m_slots[i];
        if (!s.channel)
            continue;
        s.target = 0.f;
        s.fade_speed = 1.f / fade_sec;
    }
    m_current_path.clear();
    m_active = -1;
}

void MusicPlayer::Update(float dt)
{
    if (dt < 0.f)
        dt = 0.f;
    if (dt > 0.1f)
        dt = 0.1f;

    for (int i = 0; i < 2; ++i)
    {
        Slot& s = m_slots[i];
        if (!s.channel)
            continue;

        FMOD_BOOL playing = 0;
        FMOD_Channel_IsPlaying(s.channel, &playing);
        if (!playing)
        {
            ReleaseSlot(s);
            if (m_active == i)
            {
                m_active = -1;
                m_current_path.clear();
            }
            continue;
        }

        if (s.fade_speed > 0.f)
        {
            if (s.vol < s.target)
            {
                s.vol = std::min(s.target, s.vol + s.fade_speed * dt);
            }
            else if (s.vol > s.target)
            {
                s.vol = std::max(s.target, s.vol - s.fade_speed * dt);
            }
            ApplyVolume(s);

            if (s.target <= 0.f && s.vol <= 0.001f)
            {
                ReleaseSlot(s);
                if (m_active == i)
                    m_active = -1;
            }
        }
    }
}
