#include "stunClient.h"
#include "netSockets.h"

#include <cstdio>
#include <cstring>
#include <chrono>
#include <random>

namespace
{
    constexpr uint32_t kStunMagic = 0x2112A442;
    constexpr uint16_t kStunBindingReq = 0x0001;
    constexpr uint16_t kStunBindingResp = 0x0101;
    constexpr uint16_t kAttrXorMapped = 0x0020;
    constexpr uint16_t kAttrMapped = 0x0001;

    uint16_t Rd16(const uint8_t* p)
    {
        return (uint16_t)((p[0] << 8) | p[1]);
    }

    uint32_t Rd32(const uint8_t* p)
    {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    }

    void Wr16(uint8_t* p, uint16_t v)
    {
        p[0] = (uint8_t)(v >> 8);
        p[1] = (uint8_t)(v & 0xff);
    }
}

namespace StunClient
{
    StunEndpoint Query(const std::string& stun_host, uint16_t stun_port, int timeout_ms)
    {
        StunEndpoint out;
        NetSock::Handle sock = NetSock::UdpOpen();
        if (sock == NetSock::kInvalid)
            return out;
        if (!NetSock::UdpBind(sock, 0))
        {
            NetSock::Close(sock);
            return out;
        }

        uint8_t txid[12];
        std::mt19937 rng{std::random_device{}()};
        for (int i = 0; i < 12; ++i)
            txid[i] = (uint8_t)(rng() & 0xff);

        uint8_t req[20]{};
        Wr16(req, kStunBindingReq);
        Wr16(req + 2, 0); // length
        req[4] = (uint8_t)(kStunMagic >> 24);
        req[5] = (uint8_t)(kStunMagic >> 16);
        req[6] = (uint8_t)(kStunMagic >> 8);
        req[7] = (uint8_t)(kStunMagic);
        std::memcpy(req + 8, txid, 12);

        if (NetSock::UdpSendTo(sock, (const char*)req, sizeof(req), stun_host, stun_port) <= 0)
        {
            NetSock::Close(sock);
            return out;
        }

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline)
        {
            char buf[512];
            int n = NetSock::UdpRecvFrom(sock, buf, sizeof(buf), nullptr, nullptr);
            if (n >= 20)
            {
                const uint8_t* p = (const uint8_t*)buf;
                uint16_t type = Rd16(p);
                uint16_t len = Rd16(p + 2);
                if (type == kStunBindingResp && n >= 20 + len && std::memcmp(p + 8, txid, 12) == 0)
                {
                    size_t off = 20;
                    while (off + 4 <= (size_t)n)
                    {
                        uint16_t at = Rd16(p + off);
                        uint16_t al = Rd16(p + off + 2);
                        size_t aval = off + 4;
                        if (aval + al > (size_t)n)
                            break;
                        if ((at == kAttrXorMapped || at == kAttrMapped) && al >= 8)
                        {
                            uint8_t family = p[aval + 1];
                            uint16_t port = Rd16(p + aval + 2);
                            uint32_t ip = Rd32(p + aval + 4);
                            if (at == kAttrXorMapped)
                            {
                                port ^= (uint16_t)(kStunMagic >> 16);
                                ip ^= kStunMagic;
                            }
                            if (family == 0x01)
                            {
                                char ipstr[32];
                                std::snprintf(ipstr, sizeof(ipstr), "%u.%u.%u.%u",
                                              (ip >> 24) & 0xff, (ip >> 16) & 0xff,
                                              (ip >> 8) & 0xff, ip & 0xff);
                                out.ip = ipstr;
                                out.port = port;
                                out.ok = true;
                                NetSock::Close(sock);
                                return out;
                            }
                        }
                        size_t pad = (4 - (al % 4)) % 4;
                        off = aval + al + pad;
                    }
                }
            }
            else if (n < 0)
                break;
            NetSock::SleepMs(5);
        }
        NetSock::Close(sock);
        return out;
    }
}
