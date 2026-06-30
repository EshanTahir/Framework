#include "Audio/audioDebug.h"
#include "Audio/audio.h"

#include <cstdio>
#include <cstring>

namespace
{
    AudioDebug::Event g_ring[AudioDebug::kRing]{};
    int g_write = 0;
    int g_count = 0;
    bool g_bypass = false;
    bool g_enabled = true;

    void TruncPath(char* dst, size_t cap, const char* path)
    {
        if (!path || cap == 0)
        {
            if (cap)
                dst[0] = 0;
            return;
        }
        const char* base = path;
        for (const char* p = path; *p; ++p)
        {
            if (*p == '/' || *p == '\\')
                base = p + 1;
        }
        std::snprintf(dst, cap, "%s", base);
    }
}

void AudioDebug::Record(char kind, const char* path, float x, float y, float lx, float ly,
                        float gain, float vol, const char* note)
{
    if (!g_enabled)
        return;
    Event& e = g_ring[g_write % kRing];
    e.kind = kind;
    e.x = x;
    e.y = y;
    e.lx = lx;
    e.ly = ly;
    e.gain = gain;
    e.vol = vol;
    TruncPath(e.path, sizeof(e.path), path);
    if (note)
        std::snprintf(e.note, sizeof(e.note), "%s", note);
    else
        e.note[0] = 0;
    g_write = (g_write + 1) % kRing;
    if (g_count < kRing)
        ++g_count;
}

void AudioDebug::SetBypassSpatial(bool on) { g_bypass = on; }
bool AudioDebug::BypassSpatial() { return g_bypass; }
void AudioDebug::SetEnabled(bool on) { g_enabled = on; }
bool AudioDebug::Enabled() { return g_enabled; }

const AudioDebug::Event* AudioDebug::Last()
{
    if (g_count <= 0)
        return nullptr;
    int i = (g_write + kRing - 1) % kRing;
    return &g_ring[i];
}

int AudioDebug::Count() { return g_count; }

const AudioDebug::Event* AudioDebug::At(int i)
{
    if (i < 0 || i >= g_count)
        return nullptr;
    int start = (g_write - g_count + kRing) % kRing;
    return &g_ring[(start + i) % kRing];
}

int AudioDebug::FormatHud(char* buf, size_t cap)
{
    if (!buf || cap < 8)
        return 0;
    float lx = 0, ly = 0;
    Audio::GetInstance().GetListener(lx, ly);
    int n = std::snprintf(buf, cap,
                          "AUD dbg  bypass=%s  listen=(%.1f,%.1f)  near/far=%.0f/%.0f\n"
                          "9=toggle  8=local test  7=spatial@listen  6=bypass spat\n",
                          g_bypass ? "ON" : "off", lx, ly,
                          Audio::GetInstance().SpatialNear(), Audio::GetInstance().SpatialFar());
    if (n < 0)
        return 0;
    size_t used = (size_t)n;
    for (int i = g_count - 1; i >= 0 && used + 40 < cap; --i)
    {
        const Event* e = At(i);
        if (!e)
            continue;
        int w = std::snprintf(buf + used, cap - used, "%c g=%.2f v=%.2f (%.0f,%.0f) %s %s\n",
                              e->kind, e->gain, e->vol, e->x, e->y, e->path, e->note);
        if (w < 0)
            break;
        used += (size_t)w;
    }
    return (int)used;
}
