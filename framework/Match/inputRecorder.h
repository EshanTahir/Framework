#pragma once

#include "inputFrame.h"

#include <cstdint>
#include <string>
#include <vector>

// Ring buffer of InputFrames for later replay (model A). No UI required.

struct ReplayHeader
{
    uint64_t seed = 1;
    uint64_t content_hash = 0;
    int32_t source_slot = 0;
};

class InputRecorder
{
public:
    static InputRecorder& GetInstance();

    void Clear();
    void SetEnabled(bool on) { m_enabled = on; }
    bool Enabled() const { return m_enabled; }

    void Push(const InputFrame& frame);
    size_t Count() const { return m_frames.size(); }

    void SetMeta(const ReplayHeader& h) { m_meta = h; }
    const ReplayHeader& Meta() const { return m_meta; }

    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path);

    const std::vector<InputFrame>& Frames() const { return m_frames; }

private:
    InputRecorder() = default;
    bool m_enabled = true;
    std::vector<InputFrame> m_frames;
    ReplayHeader m_meta{};
    static constexpr size_t kMaxFrames = 60 * 60 * 30; // ~30 min at 60Hz one slot
};

// Feeds recorded frames into a MatchDevice::Replay slot by sim tick.
class InputPlayback
{
public:
    static InputPlayback& GetInstance();

    void Clear();
    bool LoadFromRecorder();
    bool LoadFromFile(const std::string& path);

    void SetSourceSlot(int slot) { m_source_slot = slot; }
    int SourceSlot() const { return m_source_slot; }
    bool Active() const { return m_active; }
    const ReplayHeader& Meta() const { return m_meta; }

    // Frame for source_slot at this sim tick (empty if past end).
    InputFrame FrameAtTick(uint64_t tick) const;

private:
    InputPlayback() = default;
    bool m_active = false;
    int m_source_slot = 0;
    ReplayHeader m_meta{};
    std::vector<InputFrame> m_frames;
};
