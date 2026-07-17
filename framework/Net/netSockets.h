#pragma once

// Thin portable TCP + UDP socket façade (WinSock / BSD).

#include <cstdint>
#include <cstddef>
#include <string>

namespace NetSock
{
    using Handle = uint64_t;
    constexpr Handle kInvalid = ~0ull;

    bool Init();
    void Shutdown();

    Handle TcpListen(uint16_t port, int backlog = 4);
    Handle TcpAccept(Handle listen);
    Handle TcpConnect(const std::string& host, uint16_t port);
    // Non-blocking connect with timeout (ms). Returns kInvalid on failure.
    Handle TcpConnectTimeout(const std::string& host, uint16_t port, int timeout_ms);

    Handle UdpOpen();
    bool UdpBind(Handle h, uint16_t port); // 0 = ephemeral
    bool UdpConnect(Handle h, const std::string& host, uint16_t port); // optional filter
    int UdpSendTo(Handle h, const char* data, size_t len, const std::string& host, uint16_t port);
    // Returns bytes, 0 = would-block, -1 = fatal. Fills from_ip/from_port when set.
    int UdpRecvFrom(Handle h, char* buf, size_t len, std::string* from_ip, uint16_t* from_port);
    uint16_t LocalPort(Handle h);

    void Close(Handle& h);
    bool SetNonBlocking(Handle h);
    bool SetReuseAddr(Handle h);

    int Send(Handle h, const char* data, size_t len);
    int Recv(Handle h, char* buf, size_t len);
    bool SendAll(Handle h, const char* data, size_t len);

    void SleepMs(int ms);
    bool PeerAddress(Handle h, std::string& ip_out, uint16_t& port_out);
}
