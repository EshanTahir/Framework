#pragma once

#include <cstdint>

// seeded xorshift64*; host-only world consumer online.

class MatchRand
{
public:
    static MatchRand& GetInstance();

    void Seed(uint64_t seed);
    uint64_t SeedValue() const { return m_seed0; }

    // Rollback: save/restore generator cursor.
    uint64_t State() const { return m_state; }
    void SetState(uint64_t state) { m_state = state ? state : 0x853c49e6748fea9bULL; }

    uint32_t NextU32();
    // [0, 1)
    float NextFloat();
    // Inclusive range
    int NextRange(int lo, int hi);

private:
    MatchRand() = default;
    uint64_t m_state = 0x853c49e6748fea9bULL;
    uint64_t m_seed0 = 0;
};
