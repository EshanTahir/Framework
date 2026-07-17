#include "netAsync.h"
#include "netCtrlLink.h"
#include "netSession.h"
#include "Core/log.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
    using clock = std::chrono::steady_clock;

    bool LobbyWaitMsg(NetSock::Handle sock, NetCtrlLink& ctrl, uint8_t expect_type, void* out,
                      size_t out_len, std::string& err, int timeout_ms = 3000)
    {
        auto deadline = clock::now() + std::chrono::milliseconds(timeout_ms);
        while (clock::now() < deadline)
        {
            if (sock == NetSock::kInvalid)
            {
                err = "lobby disconnected";
                return false;
            }
            if (!ctrl.PollRecv(sock))
            {
                err = "lobby disconnected";
                return false;
            }
            uint8_t type = 0;
            std::string pay;
            while (ctrl.TakeFrame(type, pay))
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

    bool LobbyConnectEphemeral(const std::string& host, uint16_t port, NetSock::Handle& sock,
                               NetCtrlLink& ctrl, std::string& err)
    {
        sock = NetSock::TcpConnectTimeout(host, port, 3000);
        if (sock == NetSock::kInvalid)
        {
            err = "lobby connect failed";
            return false;
        }
        if (!ctrl.HandshakeClient(sock))
        {
            err = "lobby handshake failed";
            NetSock::Close(sock);
            return false;
        }
        return true;
    }

    bool RelayWaitOk(NetSock::Handle sock, NetCtrlLink& ctrl, int timeout_ms)
    {
        auto deadline = clock::now() + std::chrono::milliseconds(timeout_ms);
        while (clock::now() < deadline)
        {
            if (!ctrl.PollRecv(sock))
                return false;
            uint8_t type = 0;
            std::string pay;
            while (ctrl.TakeFrame(type, pay))
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
}

struct NetAsync::Impl
{
    struct Job
    {
        uint64_t id = 0;
        NetAsyncKind kind = NetAsyncKind::TcpConnect;
        std::string host;
        uint16_t port = 0;
        int timeout_ms = 3000;
        std::string lobby_host;
        uint16_t lobby_port = 0;
        LobbyProto::CreateRoomReq create{};
        std::string join_code;
        uint64_t content_hash = 0;
        LobbyProto::RoomInfoMsg room{};
    };

    struct DoneSlot
    {
        NetAsyncResult result;
        NetCtrlLink ctrl;
        bool has_ctrl = false;
    };

    std::mutex job_mu;
    std::condition_variable job_cv;
    std::deque<Job> jobs;
    bool stop = false;
    std::thread worker;

    std::mutex done_mu;
    std::unordered_map<uint64_t, DoneSlot> done;
    std::unordered_set<uint64_t> cancelled;

    std::atomic<uint64_t> next_id{1};

    void EnsureStarted()
    {
        std::lock_guard<std::mutex> lock(job_mu);
        if (worker.joinable())
            return;
        stop = false;
        worker = std::thread([this] { ThreadMain(); });
    }

    uint64_t Enqueue(Job job)
    {
        EnsureStarted();
        const uint64_t id = next_id.fetch_add(1);
        job.id = id;
        {
            std::lock_guard<std::mutex> lock(job_mu);
            jobs.push_back(std::move(job));
        }
        job_cv.notify_one();
        return id;
    }

    void ThreadMain()
    {
        NetSock::Init();
        for (;;)
        {
            Job job;
            {
                std::unique_lock<std::mutex> lock(job_mu);
                job_cv.wait(lock, [&] { return stop || !jobs.empty(); });
                if (stop && jobs.empty())
                    return;
                job = std::move(jobs.front());
                jobs.pop_front();
            }

            DoneSlot slot;
            slot.result.id = job.id;
            slot.result.kind = job.kind;
            RunJob(job, slot);

            bool drop = false;
            {
                std::lock_guard<std::mutex> lock(done_mu);
                if (cancelled.count(job.id))
                {
                    cancelled.erase(job.id);
                    drop = true;
                }
                else
                {
                    done[job.id] = std::move(slot);
                    slot = {};
                }
            }
            if (drop && slot.result.sock != NetSock::kInvalid)
                NetSock::Close(slot.result.sock);
        }
    }

    void RunJob(const Job& job, DoneSlot& slot)
    {
        NetAsyncResult& r = slot.result;
        switch (job.kind)
        {
        case NetAsyncKind::TcpConnect:
            r.sock = NetSock::TcpConnectTimeout(job.host, job.port, job.timeout_ms);
            r.ok = r.sock != NetSock::kInvalid;
            if (!r.ok)
                r.err = "connect failed";
            break;

        case NetAsyncKind::LobbyCreateRoom:
        {
            // Keep this TCP open - lobby ties room ownership to the peer socket.
            // Closing here used to orphan rooms (no CloseRoom / no heartbeat owner).
            NetSock::Handle sock = NetSock::kInvalid;
            NetCtrlLink ctrl;
            if (!LobbyConnectEphemeral(job.lobby_host, job.lobby_port, sock, ctrl, r.err))
                break;
            if (!ctrl.SendMsg(sock, LobbyProto::CreateRoom, &job.create, sizeof(job.create)))
            {
                r.err = "send failed";
                NetSock::Close(sock);
                break;
            }
            LobbyProto::RoomInfoMsg info{};
            if (!LobbyWaitMsg(sock, ctrl, LobbyProto::RoomInfo, &info, sizeof(info), r.err))
            {
                NetSock::Close(sock);
                break;
            }
            r.sock = sock;
            r.room = info;
            r.have_room = true;
            r.ok = true;
            slot.ctrl = std::move(ctrl);
            slot.has_ctrl = true;
            break;
        }

        case NetAsyncKind::LobbyJoinRoom:
        {
            NetSock::Handle sock = NetSock::kInvalid;
            NetCtrlLink ctrl;
            if (!LobbyConnectEphemeral(job.lobby_host, job.lobby_port, sock, ctrl, r.err))
                break;
            LobbyProto::JoinRoomReq req{};
            req.proto = LobbyProto::kProto;
            req.content_hash = job.content_hash;
            std::strncpy(req.code, job.join_code.c_str(), LobbyProto::kCodeLen);
            req.code[LobbyProto::kCodeLen] = 0;
            if (!ctrl.SendMsg(sock, LobbyProto::JoinRoom, &req, sizeof(req)))
            {
                r.err = "send failed";
                NetSock::Close(sock);
                break;
            }
            LobbyProto::RoomInfoMsg info{};
            if (!LobbyWaitMsg(sock, ctrl, LobbyProto::RoomInfo, &info, sizeof(info), r.err))
            {
                NetSock::Close(sock);
                break;
            }
            NetSock::Close(sock);
            r.room = info;
            r.have_room = true;
            r.ok = true;
            break;
        }

        case NetAsyncKind::RelayClientJoin:
        {
            if (job.room.kind == LobbyProto::Dedicated && job.room.direct_host[0])
            {
                const uint16_t dp =
                    job.room.direct_port ? job.room.direct_port : NetMsg::kDefaultPort;
                NetSock::Handle direct =
                    NetSock::TcpConnectTimeout(job.room.direct_host, dp, 800);
                if (direct != NetSock::kInvalid)
                {
                    r.sock = direct;
                    r.via_relay = false;
                    r.room = job.room;
                    r.have_room = true;
                    r.ok = true;
                    break;
                }
            }

            NetSock::Handle sock =
                NetSock::TcpConnectTimeout(job.room.relay_host, job.room.relay_port, 3000);
            if (sock == NetSock::kInvalid)
            {
                r.err = "relay connect failed";
                break;
            }
            NetCtrlLink ctrl;
            if (!ctrl.HandshakeClient(sock))
            {
                r.err = "relay handshake failed";
                NetSock::Close(sock);
                break;
            }
            RelayProto::RegisterReq reg{};
            reg.proto = RelayProto::kProto;
            reg.role = RelayProto::Client;
            std::memcpy(reg.session_id, job.room.session_id, LobbyProto::kIdBytes);
            std::memcpy(reg.token, job.room.client_token, LobbyProto::kIdBytes);
            if (!ctrl.SendMsg(sock, RelayProto::Register, &reg, sizeof(reg)) ||
                !RelayWaitOk(sock, ctrl, 3000))
            {
                r.err = "relay register failed";
                NetSock::Close(sock);
                break;
            }
            r.sock = sock;
            r.via_relay = true;
            r.room = job.room;
            r.have_room = true;
            r.ok = true;
            slot.ctrl = std::move(ctrl);
            slot.has_ctrl = true;
            break;
        }

        case NetAsyncKind::RelayHostJoin:
        {
            NetSock::Handle sock =
                NetSock::TcpConnectTimeout(job.room.relay_host, job.room.relay_port, 3000);
            if (sock == NetSock::kInvalid)
            {
                r.err = "relay connect failed";
                break;
            }
            NetCtrlLink ctrl;
            if (!ctrl.HandshakeClient(sock))
            {
                r.err = "relay handshake failed";
                NetSock::Close(sock);
                break;
            }
            RelayProto::RegisterReq reg{};
            reg.proto = RelayProto::kProto;
            reg.role = RelayProto::Host;
            std::memcpy(reg.session_id, job.room.session_id, LobbyProto::kIdBytes);
            std::memcpy(reg.token, job.room.host_token, LobbyProto::kIdBytes);
            std::memcpy(reg.peer_token, job.room.client_token, LobbyProto::kIdBytes);
            if (!ctrl.SendMsg(sock, RelayProto::Register, &reg, sizeof(reg)) ||
                !RelayWaitOk(sock, ctrl, 3000))
            {
                r.err = "relay host register failed";
                NetSock::Close(sock);
                break;
            }
            r.sock = sock;
            r.via_relay = true;
            r.room = job.room;
            r.have_room = true;
            r.ok = true;
            slot.ctrl = std::move(ctrl);
            slot.has_ctrl = true;
            break;
        }
        }
    }

    void Cancel(uint64_t id)
    {
        if (!id)
            return;
        std::lock_guard<std::mutex> lock(done_mu);
        cancelled.insert(id);
        auto it = done.find(id);
        if (it != done.end())
        {
            if (it->second.result.sock != NetSock::kInvalid)
                NetSock::Close(it->second.result.sock);
            done.erase(it);
            cancelled.erase(id);
        }
    }

    bool TryTake(uint64_t id, NetAsyncResult& out, NetCtrlLink* ctrl_out, bool* has_ctrl)
    {
        if (has_ctrl)
            *has_ctrl = false;
        std::lock_guard<std::mutex> lock(done_mu);
        if (cancelled.count(id))
        {
            cancelled.erase(id);
            auto it = done.find(id);
            if (it != done.end())
            {
                if (it->second.result.sock != NetSock::kInvalid)
                    NetSock::Close(it->second.result.sock);
                done.erase(it);
            }
            return false;
        }
        auto it = done.find(id);
        if (it == done.end())
            return false;
        out = std::move(it->second.result);
        if (ctrl_out && it->second.has_ctrl)
        {
            *ctrl_out = std::move(it->second.ctrl);
            if (has_ctrl)
                *has_ctrl = true;
        }
        else if (it->second.has_ctrl)
        {
            // Caller did not take ctrl - drop AEAD session with socket close path.
            it->second.ctrl.Clear();
        }
        done.erase(it);
        return true;
    }

    void Shutdown()
    {
        {
            std::lock_guard<std::mutex> lock(job_mu);
            stop = true;
        }
        job_cv.notify_all();
        if (worker.joinable())
            worker.join();

        std::lock_guard<std::mutex> lock(done_mu);
        for (auto& [_, slot] : done)
        {
            if (slot.result.sock != NetSock::kInvalid)
                NetSock::Close(slot.result.sock);
        }
        done.clear();
        cancelled.clear();
    }
};

NetAsync& NetAsync::GetInstance()
{
    static NetAsync instance;
    return instance;
}

void NetAsync::EnsureStarted()
{
    if (!m)
        m = new Impl();
    m->EnsureStarted();
}

void NetAsync::Shutdown()
{
    if (!m)
        return;
    m->Shutdown();
    delete m;
    m = nullptr;
}

void NetAsync::Poll() {}

uint64_t NetAsync::SubmitTcpConnect(std::string host, uint16_t port, int timeout_ms)
{
    EnsureStarted();
    Impl::Job job;
    job.kind = NetAsyncKind::TcpConnect;
    job.host = std::move(host);
    job.port = port;
    job.timeout_ms = timeout_ms > 0 ? timeout_ms : 3000;
    return m->Enqueue(std::move(job));
}

uint64_t NetAsync::SubmitLobbyCreate(std::string lobby_host, uint16_t lobby_port,
                                     LobbyProto::CreateRoomReq req)
{
    EnsureStarted();
    Impl::Job job;
    job.kind = NetAsyncKind::LobbyCreateRoom;
    job.lobby_host = std::move(lobby_host);
    job.lobby_port = lobby_port;
    job.create = req;
    return m->Enqueue(std::move(job));
}

uint64_t NetAsync::SubmitLobbyJoin(std::string lobby_host, uint16_t lobby_port, std::string code,
                                   uint64_t content_hash)
{
    EnsureStarted();
    Impl::Job job;
    job.kind = NetAsyncKind::LobbyJoinRoom;
    job.lobby_host = std::move(lobby_host);
    job.lobby_port = lobby_port;
    job.join_code = std::move(code);
    job.content_hash = content_hash;
    return m->Enqueue(std::move(job));
}

uint64_t NetAsync::SubmitRelayClientJoin(LobbyProto::RoomInfoMsg room)
{
    EnsureStarted();
    Impl::Job job;
    job.kind = NetAsyncKind::RelayClientJoin;
    job.room = room;
    return m->Enqueue(std::move(job));
}

uint64_t NetAsync::SubmitRelayHostJoin(LobbyProto::RoomInfoMsg room)
{
    EnsureStarted();
    Impl::Job job;
    job.kind = NetAsyncKind::RelayHostJoin;
    job.room = room;
    return m->Enqueue(std::move(job));
}

void NetAsync::Cancel(uint64_t id)
{
    if (m)
        m->Cancel(id);
}

bool NetAsync::TryTake(uint64_t id, NetAsyncResult& out)
{
    if (!m || !id)
        return false;
    return m->TryTake(id, out, nullptr, nullptr);
}

bool NetAsync::TryTake(uint64_t id, NetAsyncResult& out, NetCtrlLink& ctrl_out, bool& has_ctrl)
{
    if (!m || !id)
        return false;
    return m->TryTake(id, out, &ctrl_out, &has_ctrl);
}
