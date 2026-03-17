#include "EnjinTest.h"
#include "Enjin/Networking/NetworkSecurity.h"

#include <cstring>
#include <array>
#include <string>

using namespace Enjin;
using namespace Enjin::Networking;

// ===========================================================================
// SHA256 — Basic
// ===========================================================================

ENJIN_TEST(SHA256Basic, EmptyStringHash) {
    u8 hash[32];
    SHA256::Hash(nullptr, 0, hash);
    // SHA256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    ENJIN_EXPECT_EQ(hash[0], 0xe3);
    ENJIN_EXPECT_EQ(hash[1], 0xb0);
    ENJIN_EXPECT_EQ(hash[2], 0xc4);
    ENJIN_EXPECT_EQ(hash[31], 0x55);
}

ENJIN_TEST(SHA256Basic, KnownVectorABC) {
    // SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    const char* msg = "abc";
    u8 hash[32];
    SHA256::Hash((const u8*)msg, 3, hash);
    ENJIN_EXPECT_EQ(hash[0], 0xba);
    ENJIN_EXPECT_EQ(hash[1], 0x78);
    ENJIN_EXPECT_EQ(hash[2], 0x16);
    ENJIN_EXPECT_EQ(hash[3], 0xbf);
    ENJIN_EXPECT_EQ(hash[31], 0xad);
}

ENJIN_TEST(SHA256Basic, SingleByte) {
    u8 data = 0x61; // 'a'
    u8 hash[32];
    SHA256::Hash(&data, 1, hash);
    // SHA256("a") = ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb
    ENJIN_EXPECT_EQ(hash[0], 0xca);
    ENJIN_EXPECT_EQ(hash[31], 0xbb);
}

ENJIN_TEST(SHA256Basic, IncrementalMatchesOneShot) {
    const char* msg = "Hello, World!";
    u32 len = (u32)strlen(msg);

    // One-shot
    u8 hash1[32];
    SHA256::Hash((const u8*)msg, len, hash1);

    // Incremental — split at arbitrary point
    SHA256 hasher;
    hasher.Update((const u8*)msg, 5);      // "Hello"
    hasher.Update((const u8*)msg + 5, len - 5); // ", World!"
    u8 hash2[32];
    hasher.Final(hash2);

    ENJIN_EXPECT_EQ(memcmp(hash1, hash2, 32), 0);
}

ENJIN_TEST(SHA256Basic, ResetAllowsReuse) {
    SHA256 hasher;
    u8 data = 0x42;
    hasher.Update(&data, 1);
    u8 hash1[32];
    hasher.Final(hash1);

    hasher.Reset();
    hasher.Update(&data, 1);
    u8 hash2[32];
    hasher.Final(hash2);

    ENJIN_EXPECT_EQ(memcmp(hash1, hash2, 32), 0);
}

ENJIN_TEST(SHA256Basic, DifferentInputsDifferentHashes) {
    u8 hash1[32], hash2[32];
    const char* a = "test1";
    const char* b = "test2";
    SHA256::Hash((const u8*)a, 5, hash1);
    SHA256::Hash((const u8*)b, 5, hash2);
    ENJIN_EXPECT_TRUE(memcmp(hash1, hash2, 32) != 0);
}

// ===========================================================================
// SHA256 — Edge Cases
// ===========================================================================

ENJIN_TEST(SHA256Edge, ExactlyOneBlock) {
    // 55 bytes = max single-block message (55 data + 1 pad + 8 length = 64)
    u8 data[55];
    memset(data, 0xAA, 55);
    u8 hash[32];
    SHA256::Hash(data, 55, hash);
    // Just verify it doesn't crash and produces non-zero output
    bool allZero = true;
    for (int i = 0; i < 32; i++) if (hash[i] != 0) allZero = false;
    ENJIN_EXPECT_FALSE(allZero);
}

ENJIN_TEST(SHA256Edge, ExactlyTwoBlocks) {
    // 56 bytes forces padding into a second block
    u8 data[56];
    memset(data, 0xBB, 56);
    u8 hash[32];
    SHA256::Hash(data, 56, hash);
    bool allZero = true;
    for (int i = 0; i < 32; i++) if (hash[i] != 0) allZero = false;
    ENJIN_EXPECT_FALSE(allZero);
}

ENJIN_TEST(SHA256Edge, LargeInput) {
    // 1000 bytes
    u8 data[1000];
    memset(data, 0x42, 1000);
    u8 hash[32];
    SHA256::Hash(data, 1000, hash);
    bool allZero = true;
    for (int i = 0; i < 32; i++) if (hash[i] != 0) allZero = false;
    ENJIN_EXPECT_FALSE(allZero);
}

// ===========================================================================
// HMAC-SHA256
// ===========================================================================

ENJIN_TEST(HMAC, ComputeProducesNonZero) {
    u8 key[16];
    memset(key, 0x0B, 16);
    const char* data = "Hi There";
    u8 tag[HMAC_TAG_SIZE];
    HMACSHA256::Compute(key, 16, (const u8*)data, 8, tag);

    bool allZero = true;
    for (u32 i = 0; i < HMAC_TAG_SIZE; i++) if (tag[i] != 0) allZero = false;
    ENJIN_EXPECT_FALSE(allZero);
}

ENJIN_TEST(HMAC, VerifyCorrectTag) {
    u8 key[32];
    memset(key, 0xAA, 32);
    const char* msg = "test message";
    u8 tag[HMAC_TAG_SIZE];
    HMACSHA256::Compute(key, 32, (const u8*)msg, (u32)strlen(msg), tag);
    ENJIN_EXPECT_TRUE(HMACSHA256::Verify(key, 32, (const u8*)msg, (u32)strlen(msg), tag));
}

ENJIN_TEST(HMAC, VerifyWrongTagFails) {
    u8 key[32];
    memset(key, 0xAA, 32);
    const char* msg = "test message";
    u8 tag[HMAC_TAG_SIZE];
    HMACSHA256::Compute(key, 32, (const u8*)msg, (u32)strlen(msg), tag);

    // Corrupt one byte
    tag[0] ^= 0xFF;
    ENJIN_EXPECT_FALSE(HMACSHA256::Verify(key, 32, (const u8*)msg, (u32)strlen(msg), tag));
}

ENJIN_TEST(HMAC, VerifyWrongKeyFails) {
    u8 key1[32], key2[32];
    memset(key1, 0xAA, 32);
    memset(key2, 0xBB, 32);
    const char* msg = "test";
    u8 tag[HMAC_TAG_SIZE];
    HMACSHA256::Compute(key1, 32, (const u8*)msg, 4, tag);
    ENJIN_EXPECT_FALSE(HMACSHA256::Verify(key2, 32, (const u8*)msg, 4, tag));
}

ENJIN_TEST(HMAC, VerifyWrongDataFails) {
    u8 key[32];
    memset(key, 0xAA, 32);
    const char* msg1 = "message A";
    const char* msg2 = "message B";
    u8 tag[HMAC_TAG_SIZE];
    HMACSHA256::Compute(key, 32, (const u8*)msg1, (u32)strlen(msg1), tag);
    ENJIN_EXPECT_FALSE(HMACSHA256::Verify(key, 32, (const u8*)msg2, (u32)strlen(msg2), tag));
}

ENJIN_TEST(HMAC, DeterministicOutput) {
    u8 key[16];
    memset(key, 0x01, 16);
    const char* msg = "deterministic";
    u8 tag1[HMAC_TAG_SIZE], tag2[HMAC_TAG_SIZE];
    HMACSHA256::Compute(key, 16, (const u8*)msg, (u32)strlen(msg), tag1);
    HMACSHA256::Compute(key, 16, (const u8*)msg, (u32)strlen(msg), tag2);
    ENJIN_EXPECT_EQ(memcmp(tag1, tag2, HMAC_TAG_SIZE), 0);
}

ENJIN_TEST(HMAC, EmptyMessage) {
    u8 key[16];
    memset(key, 0x0C, 16);
    u8 tag[HMAC_TAG_SIZE];
    HMACSHA256::Compute(key, 16, nullptr, 0, tag);
    bool allZero = true;
    for (u32 i = 0; i < HMAC_TAG_SIZE; i++) if (tag[i] != 0) allZero = false;
    ENJIN_EXPECT_FALSE(allZero);
}

// ===========================================================================
// ReplayWindow
// ===========================================================================

ENJIN_TEST(ReplayWindow, FirstPacketAccepted) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(1));
}

ENJIN_TEST(ReplayWindow, SequentialPacketsAccepted) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(1));
    ENJIN_EXPECT_TRUE(window.Accept(2));
    ENJIN_EXPECT_TRUE(window.Accept(3));
    ENJIN_EXPECT_TRUE(window.Accept(4));
}

ENJIN_TEST(ReplayWindow, DuplicateRejected) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(1));
    ENJIN_EXPECT_FALSE(window.Accept(1)); // duplicate
}

ENJIN_TEST(ReplayWindow, OutOfOrderAccepted) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(5));
    ENJIN_EXPECT_TRUE(window.Accept(3)); // within window
    ENJIN_EXPECT_TRUE(window.Accept(4)); // within window
}

ENJIN_TEST(ReplayWindow, OutOfOrderDuplicateRejected) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(5));
    ENJIN_EXPECT_TRUE(window.Accept(3));
    ENJIN_EXPECT_FALSE(window.Accept(3)); // already seen
}

ENJIN_TEST(ReplayWindow, TooOldRejected) {
    ReplayWindow window;
    // Accept packet 100, then try packet 1 (way outside 64-bit window)
    ENJIN_EXPECT_TRUE(window.Accept(100));
    ENJIN_EXPECT_FALSE(window.Accept(1));
}

ENJIN_TEST(ReplayWindow, WindowBoundaryExact) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(65));
    // Packet 1 is exactly 64 behind — should be at edge of window
    // Packet 0 would be too old (65 behind)
    ENJIN_EXPECT_TRUE(window.Accept(2)); // 63 behind, in window
}

ENJIN_TEST(ReplayWindow, LargeJumpForward) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(1));
    ENJIN_EXPECT_TRUE(window.Accept(1000));
    ENJIN_EXPECT_FALSE(window.Accept(1)); // now too old
}

ENJIN_TEST(ReplayWindow, ResetClearsState) {
    ReplayWindow window;
    window.Accept(100);
    window.Reset();
    ENJIN_EXPECT_EQ(window.lastSequence, 0u);
    ENJIN_EXPECT_EQ(window.receivedBitmask, (u64)0);
    ENJIN_EXPECT_FALSE(window.initialized);
    // After reset, should accept packet 1 again
    ENJIN_EXPECT_TRUE(window.Accept(1));
}

ENJIN_TEST(ReplayWindow, ZeroSequenceHandled) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(0));
    ENJIN_EXPECT_FALSE(window.Accept(0)); // duplicate
}

// ===========================================================================
// GenerateSessionKey
// ===========================================================================

ENJIN_TEST(SessionKey, NonZero) {
    SessionKey key = GenerateSessionKey();
    bool allZero = true;
    for (u32 i = 0; i < SESSION_KEY_SIZE; i++) {
        if (key[i] != 0) allZero = false;
    }
    ENJIN_EXPECT_FALSE(allZero);
}

ENJIN_TEST(SessionKey, TwoKeysAreDifferent) {
    SessionKey k1 = GenerateSessionKey();
    SessionKey k2 = GenerateSessionKey();
    ENJIN_EXPECT_TRUE(k1 != k2);
}

ENJIN_TEST(SessionKey, CorrectSize) {
    ENJIN_EXPECT_EQ(SESSION_KEY_SIZE, 32u);
    SessionKey key = GenerateSessionKey();
    ENJIN_EXPECT_EQ(key.size(), (size_t)32);
}

ENJIN_TEST_MAIN()
