#include "EnjinTest.h"
#include "Enjin/Networking/WebSocketProtocol.h"
#include <cstring>
#include <string>

using namespace Enjin;
using namespace Enjin::Networking;
using namespace Enjin::Networking::WebSocket;

// ============================================================================
// RFC 6455, CHECKED AGAINST THE RFC
//
// adr-0007 Track B step 3. Every vector in here is copied from RFC 6455 itself
// (sections 1.2, 1.3 and 5.7), not produced by running this code and writing
// down what came out. That distinction is the entire value of the file: a test
// built from our own output proves the encoder agrees with itself, which it
// would also do if it were wrong, and the peer on the other end is a BROWSER
// that will not be similarly agreeable.
// ============================================================================

namespace {

std::string Str(const std::vector<u8>& v) {
    return std::string(reinterpret_cast<const char*>(v.data()), v.size());
}

} // namespace

// ---------------------------------------------------------------------------
// HANDSHAKE
// ---------------------------------------------------------------------------

ENJIN_TEST(WebSocketHandshake, AcceptKeyMatchesTheRFCExample) {
    // RFC 6455 section 1.3, verbatim: this key must produce this accept value.
    // Get this wrong by one bit and every browser refuses the connection with a
    // message about the accept header, which is at least an honest failure.
    ENJIN_EXPECT_TRUE(ComputeAcceptKey("dGhlIHNhbXBsZSBub25jZQ==")
                      == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

ENJIN_TEST(WebSocketHandshake, SHA1MatchesKnownDigests) {
    // The two canonical SHA-1 vectors. SHA-1 is used here ONLY as RFC 6455's
    // fixed handshake transform and carries no security weight, but it still
    // has to be SHA-1 and not something that merely looks like it.
    auto hex = [](const u8* d) {
        static const char* k = "0123456789abcdef";
        std::string s;
        for (int i = 0; i < 20; i++) { s += k[d[i] >> 4]; s += k[d[i] & 15]; }
        return s;
    };
    u8 d[20];
    SHA1(reinterpret_cast<const u8*>("abc"), 3, d);
    ENJIN_EXPECT_TRUE(hex(d) == "a9993e364706816aba3e25717850c26c9cd0d89d");

    SHA1(reinterpret_cast<const u8*>(""), 0, d);
    ENJIN_EXPECT_TRUE(hex(d) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

ENJIN_TEST(WebSocketHandshake, ABrowserRequestParses) {
    // RFC 6455 section 1.3's client handshake.
    const std::string raw =
        "GET /chat HTTP/1.1\r\n"
        "Host: server.example.com\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";

    ENJIN_ASSERT_TRUE(HandshakeIsComplete(raw));
    const HandshakeRequest req = ParseHandshakeRequest(raw);
    ENJIN_EXPECT_TRUE(req.valid);
    ENJIN_EXPECT_TRUE(req.key == "dGhlIHNhbXBsZSBub25jZQ==");
    ENJIN_EXPECT_TRUE(req.resource == "/chat");
}

ENJIN_TEST(WebSocketHandshake, HeaderNamesAreCaseInsensitive) {
    // The RFC says header names are case-insensitive, and a browser is not the
    // only client that will ever connect. Matching them exactly is a bug that
    // only shows up against whichever tool spells them differently.
    const std::string raw =
        "GET / HTTP/1.1\r\n"
        "UPGRADE: WebSocket\r\n"
        "CONNECTION: keep-alive, Upgrade\r\n"
        "sec-websocket-key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    const HandshakeRequest req = ParseHandshakeRequest(raw);
    // "keep-alive, Upgrade" is what a connection through a proxy looks like,
    // and an equality test on that value rejects it.
    ENJIN_EXPECT_TRUE(req.valid);
}

ENJIN_TEST(WebSocketHandshake, APartialRequestIsNotYetComplete) {
    // A handshake can arrive split across reads exactly like a frame can.
    const std::string partial =
        "GET / HTTP/1.1\r\n"
        "Upgrade: websocket\r\n";
    ENJIN_EXPECT_FALSE(HandshakeIsComplete(partial));
    ENJIN_EXPECT_FALSE(ParseHandshakeRequest(partial).valid);
}

ENJIN_TEST(WebSocketHandshake, NonWebSocketRequestsAreRefused) {
    // This is the first thing an unauthenticated stranger sends, so each of
    // these must come back invalid rather than crashing or half-accepting.
    const std::string plainHttp =
        "GET / HTTP/1.1\r\nHost: x\r\n\r\n";
    ENJIN_EXPECT_FALSE(ParseHandshakeRequest(plainHttp).valid);

    const std::string wrongVersion =
        "GET / HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 8\r\n\r\n";
    ENJIN_EXPECT_FALSE(ParseHandshakeRequest(wrongVersion).valid);

    const std::string noKey =
        "GET / HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    ENJIN_EXPECT_FALSE(ParseHandshakeRequest(noKey).valid);

    ENJIN_EXPECT_FALSE(ParseHandshakeRequest("").valid);

    // A POST is not an upgrade however well-formed the rest of it is.
    const std::string post =
        "POST / HTTP/1.1\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n";
    ENJIN_EXPECT_FALSE(ParseHandshakeRequest(post).valid);
}

ENJIN_TEST(WebSocketHandshake, TheResponseCarriesTheAcceptHeader) {
    const std::string res = BuildHandshakeResponse("dGhlIHNhbXBsZSBub25jZQ==");
    ENJIN_EXPECT_TRUE(res.rfind("HTTP/1.1 101", 0) == 0);
    ENJIN_EXPECT_TRUE(res.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n")
                      != std::string::npos);
    // Must end with the blank line, or the browser waits for more headers.
    ENJIN_EXPECT_TRUE(res.size() >= 4 && res.compare(res.size() - 4, 4, "\r\n\r\n") == 0);
}

// ---------------------------------------------------------------------------
// FRAMES
// ---------------------------------------------------------------------------

ENJIN_TEST(WebSocketFrames, DecodesTheRFCUnmaskedHelloFrame) {
    // RFC 6455 section 5.7: a single-frame unmasked text message "Hello".
    const u8 bytes[] = { 0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f };
    Frame f;
    usize consumed = 0;
    ENJIN_ASSERT_TRUE(DecodeFrame(bytes, sizeof(bytes), f, consumed) == DecodeResult::Ok);
    ENJIN_EXPECT_TRUE(f.opcode == Opcode::Text);
    ENJIN_EXPECT_TRUE(f.fin);
    ENJIN_EXPECT_TRUE(Str(f.payload) == "Hello");
    ENJIN_EXPECT_EQ(consumed, sizeof(bytes));
}

ENJIN_TEST(WebSocketFrames, DecodesTheRFCMaskedHelloFrame) {
    // RFC 6455 section 5.7: the same message MASKED, which is what every
    // browser actually sends. Unmasking is not optional and the payload is
    // gibberish without it.
    const u8 bytes[] = { 0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d,
                         0x7f, 0x9f, 0x4d, 0x51, 0x58 };
    Frame f;
    usize consumed = 0;
    ENJIN_ASSERT_TRUE(DecodeFrame(bytes, sizeof(bytes), f, consumed) == DecodeResult::Ok);
    ENJIN_EXPECT_TRUE(Str(f.payload) == "Hello");
    ENJIN_EXPECT_EQ(consumed, sizeof(bytes));
}

ENJIN_TEST(WebSocketFrames, AServerFrameIsNotMasked) {
    // The RFC forbids a server to mask, and a browser closes the connection on
    // a masked server frame rather than tolerating it.
    const std::vector<u8> payload = { 'h', 'i' };
    const std::vector<u8> encoded = EncodeFrame(Opcode::Binary, payload, /*mask*/ false);
    ENJIN_ASSERT_TRUE(encoded.size() == 4);
    ENJIN_EXPECT_EQ(encoded[0], static_cast<u8>(0x82));   // FIN + binary
    ENJIN_EXPECT_EQ(encoded[1], static_cast<u8>(0x02));   // no mask bit, length 2
}

ENJIN_TEST(WebSocketFrames, RoundTripsAcrossAllThreeLengthEncodings) {
    // The three payload-length forms are the classic place to be off by a
    // byte, and the boundaries are where it happens. 125/126 and 65535/65536
    // are the exact transitions.
    const usize sizes[] = { 0, 1, 125, 126, 127, 65535, 65536, 70000 };
    for (usize n : sizes) {
        std::vector<u8> payload(n);
        for (usize i = 0; i < n; i++) payload[i] = static_cast<u8>(i * 31 + 7);

        for (bool mask : { false, true }) {
            const std::vector<u8> encoded = EncodeFrame(Opcode::Binary, payload, mask);
            Frame f;
            usize consumed = 0;
            ENJIN_ASSERT_TRUE(DecodeFrame(encoded.data(), encoded.size(), f, consumed)
                              == DecodeResult::Ok);
            ENJIN_ASSERT_TRUE(f.payload.size() == n);
            ENJIN_EXPECT_TRUE(f.payload == payload);
            ENJIN_EXPECT_EQ(consumed, encoded.size());
        }
    }
}

ENJIN_TEST(WebSocketFrames, APartialFrameAsksForMoreRatherThanFailing) {
    // TCP is a stream: a frame arrives in as many pieces as the network feels
    // like. Every prefix of a valid frame must read as NeedMore, because
    // treating one as an error drops a healthy connection.
    const std::vector<u8> full = EncodeFrame(Opcode::Binary, std::vector<u8>(300, 0xAB), true);
    for (usize cut = 0; cut < full.size(); cut++) {
        Frame f;
        usize consumed = 0;
        const DecodeResult r = DecodeFrame(full.data(), cut, f, consumed);
        ENJIN_ASSERT_TRUE(r == DecodeResult::NeedMore);
    }
    Frame f;
    usize consumed = 0;
    ENJIN_EXPECT_TRUE(DecodeFrame(full.data(), full.size(), f, consumed) == DecodeResult::Ok);
}

ENJIN_TEST(WebSocketFrames, TwoFramesInOneBufferDecodeInOrder) {
    // A single read can carry several frames. `consumed` is what lets the
    // caller find the second one; returning the payload without it would make
    // every frame after the first unreachable.
    std::vector<u8> buf = EncodeFrame(Opcode::Binary, std::vector<u8>{ 1, 2, 3 }, true);
    const std::vector<u8> second = EncodeFrame(Opcode::Text, std::vector<u8>{ 'o', 'k' }, true);
    buf.insert(buf.end(), second.begin(), second.end());

    Frame f1, f2;
    usize c1 = 0, c2 = 0;
    ENJIN_ASSERT_TRUE(DecodeFrame(buf.data(), buf.size(), f1, c1) == DecodeResult::Ok);
    ENJIN_ASSERT_TRUE(DecodeFrame(buf.data() + c1, buf.size() - c1, f2, c2) == DecodeResult::Ok);
    ENJIN_EXPECT_TRUE(f1.payload == (std::vector<u8>{ 1, 2, 3 }));
    ENJIN_EXPECT_TRUE(f2.opcode == Opcode::Text);
    ENJIN_EXPECT_TRUE(Str(f2.payload) == "ok");
    ENJIN_EXPECT_EQ(c1 + c2, buf.size());
}

ENJIN_TEST(WebSocketFrames, ControlFramesSurvive) {
    // Ping/Pong/Close have to reach the transport or the browser eventually
    // gives up on a connection that is actually healthy.
    for (Opcode op : { Opcode::Ping, Opcode::Pong, Opcode::Close }) {
        const std::vector<u8> encoded = EncodeFrame(op, std::vector<u8>{ 0x03, 0xE8 }, false);
        Frame f;
        usize consumed = 0;
        ENJIN_ASSERT_TRUE(DecodeFrame(encoded.data(), encoded.size(), f, consumed)
                          == DecodeResult::Ok);
        ENJIN_EXPECT_TRUE(f.opcode == op);
    }
}

// ---------------------------------------------------------------------------
// HOSTILE INPUT
//
// Everything above arrives from an unauthenticated stranger before any session
// key exists. None of it may be believed.
// ---------------------------------------------------------------------------

ENJIN_TEST(WebSocketFrames, AHugeDeclaredLengthIsRefusedNotAllocated) {
    // A 14-byte header can claim a petabyte. Believing it is the whole attack:
    // the cap has to be checked BEFORE anything is reserved.
    u8 bytes[14] = { 0x82, 0xFF };
    for (int i = 0; i < 8; i++) bytes[2 + i] = 0xFF;   // 2^64-1 payload
    Frame f;
    usize consumed = 0;
    ENJIN_EXPECT_TRUE(DecodeFrame(bytes, sizeof(bytes), f, consumed) == DecodeResult::Error);

    // And a merely large one, still well over the cap.
    u8 big[14] = { 0x82, 0x7F, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };
    ENJIN_EXPECT_TRUE(DecodeFrame(big, sizeof(big), f, consumed) == DecodeResult::Error);
}

ENJIN_TEST(WebSocketFrames, ReservedBitsAndOpcodesAreRefused) {
    Frame f;
    usize consumed = 0;

    // RSV1 set with no negotiated extension: the peer believes in a protocol
    // we never agreed to, so the stream is no longer one we can read.
    const u8 rsv[] = { 0xC1, 0x00 };
    ENJIN_EXPECT_TRUE(DecodeFrame(rsv, sizeof(rsv), f, consumed) == DecodeResult::Error);

    // Reserved opcode 0x3.
    const u8 badOp[] = { 0x83, 0x00 };
    ENJIN_EXPECT_TRUE(DecodeFrame(badOp, sizeof(badOp), f, consumed) == DecodeResult::Error);
}

ENJIN_TEST(WebSocketFrames, OversizedOrFragmentedControlFramesAreRefused) {
    Frame f;
    usize consumed = 0;

    // A control frame must be <= 125 bytes and must not be fragmented. Both
    // rules exist so a control frame cannot become a smuggling route.
    const u8 longPing[] = { 0x89, 0x7E, 0x01, 0x00 };   // ping, 256 bytes
    ENJIN_EXPECT_TRUE(DecodeFrame(longPing, sizeof(longPing), f, consumed) == DecodeResult::Error);

    const u8 fragmentedClose[] = { 0x08, 0x00 };        // FIN clear on a close
    ENJIN_EXPECT_TRUE(DecodeFrame(fragmentedClose, sizeof(fragmentedClose), f, consumed)
                      == DecodeResult::Error);
}

ENJIN_TEST(WebSocketFrames, EmptyBufferIsNeedMoreNotError) {
    Frame f;
    usize consumed = 0;
    ENJIN_EXPECT_TRUE(DecodeFrame(nullptr, 0, f, consumed) == DecodeResult::NeedMore);
}

ENJIN_TEST_MAIN()
