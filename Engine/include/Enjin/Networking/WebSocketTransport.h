#pragma once

// WebSocket transport (adr-0007 Track B step 3).
//
// A browser cannot open a UDP socket, so this is not one option among several
// for the web tier: it is the only way a browser joins a game at all.
//
// WHAT THIS IS ON DESKTOP: a real WebSocket SERVER, so a host built with this
// engine can accept browser connections directly, with no relay in between.
// The relay (step 4) is what solves reachability across NAT; this is what makes
// a browser able to speak to a host once it can reach one, and it is useful on
// its own for a LAN, a port-forwarded host, or a server on a box you own.
//
// ---------------------------------------------------------------------------
// FITTING A STREAM INTO A DATAGRAM INTERFACE
//
// INetworkTransport is datagram-shaped (SendTo/ReceiveFrom by address) because
// it was written for UDP. WebSocket is a connection-oriented stream, and the
// mapping is not a compromise in either direction:
//
//  * Every WebSocket message is one datagram. Framing is already the protocol's
//    job, so a message in is a message out with no length prefix of our own --
//    which is more than UDP gives us, not less.
//  * The peer ADDRESS is the TCP connection's real remote ip:port, from
//    getpeername. That is unique per connection and stable for its lifetime, so
//    nothing above needs to know these are not datagrams.
//  * A client has no listen socket. `Bind(0)` puts this in CLIENT mode, and the
//    first `SendTo` to an address with no connection OPENS one, handshake and
//    all, queuing that payload until it completes. That is exactly the shape
//    NetworkSystem::JoinGame already has (Bind(0), then SendTo the host), so
//    joining needs no new interface and no new call.
//
// The one real difference from UDP: a stream can deliver half a message, so
// anything not yet whole is held per connection until the rest arrives. UDP
// never needed that because the kernel either gave you a datagram or did not.
//
// ---------------------------------------------------------------------------
// WHAT THIS IS NOT
//
// No TLS. This is `ws://`, not `wss://`. A page served over HTTPS will refuse
// to open a ws:// connection (mixed content), so a browser reaching this
// directly has to be served over plain HTTP, which in practice means a LAN or
// a developer machine. Terminating TLS is the relay's job in step 4, and
// pretending otherwise here would be the kind of half-answer that gets built on.
//
// No permessage-deflate. It is not negotiated, so a client asking for it does
// not get it and sends uncompressed, which is correct behaviour rather than a
// failure.

#include "Enjin/Networking/INetworkTransport.h"
#include "Enjin/Networking/WebSocketProtocol.h"
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace Enjin {
namespace Networking {

class ENJIN_API WebSocketTransport : public INetworkTransport {
public:
    WebSocketTransport() = default;
    ~WebSocketTransport() override;

    // port > 0: listen (server). port == 0: client mode, connections are opened
    // lazily by SendTo.
    bool Bind(u16 port) override;

    // Sends one WebSocket binary message. In client mode, opens a connection to
    // `addr` if there is not one yet and queues this payload behind the
    // handshake -- so the first send after a Bind(0) is a connect.
    bool SendTo(const NetworkAddress& addr, const u8* data, u32 size) override;

    // Pumps accepts, reads and handshakes, then returns ONE complete message.
    // Returns 0 when there is nothing ready, which is the same contract
    // UDPTransport has for an empty socket.
    i32 ReceiveFrom(NetworkAddress& sender, u8* buffer, u32 bufferSize) override;

    void Close() override;
    bool IsOpen() const override { return m_Open; }
    u16 GetBoundPort() const override { return m_BoundPort; }

    // How many peers are currently connected and past the handshake.
    usize GetConnectionCount() const;

private:
    struct Connection {
        u64 socket = 0;
        NetworkAddress address;

        // Handshake state. `handshakeDone` gates framing: before it, bytes are
        // HTTP; after it, they are frames.
        bool isServerSide = false;     // we accepted it (vs we dialled out)
        bool handshakeDone = false;
        std::string handshakeBuffer;   // HTTP text, until the blank line
        std::string clientKey;         // client mode: what we sent, to verify

        std::vector<u8> inBuffer;      // raw bytes not yet a whole frame
        std::vector<u8> fragment;      // reassembly across continuation frames
        WebSocket::Opcode fragmentOpcode = WebSocket::Opcode::Binary;
        bool fragmented = false;

        // Payloads handed to SendTo before the handshake finished.
        std::deque<std::vector<u8>> pendingOut;

        bool dead = false;
    };

    Connection* FindConnection(const NetworkAddress& addr);
    Connection* OpenClientConnection(const NetworkAddress& addr);
    void AcceptPending();
    void ServiceConnection(Connection& c);
    void ReadIntoBuffer(Connection& c);
    void AdvanceHandshake(Connection& c);
    void DrainFrames(Connection& c);
    bool SendFrame(Connection& c, const u8* data, usize size);
    bool SendRaw(Connection& c, const u8* data, usize size);
    void CloseConnection(Connection& c);
    void ReapDead();

    u64 m_ListenSocket = 0;
    bool m_Listening = false;
    u16 m_BoundPort = 0;
    bool m_Open = false;
    bool m_WsaInitialized = false;

    std::vector<Connection> m_Connections;
    // Messages decoded but not yet handed out, with who sent them.
    std::deque<std::pair<NetworkAddress, std::vector<u8>>> m_Inbox;
};

} // namespace Networking
} // namespace Enjin
