#include "gameSettings.h"
#include "Audio/audio.h"
#include "log.h"
#include "paths.h"

#include "json.hpp"

#include <algorithm>
#include <fstream>

using json = nlohmann::json;

GameSettings& GameSettings::GetInstance()
{
    static GameSettings instance;
    return instance;
}

float GameSettings::Clamp01(float v)
{
    if (v < 0.f)
        return 0.f;
    if (v > 1.f)
        return 1.f;
    return v;
}

std::string GameSettings::FilePath()
{
    return Paths::GetUserDataRoot() + "/settings.json";
}

void GameSettings::Load()
{
    Paths::EnsureUserDataDir();
    const std::string path = FilePath();
    std::ifstream in(path);
    if (!in)
    {
        FW_CORE_INFO("GameSettings: no file yet ({0}), using defaults", path);
        ApplyAudio();
        return;
    }

    try
    {
        json j;
        in >> j;
        m_version = j.value("v", 1);
        if (j.contains("audio"))
        {
            const auto& a = j["audio"];
            m_master = Clamp01(a.value("master", m_master));
            m_music = Clamp01(a.value("music", m_music));
            m_sfx = Clamp01(a.value("sfx", m_sfx));
            m_ui = Clamp01(a.value("ui", m_ui));
        }
        if (j.contains("a11y"))
        {
            const auto& a = j["a11y"];
            m_shake = Clamp01(a.value("shake", m_shake));
            m_rumble = Clamp01(a.value("rumble", m_rumble));
            m_reduce_flash = a.value("reduce_flash", m_reduce_flash);
        }
        if (j.contains("video"))
        {
            const auto& v = j["video"];
            m_bloom_quality = std::clamp(v.value("bloom_quality", m_bloom_quality), 0, 2);
            m_parallax = v.value("parallax", m_parallax);
            m_apertures = v.value("apertures", m_apertures);
            m_vignette = 0.f; // hard-off; ignore saved value
            m_max_apertures = std::clamp(v.value("max_apertures", m_max_apertures), 0, 8);
        }
        m_dirty = false;
        FW_CORE_INFO("GameSettings loaded from {0}", path);
    }
    catch (const std::exception& e)
    {
        FW_CORE_ERROR("GameSettings load failed: {0}", e.what());
    }
    ApplyAudio();
}

void GameSettings::Save() const
{
    Paths::EnsureUserDataDir();
    const std::string path = FilePath();
    json j;
    j["v"] = m_version;
    j["audio"] = {
        {"master", m_master},
        {"music", m_music},
        {"sfx", m_sfx},
        {"ui", m_ui},
    };
    j["a11y"] = {
        {"shake", m_shake},
        {"rumble", m_rumble},
        {"reduce_flash", m_reduce_flash},
    };
    j["video"] = {
        {"bloom_quality", m_bloom_quality},
        {"parallax", m_parallax},
        {"apertures", m_apertures},
        {"vignette", m_vignette},
        {"max_apertures", m_max_apertures},
    };

    std::ofstream out(path);
    if (!out)
    {
        FW_CORE_ERROR("GameSettings: cannot write {0}", path);
        return;
    }
    out << j.dump(2);
    FW_CORE_INFO("GameSettings saved to {0}", path);
}

void GameSettings::ApplyAudio() const
{
    auto& audio = Audio::GetInstance();
    audio.SetBusVolume(AudioBus::Master, m_master);
    audio.SetBusVolume(AudioBus::Music, m_music);
    audio.SetBusVolume(AudioBus::Sfx, m_sfx);
    audio.SetBusVolume(AudioBus::Ui, m_ui);
}

void GameSettings::SetMaster(float v)
{
    m_master = Clamp01(v);
    ApplyAudio();
    Save();
}
void GameSettings::SetMusic(float v)
{
    m_music = Clamp01(v);
    ApplyAudio();
    Save();
}
void GameSettings::SetSfx(float v)
{
    m_sfx = Clamp01(v);
    ApplyAudio();
    Save();
}
void GameSettings::SetUi(float v)
{
    m_ui = Clamp01(v);
    ApplyAudio();
    Save();
}
void GameSettings::SetShakeScale(float v)
{
    m_shake = Clamp01(v);
    Save();
}
void GameSettings::SetRumbleScale(float v)
{
    m_rumble = Clamp01(v);
    Save();
}
void GameSettings::SetBloomQuality(int q)
{
    m_bloom_quality = std::clamp(q, 0, 2);
    Save();
}
void GameSettings::SetParallaxEnabled(bool v)
{
    m_parallax = v;
    Save();
}
void GameSettings::SetAperturesEnabled(bool v)
{
    m_apertures = v;
    Save();
}
void GameSettings::SetReduceFlash(bool v)
{
    m_reduce_flash = v;
    Save();
}
void GameSettings::SetVignette(float v)
{
    m_vignette = Clamp01(v);
    Save();
}
void GameSettings::SetMaxApertures(int n)
{
    m_max_apertures = std::clamp(n, 0, 8);
    Save();
}
