#include "netCtrlLink.h"
#include "lobbyProtocol.h"
#include "monocypher.h"
#include "Core/log.h"

#include <chrono>
#include <cstring>
#include <random>

void NetCtrlLink::Clear()
{
    m_session.Clear();
    m_wire.clear();
    m_plain.clear();
}

namespace
{
    void RandomBytes(uint8_t* out, size_t n)
    {
        static std::mt19937 rng{std::random_device{}()};
        for (size_t i = 0; i < n; ++i)
            out[i] = (uint8_t)(rng() & 0xff);
    }

    void DeriveSessionKey(uint8_t out[32], const uint8_t shared[32])
    {
        uint8_t in[16]{};
        std::memcpy(in, "FW-CTRL-v1", 10);
        crypto_chacha20_h(out, shared, in);
    }
}

bool NetCtrlLink::SendAllRaw(NetSock::Handle sock, const void* data, size_t len)
{
    return NetSock::SendAll(sock, static_cast<const char*>(data), len);
}

bool NetCtrlLink::RecvExact(NetSock::Handle sock, void* out, size_t len, int timeout_ms)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    size_t got = 0;
    auto* dst = static_cast<char*>(out);
    while (got < len)
    {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;
        int n = NetSock::Recv(sock, dst + got, len - got);
        if (n > 0)
        {
            got += (size_t)n;
            continue;
        }
        if (n == 0 || n == -2)
            return false;
        NetSock::SleepMs(2);
    }
    return true;
}

bool NetCtrlLink::DoHandshake(NetSock::Handle sock, bool as_server, int timeout_ms)
{
    Clear();
    uint8_t sk[32];
    uint8_t pk[32];
    RandomBytes(sk, 32);
    crypto_x25519_public_key(pk, sk);

    uint8_t hello[36];
    hello[0] = 'F';
    hello[1] = 'W';
    hello[2] = 'C';
    hello[3] = '1';
    std::memcpy(hello + 4, pk, 32);

    uint8_t peer[36];
    if (as_server)
    {
        if (!RecvExact(sock, peer, sizeof(peer), timeout_ms))
        {
            crypto_wipe(sk, 32);
            return false;
        }
        if (!SendAllRaw(sock, hello, sizeof(hello)))
        {
            crypto_wipe(sk, 32);
            return false;
        }
    }
    else
    {
        if (!SendAllRaw(sock, hello, sizeof(hello)))
        {
            crypto_wipe(sk, 32);
            return false;
        }
        if (!RecvExact(sock, peer, sizeof(peer), timeout_ms))
        {
            crypto_wipe(sk, 32);
            return false;
        }
    }

    if (peer[0] != 'F' || peer[1] != 'W' || peer[2] != 'C' || peer[3] != '1')
    {
        crypto_wipe(sk, 32);
        FW_CORE_WARN("NetCtrlLink: bad handshake magic");
        return false;
    }

    uint8_t shared[32];
    crypto_x25519(shared, sk, peer + 4);
    crypto_wipe(sk, 32);

    uint8_t key[32];
    DeriveSessionKey(key, shared);
    crypto_wipe(shared, 32);
    m_session.SetKey(key);
    crypto_wipe(key, 32);
    return m_session.Active();
}

bool NetCtrlLink::HandshakeClient(NetSock::Handle sock, int timeout_ms)
{
    return DoHandshake(sock, false, timeout_ms);
}

bool NetCtrlLink::HandshakeServer(NetSock::Handle sock, int timeout_ms)
{
    return DoHandshake(sock, true, timeout_ms);
}

bool NetCtrlLink::SendRecord(NetSock::Handle sock, const void* bytes, size_t len)
{
    if (sock == NetSock::kInvalid)
        return false;
    std::string sealed;
    if (!m_session.Seal(kRecordType, bytes, len, sealed))
        return false;
    const uint32_t n = (uint32_t)sealed.size();
    char hdr[4] = {
        (char)(n & 0xff),
        (char)((n >> 8) & 0xff),
        (char)((n >> 16) & 0xff),
        (char)((n >> 24) & 0xff),
    };
    if (!SendAllRaw(sock, hdr, 4))
        return false;
    return SendAllRaw(sock, sealed.data(), sealed.size());
}

bool NetCtrlLink::SendMsg(NetSock::Handle sock, uint8_t type, const void* data, size_t len)
{
    std::string framed;
    NetFraming::Append(framed, type, data, len);
    return SendRecord(sock, framed.data(), framed.size());
}

bool NetCtrlLink::PollRecv(NetSock::Handle sock)
{
    if (sock == NetSock::kInvalid)
        return false;
    char tmp[4096];
    for (;;)
    {
        int n = NetSock::Recv(sock, tmp, sizeof(tmp));
        if (n > 0)
        {
            m_wire.append(tmp, tmp + n);
            continue;
        }
        if (n == 0 || n == -2)
            return false;
        break;
    }

    while (m_wire.size() >= 4)
    {
        uint32_t total = 0;
        std::memcpy(&total, m_wire.data(), 4);
        if (total < 40 || total > 1 << 20)
            return false;
        if (m_wire.size() < 4 + total)
            break;
        std::string plain;
        if (!m_session.Open(kRecordType, m_wire.data() + 4, total, plain))
            return false;
        m_plain.append(plain);
        m_wire.erase(0, 4 + total);
    }
    return true;
}

bool NetCtrlLink::TakeFrame(uint8_t& type, std::string& payload)
{
    if (m_plain.size() < 5)
        return false;
    uint32_t total = 0;
    std::memcpy(&total, m_plain.data(), 4);
    if (total < 1 || total > 1 << 20)
    {
        m_plain.clear();
        return false;
    }
    if (m_plain.size() < 4 + total)
        return false;
    type = (uint8_t)m_plain[4];
    payload.assign(m_plain.data() + 5, total - 1);
    m_plain.erase(0, 4 + total);
    return true;
}
