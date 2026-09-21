#pragma once

// RFC 6455 handshake and frame codec, as pure functions.
//
// adr-0007 Track B step 3. A browser cannot open a UDP socket, so WebSocket is
// not one transport among several for the web tier -- it is the only one there
// is. `CreateTransport` returned nullptr on web, which meant a browser build had
// no network transport at all.
//
// Deliberately separated from the socket. Everything here is a pure function
// over bytes: no fds, no state, no platform. That is what lets the hard parts --
// the accept-key derivation, the three payload-length encodings, masking, a
// frame split across two reads -- be tested against the RFC's own published
// vectors rather than against our own idea of what we wrote.
//
// Scope: this covers what a GAME needs, which is binary frames. Text frames are
// decoded and handed up (a relay may speak JSON), fragmentation is reassembled,
// and control frames are surfaced so the transport can answer a ping and honour
// a close. Extensions and compression are not implemented and are not
// negotiated, so a client asking for permessage-deflate simply does not get it.

#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Networking {
namespace WebSocket {

// The magic GUID every RFC 6455 server appends to the client key. Not a secret
// and not configurable: it is a constant of the protocol.
inline constexpr const char* kHandshakeGUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

enum class Opcode : u8 {
    Continuation = 0x0,
    Text         = 0x1,
    Binary       = 0x2,
    Close        = 0x8,
    Ping         = 0x9,
    Pong         = 0xA,
};

// A decoded frame. `payload` is unmasked by the time you see it.
struct Frame {
    Opcode opcode = Opcode::Binary;
    bool fin = true;
    std::vector<u8> payload;
};

enum class DecodeResult {
    Ok,        // a whole frame came out; `consumed` bytes may be dropped
    NeedMore,  // a valid prefix, nothing to do but read again
    Error,     // malformed or over the size cap; close the connection
};

// Caps. A frame header can declare a 63-bit length, and believing it is how a
// single hostile handshake becomes an allocation the size of the machine. The
// limit is per FRAME and per reassembled MESSAGE, because fragmentation would
// otherwise walk past a per-frame cap in small steps.
inline constexpr u64 kMaxFramePayload   = 8u * 1024u * 1024u;
inline constexpr u64 kMaxMessagePayload = 8u * 1024u * 1024u;

// ---------------------------------------------------------------------------
// HANDSHAKE
// ---------------------------------------------------------------------------

// SHA-1 of `data`, into a 20-byte digest.
//
// SHA-1 specifically, and only here. It is cryptographically broken and is NOT
// used for anything security-bearing: RFC 6455 uses it as a fixed handshake
// transform whose only job is proving both ends speak the protocol. Session
// authentication is HMAC-SHA256 in NetworkSecurity.h and is untouched by this.
void SHA1(const u8* data, usize size, u8 outDigest[20]);

// base64(SHA1(clientKey + GUID)) -- the value of Sec-WebSocket-Accept.
std::string ComputeAcceptKey(const std::string& clientKey);

// What a parsed client handshake yielded.
struct HandshakeRequest {
    bool valid = false;         // a GET with the required Upgrade headers
    std::string key;            // Sec-WebSocket-Key, verbatim
    std::string resource;       // the request path, e.g. "/game"
};

// Parse a client's opening HTTP request. Header names are matched
// case-insensitively, because the RFC says they are case-insensitive and a
// browser is not the only thing that will ever connect.
//
// Returns valid=false rather than throwing on anything unexpected: this is the
// first thing an unauthenticated stranger sends.
HandshakeRequest ParseHandshakeRequest(const std::string& raw);

// Whether `raw` holds a complete HTTP header block yet (ends with a blank line).
// A handshake can arrive split across reads exactly like a frame can.
bool HandshakeIsComplete(const std::string& raw);

// The 101 response for a parsed request.
std::string BuildHandshakeResponse(const std::string& clientKey);

// What to send instead when the request is not a WebSocket upgrade. Answering
// with a real HTTP error rather than dropping the socket is what makes a
// mistyped URL diagnosable from the browser's own console.
std::string BuildHandshakeRejection();

// ---------------------------------------------------------------------------
// FRAMES
// ---------------------------------------------------------------------------

// Decode one frame from the front of a buffer.
//
// `consumed` is set on Ok to the number of bytes the frame occupied, so a
// caller drains its buffer by that much and calls again. Untouched otherwise.
DecodeResult DecodeFrame(const u8* data, usize size, Frame& out, usize& consumed);

// Build a frame. `mask` must be true for a CLIENT and false for a SERVER: the
// RFC requires a client to mask and forbids a server to, and a browser closes
// the connection on a masked frame from the server rather than tolerating it.
std::vector<u8> EncodeFrame(Opcode opcode, const u8* payload, usize size, bool mask);

inline std::vector<u8> EncodeFrame(Opcode opcode, const std::vector<u8>& payload, bool mask) {
    return EncodeFrame(opcode, payload.data(), payload.size(), mask);
}

} // namespace WebSocket
} // namespace Networking
} // namespace Enjin
