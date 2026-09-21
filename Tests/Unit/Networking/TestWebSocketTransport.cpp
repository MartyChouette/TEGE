#include "EnjinTest.h"
#include "Enjin/Networking/WebSocketTransport.h"
#include <cstring>
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::Networking;

// ============================================================================
// WEBSOCKET TRANSPORT, OVER REAL TCP (adr-0007 Track B step 3)
//
// A server and a client on 127.0.0.1, both real sockets. What this pins down is
// the mapping of a datagram interface onto a stream, which is where the design
// could plausibly be wrong:
//
//   * one WebSocket message in is exactly one ReceiveFrom out, never half and
//     never two glued together;
//   * the peer address is the TCP remote, so the layer above can address a
//     reply without knowing any of this happened;
//   * a send issued BEFORE the handshake completes is not lost, because
//     NetworkSystem::JoinGame does exactly that and always will.
//
// The protocol itself is covered against the RFC's own vectors in
// TestWebSocketProtocol. This file does not re-test framing; it tests the
// plumbing.
//
// Ports are high and fixed, and a bind failure SKIPs rather than fails: a busy
// port on a developer machine is not a defect in this engine.
// ============================================================================

namespace {

NetworkAddress Local(u16 port) {
    NetworkAddress a;
    a.ip = NetworkAddress::ParseIP("127.0.0.1");
    a.port = port;
    return a;
}

// Pump both ends until `got` fills or we run out of patience. Everything here
// is non-blocking, so progress happens on the pump and nowhere else.
bool PumpFor(WebSocketTransport& a, WebSocketTransport& b,
             std::vector<u8>& got, NetworkAddress& from, int tries = 400) {
    u8 buf[4096];
    for (int i = 0; i < tries; i++) {
        NetworkAddress s;
        i32 n = a.ReceiveFrom(s, buf, sizeof(buf));
        if (n > 0) { got.assign(buf, buf + n); from = s; return true; }
        n = b.ReceiveFrom(s, buf, sizeof(buf));
        if (n > 0) { got.assign(buf, buf + n); from = s; return true; }
    }
    return false;
}

} // namespace

ENJIN_TEST(WebSocketTransport, AClientMessageArrivesAtTheServer) {
    // Arrange
    WebSocketTransport server;
    if (!server.Bind(48120)) ENJIN_SKIP("could not bind TCP 48120");

    WebSocketTransport client;
    ENJIN_ASSERT_TRUE(client.Bind(0));   // client mode: no listen socket

    // Act: the first SendTo to an unconnected address opens the connection and
    // queues this payload behind the handshake. This is the exact shape
    // NetworkSystem::JoinGame has, which is why it must work.
    const std::vector<u8> hello = { 'E', 'N', 'J', 0x01, 0x02, 0x03 };
    ENJIN_ASSERT_TRUE(client.SendTo(Local(48120), hello.data(),
                                    static_cast<u32>(hello.size())));

    std::vector<u8> got;
    NetworkAddress from;
    const bool arrived = PumpFor(server, client, got, from);

    // Assert
    ENJIN_ASSERT_TRUE(arrived);
    ENJIN_EXPECT_TRUE(got == hello);
    // The sender address is the TCP remote, and it must be usable to reply to.
    ENJIN_EXPECT_TRUE(from.port != 0);
    ENJIN_EXPECT_TRUE(server.GetConnectionCount() == 1);
}

ENJIN_TEST(WebSocketTransport, TheServerCanReplyToWhoeverSent) {
    WebSocketTransport server;
    if (!server.Bind(48121)) ENJIN_SKIP("could not bind TCP 48121");
    WebSocketTransport client;
    ENJIN_ASSERT_TRUE(client.Bind(0));

    const std::vector<u8> ping = { 'p', 'i', 'n', 'g' };
    client.SendTo(Local(48121), ping.data(), static_cast<u32>(ping.size()));

    std::vector<u8> got;
    NetworkAddress from;
    ENJIN_ASSERT_TRUE(PumpFor(server, client, got, from));

    // Reply to the address the message came from -- the round trip the whole
    // datagram mapping exists to support.
    const std::vector<u8> pong = { 'p', 'o', 'n', 'g' };
    ENJIN_ASSERT_TRUE(server.SendTo(from, pong.data(), static_cast<u32>(pong.size())));

    std::vector<u8> back;
    NetworkAddress backFrom;
    ENJIN_ASSERT_TRUE(PumpFor(client, server, back, backFrom));
    ENJIN_EXPECT_TRUE(back == pong);
}

ENJIN_TEST(WebSocketTransport, MessageBoundariesSurviveTheStream) {
    // The defect this guards against is the classic one when framing a stream:
    // several small sends coalescing into one read and being handed up as one
    // message, or one large send arriving in pieces and being handed up as
    // several. Both deserialize into nonsense rather than failing loudly.
    WebSocketTransport server;
    if (!server.Bind(48122)) ENJIN_SKIP("could not bind TCP 48122");
    WebSocketTransport client;
    ENJIN_ASSERT_TRUE(client.Bind(0));

    // Sizes straddling the frame-length encodings and the read chunk (8192),
    // sent back to back with no pumping in between so they land together.
    const std::vector<usize> sizes = { 1, 125, 126, 1000, 8192, 9000 };
    for (usize n : sizes) {
        std::vector<u8> payload(n);
        for (usize i = 0; i < n; i++) payload[i] = static_cast<u8>((n + i) & 0xFF);
        ENJIN_ASSERT_TRUE(client.SendTo(Local(48122), payload.data(),
                                        static_cast<u32>(payload.size())));
    }

    // Every message must come back out whole, in order, and separate.
    u8 buf[16384];
    for (usize n : sizes) {
        std::vector<u8> expect(n);
        for (usize i = 0; i < n; i++) expect[i] = static_cast<u8>((n + i) & 0xFF);

        bool got = false;
        for (int i = 0; i < 600 && !got; i++) {
            NetworkAddress s;
            const i32 r = server.ReceiveFrom(s, buf, sizeof(buf));
            if (r > 0) {
                ENJIN_ASSERT_TRUE(static_cast<usize>(r) == n);
                ENJIN_ASSERT_TRUE(std::memcmp(buf, expect.data(), n) == 0);
                got = true;
            } else {
                client.ReceiveFrom(s, buf, sizeof(buf));   // let the client pump too
            }
        }
        ENJIN_ASSERT_TRUE(got);
    }
}

ENJIN_TEST(WebSocketTransport, TwoClientsAreToldApart) {
    // Two browsers on one host. If the address mapping were not per-connection,
    // a reply meant for one would reach the other, which is the kind of bug
    // that only shows up with a second player in the room.
    WebSocketTransport server;
    if (!server.Bind(48123)) ENJIN_SKIP("could not bind TCP 48123");
    WebSocketTransport a, b;
    ENJIN_ASSERT_TRUE(a.Bind(0));
    ENJIN_ASSERT_TRUE(b.Bind(0));

    const std::vector<u8> fromA = { 'A' };
    const std::vector<u8> fromB = { 'B' };
    a.SendTo(Local(48123), fromA.data(), 1);
    b.SendTo(Local(48123), fromB.data(), 1);

    NetworkAddress addrA, addrB;
    bool haveA = false, haveB = false;
    u8 buf[256];
    for (int i = 0; i < 800 && !(haveA && haveB); i++) {
        NetworkAddress s;
        const i32 n = server.ReceiveFrom(s, buf, sizeof(buf));
        if (n == 1 && buf[0] == 'A') { addrA = s; haveA = true; }
        if (n == 1 && buf[0] == 'B') { addrB = s; haveB = true; }
        a.ReceiveFrom(s, buf, sizeof(buf));
        b.ReceiveFrom(s, buf, sizeof(buf));
    }
    ENJIN_ASSERT_TRUE(haveA && haveB);
    // Different connections, therefore different addresses.
    ENJIN_EXPECT_TRUE(!(addrA == addrB));
    ENJIN_EXPECT_TRUE(server.GetConnectionCount() == 2);
}

ENJIN_TEST(WebSocketTransport, AServerNeverDialsOut) {
    // A listening transport must not invent a connection to an address it has
    // never heard from. Doing so would turn a peer that has GONE into a
    // blocking connect attempt every time something tried to reach it.
    WebSocketTransport server;
    if (!server.Bind(48124)) ENJIN_SKIP("could not bind TCP 48124");

    const u8 payload[] = { 1, 2, 3 };
    // Nothing is listening on 48125 -- if this dialled out it would be a
    // connect to a closed port rather than an immediate false.
    ENJIN_EXPECT_FALSE(server.SendTo(Local(48125), payload, sizeof(payload)));
}

ENJIN_TEST(WebSocketTransport, ClosingReleasesThePort) {
    // Without SO_REUSEADDR a restarted server cannot re-bind its own port until
    // TIME_WAIT expires, which reads as "port in use" for minutes after a
    // clean shutdown and is the first thing anyone hits restarting a server.
    {
        WebSocketTransport first;
        if (!first.Bind(48126)) ENJIN_SKIP("could not bind TCP 48126");
        first.Close();
        ENJIN_EXPECT_FALSE(first.IsOpen());
    }
    WebSocketTransport second;
    ENJIN_EXPECT_TRUE(second.Bind(48126));
}

ENJIN_TEST_MAIN()
