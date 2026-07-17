#pragma once

// worker-thread async net I/O; sockets/ctrl only, main adopts results.

#include "lobbyProtocol.h"
#include "netSockets.h"

#include <cstdint>
#include <string>

class NetCtrlLink;

enum class NetAsyncKind : uint8_t
{
    TcpConnect = 1,
    LobbyCreateRoom,
    LobbyJoinRoom,
    RelayClientJoin,
    RelayHostJoin,
};

struct NetAsyncResult
{
    uint64_t id = 0;
    NetAsyncKind kind = NetAsyncKind::TcpConnect;
    bool ok = false;
    std::string err;
    NetSock::Handle sock = NetSock::kInvalid;
    LobbyProto::RoomInfoMsg room{};
    bool have_room = false;
    bool via_relay = false;
};

class NetAsync
{
public:
    static NetAsync& GetInstance();

    void EnsureStarted();
    void Shutdown();
    void Poll();

    uint64_t SubmitTcpConnect(std::string host, uint16_t port, int timeout_ms);
    uint64_t SubmitLobbyCreate(std::string lobby_host, uint16_t lobby_port,
                               LobbyProto::CreateRoomReq req);
    uint64_t SubmitLobbyJoin(std::string lobby_host, uint16_t lobby_port, std::string code,
                             uint64_t content_hash);
    uint64_t SubmitRelayClientJoin(LobbyProto::RoomInfoMsg room);
    // Host register on relay (same worker pattern as RelayClientJoin).
    uint64_t SubmitRelayHostJoin(LobbyProto::RoomInfoMsg room);

    void Cancel(uint64_t id);

    bool TryTake(uint64_t id, NetAsyncResult& out);
    // Relay joins may also return an established NetCtrlLink (moved out).
    bool TryTake(uint64_t id, NetAsyncResult& out, NetCtrlLink& ctrl_out, bool& has_ctrl);

private:
    NetAsync() = default;
    struct Impl;
    Impl* m = nullptr;
};
