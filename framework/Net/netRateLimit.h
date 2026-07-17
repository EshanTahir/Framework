#pragma once

#include <cstdint>
#include <deque>

// Sliding 1s window rate limit for inbound messages per peer.

class NetRateLimit
{
public:
    void SetLimit(int max_per_sec)
    {
        m_max = max_per_sec > 0 ? max_per_sec : 180;
    }

    bool Allow(uint64_t now_ms)
    {
        while (!m_hits.empty() && now_ms - m_hits.front() >= 1000)
            m_hits.pop_front();
        if ((int)m_hits.size() >= m_max)
            return false;
        m_hits.push_back(now_ms);
        return true;
    }

    void Reset() { m_hits.clear(); }

private:
    int m_max = 180;
    std::deque<uint64_t> m_hits;
};
