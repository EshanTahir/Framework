#include "lobbyClient.h"
#include "netSockets.h"
#include "netAsync.h"
#include "Core/log.h"

#include <algorithm>
#include <chrono>
#include <cstring>

LobbyClient& LobbyClient::GetInstance()
{
    static LobbyClient instance;
    return instance;
}

void LobbyClient::SetLobbyEndpoint(const std::string& host, uint16_t port)
{
    m_host = host.empty() ? "127.0.0.1" : host;
    m_port = port ? port : LobbyProto::kDefaultLobbyPort;
}

bool LobbyClient::Connect()
{
    if (m_sock != NetSock::kInvalid)
        return true;
    // Bound wait - never use blocking TcpConnect on the game thread.
    m_sock = NetSock::TcpConnectTimeout(m_host, m_port, 3000);
    if (m_sock == NetSock::kInvalid)
    {
        FW_CORE_WARN("LobbyClient: connect {0}:{1} failed", m_host, m_port);
        return false;
    }
    if (!m_ctrl.HandshakeClient(m_sock))
    {
        FW_CORE_WARN("LobbyClient: ctrl handshake failed");
        Close();
        return false;
    }
    FW_CORE_INFO("LobbyClient connected {0}:{1} (secured)", m_host, m_port);
    return true;
}

void LobbyClient::Close()
{
    NetSock::Close(m_sock);
    m_sock = NetSock::kInvalid;
    m_ctrl.Clear();
}

void LobbyClient::Poll()
{
    PollOp();
    if (m_sock == NetSock::kInvalid)
        return;
    if (!m_ctrl.PollRecv(m_sock))
    {
        // Owner TCP died - room is gone on the lobby.
        Close();
        m_have_room = false;
        m_room = {};
    }
}

const char* LobbyClient::OpStatusName() const
{
    switch (m_op_status)
    {
    case OpStatus::Busy:
        return "busy";
    case OpStatus::Ready:
        return "ready";
    case OpStatus::Failed:
        return "failed";
    default:
        return "idle";
    }
}

void LobbyClient::CancelOp()
{
    if (m_op_job)
    {
        NetAsync::GetInstance().Cancel(m_op_job);
        m_op_job = 0;
    }
    if (m_op_status == OpStatus::Busy)
    {
        m_op_status = OpStatus::Idle;
        m_op_err.clear();
    }
}

bool LobbyClient::BeginCreateRoom(const LobbyProto::CreateRoomReq& req)
{
    // Drop any previous owned room before opening a new create connection.
    if (m_have_room || m_sock != NetSock::kInvalid)
        CloseRoom();
    CancelOp();
    m_op_err.clear();
    m_op_job = NetAsync::GetInstance().SubmitLobbyCreate(m_host, m_port, req);
    m_op_status = OpStatus::Busy;
    return m_op_job != 0;
}

bool LobbyClient::BeginJoinRoom(const char* code, uint64_t content_hash)
{
    CancelOp();
    m_op_err.clear();
    m_op_job =
        NetAsync::GetInstance().SubmitLobbyJoin(m_host, m_port, code ? code : "", content_hash);
    m_op_status = OpStatus::Busy;
    return m_op_job != 0;
}

void LobbyClient::PollOp()
{
    if (m_op_status != OpStatus::Busy || !m_op_job)
        return;
    NetAsyncResult r;
    NetCtrlLink ctrl;
    bool has_ctrl = false;
    if (!NetAsync::GetInstance().TryTake(m_op_job, r, ctrl, has_ctrl))
        return;
    m_op_job = 0;
    if (!r.ok || !r.have_room)
    {
        if (r.sock != NetSock::kInvalid)
            NetSock::Close(r.sock);
        m_op_status = OpStatus::Failed;
        m_op_err = r.err.empty() ? "lobby failed" : r.err;
        return;
    }
    m_room = r.room;
    m_have_room = true;
    m_op_status = OpStatus::Ready;
    m_op_err.clear();

    // Create keeps an ownership socket; join uses ephemeral (sock left invalid).
    if (r.kind == NetAsyncKind::LobbyCreateRoom && r.sock != NetSock::kInvalid && has_ctrl)
    {
        if (m_sock != NetSock::kInvalid)
            NetSock::Close(m_sock);
        m_sock = r.sock;
        m_ctrl = std::move(ctrl);
    }
    else if (r.sock != NetSock::kInvalid)
    {
        NetSock::Close(r.sock);
    }
}

bool LobbyClient::SendMsg(uint8_t type, const void* data, size_t len)
{
    if (m_sock == NetSock::kInvalid && !Connect())
        return false;
    if (!m_ctrl.SendMsg(m_sock, type, data, len))
    {
        Close();
        return false;
    }
    return true;
}

bool LobbyClient::WaitMsg(uint8_t expect_type, void* out, size_t out_len, std::string& err, int timeout_ms)
{
    using clock = std::chrono::steady_clock;
    auto deadline = clock::now() + std::chrono::milliseconds(timeout_ms);
    while (clock::now() < deadline)
    {
        if (m_sock == NetSock::kInvalid)
        {
            err = "lobby disconnected";
            return false;
        }
        if (!m_ctrl.PollRecv(m_sock))
        {
            err = "lobby disconnected";
            Close();
            return false;
        }
        uint8_t type = 0;
        std::string pay;
        while (m_ctrl.TakeFrame(type, pay))
        {
            if (type == LobbyProto::Err)
            {
                LobbyProto::ErrMsg em{};
                if (pay.size() >= sizeof(em))
                    std::memcpy(&em, pay.data(), sizeof(em));
                err = em.text[0] ? em.text : "lobby error";
                return false;
            }
            if (type == expect_type)
            {
                if (out && out_len && pay.size() >= out_len)
                    std::memcpy(out, pay.data(), out_len);
                else if (out && out_len && !pay.empty())
                    std::memcpy(out, pay.data(), std::min(out_len, pay.size()));
                return true;
            }
        }
        NetSock::SleepMs(5);
    }
    err = "lobby timeout";
    return false;
}

bool LobbyClient::CreateRoom(const LobbyProto::CreateRoomReq& req, LobbyProto::RoomInfoMsg& out, std::string& err)
{
    if (!SendMsg(LobbyProto::CreateRoom, &req, sizeof(req)))
    {
        err = "send failed";
        return false;
    }
    if (!WaitMsg(LobbyProto::RoomInfo, &out, sizeof(out), err))
        return false;
    m_room = out;
    m_have_room = true;
    return true;
}

bool LobbyClient::JoinRoom(const char* code, uint64_t content_hash, LobbyProto::RoomInfoMsg& out, std::string& err)
{
    LobbyProto::JoinRoomReq req{};
    req.proto = LobbyProto::kProto;
    req.content_hash = content_hash;
    if (code)
    {
        std::strncpy(req.code, code, LobbyProto::kCodeLen);
        req.code[LobbyProto::kCodeLen] = 0;
    }
    if (!SendMsg(LobbyProto::JoinRoom, &req, sizeof(req)))
    {
        err = "send failed";
        return false;
    }
    if (!WaitMsg(LobbyProto::RoomInfo, &out, sizeof(out), err))
        return false;
    m_room = out;
    m_have_room = true;
    return true;
}

bool LobbyClient::RegisterDedicated(const LobbyProto::RegisterDedicatedReq& req, LobbyProto::RoomInfoMsg& out, std::string& err)
{
    if (!SendMsg(LobbyProto::RegisterDedicated, &req, sizeof(req)))
    {
        err = "send failed";
        return false;
    }
    if (!WaitMsg(LobbyProto::RoomInfo, &out, sizeof(out), err))
        return false;
    m_room = out;
    m_have_room = true;
    return true;
}

bool LobbyClient::Heartbeat()
{
    return SendMsg(LobbyProto::Heartbeat, nullptr, 0);
}

bool LobbyClient::CloseRoom()
{
    // ownership socket sends CloseRoom; skip Connect (stalls Host LAN Back)
    bool ok = true;
    if (m_sock != NetSock::kInvalid)
    {
        ok = m_ctrl.SendMsg(m_sock, LobbyProto::CloseRoom, nullptr, 0);
        Close();
    }
    CancelOp();
    m_have_room = false;
    m_room = {};
    return ok;
}
