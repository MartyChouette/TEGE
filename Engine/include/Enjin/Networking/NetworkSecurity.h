#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <array>
#include <cstring>
#include <string>

namespace Enjin {
namespace Networking {

// ============================================================================
// SESSION KEY (32-byte shared secret)
// ============================================================================

static constexpr u32 SESSION_KEY_SIZE = 32;
static constexpr u32 HMAC_TAG_SIZE = 32;

using SessionKey = std::array<u8, SESSION_KEY_SIZE>;

// ============================================================================
// REPLAY PROTECTION (sliding window)
// ============================================================================

// Replay protection using a 64-bit sliding window over u32 sequence numbers.
// Packets with sequence numbers outside the window or already received are
// rejected.
struct ReplayWindow {
    u32 lastSequence = 0;       // Highest accepted sequence number
    u64 receivedBitmask = 0;    // Bits 0..63 represent lastSequence-1 .. lastSequence-64
    bool initialized = false;   // First packet always accepted

    // Returns true if the packet with the given sequence should be accepted.
    // If accepted, the window state is updated atomically.
    bool Accept(u32 sequence) {
        if (!initialized) {
            lastSequence = sequence;
            receivedBitmask = 0;
            initialized = true;
            return true;
        }

        // Compute signed difference (handles wraparound)
        i32 diff = static_cast<i32>(sequence - lastSequence);

        if (diff > 0) {
            // New packet ahead of window — slide forward
            if (diff < 64) {
                receivedBitmask = (receivedBitmask << diff) | 1;
            } else {
                // Large jump — reset window
                receivedBitmask = 1;
            }
            lastSequence = sequence;
            return true;
        } else if (diff == 0) {
            // Duplicate of the most recent packet
            return false;
        } else {
            // Packet is behind the current sequence
            u32 age = static_cast<u32>(-diff);
            if (age > 64) {
                // Too old — outside the window
                return false;
            }
            // Check if already received
            u64 bit = 1ULL << age;
            if (receivedBitmask & bit) {
                return false;  // Already received (replay)
            }
            // Mark as received
            receivedBitmask |= bit;
            return true;
        }
    }

    void Reset() {
        lastSequence = 0;
        receivedBitmask = 0;
        initialized = false;
    }
};

// ============================================================================
// SHA-256 (compact standalone implementation)
// ============================================================================

class SHA256 {
public:
    SHA256() { Reset(); }

    void Reset() {
        m_DataLength = 0;
        m_BitLength = 0;
        m_State[0] = 0x6a09e667;
        m_State[1] = 0xbb67ae85;
        m_State[2] = 0x3c6ef372;
        m_State[3] = 0xa54ff53a;
        m_State[4] = 0x510e527f;
        m_State[5] = 0x9b05688c;
        m_State[6] = 0x1f83d9ab;
        m_State[7] = 0x5be0cd19;
    }

    void Update(const u8* data, u32 length) {
        for (u32 i = 0; i < length; i++) {
            m_Data[m_DataLength] = data[i];
            m_DataLength++;
            if (m_DataLength == 64) {
                Transform();
                m_BitLength += 512;
                m_DataLength = 0;
            }
        }
    }

    void Final(u8 hash[32]) {
        u32 i = m_DataLength;

        // Pad message
        if (m_DataLength < 56) {
            m_Data[i++] = 0x80;
            while (i < 56) m_Data[i++] = 0x00;
        } else {
            m_Data[i++] = 0x80;
            while (i < 64) m_Data[i++] = 0x00;
            Transform();
            std::memset(m_Data, 0, 56);
        }

        // Append total bit length
        m_BitLength += m_DataLength * 8;
        m_Data[63] = static_cast<u8>(m_BitLength);
        m_Data[62] = static_cast<u8>(m_BitLength >> 8);
        m_Data[61] = static_cast<u8>(m_BitLength >> 16);
        m_Data[60] = static_cast<u8>(m_BitLength >> 24);
        m_Data[59] = static_cast<u8>(m_BitLength >> 32);
        m_Data[58] = static_cast<u8>(m_BitLength >> 40);
        m_Data[57] = static_cast<u8>(m_BitLength >> 48);
        m_Data[56] = static_cast<u8>(m_BitLength >> 56);
        Transform();

        // Produce big-endian output
        for (u32 j = 0; j < 8; j++) {
            hash[j * 4 + 0] = static_cast<u8>((m_State[j] >> 24) & 0xFF);
            hash[j * 4 + 1] = static_cast<u8>((m_State[j] >> 16) & 0xFF);
            hash[j * 4 + 2] = static_cast<u8>((m_State[j] >> 8) & 0xFF);
            hash[j * 4 + 3] = static_cast<u8>((m_State[j]) & 0xFF);
        }
    }

    // Convenience: hash a single buffer
    static void Hash(const u8* data, u32 length, u8 hash[32]) {
        SHA256 ctx;
        ctx.Update(data, length);
        ctx.Final(hash);
    }

private:
    void Transform() {
        static constexpr u32 K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
            0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
            0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
            0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
            0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
            0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
            0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
            0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
            0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        u32 w[64];
        for (u32 i = 0; i < 16; i++) {
            w[i] = (static_cast<u32>(m_Data[i * 4]) << 24) |
                   (static_cast<u32>(m_Data[i * 4 + 1]) << 16) |
                   (static_cast<u32>(m_Data[i * 4 + 2]) << 8) |
                   (static_cast<u32>(m_Data[i * 4 + 3]));
        }
        for (u32 i = 16; i < 64; i++) {
            u32 s0 = RotR(w[i - 15], 7) ^ RotR(w[i - 15], 18) ^ (w[i - 15] >> 3);
            u32 s1 = RotR(w[i - 2], 17) ^ RotR(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        u32 a = m_State[0], b = m_State[1], c = m_State[2], d = m_State[3];
        u32 e = m_State[4], f = m_State[5], g = m_State[6], h = m_State[7];

        for (u32 i = 0; i < 64; i++) {
            u32 S1 = RotR(e, 6) ^ RotR(e, 11) ^ RotR(e, 25);
            u32 ch = (e & f) ^ (~e & g);
            u32 temp1 = h + S1 + ch + K[i] + w[i];
            u32 S0 = RotR(a, 2) ^ RotR(a, 13) ^ RotR(a, 22);
            u32 maj = (a & b) ^ (a & c) ^ (b & c);
            u32 temp2 = S0 + maj;

            h = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }

        m_State[0] += a; m_State[1] += b; m_State[2] += c; m_State[3] += d;
        m_State[4] += e; m_State[5] += f; m_State[6] += g; m_State[7] += h;
    }

    static u32 RotR(u32 x, u32 n) { return (x >> n) | (x << (32 - n)); }

    u8 m_Data[64] = {};
    u32 m_DataLength = 0;
    u64 m_BitLength = 0;
    u32 m_State[8] = {};
};

// ============================================================================
// HMAC-SHA256
// ============================================================================

class HMACSHA256 {
public:
    // Compute HMAC-SHA256 over data using the given key.
    // Output: 32-byte tag written to `out`.
    static void Compute(const u8* key, u32 keyLen,
                        const u8* data, u32 dataLen,
                        u8 out[HMAC_TAG_SIZE]) {
        u8 keyBlock[64];
        std::memset(keyBlock, 0, 64);

        if (keyLen > 64) {
            // Hash the key if longer than block size
            SHA256::Hash(key, keyLen, keyBlock);
        } else {
            std::memcpy(keyBlock, key, keyLen);
        }

        // Inner pad (key XOR 0x36)
        u8 ipad[64];
        for (u32 i = 0; i < 64; i++) ipad[i] = keyBlock[i] ^ 0x36;

        // Outer pad (key XOR 0x5C)
        u8 opad[64];
        for (u32 i = 0; i < 64; i++) opad[i] = keyBlock[i] ^ 0x5C;

        // Inner hash: SHA256(ipad || data)
        u8 innerHash[32];
        SHA256 inner;
        inner.Update(ipad, 64);
        inner.Update(data, dataLen);
        inner.Final(innerHash);

        // Outer hash: SHA256(opad || innerHash)
        SHA256 outer;
        outer.Update(opad, 64);
        outer.Update(innerHash, 32);
        outer.Final(out);
    }

    // Verify an HMAC tag (constant-time comparison to prevent timing attacks)
    static bool Verify(const u8* key, u32 keyLen,
                       const u8* data, u32 dataLen,
                       const u8 tag[HMAC_TAG_SIZE]) {
        u8 computed[HMAC_TAG_SIZE];
        Compute(key, keyLen, data, dataLen, computed);

        // Constant-time comparison
        u8 diff = 0;
        for (u32 i = 0; i < HMAC_TAG_SIZE; i++) {
            diff |= (computed[i] ^ tag[i]);
        }
        return diff == 0;
    }
};

// ============================================================================
// SESSION KEY GENERATION
// ============================================================================

// Generate a cryptographically random 32-byte session key using platform RNG.
// Windows: BCryptGenRandom / RtlGenRandom
// POSIX: /dev/urandom
SessionKey GenerateSessionKey();

} // namespace Networking
} // namespace Enjin
