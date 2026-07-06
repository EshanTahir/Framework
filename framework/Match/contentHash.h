#pragma once

#include <cstdint>
#include <string>
#include <vector>

// FNV-1a fingerprint of caller-supplied paths (join checks).

class ContentHash
{
public:
    static uint64_t HashFile(const std::string& path);
    static uint64_t HashFiles(const std::vector<std::string>& paths);
    static std::string ToHex(uint64_t h);
};
