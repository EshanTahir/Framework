#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Per-connection ChaCha20-Poly1305 session (Monocypher).
// Wire payload: nonce[24] | mac[16] | ciphertext. AD = type byte.

class NetAeadSession
{
public:
    void SetKey(const uint8_t key[32]);
    void Clear();
    bool Active() const { return m_active; }

    bool Seal(uint8_t type, const void* plain, size_t len, std::string& out);
    bool Open(uint8_t type, const void* wire, size_t len, std::string& plain);

private:
    uint8_t m_key[32]{};
    bool m_active = false;
    uint64_t m_send_seq = 0;
};
