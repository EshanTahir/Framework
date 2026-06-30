#pragma once

#include <cstddef>

// Ring of recent one-shot attempts for in-game audio diagnosis (key 9 HUD).
namespace AudioDebug
{
    constexpr int kRing = 12;

    struct Event
    {
        char kind = '?'; // L=local S=spatial C=culled V=voiceCap F=fail N=noApi
        float x = 0, y = 0;
        float lx = 0, ly = 0;
        float gain = 0, vol = 0;
        char path[48]{};
        char note[24]{};
    };

    void Record(char kind, const char* path, float x, float y, float lx, float ly, float gain,
                float vol, const char* note);
    void SetBypassSpatial(bool on);
    bool BypassSpatial();
    void SetEnabled(bool on);
    bool Enabled();

    // Fills buf with multi-line HUD text. Returns bytes written (excl nul).
    int FormatHud(char* buf, size_t cap);
    const Event* Last();
    int Count();
    const Event* At(int i); // 0 = oldest in window, Count()-1 = newest
}
