#pragma once

#include "Match/inputFrame.h"
#include "Match/matchEvents.h"
#include "Net/netTransport.h"
#include "Net/netRateLimit.h"
#include "Net/lobbyProtocol.h"
#include "Net/netInputGuard.h"
#include "Net/netCtrlLink.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

namespace NetMsg
{
    constexpr uint8_t kProto = 1;
    constexpr uint16_t kDefaultPort = 41234;

    enum Type : uint8_t
    {
        Hello = 1,
        Welcome = 2,
        StartMatch = 3,
        Abort = 4,
        Input = 5,
        Snapshot = 6,
        Ping = 7,
        HashReject = 8,
        PeerLeft = 9,
        Pong = 10,
        Candidates = 11, // TCP: exchange UDP candidates for punch
        Event = 12,      // one-shot MatchEventWire (sound / cue)
        StateChecksum = 13, // rollback panic: { tick, hash }
        StateBlob = 14,     // rollback panic: tick + opaque bytes
        StateBlobReq = 15,  // request StateBlob for tick
    };
}

#pragma pack(push, 1)
struct NetWelcome
{
    uint8_t proto = NetMsg::kProto;
    int32_t assigned_slot = 0;
    uint64_t seed = 0;
    uint64_t content_hash = 0;
};

struct NetSnapshotHeader
{
    uint64_t tick = 0;
    uint8_t slot_count = 0;
    uint16_t channel_bytes = 0;
};

struct NetPawnState
{
    int32_t slot = 0;
    float x = 0, y = 0, vx = 0, vy = 0;
    uint32_t flags = 0;
};

struct NetCandidateMsg
{
    uint16_t udp_port = 0;
    char ip0[64]{};
    char ip1[64]{}; // optional second (STUN)
};

struct NetStateChecksum
{
    uint64_t tick = 0;
    uint64_t hash = 0;
};

struct NetStateBlobHeader
{
    uint64_t tick = 0;
    uint32_t byte_count = 0;
};
#pragma pack(pop)

namespace NetPawnFlags
{
    constexpr uint32_t Alive = 1u << 0;
    constexpr uint32_t ForceSnap = 1u << 1;
}

class NetHost
{
public:
    static NetHost& GetInstance();

    bool Listen(uint16_t port = NetMsg::kDefaultPort);
    bool AttachRelay(const LobbyProto::RoomInfoMsg& room);
    // Async attach - poll with AttachRelayStatus / AttachRelayStatusName.
    enum class AttachStatus : uint8_t
    {
        Idle = 0,
        Connecting,
        Ready,
        Failed
    };
    bool BeginAttachRelay(const LobbyProto::RoomInfoMsg& room);
    void CancelAttachRelay();
    AttachStatus GetAttachStatus() const { return m_attach_status; }
    const char* AttachRelayStatusName() const;
    const std::string& AttachRelayError() const { return m_attach_err; }
    // Prepare local UDP + STUN; call before/with online host.
    bool PrepareUdpPunch(const uint8_t session_id[LobbyProto::kIdBytes], uint16_t udp_port = 0);
    void Close();
    bool IsListening() const { return m_listen != ~0ull || m_relay; }
    bool UsingRelay() const { return m_relay; }
    bool UdpReady() const;
    int ClientCount() const { return (int)m_clients.size(); }

    void Poll();
    void Broadcast(uint8_t type, const void* data, size_t len);
    // Returns false if send failed (client queued for drop - do not SetClientSlot).
    bool SendTo(int client_index, uint8_t type, const void* data, size_t len);
    void SetClientSlot(int client_index, int slot);
    int GetClientSlot(int client_index) const;

    std::vector<InputFrame> TakeInputs();
    std::vector<uint64_t> TakeStateBlobReqs();
    std::vector<int> TakeDroppedSlots(); // deferred on_disconnect slots

    std::function<void(int /*client*/, const NetWelcome& /*req*/)> on_hello;
    std::function<void(int /*slot*/)> on_disconnect;

    void SetMaxMessagesPerSec(int n) { m_msg_limit = n; }
    void FlushDisconnectCallbacks();
    void Kick(int client_index);

private:
    NetHost() = default;
    uint64_t m_listen = ~0ull;
    bool m_relay = false;
    struct Client
    {
        uint64_t sock = ~0ull;
        uint16_t relay_id = 0;
        std::string buf;
        int slot = -1;
        NetRateLimit rate;
        NetInputGuard::ClientState guard;
        bool udp_ok = false;
        std::string udp_ip;
        uint16_t udp_port = 0;
    };
    std::vector<Client> m_clients;
    std::unordered_map<uint16_t, size_t> m_relay_index;
    std::vector<InputFrame> m_inputs;
    std::vector<uint64_t> m_blob_reqs;
    std::vector<int> m_pending_disconnect_slots;
    std::vector<size_t> m_pending_drop_indices;
    int m_msg_limit = 180;
    bool m_dropping = false;

    AttachStatus m_attach_status = AttachStatus::Idle;
    uint64_t m_attach_job = 0;
    std::string m_attach_err;
    LobbyProto::RoomInfoMsg m_attach_room{};

    void PollAttachRelay();
    void QueueDrop(size_t index);
    void ProcessQueuedDrops();
    void HandleClient(size_t index);
    void DropClientNow(size_t index);
    void PollRelay();
    void PollUdp();
    bool SendRaw(Client& c, const std::string& msg);
    void RebuildRelayIndex();
};

class NetClient
{
public:
    static NetClient& GetInstance();

    bool Connect(const std::string& host, uint16_t port = NetMsg::kDefaultPort);
    bool ConnectTimeout(const std::string& host, uint16_t port, int timeout_ms);
    bool ConnectRelay(const LobbyProto::RoomInfoMsg& room);
    // Online join: STUN + punch toward host candidates, else relay. TCP control optional.
    bool ConnectOnline(const LobbyProto::RoomInfoMsg& room);

    // Async (worker-thread) connect - never blocks the game thread.
    enum class ConnectStatus : uint8_t
    {
        Idle = 0,
        Connecting,
        Ready,
        Failed
    };
    bool BeginConnect(const std::string& host, uint16_t port = NetMsg::kDefaultPort,
                      int timeout_ms = 3000);
    // After LobbyClient has a room: async direct/relay TCP (+ relay register on worker).
    bool BeginJoinFromLobby();
    void CancelConnect();
    ConnectStatus GetConnectStatus() const { return m_connect_status; }
    const char* ConnectStatusName() const;
    const std::string& ConnectError() const { return m_connect_err; }

    void Close();
    bool IsConnected() const { return m_sock != ~0ull || m_udp_only; }
    bool UdpReady() const;

    void Poll();
    void Send(uint8_t type, const void* data, size_t len);

    bool HasWelcome() const { return m_got_welcome; }
    const NetWelcome& Welcome() const { return m_welcome; }
    bool HashRejected() const { return m_hash_rejected; }
    bool StartRequested() const { return m_start; }
    void ClearStart() { m_start = false; }
    bool Aborted() const { return m_aborted; }

    std::vector<NetPawnState> TakePawns();
    std::string TakeChannelBlob();
    std::vector<MatchEventWire> TakeEvents();
    std::vector<InputFrame> TakePeerInputs(); // rollback: inputs from host/peers
    std::vector<NetStateChecksum> TakeChecksums();
    std::vector<std::pair<uint64_t, std::string>> TakeStateBlobs(); // tick, bytes
    std::vector<uint64_t> TakeStateBlobReqs();
    std::vector<int32_t> TakePeerLeft();
    uint64_t LastSnapshotTick() const { return m_last_snap_tick; }
    int64_t LastRttMs() const { return m_rtt_ms; }

private:
    NetClient() = default;
    uint64_t m_sock = ~0ull;
    std::string m_buf;
    NetCtrlLink m_ctrl;
    NetWelcome m_welcome{};
    bool m_got_welcome = false;
    bool m_hash_rejected = false;
    bool m_start = false;
    bool m_aborted = false;
    bool m_via_relay = false;
    bool m_udp_only = false;
    std::vector<NetPawnState> m_pawns;
    std::string m_channels;
    std::vector<MatchEventWire> m_events;
    std::vector<InputFrame> m_peer_inputs;
    std::vector<NetStateChecksum> m_checksums;
    std::vector<std::pair<uint64_t, std::string>> m_state_blobs;
    std::vector<uint64_t> m_blob_reqs;
    std::vector<int32_t> m_peer_left;
    uint64_t m_last_snap_tick = 0;
    uint64_t m_ping_sent_ms = 0;
    int64_t m_rtt_ms = -1;

    ConnectStatus m_connect_status = ConnectStatus::Idle;
    uint64_t m_connect_job = 0;
    std::string m_connect_err;
    bool m_connect_send_hello = false;
    LobbyProto::RoomInfoMsg m_connect_room{};
    bool m_connect_expect_room = false;

    void PollConnect();
    void ArmClientSession();
    void HandleBuffer();
    bool WaitRelayOk(int timeout_ms);
    void PollUdpGameplay();
};
