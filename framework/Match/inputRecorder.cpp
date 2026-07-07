#include "inputRecorder.h"
#include "Core/paths.h"
#include "Core/log.h"

#include <cstdio>
#include <cstring>

namespace
{
    bool ReadReplayFile(const std::string& path, ReplayHeader& meta, std::vector<InputFrame>& frames)
    {
        const std::string full = Paths::Resolve(path);
        FILE* f = std::fopen(full.c_str(), "rb");
        if (!f)
            return false;
        char magic[8];
        if (std::fread(magic, 1, 8, f) != 8)
        {
            std::fclose(f);
            return false;
        }
        meta = {};
        if (std::memcmp(magic, "IFRM0002", 8) == 0)
        {
            if (std::fread(&meta.seed, sizeof(meta.seed), 1, f) != 1
                || std::fread(&meta.content_hash, sizeof(meta.content_hash), 1, f) != 1
                || std::fread(&meta.source_slot, sizeof(meta.source_slot), 1, f) != 1)
            {
                std::fclose(f);
                return false;
            }
        }
        else if (std::memcmp(magic, "IFRM0001", 8) != 0)
        {
            std::fclose(f);
            return false;
        }

        uint32_t n = 0;
        if (std::fread(&n, sizeof(n), 1, f) != 1)
        {
            std::fclose(f);
            return false;
        }
        frames.assign(n, {});
        if (n && std::fread(frames.data(), sizeof(InputFrame), n, f) != n)
        {
            frames.clear();
            std::fclose(f);
            return false;
        }
        std::fclose(f);
        return true;
    }
}

InputRecorder& InputRecorder::GetInstance()
{
    static InputRecorder instance;
    return instance;
}

void InputRecorder::Clear()
{
    m_frames.clear();
}

void InputRecorder::Push(const InputFrame& frame)
{
    if (!m_enabled)
        return;
    if (m_frames.size() >= kMaxFrames)
        m_frames.erase(m_frames.begin(), m_frames.begin() + (kMaxFrames / 8));
    m_frames.push_back(frame);
}

bool InputRecorder::SaveToFile(const std::string& path) const
{
    const std::string full = Paths::Resolve(path);
    FILE* f = std::fopen(full.c_str(), "wb");
    if (!f)
        return false;
    const char magic[8] = {'I', 'F', 'R', 'M', '0', '0', '0', '2'};
    std::fwrite(magic, 1, 8, f);
    std::fwrite(&m_meta.seed, sizeof(m_meta.seed), 1, f);
    std::fwrite(&m_meta.content_hash, sizeof(m_meta.content_hash), 1, f);
    std::fwrite(&m_meta.source_slot, sizeof(m_meta.source_slot), 1, f);
    uint32_t n = (uint32_t)m_frames.size();
    std::fwrite(&n, sizeof(n), 1, f);
    if (n)
        std::fwrite(m_frames.data(), sizeof(InputFrame), n, f);
    std::fclose(f);
    return true;
}

bool InputRecorder::LoadFromFile(const std::string& path)
{
    ReplayHeader meta{};
    std::vector<InputFrame> frames;
    if (!ReadReplayFile(path, meta, frames))
        return false;
    m_meta = meta;
    m_frames = std::move(frames);
    return true;
}

InputPlayback& InputPlayback::GetInstance()
{
    static InputPlayback instance;
    return instance;
}

void InputPlayback::Clear()
{
    m_active = false;
    m_frames.clear();
    m_meta = {};
    m_source_slot = 0;
}

bool InputPlayback::LoadFromRecorder()
{
    auto& rec = InputRecorder::GetInstance();
    m_frames = rec.Frames();
    m_meta = rec.Meta();
    m_source_slot = m_meta.source_slot;
    m_active = !m_frames.empty();
    return m_active;
}

bool InputPlayback::LoadFromFile(const std::string& path)
{
    ReplayHeader meta{};
    std::vector<InputFrame> frames;
    if (!ReadReplayFile(path, meta, frames))
        return false;
    m_meta = meta;
    m_frames = std::move(frames);
    m_source_slot = m_meta.source_slot;
    m_active = !m_frames.empty();
    return m_active;
}

InputFrame InputPlayback::FrameAtTick(uint64_t tick) const
{
    InputFrame empty{};
    empty.tick = tick;
    empty.slot = m_source_slot;
    if (!m_active)
        return empty;

    const InputFrame* found = nullptr;
    for (const auto& f : m_frames)
    {
        if (f.slot != m_source_slot)
            continue;
        if (f.tick == tick)
            found = &f;
        if (f.tick > tick && found)
            break;
    }
    if (found)
        return *found;

    // Ordinal fallback when absolute ticks were reset between record and play.
    uint64_t ordinal = 0;
    uint64_t last_t = ~0ull;
    for (const auto& f : m_frames)
    {
        if (f.slot != m_source_slot)
            continue;
        if (f.tick != last_t)
        {
            if (ordinal == tick)
            {
                InputFrame out = f;
                out.tick = tick;
                return out;
            }
            last_t = f.tick;
            ++ordinal;
        }
    }
    return empty;
}
