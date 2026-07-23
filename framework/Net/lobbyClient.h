#pragma once

#include "lobbyProtocol.h"
#include "netSockets.h"
#include "netCtrlLink.h"

#include <cstdint>
#include <functional>
#include <string>

// Talks to LobbyServer (room codes). Control plane uses NetCtrlLink (X25519+AEAD).

class LobbyClient
{
public:
    static LobbyClient& GetInstance();

    void SetLobbyEndpoint(const std::string& host, uint16_t port = LobbyProto::kDefaultLobbyPort);
    const std::string& LobbyHost() const { return m_host; }
    uint16_t LobbyPort() const { return m_port; }

    bool Connected() const { return m_sock != ~0ull; }
    bool Connect();
    void Close();
    void Poll();

    bool CreateRoom(const LobbyProto::CreateRoomReq& req, LobbyProto::RoomInfoMsg& out, std::string& err);
    bool JoinRoom(const char* code, uint64_t content_hash, LobbyProto::RoomInfoMsg& out, std::string& err);
    bool RegisterDedicated(const LobbyProto::RegisterDedicatedReq& req, LobbyProto::RoomInfoMsg& out, std::string& err);
    bool Heartbeat();
    bool CloseRoom();

    // Async lobby RPC (ephemeral worker connection). Poll via Poll() / OpStatus.
    enum class OpStatus : uint8_t
    {
        Idle = 0,
        Busy,
        Ready,
        Failed
    };
    bool BeginCreateRoom(const LobbyProto::CreateRoomReq& req);
    bool BeginJoinRoom(const char* code, uint64_t content_hash);
    void CancelOp();
    OpStatus GetOpStatus() const { return m_op_status; }
    const char* OpStatusName() const;
    const std::string& OpError() const { return m_op_err; }

    const LobbyProto::RoomInfoMsg& LastRoom() const { return m_room; }
    bool HasRoom() const { return m_have_room; }

private:
    LobbyClient() = default;
    uint64_t m_sock = ~0ull;
    NetCtrlLink m_ctrl;
    std::string m_host = "127.0.0.1";
    uint16_t m_port = LobbyProto::kDefaultLobbyPort;
    LobbyProto::RoomInfoMsg m_room{};
    bool m_have_room = false;

    OpStatus m_op_status = OpStatus::Idle;
    uint64_t m_op_job = 0;
    std::string m_op_err;

    void PollOp();
    bool SendMsg(uint8_t type, const void* data, size_t len);
    bool WaitMsg(uint8_t expect_ok_type, void* out, size_t out_len, std::string& err, int timeout_ms = 3000);
};
