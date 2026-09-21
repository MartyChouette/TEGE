#include "Enjin/Networking/WebSocketProtocol.h"
#include "Enjin/Encoding/Base64.h"
#include <algorithm>
#include <cstring>
#include <random>

namespace Enjin {
namespace Networking {
namespace WebSocket {

namespace {

u32 RotL(u32 v, u32 n) { return (v << n) | (v >> (32 - n)); }

// Lowercase ASCII only. Header names are ASCII by definition, and a
// locale-aware tolower is a well-known way to get a surprise in a Turkish
// locale (dotless i), which is not a trade worth making for a header match.
char LowerASCII(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string LowerASCII(const std::string& s) {
    std::string out(s);
    for (char& c : out) c = LowerASCII(c);
    return out;
}

std::string Trim(const std::string& s) {
    usize b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t')) b++;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) e--;
    return s.substr(b, e - b);
}

} // namespace

// ---------------------------------------------------------------------------
// SHA-1
// ---------------------------------------------------------------------------

void SHA1(const u8* data, usize size, u8 outDigest[20]) {
    u32 h[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };

    // Message + 0x80 + zero pad + 64-bit big-endian bit length, to a multiple
    // of 64 bytes. Built as one buffer rather than streamed: every input here
    // is a handshake key plus a 36-byte GUID, so it is always under 128 bytes.
    std::vector<u8> msg(data, data + size);
    const u64 bitLen = static_cast<u64>(size) * 8u;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0x00);
    for (int i = 7; i >= 0; i--) msg.push_back(static_cast<u8>((bitLen >> (i * 8)) & 0xFF));

    for (usize off = 0; off < msg.size(); off += 64) {
        u32 w[80];
        for (int i = 0; i < 16; i++) {
            w[i] = (static_cast<u32>(msg[off + i * 4 + 0]) << 24) |
                   (static_cast<u32>(msg[off + i * 4 + 1]) << 16) |
                   (static_cast<u32>(msg[off + i * 4 + 2]) << 8) |
                    static_cast<u32>(msg[off + i * 4 + 3]);
        }
        for (int i = 16; i < 80; i++) {
            w[i] = RotL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        u32 a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            u32 f, k;
            if (i < 20)      { f = (b & c) | ((~b) & d);        k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                   k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                   k = 0xCA62C1D6u; }

            const u32 tmp = RotL(a, 5) + f + e + k + w[i];
            e = d; d = c; c = RotL(b, 30); b = a; a = tmp;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }

    for (int i = 0; i < 5; i++) {
        outDigest[i * 4 + 0] = static_cast<u8>((h[i] >> 24) & 0xFF);
        outDigest[i * 4 + 1] = static_cast<u8>((h[i] >> 16) & 0xFF);
        outDigest[i * 4 + 2] = static_cast<u8>((h[i] >> 8) & 0xFF);
        outDigest[i * 4 + 3] = static_cast<u8>(h[i] & 0xFF);
    }
}

std::string ComputeAcceptKey(const std::string& clientKey) {
    const std::string combined = clientKey + kHandshakeGUID;
    u8 digest[20];
    SHA1(reinterpret_cast<const u8*>(combined.data()), combined.size(), digest);
    return Encoding::Base64Encode(digest, 20);
}

// ---------------------------------------------------------------------------
// HANDSHAKE
// ---------------------------------------------------------------------------

bool HandshakeIsComplete(const std::string& raw) {
    return raw.find("\r\n\r\n") != std::string::npos;
}

HandshakeRequest ParseHandshakeRequest(const std::string& raw) {
    HandshakeRequest req;

    const usize headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return req;   // not complete; caller checks first

    // Request line: GET <resource> HTTP/1.1
    const usize firstEOL = raw.find("\r\n");
    const std::string requestLine = raw.substr(0, firstEOL);
    if (requestLine.rfind("GET ", 0) != 0) return req;
    {
        const usize sp = requestLine.find(' ', 4);
        req.resource = (sp == std::string::npos) ? "/" : requestLine.substr(4, sp - 4);
    }

    bool haveUpgrade = false, haveConnection = false, haveVersion = false;

    usize pos = firstEOL + 2;
    while (pos < headerEnd) {
        const usize eol = raw.find("\r\n", pos);
        if (eol == std::string::npos || eol > headerEnd) break;
        const std::string line = raw.substr(pos, eol - pos);
        pos = eol + 2;

        const usize colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string name = LowerASCII(Trim(line.substr(0, colon)));
        const std::string value = Trim(line.substr(colon + 1));

        if (name == "sec-websocket-key") {
            req.key = value;
        } else if (name == "upgrade") {
            // Value is case-insensitive, and a browser sends "websocket".
            haveUpgrade = LowerASCII(value).find("websocket") != std::string::npos;
        } else if (name == "connection") {
            // Comma-separated token list, and "Upgrade" is one token among
            // several through a proxy ("keep-alive, Upgrade"), so this is a
            // substring test rather than an equality test on purpose.
            haveConnection = LowerASCII(value).find("upgrade") != std::string::npos;
        } else if (name == "sec-websocket-version") {
            // 13 is the only version RFC 6455 defines. Anything else is either
            // a draft from before the RFC or a probe, and both get refused.
            haveVersion = (value == "13");
        }
    }

    req.valid = haveUpgrade && haveConnection && haveVersion && !req.key.empty();
    return req;
}

std::string BuildHandshakeResponse(const std::string& clientKey) {
    return "HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Accept: " + ComputeAcceptKey(clientKey) + "\r\n"
           "\r\n";
}

std::string BuildHandshakeRejection() {
    return "HTTP/1.1 400 Bad Request\r\n"
           "Content-Type: text/plain\r\n"
           "Content-Length: 44\r\n"
           "Connection: close\r\n"
           "\r\n"
           "This port speaks WebSocket, not plain HTTP.\n";
}

// ---------------------------------------------------------------------------
// FRAMES
// ---------------------------------------------------------------------------

DecodeResult DecodeFrame(const u8* data, usize size, Frame& out, usize& consumed) {
    if (size < 2) return DecodeResult::NeedMore;

    const u8 b0 = data[0];
    const u8 b1 = data[1];

    out.fin = (b0 & 0x80) != 0;
    // RSV1-3 must be zero: they only mean anything under a negotiated
    // extension, and this server negotiates none. Set bits mean the peer
    // believes in an extension we never agreed to, so the stream is no longer
    // one we can read.
    if ((b0 & 0x70) != 0) return DecodeResult::Error;

    const u8 rawOpcode = b0 & 0x0F;
    switch (rawOpcode) {
        case 0x0: case 0x1: case 0x2: case 0x8: case 0x9: case 0xA:
            out.opcode = static_cast<Opcode>(rawOpcode);
            break;
        default:
            return DecodeResult::Error;   // reserved opcode
    }

    const bool masked = (b1 & 0x80) != 0;
    u64 payloadLen = b1 & 0x7F;
    usize offset = 2;

    if (payloadLen == 126) {
        if (size < offset + 2) return DecodeResult::NeedMore;
        payloadLen = (static_cast<u64>(data[offset]) << 8) | data[offset + 1];
        offset += 2;
    } else if (payloadLen == 127) {
        if (size < offset + 8) return DecodeResult::NeedMore;
        payloadLen = 0;
        for (int i = 0; i < 8; i++) payloadLen = (payloadLen << 8) | data[offset + i];
        offset += 8;
        // The top bit MUST be clear per the RFC, and the cap below would catch
        // it anyway -- but rejecting it here keeps the shift above from being
        // the thing anyone has to reason about.
        if (payloadLen & 0x8000000000000000ull) return DecodeResult::Error;
    }

    // Checked BEFORE reserving anything. A frame header is 14 bytes and can
    // claim a petabyte; taking it at its word is the whole attack.
    if (payloadLen > kMaxFramePayload) return DecodeResult::Error;

    // A control frame must be short and must not be fragmented. Both are RFC
    // requirements and both keep a control frame from being a smuggling route.
    if (rawOpcode >= 0x8 && (payloadLen > 125 || !out.fin)) return DecodeResult::Error;

    u8 maskKey[4] = { 0, 0, 0, 0 };
    if (masked) {
        if (size < offset + 4) return DecodeResult::NeedMore;
        std::memcpy(maskKey, data + offset, 4);
        offset += 4;
    }

    if (size < offset + payloadLen) return DecodeResult::NeedMore;

    out.payload.assign(data + offset, data + offset + payloadLen);
    if (masked) {
        for (usize i = 0; i < out.payload.size(); i++) {
            out.payload[i] ^= maskKey[i % 4];
        }
    }

    consumed = offset + static_cast<usize>(payloadLen);
    return DecodeResult::Ok;
}

std::vector<u8> EncodeFrame(Opcode opcode, const u8* payload, usize size, bool mask) {
    std::vector<u8> out;
    out.reserve(size + 14);

    out.push_back(static_cast<u8>(0x80 | static_cast<u8>(opcode)));   // FIN + opcode

    const u8 maskBit = mask ? 0x80 : 0x00;
    if (size < 126) {
        out.push_back(static_cast<u8>(maskBit | size));
    } else if (size <= 0xFFFF) {
        out.push_back(static_cast<u8>(maskBit | 126));
        out.push_back(static_cast<u8>((size >> 8) & 0xFF));
        out.push_back(static_cast<u8>(size & 0xFF));
    } else {
        out.push_back(static_cast<u8>(maskBit | 127));
        for (int i = 7; i >= 0; i--) {
            out.push_back(static_cast<u8>((static_cast<u64>(size) >> (i * 8)) & 0xFF));
        }
    }

    if (mask) {
        // Per-frame random key. The RFC requires it to be unpredictable: its
        // job is to stop a client from being steered into emitting bytes that
        // a broken intermediary would read as a request of its own.
        static thread_local std::mt19937 rng{std::random_device{}()};
        u8 key[4];
        for (int i = 0; i < 4; i++) key[i] = static_cast<u8>(rng() & 0xFF);
        out.insert(out.end(), key, key + 4);
        for (usize i = 0; i < size; i++) {
            out.push_back(static_cast<u8>(payload[i] ^ key[i % 4]));
        }
    } else {
        out.insert(out.end(), payload, payload + size);
    }

    return out;
}

} // namespace WebSocket
} // namespace Networking
} // namespace Enjin
