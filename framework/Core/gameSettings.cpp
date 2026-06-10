#include "gameSettings.h"
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
    if (v < 0.f) return 0.f;
    if (v > 1.f) return 1.f;
    return v;
}

std::string GameSettings::FilePath()
{
    return Paths::GetUserDataRoot() + "/settings.json";
}

void GameSettings::ApplyAudio() const {}

void GameSettings::Load()
{
    Paths::EnsureUserDataDir();
    const std::string path = FilePath();
    std::ifstream in(path);
    if (!in)
    {
        FW_CORE_INFO("GameSettings: no file yet ({0}), using defaults", path);
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
        m_dirty = false;
    }
    catch (const std::exception& e)
    {
        FW_CORE_ERROR("GameSettings load failed: {0}", e.what());
    }
}

void GameSettings::Save() const
{
    Paths::EnsureUserDataDir();
    const std::string path = FilePath();
    json j;
    j["v"] = m_version;
    j["audio"] = {{"master", m_master}, {"music", m_music}, {"sfx", m_sfx}, {"ui", m_ui}};
    std::ofstream out(path);
    if (!out) { FW_CORE_ERROR("GameSettings: cannot write {0}", path); return; }
    out << j.dump(2);
}

void GameSettings::SetMaster(float v) { m_master = Clamp01(v); Save(); }
void GameSettings::SetMusic(float v) { m_music = Clamp01(v); Save(); }
void GameSettings::SetSfx(float v) { m_sfx = Clamp01(v); Save(); }
void GameSettings::SetUi(float v) { m_ui = Clamp01(v); Save(); }
void GameSettings::SetShakeScale(float v) { m_shake = Clamp01(v); Save(); }
void GameSettings::SetRumbleScale(float v) { m_rumble = Clamp01(v); Save(); }
void GameSettings::SetBloomQuality(int q) { m_bloom_quality = std::clamp(q, 0, 2); Save(); }
void GameSettings::SetParallaxEnabled(bool v) { m_parallax = v; Save(); }
void GameSettings::SetAperturesEnabled(bool v) { m_apertures = v; Save(); }
void GameSettings::SetReduceFlash(bool v) { m_reduce_flash = v; Save(); }
void GameSettings::SetVignette(float v) { m_vignette = Clamp01(v); Save(); }
void GameSettings::SetMaxApertures(int n) { m_max_apertures = std::clamp(n, 0, 8); Save(); }
