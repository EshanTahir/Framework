#pragma once

#include "lobbyProtocol.h"
#include "netSockets.h"

#include <cstdint>
#include <string>
#include <vector>

// UDP hole-punch: STUN + candidate spam; Ready() when peer responds.

struct NatCandidate
{
    std::string ip;
    uint16_t port = 0;
};

class NatPunch
{
public:
    static NatPunch& GetInstance();

    void Reset();
    bool Prepare(uint16_t preferred_port = 0);
    void SetSession(const uint8_t session_id[LobbyProto::kIdBytes]);
    void SetRemoteCandidates(const std::vector<NatCandidate>& remotes);

    const std::vector<NatCandidate>& LocalCandidates() const { return m_local; }
    uint16_t LocalUdpPort() const { return m_local_port; }

    bool Poll();
    bool Ready() const { return m_ready; }
    const NatCandidate& Peer() const { return m_peer; }
    NetSock::Handle Socket() const { return m_sock; }

    bool SendGame(const void* data, size_t len);
    bool RecvGame(std::string& out);

private:
    NatPunch() = default;
    NetSock::Handle m_sock = NetSock::kInvalid;
    uint16_t m_local_port = 0;
    std::vector<NatCandidate> m_local;
    std::vector<NatCandidate> m_remote;
    uint8_t m_session[LobbyProto::kIdBytes]{};
    bool m_ready = false;
    NatCandidate m_peer{};
    uint64_t m_last_send_ms = 0;

    void SendPunches();
    bool HandleIncoming();
};
