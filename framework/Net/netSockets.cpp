#include "netSockets.h"
#include "Core/log.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
using NativeSock = SOCKET;
static constexpr NativeSock kNativeInvalid = INVALID_SOCKET;
#define NET_CLOSESOCKET closesocket
#define NET_IOCTL ioctlsocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
using NativeSock = int;
static constexpr NativeSock kNativeInvalid = -1;
#define NET_CLOSESOCKET ::close
#endif

namespace NetSock
{
    namespace
    {
        int g_refs = 0;

        NativeSock AsNative(Handle h)
        {
            return (h == kInvalid) ? kNativeInvalid : (NativeSock)h;
        }

        Handle FromNative(NativeSock s)
        {
            return (s == kNativeInvalid) ? kInvalid : (Handle)(uint64_t)s;
        }

        bool WouldBlock()
        {
#ifdef _WIN32
            const int e = WSAGetLastError();
            return e == WSAEWOULDBLOCK || e == WSAEINTR;
#else
            return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR;
#endif
        }
    }

    bool Init()
    {
        if (g_refs++ > 0)
            return true;
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            g_refs = 0;
            FW_CORE_ERROR("WSAStartup failed");
            return false;
        }
#endif
        return true;
    }

    void Shutdown()
    {
        if (g_refs <= 0)
            return;
        if (--g_refs > 0)
            return;
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool SetReuseAddr(Handle h)
    {
        NativeSock s = AsNative(h);
        if (s == kNativeInvalid)
            return false;
        int yes = 1;
        return setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)) == 0;
    }

    bool SetNonBlocking(Handle h)
    {
        NativeSock s = AsNative(h);
        if (s == kNativeInvalid)
            return false;
#ifdef _WIN32
        u_long mode = 1;
        return NET_IOCTL(s, FIONBIO, &mode) == 0;
#else
        int flags = fcntl(s, F_GETFL, 0);
        if (flags < 0)
            return false;
        return fcntl(s, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
    }

    Handle TcpListen(uint16_t port, int backlog)
    {
        if (!Init())
            return kInvalid;
        NativeSock s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == kNativeInvalid)
            return kInvalid;
        Handle h = FromNative(s);
        SetReuseAddr(h);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(s, backlog) != 0)
        {
            Close(h);
            return kInvalid;
        }
        SetNonBlocking(h);
        return h;
    }

    Handle TcpAccept(Handle listen)
    {
        NativeSock ls = AsNative(listen);
        if (ls == kNativeInvalid)
            return kInvalid;
        sockaddr_in addr{};
#ifdef _WIN32
        int alen = (int)sizeof(addr);
#else
        socklen_t alen = sizeof(addr);
#endif
        NativeSock c = ::accept(ls, (sockaddr*)&addr, &alen);
        if (c == kNativeInvalid)
            return kInvalid;
        Handle h = FromNative(c);
        SetNonBlocking(h);
        return h;
    }

    Handle TcpConnect(const std::string& host, uint16_t port)
    {
        if (!Init())
            return kInvalid;
        NativeSock s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == kNativeInvalid)
            return kInvalid;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
        {
            // Hostname resolve
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            addrinfo* res = nullptr;
            char port_str[16];
            std::snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);
            if (getaddrinfo(host.c_str(), port_str, &hints, &res) != 0 || !res)
            {
                NET_CLOSESOCKET(s);
                return kInvalid;
            }
            std::memcpy(&addr, res->ai_addr, sizeof(sockaddr_in));
            freeaddrinfo(res);
        }

        if (::connect(s, (sockaddr*)&addr, sizeof(addr)) != 0)
        {
            NET_CLOSESOCKET(s);
            return kInvalid;
        }
        Handle h = FromNative(s);
        SetNonBlocking(h);
        return h;
    }

    void Close(Handle& h)
    {
        if (h == kInvalid)
            return;
        // close fd only; Shutdown() is process teardown (refcount / WSACleanup).
        NET_CLOSESOCKET(AsNative(h));
        h = kInvalid;
    }

    int Send(Handle h, const char* data, size_t len)
    {
        NativeSock s = AsNative(h);
        if (s == kNativeInvalid || !data || len == 0)
            return -1;
        int n = ::send(s, data, (int)len, 0);
        if (n > 0)
            return n;
        if (n == 0)
            return -1;
        if (WouldBlock())
            return 0;
        return -1;
    }

    int Recv(Handle h, char* buf, size_t len)
    {
        NativeSock s = AsNative(h);
        if (s == kNativeInvalid || !buf || len == 0)
            return -2;
        int n = ::recv(s, buf, (int)len, 0);
        if (n > 0)
            return n;
        if (n == 0)
            return 0; // peer closed
        if (WouldBlock())
            return -1;
        return -2;
    }

    bool SendAll(Handle h, const char* data, size_t len)
    {
        size_t sent = 0;
        while (sent < len)
        {
            int n = Send(h, data + sent, len - sent);
            if (n < 0)
                return false;
            if (n == 0)
            {
                // Would-block mid-message - treat as failure for TCP game framing
                // (caller can upgrade to buffered send later).
                return false;
            }
            sent += (size_t)n;
        }
        return true;
    }

    void SleepMs(int ms)
    {
        if (ms < 0)
            ms = 0;
#ifdef _WIN32
        Sleep((DWORD)ms);
#else
        usleep((useconds_t)ms * 1000);
#endif
    }

    bool PeerAddress(Handle h, std::string& ip_out, uint16_t& port_out)
    {
        NativeSock s = AsNative(h);
        if (s == kNativeInvalid)
            return false;
        sockaddr_in addr{};
#ifdef _WIN32
        int alen = (int)sizeof(addr);
#else
        socklen_t alen = sizeof(addr);
#endif
        if (getpeername(s, (sockaddr*)&addr, &alen) != 0)
            return false;
        char buf[INET_ADDRSTRLEN]{};
        if (!inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf)))
            return false;
        ip_out = buf;
        port_out = ntohs(addr.sin_port);
        return true;
    }

    bool ResolveV4(const std::string& host, uint16_t port, sockaddr_in& addr)
    {
        addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) == 1)
            return true;
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* res = nullptr;
        char port_str[16];
        std::snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);
        if (getaddrinfo(host.c_str(), port_str, &hints, &res) != 0 || !res)
            return false;
        std::memcpy(&addr, res->ai_addr, sizeof(sockaddr_in));
        freeaddrinfo(res);
        return true;
    }

    Handle TcpConnectTimeout(const std::string& host, uint16_t port, int timeout_ms)
    {
        if (!Init())
            return kInvalid;
        NativeSock s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == kNativeInvalid)
            return kInvalid;
        Handle h = FromNative(s);
        SetNonBlocking(h);
        sockaddr_in addr{};
        if (!ResolveV4(host, port, addr))
        {
            Close(h);
            return kInvalid;
        }
        int rc = ::connect(s, (sockaddr*)&addr, sizeof(addr));
        if (rc == 0)
            return h;
#ifdef _WIN32
        if (WSAGetLastError() != WSAEWOULDBLOCK)
#else
        if (errno != EINPROGRESS && errno != EWOULDBLOCK)
#endif
        {
            Close(h);
            return kInvalid;
        }
        fd_set wset;
        FD_ZERO(&wset);
        FD_SET(s, &wset);
        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        rc = ::select((int)s + 1, nullptr, &wset, nullptr, &tv);
        if (rc <= 0)
        {
            Close(h);
            return kInvalid;
        }
        int soerr = 0;
#ifdef _WIN32
        int slen = sizeof(soerr);
#else
        socklen_t slen = sizeof(soerr);
#endif
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&soerr, &slen);
        if (soerr != 0)
        {
            Close(h);
            return kInvalid;
        }
        return h;
    }

    Handle UdpOpen()
    {
        if (!Init())
            return kInvalid;
        NativeSock s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == kNativeInvalid)
            return kInvalid;
        Handle h = FromNative(s);
        SetNonBlocking(h);
        return h;
    }

    bool UdpBind(Handle h, uint16_t port)
    {
        NativeSock s = AsNative(h);
        if (s == kNativeInvalid)
            return false;
        SetReuseAddr(h);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);
        return ::bind(s, (sockaddr*)&addr, sizeof(addr)) == 0;
    }

    bool UdpConnect(Handle h, const std::string& host, uint16_t port)
    {
        NativeSock s = AsNative(h);
        if (s == kNativeInvalid)
            return false;
        sockaddr_in addr{};
        if (!ResolveV4(host, port, addr))
            return false;
        return ::connect(s, (sockaddr*)&addr, sizeof(addr)) == 0;
    }

    int UdpSendTo(Handle h, const char* data, size_t len, const std::string& host, uint16_t port)
    {
        NativeSock s = AsNative(h);
        if (s == kNativeInvalid || !data || len == 0)
            return -1;
        sockaddr_in addr{};
        if (!ResolveV4(host, port, addr))
            return -1;
        int n = ::sendto(s, data, (int)len, 0, (sockaddr*)&addr, sizeof(addr));
        if (n >= 0)
            return n;
        if (WouldBlock())
            return 0;
        return -1;
    }

    int UdpRecvFrom(Handle h, char* buf, size_t len, std::string* from_ip, uint16_t* from_port)
    {
        NativeSock s = AsNative(h);
        if (s == kNativeInvalid || !buf || len == 0)
            return -1;
        sockaddr_in addr{};
#ifdef _WIN32
        int alen = (int)sizeof(addr);
#else
        socklen_t alen = sizeof(addr);
#endif
        int n = ::recvfrom(s, buf, (int)len, 0, (sockaddr*)&addr, &alen);
        if (n > 0)
        {
            if (from_ip)
            {
                char ip[INET_ADDRSTRLEN]{};
                inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
                *from_ip = ip;
            }
            if (from_port)
                *from_port = ntohs(addr.sin_port);
            return n;
        }
        if (WouldBlock())
            return 0;
        return -1;
    }

    uint16_t LocalPort(Handle h)
    {
        NativeSock s = AsNative(h);
        if (s == kNativeInvalid)
            return 0;
        sockaddr_in addr{};
#ifdef _WIN32
        int alen = (int)sizeof(addr);
#else
        socklen_t alen = sizeof(addr);
#endif
        if (getsockname(s, (sockaddr*)&addr, &alen) != 0)
            return 0;
        return ntohs(addr.sin_port);
    }
}
