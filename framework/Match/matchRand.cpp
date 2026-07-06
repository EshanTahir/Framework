#include "matchRand.h"

MatchRand& MatchRand::GetInstance()
{
    static MatchRand instance;
    return instance;
}

void MatchRand::Seed(uint64_t seed)
{
    m_seed0 = seed;
    m_state = seed ? seed : 0x853c49e6748fea9bULL;
}

uint32_t MatchRand::NextU32()
{
    uint64_t x = m_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    m_state = x;
    return (uint32_t)((x * 0x2545F4914F6CDD1DULL) >> 32);
}

float MatchRand::NextFloat()
{
    return (NextU32() >> 8) * (1.f / 16777216.f);
}

int MatchRand::NextRange(int lo, int hi)
{
    if (hi < lo)
    {
        int t = lo;
        lo = hi;
        hi = t;
    }
    if (lo == hi)
        return lo;
    uint32_t span = (uint32_t)(hi - lo) + 1u;
    return lo + (int)(NextU32() % span);
}
