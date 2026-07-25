#pragma once

#include "netAeadSession.h"

#include <cstddef>
#include <cstdint>
#include <string>

// Room-scoped AEAD for gameplay NetMsg payloads (lobby-issued room_key).

class NetCrypto
{
public:
    static NetCrypto& GetInstance();

    void SetRoomKey(const uint8_t key[32]);
    void Clear();
    bool Active() const;

    bool Seal(uint8_t type, const void* plain, size_t len, std::string& out);
    bool Open(uint8_t type, const void* wire, size_t len, std::string& plain);

private:
    NetCrypto() = default;
    NetAeadSession m_session;
};
