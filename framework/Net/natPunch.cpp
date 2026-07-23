#include "natPunch.h"
#include "stunClient.h"
#include "netTransport.h"
#include "Core/log.h"

#include <chrono>
#include <cstring>

namespace
{
    constexpr char kPunchMagic[4] = {'P', 'K', 'P', '1'};
    constexpr uint8_t kPunchReq = 1;
    constexpr uint8_t kPunchAck = 2;
    constexpr uint8_t kGameFrame = 3;

    uint64_t NowMs()
    {
        using namespace std::chrono;
        return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
}

NatPunch& NatPunch::GetInstance()
{
    static NatPunch instance;
    return instance;
}

void NatPunch::Reset()
{
    NetSock::Close(m_sock);
    m_local.clear();
    m_remote.clear();
    m_ready = false;
    m_peer = {};
    m_local_port = 0;
    m_last_send_ms = 0;
    std::memset(m_session, 0, sizeof(m_session));
}

bool NatPunch::Prepare(uint16_t preferred_port)
{
    Reset();
    m_sock = NetSock::UdpOpen();
    if (m_sock == NetSock::kInvalid)
        return false;
    if (!NetSock::UdpBind(m_sock, preferred_port))
    {
        NetSock::Close(m_sock);
        return false;
    }
    m_local_port = NetSock::LocalPort(m_sock);

    // Host-local candidate (LAN)
    m_local.push_back({"127.0.0.1", m_local_port});

    StunEndpoint st = StunClient::Query();
    if (st.ok)
    {
        m_local.push_back({st.ip, st.port});
        FW_CORE_INFO("STUN reflexive {0}:{1} (local udp {2})", st.ip, st.port, m_local_port);
    }
    else
        FW_CORE_WARN("STUN query failed - punch may only work on LAN");

    return true;
}

void NatPunch::SetSession(const uint8_t session_id[LobbyProto::kIdBytes])
{
    std::memcpy(m_session, session_id, LobbyProto::kIdBytes);
}

void NatPunch::SetRemoteCandidates(const std::vector<NatCandidate>& remotes)
{
    m_remote = remotes;
}

void NatPunch::SendPunches()
{
    if (m_sock == NetSock::kInvalid || m_remote.empty())
        return;
    uint8_t pkt[4 + 1 + LobbyProto::kIdBytes];
    std::memcpy(pkt, kPunchMagic, 4);
    pkt[4] = kPunchReq;
    std::memcpy(pkt + 5, m_session, LobbyProto::kIdBytes);
    for (const auto& r : m_remote)
    {
        if (r.ip.empty() || r.port == 0)
            continue;
        NetSock::UdpSendTo(m_sock, (const char*)pkt, sizeof(pkt), r.ip, r.port);
    }
}

bool NatPunch::HandleIncoming()
{
    if (m_sock == NetSock::kInvalid)
        return false;
    bool got_game = false;
    for (int i = 0; i < 32; ++i)
    {
        char buf[2048];
        std::string from;
        uint16_t fport = 0;
        int n = NetSock::UdpRecvFrom(m_sock, buf, sizeof(buf), &from, &fport);
        if (n <= 0)
            break;
        if (n >= 5 + LobbyProto::kIdBytes && std::memcmp(buf, kPunchMagic, 4) == 0)
        {
            uint8_t kind = (uint8_t)buf[4];
            if (std::memcmp(buf + 5, m_session, LobbyProto::kIdBytes) != 0)
                continue;
            if (kind == kPunchReq)
            {
                uint8_t ack[4 + 1 + LobbyProto::kIdBytes];
                std::memcpy(ack, kPunchMagic, 4);
                ack[4] = kPunchAck;
                std::memcpy(ack + 5, m_session, LobbyProto::kIdBytes);
                NetSock::UdpSendTo(m_sock, (const char*)ack, sizeof(ack), from, fport);
                if (!m_ready)
                {
                    m_peer = {from, fport};
                    m_ready = true;
                    FW_CORE_INFO("UDP punch established with {0}:{1}", from, fport);
                }
            }
            else if (kind == kPunchAck)
            {
                if (!m_ready)
                {
                    m_peer = {from, fport};
                    m_ready = true;
                    FW_CORE_INFO("UDP punch ACK from {0}:{1}", from, fport);
                }
            }
            else if (kind == kGameFrame && m_ready && n > 5 + LobbyProto::kIdBytes)
            {
                // Stored via RecvGame path - push into a side buffer?
                // For simplicity RecvGame reads raw; here we only handle punch.
                (void)got_game;
            }
        }
    }
    return m_ready;
}

bool NatPunch::Poll()
{
    if (m_sock == NetSock::kInvalid)
        return false;
    const uint64_t now = NowMs();
    if (!m_ready && now - m_last_send_ms >= 50)
    {
        SendPunches();
        m_last_send_ms = now;
    }
    HandleIncoming();
    return m_ready;
}

bool NatPunch::SendGame(const void* data, size_t len)
{
    if (!m_ready || m_sock == NetSock::kInvalid || !data || len == 0)
        return false;
    std::string pkt;
    pkt.resize(5 + LobbyProto::kIdBytes + len);
    std::memcpy(pkt.data(), kPunchMagic, 4);
    pkt[4] = (char)kGameFrame;
    std::memcpy(pkt.data() + 5, m_session, LobbyProto::kIdBytes);
    std::memcpy(pkt.data() + 5 + LobbyProto::kIdBytes, data, len);
    return NetSock::UdpSendTo(m_sock, pkt.data(), pkt.size(), m_peer.ip, m_peer.port) > 0;
}

bool NatPunch::RecvGame(std::string& out)
{
    out.clear();
    if (m_sock == NetSock::kInvalid)
        return false;
    char buf[4096];
    std::string from;
    uint16_t fport = 0;
    int n = NetSock::UdpRecvFrom(m_sock, buf, sizeof(buf), &from, &fport);
    if (n <= 0)
        return false;
    if (n >= 5 + LobbyProto::kIdBytes && std::memcmp(buf, kPunchMagic, 4) == 0)
    {
        uint8_t kind = (uint8_t)buf[4];
        if (std::memcmp(buf + 5, m_session, LobbyProto::kIdBytes) != 0)
            return false;
        if (kind == kPunchReq || kind == kPunchAck)
        {
            // Keep punch alive
            if (kind == kPunchReq)
            {
                uint8_t ack[4 + 1 + LobbyProto::kIdBytes];
                std::memcpy(ack, kPunchMagic, 4);
                ack[4] = kPunchAck;
                std::memcpy(ack + 5, m_session, LobbyProto::kIdBytes);
                NetSock::UdpSendTo(m_sock, (const char*)ack, sizeof(ack), from, fport);
            }
            if (!m_ready)
            {
                m_peer = {from, fport};
                m_ready = true;
            }
            return false;
        }
        if (kind == kGameFrame)
        {
            out.assign(buf + 5 + LobbyProto::kIdBytes, buf + n);
            return true;
        }
    }
    return false;
}
