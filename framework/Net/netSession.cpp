#include "netSession.h"
#include "netSockets.h"
#include "netCrypto.h"
#include "netInputGuard.h"
#include "netAsync.h"
#include "lobbyClient.h"
#include "relayHostLink.h"
#include "natPunch.h"
#include "Match/matchSession.h"
#include "Match/simClock.h"
#include "Core/log.h"

#include <cstring>
#include <cstddef>
#include <chrono>
#include <algorithm>
#include <functional>

namespace
{
    uint64_t NowMs()
    {
        using namespace std::chrono;
        return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    void AppendMsg(std::string& out, uint8_t type, const void* data, size_t len)
    {
        std::string payload;
        if (!NetCrypto::GetInstance().Seal(type, data, len, payload))
            return;
        NetFraming::Append(out, type, payload.data(), payload.size());
    }

    bool ParseOneFrame(std::string& buf, uint8_t& type, const char*& payload, size_t& plen, size_t& frame_bytes)
    {
        if (buf.size() < 5)
            return false;
        uint32_t total = 0;
        std::memcpy(&total, buf.data(), 4);
        if (total < 1 || total > 1 << 20)
        {
            buf.clear();
            return false;
        }
        if (buf.size() < 4 + total)
            return false;
        type = (uint8_t)buf[4];
        payload = buf.data() + 5;
        plen = total - 1;
        frame_bytes = 4 + total;
        return true;
    }

    bool OpenFramePayload(uint8_t type, const char* payload, size_t plen, std::string& plain)
    {
        return NetCrypto::GetInstance().Open(type, payload, plen, plain);
    }

    bool GuardInput(NetInputGuard::ClientState& guard, int assigned_slot, InputFrame& f,
                    std::vector<InputFrame>& out, bool* should_kick)
    {
        bool kick = false;
        std::string why;
        if (!NetInputGuard::Accept(guard, f, assigned_slot, SimClock::GetInstance().Tick(), NowMs(), &kick, &why))
        {
            FW_CORE_WARN("NetHost: drop input ({0}) strikes={1}", why, guard.strikes);
            if (should_kick)
                *should_kick = kick;
            return false;
        }
        if (assigned_slot >= 0)
            f.slot = assigned_slot;
        out.push_back(f);
        if (should_kick)
            *should_kick = false;
        return true;
    }
}

NetHost& NetHost::GetInstance()
{
    static NetHost instance;
    return instance;
}

void NetHost::RebuildRelayIndex()
{
    m_relay_index.clear();
    for (size_t i = 0; i < m_clients.size(); ++i)
        if (m_clients[i].relay_id)
            m_relay_index[m_clients[i].relay_id] = i;
}

bool NetHost::Listen(uint16_t port)
{
    if (m_listen != NetSock::kInvalid)
        NetSock::Close(m_listen);
    m_listen = NetSock::TcpListen(port, 8);
    if (m_listen == NetSock::kInvalid)
        return false;
    FW_CORE_INFO("NetHost listening on port {0}", port);
    return true;
}

bool NetHost::PrepareUdpPunch(const uint8_t session_id[LobbyProto::kIdBytes], uint16_t udp_port)
{
    auto& punch = NatPunch::GetInstance();
    if (!punch.Prepare(udp_port))
        return false;
    punch.SetSession(session_id);
    return true;
}

bool NetHost::AttachRelay(const LobbyProto::RoomInfoMsg& room)
{
    auto& link = RelayHostLink::GetInstance();
    if (!link.Connect(room.relay_host, room.relay_port, room.session_id, room.host_token, room.client_token))
    {
        FW_CORE_WARN("NetHost AttachRelay failed");
        return false;
    }
    NetCrypto::GetInstance().SetRoomKey(room.room_key);
    m_relay = true;
    PrepareUdpPunch(room.session_id, 0);
    FW_CORE_INFO("NetHost relay attached session code {0} crypto={1}", room.code,
                 NetCrypto::GetInstance().Active() ? 1 : 0);
    return true;
}

const char* NetHost::AttachRelayStatusName() const
{
    switch (m_attach_status)
    {
    case AttachStatus::Connecting:
        return "connecting";
    case AttachStatus::Ready:
        return "ready";
    case AttachStatus::Failed:
        return "failed";
    default:
        return "idle";
    }
}

void NetHost::CancelAttachRelay()
{
    if (m_attach_job)
    {
        NetAsync::GetInstance().Cancel(m_attach_job);
        m_attach_job = 0;
    }
    if (m_attach_status == AttachStatus::Connecting)
    {
        m_attach_status = AttachStatus::Idle;
        m_attach_err.clear();
    }
}

bool NetHost::BeginAttachRelay(const LobbyProto::RoomInfoMsg& room)
{
    CancelAttachRelay();
    m_attach_room = room;
    m_attach_err.clear();
    m_attach_job = NetAsync::GetInstance().SubmitRelayHostJoin(room);
    m_attach_status = AttachStatus::Connecting;
    FW_CORE_INFO("NetHost async attach relay room {0}", room.code);
    return m_attach_job != 0;
}

void NetHost::PollAttachRelay()
{
    if (m_attach_status != AttachStatus::Connecting || !m_attach_job)
        return;
    NetAsyncResult r;
    NetCtrlLink ctrl;
    bool has_ctrl = false;
    if (!NetAsync::GetInstance().TryTake(m_attach_job, r, ctrl, has_ctrl))
        return;
    m_attach_job = 0;
    if (!r.ok || r.sock == NetSock::kInvalid || !has_ctrl)
    {
        m_attach_status = AttachStatus::Failed;
        m_attach_err = r.err.empty() ? "relay attach failed" : r.err;
        if (r.sock != NetSock::kInvalid)
            NetSock::Close(r.sock);
        return;
    }
    if (!RelayHostLink::GetInstance().Adopt(r.sock, std::move(ctrl)))
    {
        m_attach_status = AttachStatus::Failed;
        m_attach_err = "relay adopt failed";
        NetSock::Close(r.sock);
        return;
    }
    const LobbyProto::RoomInfoMsg& room = r.have_room ? r.room : m_attach_room;
    NetCrypto::GetInstance().SetRoomKey(room.room_key);
    m_relay = true;
    PrepareUdpPunch(room.session_id, 0);
    m_attach_status = AttachStatus::Ready;
    m_attach_err.clear();
    FW_CORE_INFO("NetHost async relay attached code {0}", room.code);
}

bool NetHost::UdpReady() const
{
    return NatPunch::GetInstance().Ready();
}

void NetHost::Close()
{
    CancelAttachRelay();
    for (auto& c : m_clients)
        if (c.sock != NetSock::kInvalid)
            NetSock::Close(c.sock);
    m_clients.clear();
    m_relay_index.clear();
    NetSock::Close(m_listen);
    RelayHostLink::GetInstance().Close();
    NatPunch::GetInstance().Reset();
    NetCrypto::GetInstance().Clear();
    m_relay = false;
    m_attach_status = AttachStatus::Idle;
    m_attach_err.clear();
    m_inputs.clear();
    m_blob_reqs.clear();
    m_pending_disconnect_slots.clear();
    m_pending_drop_indices.clear();
}

bool NetHost::SendRaw(Client& c, const std::string& msg)
{
    // Prefer UDP for Input/Snapshot when punched to this peer
    if (c.udp_ok && NatPunch::GetInstance().Ready())
    {
        // Only when msg is Input/Snapshot - caller uses Broadcast path; for per-client
        // still OK to send full framed blob over UDP game channel.
        if (NatPunch::GetInstance().SendGame(msg.data(), msg.size()))
            return true;
    }
    if (c.relay_id != 0)
        return RelayHostLink::GetInstance().SendToClient(c.relay_id, msg.data(), msg.size());
    if (c.sock == NetSock::kInvalid)
        return false;
    return NetSock::SendAll(c.sock, msg.data(), msg.size());
}

void NetHost::QueueDrop(size_t index)
{
    if (index < m_clients.size())
        m_pending_drop_indices.push_back(index);
}

void NetHost::DropClientNow(size_t index)
{
    if (index >= m_clients.size())
        return;
    const int slot = m_clients[index].slot;
    const uint16_t rid = m_clients[index].relay_id;
    if (m_clients[index].sock != NetSock::kInvalid)
        NetSock::Close(m_clients[index].sock);
    m_clients.erase(m_clients.begin() + (ptrdiff_t)index);
    RebuildRelayIndex();
    if (slot >= 0)
    {
        m_pending_disconnect_slots.push_back(slot);
        // PeerLeft without recursive DropClient
        std::string msg;
        const int32_t left = (int32_t)slot;
        AppendMsg(msg, NetMsg::PeerLeft, &left, sizeof(left));
        for (size_t i = 0; i < m_clients.size(); ++i)
        {
            if (!SendRaw(m_clients[i], msg))
                m_pending_drop_indices.push_back(i);
        }
    }
    FW_CORE_INFO("NetHost: client dropped (slot {0}, {1} remain)", slot, (int)m_clients.size());
}

void NetHost::ProcessQueuedDrops()
{
    if (m_pending_drop_indices.empty())
        return;
    // Sort descending so erase order is safe
    std::sort(m_pending_drop_indices.begin(), m_pending_drop_indices.end(), std::greater<>());
    m_pending_drop_indices.erase(std::unique(m_pending_drop_indices.begin(), m_pending_drop_indices.end()),
                                 m_pending_drop_indices.end());
    m_dropping = true;
    for (size_t idx : m_pending_drop_indices)
        DropClientNow(idx);
    m_pending_drop_indices.clear();
    m_dropping = false;
}

void NetHost::FlushDisconnectCallbacks()
{
    ProcessQueuedDrops();
    if (!on_disconnect)
    {
        m_pending_disconnect_slots.clear();
        return;
    }
    std::vector<int> slots;
    slots.swap(m_pending_disconnect_slots);
    for (int s : slots)
        on_disconnect(s);
}

void NetHost::Kick(int client_index)
{
    if (client_index < 0 || client_index >= (int)m_clients.size())
        return;
    QueueDrop((size_t)client_index);
    ProcessQueuedDrops();
    FlushDisconnectCallbacks();
}

std::vector<int> NetHost::TakeDroppedSlots()
{
    ProcessQueuedDrops();
    std::vector<int> out;
    out.swap(m_pending_disconnect_slots);
    return out;
}

void NetHost::Broadcast(uint8_t type, const void* data, size_t len)
{
    std::string msg;
    AppendMsg(msg, type, data, len);
    // UDP broadcast when punched: send once to each udp peer + TCP/relay for others
    if ((type == NetMsg::Snapshot || type == NetMsg::Input) && NatPunch::GetInstance().Ready())
    {
        // one punched peer for now; UDP once + TCP for others.
        NatPunch::GetInstance().SendGame(msg.data(), msg.size());
    }
    for (size_t i = 0; i < m_clients.size(); ++i)
    {
        if (m_clients[i].udp_ok && (type == NetMsg::Snapshot || type == NetMsg::Input))
            continue; // already sent via UDP
        if (!SendRaw(m_clients[i], msg))
            QueueDrop(i);
    }
    ProcessQueuedDrops();
}

bool NetHost::SendTo(int client_index, uint8_t type, const void* data, size_t len)
{
    if (client_index < 0 || client_index >= (int)m_clients.size())
        return false;
    std::string msg;
    AppendMsg(msg, type, data, len);
    if (!SendRaw(m_clients[(size_t)client_index], msg))
    {
        QueueDrop((size_t)client_index);
        ProcessQueuedDrops();
        return false;
    }
    return true;
}

void NetHost::SetClientSlot(int client_index, int slot)
{
    if (client_index < 0 || client_index >= (int)m_clients.size())
        return;
    m_clients[(size_t)client_index].slot = slot;
}

int NetHost::GetClientSlot(int client_index) const
{
    if (client_index < 0 || client_index >= (int)m_clients.size())
        return -1;
    return m_clients[(size_t)client_index].slot;
}

std::vector<InputFrame> NetHost::TakeInputs()
{
    std::vector<InputFrame> out;
    out.swap(m_inputs);
    return out;
}

std::vector<uint64_t> NetHost::TakeStateBlobReqs()
{
    std::vector<uint64_t> out;
    out.swap(m_blob_reqs);
    return out;
}

void NetHost::HandleClient(size_t index)
{
    if (index >= m_clients.size())
        return;
    if (m_clients[index].sock == NetSock::kInvalid)
        return;

    char tmp[2048];
    for (;;)
    {
        int n = NetSock::Recv(m_clients[index].sock, tmp, sizeof(tmp));
        if (n > 0)
        {
            m_clients[index].buf.append(tmp, tmp + n);
            continue;
        }
        if (n == 0 || n == -2)
        {
            QueueDrop(index);
            return;
        }
        break;
    }

    const uint64_t now = NowMs();
    while (true)
    {
        if (index >= m_clients.size())
            return;
        uint8_t type = 0;
        const char* payload = nullptr;
        size_t plen = 0, frame = 0;
        if (!ParseOneFrame(m_clients[index].buf, type, payload, plen, frame))
            break;
        if (!m_clients[index].rate.Allow(now))
        {
            QueueDrop(index);
            return;
        }
        // Copy + decrypt payload before any drop
        std::string pay;
        if (!OpenFramePayload(type, payload, plen, pay))
        {
            m_clients[index].buf.erase(0, frame);
            QueueDrop(index);
            return;
        }
        m_clients[index].buf.erase(0, frame);

        if (type == NetMsg::Hello && pay.size() >= sizeof(NetWelcome))
        {
            NetWelcome req{};
            std::memcpy(&req, pay.data(), sizeof(req));
            if (on_hello)
                on_hello((int)index, req);
            if (index >= m_clients.size())
                return;
        }
        else if (type == NetMsg::Input && pay.size() >= sizeof(InputFrame))
        {
            InputFrame f{};
            std::memcpy(&f, pay.data(), sizeof(f));
            bool kick = false;
            if (!GuardInput(m_clients[index].guard, m_clients[index].slot, f, m_inputs, &kick) && kick)
            {
                QueueDrop(index);
                return;
            }
        }
        else if (type == NetMsg::Ping)
        {
            SendTo((int)index, NetMsg::Pong, pay.data(), pay.size());
            if (index >= m_clients.size())
                return;
        }
        else if (type == NetMsg::StateBlobReq && pay.size() >= sizeof(uint64_t))
        {
            uint64_t tick = 0;
            std::memcpy(&tick, pay.data(), sizeof(tick));
            m_blob_reqs.push_back(tick);
        }
        else if (type == NetMsg::Candidates && pay.size() >= sizeof(NetCandidateMsg))
        {
            NetCandidateMsg cm{};
            std::memcpy(&cm, pay.data(), sizeof(cm));
            m_clients[index].udp_ip = cm.ip0[0] ? cm.ip0 : cm.ip1;
            m_clients[index].udp_port = cm.udp_port;
            std::vector<NatCandidate> rem;
            if (cm.ip0[0])
                rem.push_back({cm.ip0, cm.udp_port});
            if (cm.ip1[0])
                rem.push_back({cm.ip1, cm.udp_port});
            NatPunch::GetInstance().SetRemoteCandidates(rem);
            // Reply with our candidates
            NetCandidateMsg ours{};
            ours.udp_port = NatPunch::GetInstance().LocalUdpPort();
            const auto& locs = NatPunch::GetInstance().LocalCandidates();
            if (!locs.empty())
                std::strncpy(ours.ip0, locs[0].ip.c_str(), sizeof(ours.ip0) - 1);
            if (locs.size() > 1)
                std::strncpy(ours.ip1, locs[1].ip.c_str(), sizeof(ours.ip1) - 1);
            SendTo((int)index, NetMsg::Candidates, &ours, sizeof(ours));
        }
    }
}

void NetHost::PollUdp()
{
    auto& punch = NatPunch::GetInstance();
    punch.Poll();
    std::string frame;
    while (punch.RecvGame(frame))
    {
        // Incoming UDP NetMsg frame(s)
        std::string buf = frame;
        uint8_t type = 0;
        const char* payload = nullptr;
        size_t plen = 0, nbytes = 0;
        if (!ParseOneFrame(buf, type, payload, plen, nbytes))
            continue;
        std::string pay;
        if (!OpenFramePayload(type, payload, plen, pay))
            continue;
        if (type == NetMsg::Input && pay.size() >= sizeof(InputFrame))
        {
            InputFrame f{};
            std::memcpy(&f, pay.data(), sizeof(f));
            // UDP path: assign to first punched client with matching guard loosely
            if (!m_clients.empty())
            {
                bool kick = false;
                size_t idx = 0;
                for (size_t i = 0; i < m_clients.size(); ++i)
                    if (m_clients[i].udp_ok)
                    {
                        idx = i;
                        break;
                    }
                if (!GuardInput(m_clients[idx].guard, m_clients[idx].slot, f, m_inputs, &kick) && kick)
                    QueueDrop(idx);
            }
        }
        // Mark matching client udp_ok
        for (auto& c : m_clients)
        {
            if (c.udp_port && punch.Ready() && punch.Peer().port == c.udp_port)
                c.udp_ok = true;
        }
    }
    if (punch.Ready())
    {
        for (auto& c : m_clients)
            if (c.udp_port)
                c.udp_ok = true;
    }
}

void NetHost::PollRelay()
{
    if (!m_relay)
        return;
    auto& link = RelayHostLink::GetInstance();
    link.Poll();
    if (!link.IsConnected())
    {
        for (size_t i = 0; i < m_clients.size(); ++i)
            if (m_clients[i].relay_id)
                QueueDrop(i);
        m_relay = false;
        ProcessQueuedDrops();
        return;
    }

    for (auto& ev : link.TakeOpens())
    {
        Client c;
        c.relay_id = ev.id;
        c.rate.SetLimit(m_msg_limit);
        m_clients.push_back(std::move(c));
        RebuildRelayIndex();
    }
    for (auto& ev : link.TakeCloses())
    {
        auto it = m_relay_index.find(ev.id);
        if (it != m_relay_index.end())
            QueueDrop(it->second);
    }
    ProcessQueuedDrops();

    for (auto& ev : link.TakeData())
    {
        auto it = m_relay_index.find(ev.id);
        if (it == m_relay_index.end())
            continue;
        size_t index = it->second;
        if (index >= m_clients.size())
            continue;
        m_clients[index].buf.append(ev.bytes);
        // Re-enter HandleClient-like parse without socket
        const uint64_t now = NowMs();
        while (index < m_clients.size())
        {
            uint8_t type = 0;
            const char* payload = nullptr;
            size_t plen = 0, frame = 0;
            if (!ParseOneFrame(m_clients[index].buf, type, payload, plen, frame))
                break;
            if (!m_clients[index].rate.Allow(now))
            {
                QueueDrop(index);
                break;
            }
            std::string pay;
            if (!OpenFramePayload(type, payload, plen, pay))
            {
                m_clients[index].buf.erase(0, frame);
                QueueDrop(index);
                break;
            }
            m_clients[index].buf.erase(0, frame);
            if (type == NetMsg::Hello && pay.size() >= sizeof(NetWelcome))
            {
                NetWelcome req{};
                std::memcpy(&req, pay.data(), sizeof(req));
                if (on_hello)
                    on_hello((int)index, req);
            }
            else if (type == NetMsg::Input && pay.size() >= sizeof(InputFrame))
            {
                InputFrame f{};
                std::memcpy(&f, pay.data(), sizeof(f));
                bool kick = false;
                if (!GuardInput(m_clients[index].guard, m_clients[index].slot, f, m_inputs, &kick) && kick)
                {
                    QueueDrop(index);
                    break;
                }
            }
            else if (type == NetMsg::Ping)
                SendTo((int)index, NetMsg::Pong, pay.data(), pay.size());
            else if (type == NetMsg::StateBlobReq && pay.size() >= sizeof(uint64_t))
            {
                uint64_t tick = 0;
                std::memcpy(&tick, pay.data(), sizeof(tick));
                m_blob_reqs.push_back(tick);
            }
            if (index >= m_clients.size() || m_clients[index].relay_id != ev.id)
                break;
        }
    }
    ProcessQueuedDrops();
}

void NetHost::Poll()
{
    PollAttachRelay();
    if (m_listen != NetSock::kInvalid)
    {
        for (;;)
        {
            NetSock::Handle c = NetSock::TcpAccept(m_listen);
            if (c == NetSock::kInvalid)
                break;
            Client cl;
            cl.sock = c;
            cl.rate.SetLimit(m_msg_limit);
            m_clients.push_back(std::move(cl));
        }
        for (size_t i = 0; i < m_clients.size(); ++i)
            if (m_clients[i].sock != NetSock::kInvalid)
                HandleClient(i);
        ProcessQueuedDrops();
    }
    PollRelay();
    PollUdp();
    FlushDisconnectCallbacks();
}

// ---- NetClient ----

NetClient& NetClient::GetInstance()
{
    static NetClient instance;
    return instance;
}

bool NetClient::Connect(const std::string& host, uint16_t port)
{
    return ConnectTimeout(host, port, 3000);
}

bool NetClient::ConnectTimeout(const std::string& host, uint16_t port, int timeout_ms)
{
    CancelConnect();
    Close();
    m_sock = NetSock::TcpConnectTimeout(host, port, timeout_ms);
    if (m_sock == NetSock::kInvalid)
        return false;
    m_via_relay = false;
    m_got_welcome = false;
    m_hash_rejected = false;
    m_start = false;
    m_aborted = false;
    m_connect_status = ConnectStatus::Ready;
    m_connect_err.clear();
    FW_CORE_INFO("NetClient connected to {0}:{1}", host, port);
    return true;
}

const char* NetClient::ConnectStatusName() const
{
    switch (m_connect_status)
    {
    case ConnectStatus::Connecting:
        return "connecting";
    case ConnectStatus::Ready:
        return "ready";
    case ConnectStatus::Failed:
        return "failed";
    default:
        return "idle";
    }
}

void NetClient::CancelConnect()
{
    if (m_connect_job)
    {
        NetAsync::GetInstance().Cancel(m_connect_job);
        m_connect_job = 0;
    }
    if (m_connect_status == ConnectStatus::Connecting)
    {
        m_connect_status = ConnectStatus::Idle;
        m_connect_err.clear();
    }
    m_connect_send_hello = false;
    m_connect_expect_room = false;
}

bool NetClient::BeginConnect(const std::string& host, uint16_t port, int timeout_ms)
{
    CancelConnect();
    Close();
    m_connect_err.clear();
    m_connect_send_hello = true;
    m_connect_expect_room = false;
    m_connect_job = NetAsync::GetInstance().SubmitTcpConnect(host, port, timeout_ms);
    m_connect_status = ConnectStatus::Connecting;
    FW_CORE_INFO("NetClient async connect {0}:{1}", host, port);
    return m_connect_job != 0;
}

bool NetClient::BeginJoinFromLobby()
{
    if (!LobbyClient::GetInstance().HasRoom())
    {
        m_connect_status = ConnectStatus::Failed;
        m_connect_err = "no lobby room";
        return false;
    }
    CancelConnect();
    Close();
    m_connect_room = LobbyClient::GetInstance().LastRoom();
    m_connect_expect_room = true;
    m_connect_send_hello = true;
    m_connect_err.clear();
    m_connect_job = NetAsync::GetInstance().SubmitRelayClientJoin(m_connect_room);
    m_connect_status = ConnectStatus::Connecting;
    FW_CORE_INFO("NetClient async join-from-lobby room {0}", m_connect_room.code);
    return m_connect_job != 0;
}

void NetClient::ArmClientSession()
{
    MatchSession::GetInstance().SetAuthority(MatchAuthority::NetClient);
    NetWelcome hello{};
    hello.proto = NetMsg::kProto;
    hello.content_hash = MatchSession::GetInstance().ContentHashValue();
    hello.seed = 0;
    Send(NetMsg::Hello, &hello, sizeof(hello));
}

void NetClient::PollConnect()
{
    if (m_connect_status != ConnectStatus::Connecting || !m_connect_job)
        return;

    NetAsyncResult r;
    NetCtrlLink ctrl;
    bool has_ctrl = false;
    if (!NetAsync::GetInstance().TryTake(m_connect_job, r, ctrl, has_ctrl))
        return;

    m_connect_job = 0;
    if (!r.ok || r.sock == NetSock::kInvalid)
    {
        m_connect_status = ConnectStatus::Failed;
        m_connect_err = r.err.empty() ? "connect failed" : r.err;
        if (r.sock != NetSock::kInvalid)
            NetSock::Close(r.sock);
        return;
    }

    m_sock = r.sock;
    m_via_relay = r.via_relay;
    m_got_welcome = false;
    m_hash_rejected = false;
    m_start = false;
    m_aborted = false;
    m_udp_only = false;
    m_buf.clear();

    if (m_connect_expect_room || r.have_room)
    {
        const LobbyProto::RoomInfoMsg& room = r.have_room ? r.room : m_connect_room;
        NetCrypto::GetInstance().SetRoomKey(room.room_key);
    }

    if (has_ctrl)
        m_ctrl = std::move(ctrl);
    else
        m_ctrl.Clear();

    m_connect_status = ConnectStatus::Ready;
    m_connect_err.clear();
    FW_CORE_INFO("NetClient async connect ready (relay={0})", m_via_relay ? 1 : 0);

    if (m_connect_send_hello)
        ArmClientSession();
    m_connect_send_hello = false;
    m_connect_expect_room = false;
}

bool NetClient::WaitRelayOk(int timeout_ms)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (!m_ctrl.PollRecv(m_sock))
            return false;
        uint8_t type = 0;
        std::string pay;
        while (m_ctrl.TakeFrame(type, pay))
        {
            if (type == RelayProto::Ok)
                return true;
            if (type == RelayProto::Err)
                return false;
        }
        NetSock::SleepMs(5);
    }
    return false;
}

bool NetClient::ConnectRelay(const LobbyProto::RoomInfoMsg& room)
{
    Close();
    m_sock = NetSock::TcpConnectTimeout(room.relay_host, room.relay_port, 3000);
    if (m_sock == NetSock::kInvalid)
        return false;

    // RelayProto must not use room AEAD - clear until after register.
    NetCrypto::GetInstance().Clear();

    if (!m_ctrl.HandshakeClient(m_sock))
    {
        FW_CORE_WARN("NetClient relay ctrl handshake failed");
        Close();
        return false;
    }

    RelayProto::RegisterReq reg{};
    reg.proto = RelayProto::kProto;
    reg.role = RelayProto::Client;
    std::memcpy(reg.session_id, room.session_id, LobbyProto::kIdBytes);
    std::memcpy(reg.token, room.client_token, LobbyProto::kIdBytes);
    if (!m_ctrl.SendMsg(m_sock, RelayProto::Register, &reg, sizeof(reg)) || !WaitRelayOk(3000))
    {
        Close();
        return false;
    }
    NetCrypto::GetInstance().SetRoomKey(room.room_key);
    m_via_relay = true;
    m_got_welcome = false;
    FW_CORE_INFO("NetClient relay join room {0}", room.code);
    return true;
}

bool NetClient::ConnectOnline(const LobbyProto::RoomInfoMsg& room)
{
    NetCrypto::GetInstance().SetRoomKey(room.room_key);
    // 1) Prepare punch + STUN
    auto& punch = NatPunch::GetInstance();
    punch.Prepare(0);
    punch.SetSession(room.session_id);
    std::vector<NatCandidate> rem;
    if (room.direct_host[0] && room.direct_port)
        rem.push_back({room.direct_host, room.direct_port});
    // Host UDP often same as game port for listen; also try game port on direct_host
    if (room.direct_host[0])
        rem.push_back({room.direct_host, NetMsg::kDefaultPort});
    punch.SetRemoteCandidates(rem);

    // 2) Punch window ~1.2s
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
    while (std::chrono::steady_clock::now() < deadline)
    {
        punch.Poll();
        if (punch.Ready())
            break;
        NetSock::SleepMs(10);
    }

    // 3) TCP control: prefer relay (NAT-safe); short direct try only if dedicated
    bool tcp_ok = false;
    if (room.kind == LobbyProto::Dedicated && room.direct_host[0])
        tcp_ok = ConnectTimeout(room.direct_host, room.direct_port ? room.direct_port : NetMsg::kDefaultPort, 800);
    if (!tcp_ok)
        tcp_ok = ConnectRelay(room);
    if (!tcp_ok && punch.Ready())
    {
        // UDP-only fallback rare - still need Welcome on TCP for slots
        FW_CORE_WARN("Punch ready but TCP failed");
        return false;
    }
    if (!tcp_ok)
        return false;

    // ConnectTimeout/Close wipe room AEAD - always restore after TCP is up.
    NetCrypto::GetInstance().SetRoomKey(room.room_key);

    // 4) Exchange candidates over TCP
    NetCandidateMsg ours{};
    ours.udp_port = punch.LocalUdpPort();
    const auto& locs = punch.LocalCandidates();
    if (!locs.empty())
        std::strncpy(ours.ip0, locs[0].ip.c_str(), sizeof(ours.ip0) - 1);
    if (locs.size() > 1)
        std::strncpy(ours.ip1, locs[1].ip.c_str(), sizeof(ours.ip1) - 1);
    Send(NetMsg::Candidates, &ours, sizeof(ours));

    // Keep punching a bit more after candidate exchange
    deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(800);
    while (std::chrono::steady_clock::now() < deadline)
    {
        punch.Poll();
        Poll(); // may receive host candidates
        if (punch.Ready())
            break;
        NetSock::SleepMs(10);
    }
    return true;
}

bool NetClient::UdpReady() const
{
    return NatPunch::GetInstance().Ready();
}

void NetClient::Close()
{
    CancelConnect();
    NetSock::Close(m_sock);
    NatPunch::GetInstance().Reset();
    NetCrypto::GetInstance().Clear();
    m_ctrl.Clear();
    m_buf.clear();
    m_pawns.clear();
    m_channels.clear();
    m_events.clear();
    m_peer_inputs.clear();
    m_checksums.clear();
    m_state_blobs.clear();
    m_blob_reqs.clear();
    m_peer_left.clear();
    m_got_welcome = false;
    m_hash_rejected = false;
    m_start = false;
    m_aborted = false;
    m_via_relay = false;
    m_udp_only = false;
    m_last_snap_tick = 0;
    m_ping_sent_ms = 0;
    m_rtt_ms = -1;
    m_welcome = {};
    m_connect_status = ConnectStatus::Idle;
    m_connect_err.clear();
    // Drop must leave OfflineHost - otherwise scripts keep treating us as a
    // client (early-return) with no hazard/retry authority after TCP/UDP die.
    MatchSession::GetInstance().ResetForLocal();
}

void NetClient::Send(uint8_t type, const void* data, size_t len)
{
    std::string msg;
    AppendMsg(msg, type, data, len);
    if ((type == NetMsg::Input || type == NetMsg::Snapshot) && NatPunch::GetInstance().Ready())
    {
        if (NatPunch::GetInstance().SendGame(msg.data(), msg.size()))
            return;
    }
    if (m_sock == NetSock::kInvalid)
        return;
    if (m_ctrl.Active())
    {
        if (!m_ctrl.SendRecord(m_sock, msg.data(), msg.size()))
            Close();
        return;
    }
    if (!NetSock::SendAll(m_sock, msg.data(), msg.size()))
        Close();
}

std::vector<NetPawnState> NetClient::TakePawns()
{
    std::vector<NetPawnState> out;
    out.swap(m_pawns);
    return out;
}

std::string NetClient::TakeChannelBlob()
{
    std::string out;
    out.swap(m_channels);
    return out;
}

std::vector<MatchEventWire> NetClient::TakeEvents()
{
    std::vector<MatchEventWire> out;
    out.swap(m_events);
    return out;
}

std::vector<InputFrame> NetClient::TakePeerInputs()
{
    std::vector<InputFrame> out;
    out.swap(m_peer_inputs);
    return out;
}

std::vector<NetStateChecksum> NetClient::TakeChecksums()
{
    std::vector<NetStateChecksum> out;
    out.swap(m_checksums);
    return out;
}

std::vector<std::pair<uint64_t, std::string>> NetClient::TakeStateBlobs()
{
    std::vector<std::pair<uint64_t, std::string>> out;
    out.swap(m_state_blobs);
    return out;
}

std::vector<uint64_t> NetClient::TakeStateBlobReqs()
{
    std::vector<uint64_t> out;
    out.swap(m_blob_reqs);
    return out;
}

std::vector<int32_t> NetClient::TakePeerLeft()
{
    std::vector<int32_t> out;
    out.swap(m_peer_left);
    return out;
}

void NetClient::HandleBuffer()
{
    while (m_buf.size() >= 5)
    {
        uint8_t type = 0;
        const char* payload = nullptr;
        size_t plen = 0, frame = 0;
        if (!ParseOneFrame(m_buf, type, payload, plen, frame))
            break;
        std::string pay;
        if (!OpenFramePayload(type, payload, plen, pay))
        {
            m_buf.erase(0, frame);
            Close();
            return;
        }
        m_buf.erase(0, frame);

        if (type == NetMsg::Welcome && pay.size() >= sizeof(NetWelcome))
        {
            std::memcpy(&m_welcome, pay.data(), sizeof(m_welcome));
            m_got_welcome = true;
        }
        else if (type == NetMsg::HashReject)
            m_hash_rejected = true;
        else if (type == NetMsg::StartMatch)
            m_start = true;
        else if (type == NetMsg::Abort)
            m_aborted = true;
        else if (type == NetMsg::PeerLeft && pay.size() >= sizeof(int32_t))
        {
            int32_t slot = -1;
            std::memcpy(&slot, pay.data(), sizeof(slot));
            m_peer_left.push_back(slot);
        }
        else if (type == NetMsg::Pong && pay.size() >= sizeof(uint64_t))
        {
            uint64_t sent = 0;
            std::memcpy(&sent, pay.data(), sizeof(sent));
            m_rtt_ms = (int64_t)(NowMs() - sent);
        }
        else if (type == NetMsg::Candidates && pay.size() >= sizeof(NetCandidateMsg))
        {
            NetCandidateMsg cm{};
            std::memcpy(&cm, pay.data(), sizeof(cm));
            std::vector<NatCandidate> rem;
            if (cm.ip0[0])
                rem.push_back({cm.ip0, cm.udp_port});
            if (cm.ip1[0])
                rem.push_back({cm.ip1, cm.udp_port});
            NatPunch::GetInstance().SetRemoteCandidates(rem);
        }
        else if (type == NetMsg::Snapshot && pay.size() >= sizeof(NetSnapshotHeader))
        {
            NetSnapshotHeader hdr{};
            std::memcpy(&hdr, pay.data(), sizeof(hdr));
            size_t off = sizeof(hdr);
            m_last_snap_tick = hdr.tick;
            m_pawns.clear();
            for (uint8_t i = 0; i < hdr.slot_count; ++i)
            {
                if (off + sizeof(NetPawnState) > pay.size())
                    break;
                NetPawnState p{};
                std::memcpy(&p, pay.data() + off, sizeof(p));
                off += sizeof(p);
                m_pawns.push_back(p);
            }
            if (hdr.channel_bytes > 0 && off + hdr.channel_bytes <= pay.size())
                m_channels.append(pay.data() + off, hdr.channel_bytes);
        }
        else if (type == NetMsg::Event && pay.size() >= sizeof(MatchEventWire))
        {
            MatchEventWire ev{};
            std::memcpy(&ev, pay.data(), sizeof(ev));
            m_events.push_back(ev);
        }
        else if (type == NetMsg::Input && pay.size() >= sizeof(InputFrame))
        {
            InputFrame f{};
            std::memcpy(&f, pay.data(), sizeof(f));
            m_peer_inputs.push_back(f);
        }
        else if (type == NetMsg::StateChecksum && pay.size() >= sizeof(NetStateChecksum))
        {
            NetStateChecksum cs{};
            std::memcpy(&cs, pay.data(), sizeof(cs));
            m_checksums.push_back(cs);
        }
        else if (type == NetMsg::StateBlob && pay.size() >= sizeof(NetStateBlobHeader))
        {
            NetStateBlobHeader hdr{};
            std::memcpy(&hdr, pay.data(), sizeof(hdr));
            if (pay.size() >= sizeof(hdr) + hdr.byte_count)
            {
                std::string blob(pay.data() + sizeof(hdr), hdr.byte_count);
                m_state_blobs.emplace_back(hdr.tick, std::move(blob));
            }
        }
        else if (type == NetMsg::StateBlobReq && pay.size() >= sizeof(uint64_t))
        {
            uint64_t tick = 0;
            std::memcpy(&tick, pay.data(), sizeof(tick));
            m_blob_reqs.push_back(tick);
        }
    }
}

void NetClient::PollUdpGameplay()
{
    auto& punch = NatPunch::GetInstance();
    punch.Poll();
    std::string frame;
    while (punch.RecvGame(frame))
    {
        m_buf.append(frame);
        HandleBuffer();
    }
}

void NetClient::Poll()
{
    PollConnect();
    PollUdpGameplay();
    if (m_sock == NetSock::kInvalid)
        return;
    const uint64_t now = NowMs();
    if (m_got_welcome && (m_ping_sent_ms == 0 || now - m_ping_sent_ms > 1000))
    {
        m_ping_sent_ms = now;
        Send(NetMsg::Ping, &now, sizeof(now));
    }
    if (m_ctrl.Active())
    {
        if (!m_ctrl.PollRecv(m_sock))
        {
            m_aborted = true;
            Close();
            return;
        }
        if (!m_ctrl.Plain().empty())
        {
            m_buf.append(m_ctrl.Plain());
            m_ctrl.Plain().clear();
        }
        HandleBuffer();
        return;
    }
    char tmp[4096];
    for (;;)
    {
        int n = NetSock::Recv(m_sock, tmp, sizeof(tmp));
        if (n > 0)
        {
            m_buf.append(tmp, tmp + n);
            continue;
        }
        if (n == 0)
        {
            m_aborted = true;
            Close();
            return;
        }
        if (n == -1)
            break;
        m_aborted = true;
        Close();
        return;
    }
    HandleBuffer();
}
