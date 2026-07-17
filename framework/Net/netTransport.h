#pragma once

// TCP framing today; reserved IDs for planned UDP / NAT / dedicated split.

#include <cstdint>

namespace NetTransport
{
// planned: TcpControl (reliable hello/welcome/abort/hash) + UdpGameplay (input/snapshot after punch)
enum class Kind : uint8_t
{
    TcpControl = 0,
    UdpGameplay = 1,
};

    // Future endpoint kinds for MatchSession / process role.
    enum class EndpointRole : uint8_t
    {
        ListenServer = 0, // player host (current NetHost)
        Dedicated = 1,    // headless sim authority, no local human required
        Client = 2,
    };
}

namespace NetMsg
{
    // reserved udp band 32-63
    enum ReservedUdp : uint8_t
    {
        UdpHello = 32,
        UdpPunch = 33,  // NAT hole-punch candidate exchange
        UdpRelay = 34,  // optional relay allocation
        UdpInput = 35,
        UdpSnapshot = 36,
        UdpKeepAlive = 37,
    };
}
