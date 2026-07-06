#include "contentHash.h"
#include "Core/paths.h"

#include <cstdio>
#include <sstream>
#include <iomanip>

namespace
{
    uint64_t Fnv1a64(const void* data, size_t len, uint64_t h = 14695981039346656037ULL)
    {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < len; ++i)
        {
            h ^= p[i];
            h *= 1099511628211ULL;
        }
        return h;
    }
}

uint64_t ContentHash::HashFile(const std::string& path)
{
    const std::string full = Paths::Resolve(path);
    FILE* f = std::fopen(full.c_str(), "rb");
    if (!f)
        return Fnv1a64(path.data(), path.size());

    uint64_t h = Fnv1a64(path.data(), path.size());
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        h = Fnv1a64(buf, n, h);
    std::fclose(f);
    return h;
}

uint64_t ContentHash::HashFiles(const std::vector<std::string>& paths)
{
    uint64_t h = 14695981039346656037ULL;
    for (const auto& p : paths)
    {
        uint64_t fh = HashFile(p);
        h = Fnv1a64(&fh, sizeof(fh), h);
        h = Fnv1a64(p.data(), p.size(), h);
    }
    return h;
}

std::string ContentHash::ToHex(uint64_t v)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(16) << v;
    return oss.str();
}
