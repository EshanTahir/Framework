#pragma once

#include "lobbyProtocol.h"
#include "netSockets.h"
#include "netCtrlLink.h"

#include <cstdint>
#include <string>
#include <vector>

// Host side of the relay multiplex. Control plane encrypted via NetCtrlLink.

class RelayHostLink
{
public:
    static RelayHostLink& GetInstance();

    bool Connect(const std::string& host, uint16_t port,
                 const uint8_t session_id[LobbyProto::kIdBytes],
                 const uint8_t host_token[LobbyProto::kIdBytes],
                 const uint8_t client_token[LobbyProto::kIdBytes]);
    // Take ownership of an already-registered relay host socket + ctrl session.
    bool Adopt(NetSock::Handle sock, NetCtrlLink&& ctrl);
    void Close();
    bool IsConnected() const { return m_sock != NetSock::kInvalid; }

    void Poll();

    struct PeerOpen { uint16_t id = 0; };
    struct PeerClose { uint16_t id = 0; };
    struct DataEvent { uint16_t id = 0; std::string bytes; };

    std::vector<PeerOpen> TakeOpens();
    std::vector<PeerClose> TakeCloses();
    std::vector<DataEvent> TakeData();

    bool SendToClient(uint16_t id, const void* data, size_t len);

private:
    RelayHostLink() = default;
    uint64_t m_sock = NetSock::kInvalid;
    NetCtrlLink m_ctrl;
    std::vector<PeerOpen> m_opens;
    std::vector<PeerClose> m_closes;
    std::vector<DataEvent> m_data;
    void HandleFrames();
};
