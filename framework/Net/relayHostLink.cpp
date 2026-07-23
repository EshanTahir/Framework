#include "relayHostLink.h"
#include "Core/log.h"

#include <chrono>
#include <cstring>

RelayHostLink& RelayHostLink::GetInstance()
{
    static RelayHostLink instance;
    return instance;
}

bool RelayHostLink::Connect(const std::string& host, uint16_t port,
                            const uint8_t session_id[LobbyProto::kIdBytes],
                            const uint8_t host_token[LobbyProto::kIdBytes],
                            const uint8_t client_token[LobbyProto::kIdBytes])
{
    Close();
    m_sock = NetSock::TcpConnectTimeout(host, port, 3000);
    if (m_sock == NetSock::kInvalid)
        return false;
    if (!m_ctrl.HandshakeClient(m_sock))
    {
        FW_CORE_WARN("RelayHostLink ctrl handshake failed");
        Close();
        return false;
    }

    RelayProto::RegisterReq reg{};
    reg.proto = RelayProto::kProto;
    reg.role = RelayProto::Host;
    std::memcpy(reg.session_id, session_id, LobbyProto::kIdBytes);
    std::memcpy(reg.token, host_token, LobbyProto::kIdBytes);
    std::memcpy(reg.peer_token, client_token, LobbyProto::kIdBytes);
    if (!m_ctrl.SendMsg(m_sock, RelayProto::Register, &reg, sizeof(reg)))
    {
        Close();
        return false;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (!m_ctrl.PollRecv(m_sock))
        {
            Close();
            return false;
        }
        uint8_t type = 0;
        std::string pay;
        while (m_ctrl.TakeFrame(type, pay))
        {
            if (type == RelayProto::Ok)
            {
                FW_CORE_INFO("RelayHostLink registered on {0}:{1}", host, port);
                return true;
            }
            if (type == RelayProto::Err)
            {
                Close();
                return false;
            }
        }
        NetSock::SleepMs(5);
    }
    Close();
    return false;
}

bool RelayHostLink::Adopt(NetSock::Handle sock, NetCtrlLink&& ctrl)
{
    Close();
    if (sock == NetSock::kInvalid)
        return false;
    m_sock = sock;
    m_ctrl = std::move(ctrl);
    FW_CORE_INFO("RelayHostLink adopted async session");
    return true;
}

void RelayHostLink::Close()
{
    NetSock::Close(m_sock);
    m_ctrl.Clear();
    m_opens.clear();
    m_closes.clear();
    m_data.clear();
}

void RelayHostLink::Poll()
{
    if (m_sock == NetSock::kInvalid)
        return;
    if (!m_ctrl.PollRecv(m_sock))
    {
        Close();
        return;
    }
    HandleFrames();
}

void RelayHostLink::HandleFrames()
{
    uint8_t type = 0;
    std::string pay;
    while (m_ctrl.TakeFrame(type, pay))
    {
        if (type == RelayProto::ClientOpen && pay.size() >= 2)
        {
            uint16_t id = 0;
            std::memcpy(&id, pay.data(), 2);
            m_opens.push_back(PeerOpen{id});
        }
        else if (type == RelayProto::ClientClose && pay.size() >= 2)
        {
            uint16_t id = 0;
            std::memcpy(&id, pay.data(), 2);
            m_closes.push_back(PeerClose{id});
        }
        else if (type == RelayProto::ToHost && pay.size() >= 2)
        {
            uint16_t id = 0;
            std::memcpy(&id, pay.data(), 2);
            DataEvent ev;
            ev.id = id;
            if (pay.size() > 2)
                ev.bytes.assign(pay.begin() + 2, pay.end());
            m_data.push_back(std::move(ev));
        }
    }
}

std::vector<RelayHostLink::PeerOpen> RelayHostLink::TakeOpens()
{
    std::vector<PeerOpen> o;
    o.swap(m_opens);
    return o;
}

std::vector<RelayHostLink::PeerClose> RelayHostLink::TakeCloses()
{
    std::vector<PeerClose> o;
    o.swap(m_closes);
    return o;
}

std::vector<RelayHostLink::DataEvent> RelayHostLink::TakeData()
{
    std::vector<DataEvent> o;
    o.swap(m_data);
    return o;
}

bool RelayHostLink::SendToClient(uint16_t id, const void* data, size_t len)
{
    if (m_sock == NetSock::kInvalid)
        return false;
    std::string payload;
    payload.resize(2 + len);
    std::memcpy(payload.data(), &id, 2);
    if (len && data)
        std::memcpy(payload.data() + 2, data, len);
    if (!m_ctrl.SendMsg(m_sock, RelayProto::ToClient, payload.data(), payload.size()))
    {
        Close();
        return false;
    }
    return true;
}
