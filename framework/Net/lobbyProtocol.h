#pragma once

// lobby/relay wire formats (length-prefixed like NetMsg). ports 41300/41301.

#include <cstdint>
#include <cstring>
#include <string>

namespace LobbyProto
{
    constexpr uint16_t kDefaultLobbyPort = 41300;
    constexpr uint16_t kDefaultRelayPort = 41301;
    constexpr uint8_t kProto = 2; // v2: RoomInfo includes room_key[32]
    constexpr int kCodeLen = 6;
    constexpr int kCodeBuf = 8;
    constexpr int kHostBuf = 64;
    constexpr int kIdBytes = 16;
    constexpr int kKeyBytes = 32;

    enum Type : uint8_t
    {
        CreateRoom = 1,
        JoinRoom = 2,
        Heartbeat = 3,
        CloseRoom = 4,
        RegisterDedicated = 5,
        Ok = 10,
        Err = 11,
        RoomInfo = 12,
    };

    enum RoomKind : uint8_t
    {
        ListenRelay = 0, // player host; joiners use relay (and optional LAN direct)
        Dedicated = 1,   // headless / dedicated process; joiners dial direct_hint
    };

    enum ErrCode : uint8_t
    {
        None = 0,
        BadProto = 1,
        NotFound = 2,
        Full = 3,
        HashMismatch = 4,
        Unauthorized = 5,
        Internal = 6,
    };

#pragma pack(push, 1)
    struct CreateRoomReq
    {
        uint8_t proto = kProto;
        uint8_t kind = ListenRelay;
        uint64_t content_hash = 0;
        uint16_t game_port = 41234; // LAN listen / dedicated listen
        char advertise_host[kHostBuf]{}; // optional LAN IP hint for same-subnet joins
    };

    struct JoinRoomReq
    {
        uint8_t proto = kProto;
        char code[kCodeBuf]{};
        uint64_t content_hash = 0; // 0 = skip check
    };

    struct RegisterDedicatedReq
    {
        uint8_t proto = kProto;
        uint64_t content_hash = 0;
        uint16_t game_port = 41234;
        char advertise_host[kHostBuf]{}; // public/VPS address clients should dial
        uint8_t max_clients = 8;
    };

    struct RoomInfoMsg
    {
        uint8_t proto = kProto;
        uint8_t kind = ListenRelay;
        char code[kCodeBuf]{};
        char relay_host[kHostBuf]{};
        uint16_t relay_port = kDefaultRelayPort;
        uint8_t session_id[kIdBytes]{};
        uint8_t host_token[kIdBytes]{};
        uint8_t client_token[kIdBytes]{};
        char direct_host[kHostBuf]{}; // LAN or dedicated public host
        uint16_t direct_port = 0;
        uint64_t content_hash = 0;
        uint8_t is_host = 0; // 1 if this peer created / owns the room
        // Shared AEAD key for NetHost↔NetClient frames (lobby-issued).
        // All-zero = encryption disabled (legacy / LAN without lobby).
        uint8_t room_key[kKeyBytes]{};
    };

    struct ErrMsg
    {
        uint8_t code = Internal;
        char text[48]{};
    };
#pragma pack(pop)
}

namespace RelayProto
{
    constexpr uint8_t kProto = 1;

    enum Type : uint8_t
    {
        Register = 1,   // → Ok / Err
        Ok = 2,
        Err = 3,
        ClientOpen = 10,  // host←relay: u16 client_id
        ClientClose = 11, // host←relay: u16 client_id
        ToHost = 12,      // host←relay: u16 id + payload (raw NetMsg frame bytes)
        ToClient = 13,    // host→relay: u16 id + payload
    };

    enum Role : uint8_t
    {
        Host = 1,
        Client = 2,
    };

#pragma pack(push, 1)
    struct RegisterReq
    {
        uint8_t proto = kProto;
        uint8_t role = Client;
        uint8_t session_id[LobbyProto::kIdBytes]{};
        uint8_t token[LobbyProto::kIdBytes]{}; // host_token or client_token by role
        uint8_t peer_token[LobbyProto::kIdBytes]{}; // host role: expected client_token
    };

    struct ErrMsg
    {
        uint8_t code = 1;
        char text[48]{};
    };
#pragma pack(pop)
}

namespace NetFraming
{
    inline void Append(std::string& out, uint8_t type, const void* data, size_t len)
    {
        const uint32_t total = (uint32_t)(1 + len);
        out.push_back((char)(total & 0xff));
        out.push_back((char)((total >> 8) & 0xff));
        out.push_back((char)((total >> 16) & 0xff));
        out.push_back((char)((total >> 24) & 0xff));
        out.push_back((char)type);
        if (len && data)
            out.append(static_cast<const char*>(data), len);
    }
}
