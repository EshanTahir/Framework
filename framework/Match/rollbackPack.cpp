#include "rollbackPack.h"
#include "matchChannels.h"
#include "matchRand.h"

#include <cstring>

namespace
{
    void AppendU16(std::vector<uint8_t>& out, uint16_t v)
    {
        out.push_back((uint8_t)(v & 0xff));
        out.push_back((uint8_t)((v >> 8) & 0xff));
    }

    void AppendBytes(std::vector<uint8_t>& out, const void* p, size_t n)
    {
        const auto* b = static_cast<const uint8_t*>(p);
        out.insert(out.end(), b, b + n);
    }
}

void RollbackPack::PackChannels(std::vector<uint8_t>& out)
{
    // Reuse ExportDirty format but for all keys via temporary dirty mark.
    auto& ch = MatchChannels::GetInstance();
    // Export by Import-compatible blob: mark all dirty then ExportDirty.
    // MatchChannels has no ExportAll - use a local walk via Get by re-setting.
    // Simpler: call ExportDirty after forcing dirty on known keys is impossible.
    // Add ExportAll by duplicating ExportDirty logic with full map - done in matchChannels.
    std::string blob;
    ch.ExportAll(blob);
    const uint32_t n = (uint32_t)blob.size();
    AppendBytes(out, &n, sizeof(n));
    if (n)
        AppendBytes(out, blob.data(), blob.size());
}

bool RollbackPack::UnpackChannels(const uint8_t* data, size_t len, size_t* consumed)
{
    if (!data || len < 4)
        return false;
    uint32_t n = 0;
    std::memcpy(&n, data, 4);
    if (4 + n > len)
        return false;
    if (n)
        MatchChannels::GetInstance().ImportBlob(data + 4, n);
    if (consumed)
        *consumed = 4 + n;
    return true;
}

void RollbackPack::PackRandState(std::vector<uint8_t>& out)
{
    uint64_t st = MatchRand::GetInstance().State();
    uint64_t seed = MatchRand::GetInstance().SeedValue();
    AppendBytes(out, &seed, sizeof(seed));
    AppendBytes(out, &st, sizeof(st));
}

bool RollbackPack::UnpackRandState(const uint8_t* data, size_t len, size_t* consumed)
{
    if (!data || len < 16)
        return false;
    uint64_t seed = 0, st = 0;
    std::memcpy(&seed, data, 8);
    std::memcpy(&st, data + 8, 8);
    MatchRand::GetInstance().Seed(seed);
    MatchRand::GetInstance().SetState(st);
    if (consumed)
        *consumed = 16;
    return true;
}
