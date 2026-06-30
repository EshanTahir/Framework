#include "Audio/audio.h"
#include "Core/assetCache.h"
#include "Audio/audioDebug.h"
#include "Core/log.h"

#include <cmath>

Audio& Audio::GetInstance()
{
    static Audio instance;
    return instance;
}

Audio::~Audio()
{
    Shutdown();
}

void Audio::Shutdown()
{
    if (m_ui)
    {
        FMOD_ChannelGroup_Release(m_ui);
        m_ui = nullptr;
    }
    if (m_sfx)
    {
        FMOD_ChannelGroup_Release(m_sfx);
        m_sfx = nullptr;
    }
    if (m_music)
    {
        FMOD_ChannelGroup_Release(m_music);
        m_music = nullptr;
    }
    if (m_master)
    {
        FMOD_ChannelGroup_Release(m_master);
        m_master = nullptr;
    }
    if (audio_system)
    {
        FMOD_System_Release(audio_system);
        audio_system = nullptr;
    }
}

void Audio::Init()
{
    if (audio_system)
        return;

    FMOD_System_Create(&audio_system, FMOD_VERSION);
    FMOD_System_Init(audio_system, 64, FMOD_INIT_NORMAL, nullptr);

    FMOD_System_CreateChannelGroup(audio_system, "Master", &m_master);
    FMOD_System_CreateChannelGroup(audio_system, "Music", &m_music);
    FMOD_System_CreateChannelGroup(audio_system, "SFX", &m_sfx);
    FMOD_System_CreateChannelGroup(audio_system, "UI", &m_ui);

    if (m_master && m_music)
        FMOD_ChannelGroup_AddGroup(m_master, m_music, false, nullptr);
    if (m_master && m_sfx)
        FMOD_ChannelGroup_AddGroup(m_master, m_sfx, false, nullptr);
    if (m_master && m_ui)
        FMOD_ChannelGroup_AddGroup(m_master, m_ui, false, nullptr);

    // Route default master output through our Master group
    FMOD_CHANNELGROUP* system_master = nullptr;
    FMOD_System_GetMasterChannelGroup(audio_system, &system_master);
    if (system_master && m_master)
        FMOD_ChannelGroup_AddGroup(system_master, m_master, false, nullptr);

    FW_CORE_INFO("Audio buses ready (Master/Music/SFX/UI)");
}

FMOD_CHANNELGROUP* Audio::Group(AudioBus bus) const
{
    switch (bus)
    {
    case AudioBus::Music:
        return m_music;
    case AudioBus::Sfx:
        return m_sfx;
    case AudioBus::Ui:
        return m_ui;
    case AudioBus::Master:
    default:
        return m_master;
    }
}

void Audio::SetBusVolume(AudioBus bus, float linear01)
{
    if (linear01 < 0.f)
        linear01 = 0.f;
    if (linear01 > 1.f)
        linear01 = 1.f;
    FMOD_CHANNELGROUP* g = Group(bus);
    if (g)
        FMOD_ChannelGroup_SetVolume(g, linear01);
}

float Audio::GetBusVolume(AudioBus bus) const
{
    FMOD_CHANNELGROUP* g = Group(bus);
    float v = 1.f;
    if (g)
        FMOD_ChannelGroup_GetVolume(g, &v);
    return v;
}

FMOD_CHANNEL* Audio::Play(const char* path, AudioBus bus, float volume, bool looping, float pan)
{
    if (!audio_system || !path)
        return nullptr;
    FMOD_SOUND* sound = AssetCache::GetInstance().GetSound(audio_system, path);
    if (!sound)
    {
        AudioDebug::Record('F', path, 0, 0, m_listener_x, m_listener_y, 0, volume, "load_fail");
        return nullptr;
    }
    FMOD_CHANNEL* channel = nullptr;
    FMOD_CHANNELGROUP* group = Group(bus);
    if (!group)
        group = m_sfx;
    FMOD_System_PlaySound(audio_system, sound, group, true, &channel);
    if (!channel)
        return nullptr;
    FMOD_Channel_SetVolume(channel, volume);
    if (pan < -1.f)
        pan = -1.f;
    if (pan > 1.f)
        pan = 1.f;
    FMOD_Channel_SetPan(channel, pan);
    if (looping)
        FMOD_Channel_SetMode(channel, FMOD_LOOP_NORMAL);
    FMOD_Channel_SetPaused(channel, false);
    return channel;
}

void Audio::SetListener(float x, float y)
{
    m_listener_x = x;
    m_listener_y = y;
}

void Audio::GetListener(float& x, float& y) const
{
    x = m_listener_x;
    y = m_listener_y;
}

void Audio::SetSpatialRanges(float near_dist, float far_dist)
{
    if (near_dist < 0.f)
        near_dist = 0.f;
    if (far_dist < near_dist + 0.01f)
        far_dist = near_dist + 0.01f;
    m_spatial_near = near_dist;
    m_spatial_far = far_dist;
}

float Audio::SpatialGain(float world_x, float world_y) const
{
    const float dx = world_x - m_listener_x;
    const float dy = world_y - m_listener_y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    if (dist >= m_spatial_far)
        return 0.f;
    if (dist <= m_spatial_near)
        return 1.f;
    return 1.f - (dist - m_spatial_near) / (m_spatial_far - m_spatial_near);
}

float Audio::SpatialPan(float world_x, float /*world_y*/) const
{
    const float dx = world_x - m_listener_x;
    // Soft pan: full L/R only when source is far sideways relative to near range.
    float pan = dx / m_spatial_near;
    if (pan < -1.f)
        pan = -1.f;
    if (pan > 1.f)
        pan = 1.f;
    return pan;
}
