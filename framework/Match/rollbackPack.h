#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Optional helpers games may append into their rollback blob.

namespace RollbackPack
{
    // Pack entire MatchChannels map (not just dirty).
    void PackChannels(std::vector<uint8_t>& out);
    bool UnpackChannels(const uint8_t* data, size_t len, size_t* consumed = nullptr);

    void PackRandState(std::vector<uint8_t>& out);
    bool UnpackRandState(const uint8_t* data, size_t len, size_t* consumed = nullptr);
}
