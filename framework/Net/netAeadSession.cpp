#include "netAeadSession.h"
#include "monocypher.h"

#include <cstring>
#include <random>

void NetAeadSession::SetKey(const uint8_t key[32])
{
    if (!key)
    {
        Clear();
        return;
    }
    bool any = false;
    for (int i = 0; i < 32; ++i)
        if (key[i])
        {
            any = true;
            break;
        }
    if (!any)
    {
        Clear();
        return;
    }
    std::memcpy(m_key, key, 32);
    m_active = true;
    m_send_seq = 0;
}

void NetAeadSession::Clear()
{
    crypto_wipe(m_key, sizeof(m_key));
    m_active = false;
    m_send_seq = 0;
}

namespace
{
    void FillNonce(uint8_t nonce[24], uint64_t seq)
    {
        std::memset(nonce, 0, 24);
        for (int i = 0; i < 8; ++i)
            nonce[i] = (uint8_t)((seq >> (8 * i)) & 0xff);
        static std::mt19937 rng{std::random_device{}()};
        for (int i = 8; i < 24; ++i)
            nonce[i] = (uint8_t)(rng() & 0xff);
    }
}

bool NetAeadSession::Seal(uint8_t type, const void* plain, size_t len, std::string& out)
{
    if (!m_active)
    {
        if (len && plain)
            out.assign(static_cast<const char*>(plain), len);
        else
            out.clear();
        return true;
    }
    const uint8_t* p = static_cast<const uint8_t*>(plain);
    if (len && !p)
        return false;

    uint8_t nonce[24];
    FillNonce(nonce, ++m_send_seq);
    out.resize(24 + 16 + len);
    std::memcpy(&out[0], nonce, 24);
    uint8_t* mac = reinterpret_cast<uint8_t*>(&out[24]);
    uint8_t* cipher = reinterpret_cast<uint8_t*>(&out[40]);
    crypto_aead_lock(cipher, mac, m_key, nonce, &type, 1, p, len);
    return true;
}

bool NetAeadSession::Open(uint8_t type, const void* wire, size_t len, std::string& plain)
{
    if (!m_active)
    {
        if (len && wire)
            plain.assign(static_cast<const char*>(wire), len);
        else
            plain.clear();
        return true;
    }
    if (!wire || len < 40)
        return false;
    const uint8_t* w = static_cast<const uint8_t*>(wire);
    const size_t clen = len - 40;
    plain.resize(clen);
    uint8_t* out = clen ? reinterpret_cast<uint8_t*>(&plain[0]) : nullptr;
    if (crypto_aead_unlock(out, w + 24, m_key, w, &type, 1, w + 40, clen) != 0)
    {
        plain.clear();
        return false;
    }
    return true;
}
