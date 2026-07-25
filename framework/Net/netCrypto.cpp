#include "netCrypto.h"
#include "netAeadSession.h"

NetCrypto& NetCrypto::GetInstance()
{
    static NetCrypto instance;
    return instance;
}

void NetCrypto::SetRoomKey(const uint8_t key[32])
{
    m_session.SetKey(key);
}

void NetCrypto::Clear()
{
    m_session.Clear();
}

bool NetCrypto::Active() const
{
    return m_session.Active();
}

bool NetCrypto::Seal(uint8_t type, const void* plain, size_t len, std::string& out)
{
    return m_session.Seal(type, plain, len, out);
}

bool NetCrypto::Open(uint8_t type, const void* wire, size_t len, std::string& plain)
{
    return m_session.Open(type, wire, len, plain);
}
