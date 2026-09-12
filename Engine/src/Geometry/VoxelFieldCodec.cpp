#include "Enjin/Geometry/VoxelFieldCodec.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Geometry {

namespace {

constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// -1 for anything that is not a base64 digit, so a corrupt string is rejected
// rather than silently decoding to something plausible.
i32 DecodeChar(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::string Base64Encode(const std::vector<u8>& bytes) {
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    usize i = 0;
    while (i + 2 < bytes.size()) {
        const u32 n = (static_cast<u32>(bytes[i]) << 16) |
                      (static_cast<u32>(bytes[i + 1]) << 8) |
                      static_cast<u32>(bytes[i + 2]);
        out += kAlphabet[(n >> 18) & 63];
        out += kAlphabet[(n >> 12) & 63];
        out += kAlphabet[(n >> 6) & 63];
        out += kAlphabet[n & 63];
        i += 3;
    }
    const usize rest = bytes.size() - i;
    if (rest == 1) {
        const u32 n = static_cast<u32>(bytes[i]) << 16;
        out += kAlphabet[(n >> 18) & 63];
        out += kAlphabet[(n >> 12) & 63];
        out += "==";
    } else if (rest == 2) {
        const u32 n = (static_cast<u32>(bytes[i]) << 16) |
                      (static_cast<u32>(bytes[i + 1]) << 8);
        out += kAlphabet[(n >> 18) & 63];
        out += kAlphabet[(n >> 12) & 63];
        out += kAlphabet[(n >> 6) & 63];
        out += '=';
    }
    return out;
}

bool Base64Decode(const std::string& text, std::vector<u8>& out) {
    out.clear();
    out.reserve(text.size() / 4 * 3);

    u32 acc = 0;
    u32 bits = 0;
    for (char c : text) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        const i32 d = DecodeChar(c);
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

} // namespace

std::string EncodeVoxelField(const std::vector<f32>& field, f32 band) {
    if (field.empty() || band <= 0.0f) return std::string();

    // Quantize, then run-length encode.
    //
    // Runs are capped at 255 because the count is one byte. A cap is not a
    // limitation worth widening: the long runs are thousands of samples and
    // splitting one into chunks of 255 costs two bytes each, against a field
    // that would otherwise be four bytes per sample.
    std::vector<u8> packed;
    packed.reserve(field.size() / 8 + 16);

    auto quantize = [&](f32 v) -> u8 {
        const f32 clamped = std::max(-band, std::min(band, v));
        const f32 scaled = (clamped / band) * 127.0f;
        const i32 q = static_cast<i32>(std::lround(scaled));
        return static_cast<u8>(std::max(-127, std::min(127, q)) + 128);
    };

    u8 run = quantize(field[0]);
    u32 count = 1;
    for (usize i = 1; i < field.size(); ++i) {
        const u8 q = quantize(field[i]);
        if (q == run && count < 255) {
            ++count;
            continue;
        }
        packed.push_back(static_cast<u8>(count));
        packed.push_back(run);
        run = q;
        count = 1;
    }
    packed.push_back(static_cast<u8>(count));
    packed.push_back(run);

    return Base64Encode(packed);
}

bool DecodeVoxelField(const std::string& text, f32 band, usize expectedCount,
                      std::vector<f32>& out) {
    if (text.empty() || band <= 0.0f || expectedCount == 0) return false;

    std::vector<u8> packed;
    if (!Base64Decode(text, packed)) return false;
    if (packed.empty() || (packed.size() % 2) != 0) return false;

    std::vector<f32> decoded;
    decoded.reserve(expectedCount);
    for (usize i = 0; i + 1 < packed.size(); i += 2) {
        const u32 count = packed[i];
        const f32 value = (static_cast<f32>(static_cast<i32>(packed[i + 1]) - 128) / 127.0f) * band;
        if (count == 0) return false;                       // a run of nothing is corrupt
        if (decoded.size() + count > expectedCount) return false;   // and so is one too long
        decoded.insert(decoded.end(), count, value);
    }

    // Exactly the right number of samples, or nothing. A short field is a
    // corrupt scene; padding it would put a wall somewhere nobody carved one.
    if (decoded.size() != expectedCount) return false;

    out = std::move(decoded);
    return true;
}

} // namespace Geometry
} // namespace Enjin
