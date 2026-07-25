#pragma once

#include "netAeadSession.h"
#include "netSockets.h"

#include <cstdint>
#include <string>

// TLS-substitute for lobby/relay TCP: ephemeral X25519 + ChaCha20-Poly1305 records.
// After Handshake*, every app byte stream is sent as:
//   u32 le record_len | AEAD(record)   AD-type = 0
// Plaintext inside records is normal NetFraming (or opaque NetMsg bytes).

class NetCtrlLink
{
public:
    static constexpr uint8_t kRecordType = 0;

    NetCtrlLink() = default;
    NetCtrlLink(NetCtrlLink&&) noexcept = default;
    NetCtrlLink& operator=(NetCtrlLink&&) noexcept = default;
    NetCtrlLink(const NetCtrlLink&) = delete;
    NetCtrlLink& operator=(const NetCtrlLink&) = delete;

    bool HandshakeClient(NetSock::Handle sock, int timeout_ms = 3000);
    bool HandshakeServer(NetSock::Handle sock, int timeout_ms = 3000);
    void Clear();

    bool Active() const { return m_session.Active(); }

    bool SendRecord(NetSock::Handle sock, const void* bytes, size_t len);
    bool SendMsg(NetSock::Handle sock, uint8_t type, const void* data, size_t len);

    bool PollRecv(NetSock::Handle sock);

    std::string& Plain() { return m_plain; }
    const std::string& Plain() const { return m_plain; }

    bool TakeFrame(uint8_t& type, std::string& payload);

private:
    NetAeadSession m_session;
    std::string m_wire;
    std::string m_plain;

    bool DoHandshake(NetSock::Handle sock, bool as_server, int timeout_ms);
    bool SendAllRaw(NetSock::Handle sock, const void* data, size_t len);
    bool RecvExact(NetSock::Handle sock, void* out, size_t len, int timeout_ms);
};
