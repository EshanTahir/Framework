#include "matchSession.h"
#include "contentHash.h"
#include "inputRecorder.h"
#include "inputSampler.h"
#include "matchChannels.h"
#include "matchConfig.h"
#include "matchRand.h"
#include "simClock.h"
#include "Core/log.h"

MatchSession& MatchSession::GetInstance()
{
    static MatchSession instance;
    return instance;
}

void MatchSession::ResetForLocal()
{
    m_authority = MatchAuthority::OfflineHost;
    m_net_model = MatchNetModel::HostSnapshot;
    m_local_slot = 0;
    m_force_snap = {};
    m_replay_ghost_slot = -1;
    MatchChannels::GetInstance().Clear();
    InputRecorder::GetInstance().Clear();
    InputPlayback::GetInstance().Clear();
    SimClock::GetInstance().Reset();
    if (m_seed == 0)
        m_seed = 1;
    MatchRand::GetInstance().Seed(m_seed);
}

void MatchSession::SetAuthority(MatchAuthority a)
{
    m_authority = a;
}

void MatchSession::SetSeed(uint64_t seed)
{
    m_seed = seed ? seed : 1;
    MatchRand::GetInstance().Seed(m_seed);
}

void MatchSession::SetContentPaths(std::vector<std::string> paths)
{
    m_content_paths = std::move(paths);
}

void MatchSession::RecomputeContentHash()
{
    m_content_hash = ContentHash::HashFiles(m_content_paths);
}

std::string MatchSession::ContentHashHex() const
{
    return ContentHash::ToHex(m_content_hash);
}

void MatchSession::ApplyRemoteInput(const InputFrame& frame)
{
    InputSampler::GetInstance().SetRemoteFrame(frame.slot, frame);
}

void MatchSession::ApplyRemoteInputQueue(const InputFrame* frames, size_t count)
{
    InputSampler::GetInstance().ApplyRemoteQueue(frames, count);
}

void MatchSession::RequestForceSnap(int slot)
{
    if (slot >= 0 && slot < (int)m_force_snap.size())
        m_force_snap[(size_t)slot] = true;
}

bool MatchSession::ConsumeForceSnap(int slot)
{
    if (slot < 0 || slot >= (int)m_force_snap.size())
        return false;
    const bool v = m_force_snap[(size_t)slot];
    m_force_snap[(size_t)slot] = false;
    return v;
}

void MatchSession::BeginSimTick(uint64_t tick, bool resample_devices)
{
    InputSampler::GetInstance().BeginTick(tick, resample_devices);
}

void MatchSession::RecordSimInputs()
{
    if (InputPlayback::GetInstance().Active())
        return; // don't overwrite tape while playing back
    auto& rec = InputRecorder::GetInstance();
    ReplayHeader meta = rec.Meta();
    meta.seed = m_seed;
    meta.content_hash = m_content_hash;
    meta.source_slot = m_local_slot;
    rec.SetMeta(meta);
    auto& mc = MatchConfig::GetInstance();
    for (int i = 0; i < mc.Count(); ++i)
    {
        const MatchSlot* s = mc.Slot(i);
        if (!s || s->vacant || s->device == MatchDevice::Replay)
            continue;
        rec.Push(InputSampler::GetInstance().Frame(i));
    }
}

bool MatchSession::ReplaySave(const std::string& path) const
{
    return InputRecorder::GetInstance().SaveToFile(path);
}

bool MatchSession::ReplayLoad(const std::string& path, int source_slot)
{
    auto& play = InputPlayback::GetInstance();
    if (!play.LoadFromFile(path))
    {
        FW_CORE_WARN("ReplayLoad failed: {0}", path);
        return false;
    }
    if (source_slot >= 0)
        play.SetSourceSlot(source_slot);
    else
        play.SetSourceSlot(play.Meta().source_slot);

    const ReplayHeader& meta = play.Meta();
    if (meta.seed)
        SetSeed(meta.seed);

    auto& mc = MatchConfig::GetInstance();
    m_replay_ghost_slot = -1;
    for (int i = 0; i < mc.Count(); ++i)
    {
        const MatchSlot* s = mc.Slot(i);
        if (s && s->device == MatchDevice::Replay && !s->vacant)
        {
            m_replay_ghost_slot = i;
            break;
        }
    }
    if (m_replay_ghost_slot < 0)
        m_replay_ghost_slot = mc.AddReplay();

    SimClock::GetInstance().Reset();
    FW_CORE_INFO("Replay armed ghost slot {0} from {1}", m_replay_ghost_slot, path);
    return m_replay_ghost_slot >= 0;
}

bool MatchSession::ReplayActive() const
{
    return InputPlayback::GetInstance().Active();
}

void MatchSession::ReplayStop()
{
    m_replay_ghost_slot = -1;
    InputPlayback::GetInstance().Clear();
}
