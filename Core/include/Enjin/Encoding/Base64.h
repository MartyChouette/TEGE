#pragma once

// Base64, in one place.
//
// This lived as a file-local copy inside VoxelFieldCodec.cpp until the
// WebSocket handshake needed the encoder too (RFC 6455 requires the SHA-1 of
// the client key, base64'd). A second hand-written copy is the shape of defect
// this codebase keeps paying for, so it moved here rather than being written
// twice. Header-only because it is thirty lines and both callers are in
// different libraries.
//
// Encode produces standard base64 with '=' padding. Decode SKIPS padding and
// newlines and REJECTS anything else, so a corrupt string fails rather than
// decoding to something plausible -- which matters for both callers: a scene
// field and an untrusted handshake header.

#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Encoding {

namespace detail {

inline constexpr char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// -1 for anything that is not a base64 digit.
inline i32 Base64DecodeChar(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

} // namespace detail

inline std::string Base64Encode(const u8* bytes, usize count) {
    using detail::kBase64Alphabet;
    std::string out;
    out.reserve((count + 2) / 3 * 4);
    usize i = 0;
    while (i + 2 < count) {
        const u32 n = (static_cast<u32>(bytes[i]) << 16) |
                      (static_cast<u32>(bytes[i + 1]) << 8) |
                      static_cast<u32>(bytes[i + 2]);
        out += kBase64Alphabet[(n >> 18) & 63];
        out += kBase64Alphabet[(n >> 12) & 63];
        out += kBase64Alphabet[(n >> 6) & 63];
        out += kBase64Alphabet[n & 63];
        i += 3;
    }
    const usize rest = count - i;
    if (rest == 1) {
        const u32 n = static_cast<u32>(bytes[i]) << 16;
        out += kBase64Alphabet[(n >> 18) & 63];
        out += kBase64Alphabet[(n >> 12) & 63];
        out += "==";
    } else if (rest == 2) {
        const u32 n = (static_cast<u32>(bytes[i]) << 16) |
                      (static_cast<u32>(bytes[i + 1]) << 8);
        out += kBase64Alphabet[(n >> 18) & 63];
        out += kBase64Alphabet[(n >> 12) & 63];
        out += kBase64Alphabet[(n >> 6) & 63];
        out += '=';
    }
    return out;
}

inline std::string Base64Encode(const std::vector<u8>& bytes) {
    return Base64Encode(bytes.data(), bytes.size());
}

inline bool Base64Decode(const std::string& text, std::vector<u8>& out) {
    out.clear();
    out.reserve(text.size() / 4 * 3);

    u32 acc = 0;
    u32 bits = 0;
    for (char c : text) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        const i32 d = detail::Base64DecodeChar(c);
        if (d < 0) return false;
        acc = (acc << 6) | static_cast<u32>(d);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<u8>((acc >> bits) & 0xFFu));
        }
    }
    return true;
}

} // namespace Encoding
} // namespace Enjin
