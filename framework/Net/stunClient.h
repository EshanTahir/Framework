#pragma once

#include <cstdint>
#include <string>

// RFC 5389 STUN Binding (minimal) - learns reflexive public IP:port for UDP.

struct StunEndpoint
{
    std::string ip;
    uint16_t port = 0;
    bool ok = false;
};

namespace StunClient
{
    // Default: Google public STUN. Override host/port as needed.
    StunEndpoint Query(const std::string& stun_host = "stun.l.google.com",
                       uint16_t stun_port = 19302,
                       int timeout_ms = 800);
}
