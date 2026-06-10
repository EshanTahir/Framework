#pragma once

#include <string>

// Device/profile prefs (volumes, video, a11y). Not long-term progress saves.
// Lives under Paths::GetUserDataRoot()/settings.json

class GameSettings
{
public:
    static GameSettings& GetInstance();

    void Load();
    void Save() const;
    void ApplyAudio() const;

    float Master() const { return m_master; }
    float Music() const { return m_music; }
    float Sfx() const { return m_sfx; }
    float Ui() const { return m_ui; }

    void SetMaster(float v);
    void SetMusic(float v);
    void SetSfx(float v);
    void SetUi(float v);

    float ShakeScale() const { return m_shake; }
    float RumbleScale() const { return m_rumble; }
    void SetShakeScale(float v);
    void SetRumbleScale(float v);

    // Video / presentation
    // bloom_quality: 0=off, 1=low, 2=high
    int BloomQuality() const { return m_bloom_quality; }
    void SetBloomQuality(int q);
    bool ParallaxEnabled() const { return m_parallax; }
    void SetParallaxEnabled(bool v);
    bool AperturesEnabled() const { return m_apertures; }
    void SetAperturesEnabled(bool v);
    bool ReduceFlash() const { return m_reduce_flash; }
    void SetReduceFlash(bool v);
    float Vignette() const { return m_vignette; }
    void SetVignette(float v);
    int MaxApertures() const { return m_max_apertures; }
    void SetMaxApertures(int n);

    static std::string FilePath();

private:
    GameSettings() = default;
    static float Clamp01(float v);

    int m_version = 1;
    float m_master = 1.f;
    float m_music = 0.75f;
    float m_sfx = 1.f;
    float m_ui = 1.f;
    float m_shake = 1.f;
    float m_rumble = 1.f;

    int m_bloom_quality = 0; // off by default - selective emissive glows, not scene bloom
    bool m_parallax = true;
    bool m_apertures = true;
    bool m_reduce_flash = false;
    float m_vignette = 0.f; // off; API kept
    int m_max_apertures = 2;

    bool m_dirty = false;
};
