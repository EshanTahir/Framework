#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

// shared int/float/string blackboard; host writes, all read.

class MatchChannels
{
public:
    static MatchChannels& GetInstance();

    void Clear();

    void SetInt(const std::string& key, int32_t v);
    void SetFloat(const std::string& key, float v);
    void SetU64(const std::string& key, uint64_t v);

    bool Has(const std::string& key) const;
    int32_t GetInt(const std::string& key, int32_t fallback = 0) const;
    float GetFloat(const std::string& key, float fallback = 0.f) const;
    uint64_t GetU64(const std::string& key, uint64_t fallback = 0) const;

    // Serialization helpers for net snapshots (packed key=type+payload list).
    void ExportDirty(std::string& out_bytes);
    void ExportAll(std::string& out_bytes);
    void ImportBlob(const void* data, size_t len);
    void ClearDirty();

private:
    MatchChannels() = default;

    enum class Type : uint8_t
    {
        Int = 1,
        Float = 2,
        U64 = 3
    };

    struct Entry
    {
        Type type = Type::Int;
        int32_t i = 0;
        float f = 0.f;
        uint64_t u = 0;
    };

    std::unordered_map<std::string, Entry> m_map;
    std::unordered_map<std::string, bool> m_dirty;
};
