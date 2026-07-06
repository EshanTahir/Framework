#include "matchChannels.h"

#include <cstring>

MatchChannels& MatchChannels::GetInstance()
{
    static MatchChannels instance;
    return instance;
}

void MatchChannels::Clear()
{
    m_map.clear();
    m_dirty.clear();
}

void MatchChannels::SetInt(const std::string& key, int32_t v)
{
    Entry& e = m_map[key];
    // Always dirty so clients re-receive even if value is unchanged (e.g. re-assert).
    e.type = Type::Int;
    e.i = v;
    m_dirty[key] = true;
}

void MatchChannels::SetFloat(const std::string& key, float v)
{
    Entry& e = m_map[key];
    e.type = Type::Float;
    e.f = v;
    m_dirty[key] = true;
}

void MatchChannels::SetU64(const std::string& key, uint64_t v)
{
    Entry& e = m_map[key];
    e.type = Type::U64;
    e.u = v;
    m_dirty[key] = true;
}

bool MatchChannels::Has(const std::string& key) const
{
    return m_map.find(key) != m_map.end();
}

int32_t MatchChannels::GetInt(const std::string& key, int32_t fallback) const
{
    auto it = m_map.find(key);
    if (it == m_map.end() || it->second.type != Type::Int)
        return fallback;
    return it->second.i;
}

float MatchChannels::GetFloat(const std::string& key, float fallback) const
{
    auto it = m_map.find(key);
    if (it == m_map.end() || it->second.type != Type::Float)
        return fallback;
    return it->second.f;
}

uint64_t MatchChannels::GetU64(const std::string& key, uint64_t fallback) const
{
    auto it = m_map.find(key);
    if (it == m_map.end() || it->second.type != Type::U64)
        return fallback;
    return it->second.u;
}

void MatchChannels::ClearDirty()
{
    m_dirty.clear();
}

void MatchChannels::ExportDirty(std::string& out_bytes)
{
    out_bytes.clear();
    for (const auto& kv : m_dirty)
    {
        if (!kv.second)
            continue;
        auto it = m_map.find(kv.first);
        if (it == m_map.end())
            continue;
        const std::string& key = kv.first;
        const Entry& e = it->second;
        const uint16_t klen = (uint16_t)std::min<size_t>(key.size(), 65535);
        out_bytes.push_back((char)e.type);
        out_bytes.push_back((char)(klen & 0xff));
        out_bytes.push_back((char)((klen >> 8) & 0xff));
        out_bytes.append(key.data(), klen);
        if (e.type == Type::Int)
        {
            int32_t v = e.i;
            out_bytes.append(reinterpret_cast<const char*>(&v), sizeof(v));
        }
        else if (e.type == Type::Float)
        {
            float v = e.f;
            out_bytes.append(reinterpret_cast<const char*>(&v), sizeof(v));
        }
        else
        {
            uint64_t v = e.u;
            out_bytes.append(reinterpret_cast<const char*>(&v), sizeof(v));
        }
    }
}

void MatchChannels::ExportAll(std::string& out_bytes)
{
    out_bytes.clear();
    for (const auto& kv : m_map)
    {
        const std::string& key = kv.first;
        const Entry& e = kv.second;
        const uint16_t klen = (uint16_t)std::min<size_t>(key.size(), 65535);
        out_bytes.push_back((char)e.type);
        out_bytes.push_back((char)(klen & 0xff));
        out_bytes.push_back((char)((klen >> 8) & 0xff));
        out_bytes.append(key.data(), klen);
        if (e.type == Type::Int)
        {
            int32_t v = e.i;
            out_bytes.append(reinterpret_cast<const char*>(&v), sizeof(v));
        }
        else if (e.type == Type::Float)
        {
            float v = e.f;
            out_bytes.append(reinterpret_cast<const char*>(&v), sizeof(v));
        }
        else
        {
            uint64_t v = e.u;
            out_bytes.append(reinterpret_cast<const char*>(&v), sizeof(v));
        }
    }
}

void MatchChannels::ImportBlob(const void* data, size_t len)
{
    const uint8_t* p = static_cast<const uint8_t*>(data);
    size_t i = 0;
    while (i + 3 <= len)
    {
        Type type = (Type)p[i++];
        uint16_t klen = (uint16_t)p[i] | ((uint16_t)p[i + 1] << 8);
        i += 2;
        if (i + klen > len)
            break;
        std::string key(reinterpret_cast<const char*>(p + i), klen);
        i += klen;
        Entry e;
        e.type = type;
        if (type == Type::Int)
        {
            if (i + sizeof(int32_t) > len)
                break;
            std::memcpy(&e.i, p + i, sizeof(int32_t));
            i += sizeof(int32_t);
            m_map[key] = e;
        }
        else if (type == Type::Float)
        {
            if (i + sizeof(float) > len)
                break;
            std::memcpy(&e.f, p + i, sizeof(float));
            i += sizeof(float);
            m_map[key] = e;
        }
        else if (type == Type::U64)
        {
            if (i + sizeof(uint64_t) > len)
                break;
            std::memcpy(&e.u, p + i, sizeof(uint64_t));
            i += sizeof(uint64_t);
            m_map[key] = e;
        }
        else
            break;
    }
}
