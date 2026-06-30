#pragma once

#include "fmod.h"

#include <string>

// dual-slot music bed; local only, do not net-sync.
class MusicPlayer
{
public:
    static MusicPlayer& GetInstance();

    void Update(float dt);

    // Start or crossfade to path. fade_sec is both out (old) and in (new).
    // Returns false if path fails to load.
    bool Play(const char* path, float fade_sec = 1.6f);

    void Stop(float fade_sec = 1.0f);

    const std::string& CurrentPath() const { return m_current_path; }
    bool IsPlaying() const;

private:
    MusicPlayer() = default;
    ~MusicPlayer();

    struct Slot
    {
        FMOD_CHANNEL* channel = nullptr;
        FMOD_SOUND* sound = nullptr; // streamed; owned here
        float vol = 0.f;
        float target = 0.f;
        float fade_speed = 0.f; // |dvol/dt| toward target
        std::string path;
    };

    void ReleaseSlot(Slot& s);
    Slot* FreeSlot();
    Slot* ActiveSlot();
    void ApplyVolume(Slot& s);

    Slot m_slots[2];
    int m_active = -1; // index into m_slots, or -1
    std::string m_current_path;
};
