#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Networking/NetworkTypes.h"
#include "Enjin/Networking/NetworkSecurity.h"
#include "Enjin/Networking/NetworkSerializer.h"
#include <cstring>
#include <vector>
#include <string>

using namespace Enjin;
using namespace Enjin::Networking;

// ============================================================================
// PACKET SERIALIZATION / DESERIALIZATION
// ============================================================================

ENJIN_TEST(PacketSerializer, HeaderRoundTrip) {
    PacketHeader original;
    original.type = static_cast<u8>(MessageType::EntitySnapshot);
    original.sequence = 12345;
    original.ackSequence = 9876;
    original.ackBitfield = 0xDEADBEEF;
    original.senderId = 3;
    original.payloadSize = 128;

    std::vector<u8> buf;
    WritePacketHeader(buf, original);
    ENJIN_EXPECT_EQ(buf.size(), static_cast<usize>(PACKET_HEADER_SIZE));

    u32 offset = 0;
    PacketHeader decoded = ReadPacketHeader(buf.data(), offset, static_cast<u32>(buf.size()));
    ENJIN_EXPECT_EQ(offset, PACKET_HEADER_SIZE);

    ENJIN_EXPECT_EQ(decoded.type, original.type);
    ENJIN_EXPECT_EQ(decoded.sequence, original.sequence);
    ENJIN_EXPECT_EQ(decoded.ackSequence, original.ackSequence);
    ENJIN_EXPECT_EQ(decoded.ackBitfield, original.ackBitfield);
    ENJIN_EXPECT_EQ(decoded.senderId, original.senderId);
    ENJIN_EXPECT_EQ(decoded.payloadSize, original.payloadSize);
}

ENJIN_TEST(PacketSerializer, PrimitiveTypesRoundTrip) {
    std::vector<u8> buf;

    WriteU8(buf, 0xAB);
    WriteU16(buf, 0x1234);
    WriteU32(buf, 0xDEADBEEF);
    WriteI32(buf, -42);
    WriteF32(buf, 3.14159f);
    WriteString(buf, "Hello, Network!");

    u32 offset = 0;
    u32 maxSize = static_cast<u32>(buf.size());

    u8 v8 = ReadU8(buf.data(), offset, maxSize);
    ENJIN_EXPECT_EQ(v8, 0xAB);

    u16 v16 = ReadU16(buf.data(), offset, maxSize);
    ENJIN_EXPECT_EQ(v16, 0x1234);

    u32 v32 = ReadU32(buf.data(), offset, maxSize);
    ENJIN_EXPECT_EQ(v32, 0xDEADBEEF);

    i32 vi32 = ReadI32(buf.data(), offset, maxSize);
    ENJIN_EXPECT_EQ(vi32, -42);

    f32 vf32 = ReadF32(buf.data(), offset, maxSize);
    ENJIN_EXPECT_FLOAT_EQ(vf32, 3.14159f);

    std::string vstr = ReadString(buf.data(), offset, maxSize);
    ENJIN_EXPECT_STR_EQ(vstr, "Hello, Network!");
}

ENJIN_TEST(PacketSerializer, Vector3AndQuaternionRoundTrip) {
    std::vector<u8> buf;

    Math::Vector3 pos(1.5f, -2.75f, 100.0f);
    Math::Quaternion rot(0.1f, 0.2f, 0.3f, 0.9327f);

    WriteVector3(buf, pos);
    WriteQuaternion(buf, rot);

    u32 offset = 0;
    u32 maxSize = static_cast<u32>(buf.size());

    Math::Vector3 readPos = ReadVector3(buf.data(), offset, maxSize);
    ENJIN_EXPECT_FLOAT_EQ(readPos.x, pos.x);
    ENJIN_EXPECT_FLOAT_EQ(readPos.y, pos.y);
    ENJIN_EXPECT_FLOAT_EQ(readPos.z, pos.z);

    Math::Quaternion readRot = ReadQuaternion(buf.data(), offset, maxSize);
    ENJIN_EXPECT_FLOAT_EQ(readRot.x, rot.x);
    ENJIN_EXPECT_FLOAT_EQ(readRot.y, rot.y);
    ENJIN_EXPECT_FLOAT_EQ(readRot.z, rot.z);
    ENJIN_EXPECT_FLOAT_EQ(readRot.w, rot.w);
}

// ============================================================================
// HMAC-SHA256
// ============================================================================

ENJIN_TEST(HMAC, ComputeConsistency) {
    // Same key + same data must always produce the same tag
    const u8 key[] = "secret_session_key_12345";
    const u8 data[] = "packet payload data for entity sync";
    u32 keyLen = sizeof(key) - 1;
    u32 dataLen = sizeof(data) - 1;

    u8 tag1[HMAC_TAG_SIZE];
    u8 tag2[HMAC_TAG_SIZE];

    HMACSHA256::Compute(key, keyLen, data, dataLen, tag1);
    HMACSHA256::Compute(key, keyLen, data, dataLen, tag2);

    bool same = (std::memcmp(tag1, tag2, HMAC_TAG_SIZE) == 0);
    ENJIN_EXPECT_TRUE(same);
}

ENJIN_TEST(HMAC, VerifyCorrectKeyPasses) {
    const u8 key[] = "correct_key";
    const u8 data[] = "important network message";
    u32 keyLen = sizeof(key) - 1;
    u32 dataLen = sizeof(data) - 1;

    u8 tag[HMAC_TAG_SIZE];
    HMACSHA256::Compute(key, keyLen, data, dataLen, tag);

    bool verified = HMACSHA256::Verify(key, keyLen, data, dataLen, tag);
    ENJIN_EXPECT_TRUE(verified);
}

ENJIN_TEST(HMAC, VerifyWrongKeyFails) {
    const u8 correctKey[] = "correct_key";
    const u8 wrongKey[] = "wrong_key!!";
    const u8 data[] = "important network message";
    u32 dataLen = sizeof(data) - 1;

    u8 tag[HMAC_TAG_SIZE];
    HMACSHA256::Compute(correctKey, sizeof(correctKey) - 1, data, dataLen, tag);

    bool verified = HMACSHA256::Verify(wrongKey, sizeof(wrongKey) - 1, data, dataLen, tag);
    ENJIN_EXPECT_FALSE(verified);
}

ENJIN_TEST(HMAC, DifferentDataProducesDifferentTag) {
    const u8 key[] = "shared_key";
    const u8 data1[] = "message one";
    const u8 data2[] = "message two";

    u8 tag1[HMAC_TAG_SIZE];
    u8 tag2[HMAC_TAG_SIZE];

    HMACSHA256::Compute(key, sizeof(key) - 1, data1, sizeof(data1) - 1, tag1);
    HMACSHA256::Compute(key, sizeof(key) - 1, data2, sizeof(data2) - 1, tag2);

    bool same = (std::memcmp(tag1, tag2, HMAC_TAG_SIZE) == 0);
    ENJIN_EXPECT_FALSE(same);
}

// ============================================================================
// REPLAY PROTECTION (ReplayWindow)
// ============================================================================

ENJIN_TEST(ReplayWindow, FirstPacketAccepted) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(100));
    ENJIN_EXPECT_TRUE(window.initialized);
    ENJIN_EXPECT_EQ(window.lastSequence, 100u);
}

ENJIN_TEST(ReplayWindow, DuplicateRejected) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(1));
    ENJIN_EXPECT_FALSE(window.Accept(1));  // Duplicate of most recent
}

ENJIN_TEST(ReplayWindow, MonotonicSequenceAccepted) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(1));
    ENJIN_EXPECT_TRUE(window.Accept(2));
    ENJIN_EXPECT_TRUE(window.Accept(3));
    ENJIN_EXPECT_TRUE(window.Accept(4));
    ENJIN_EXPECT_EQ(window.lastSequence, 4u);
}

ENJIN_TEST(ReplayWindow, OutOfOrderWithinWindowAccepted) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(10));
    ENJIN_EXPECT_TRUE(window.Accept(12));  // Skip 11
    ENJIN_EXPECT_TRUE(window.Accept(11));  // Out-of-order but within window
    // Now replaying 11 should fail
    ENJIN_EXPECT_FALSE(window.Accept(11));
}

ENJIN_TEST(ReplayWindow, TooOldRejected) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(100));
    // Jump way ahead to push the old sequence out of the 64-bit window
    ENJIN_EXPECT_TRUE(window.Accept(200));
    // Sequence 100 is now 100 behind (> 64), should be rejected
    ENJIN_EXPECT_FALSE(window.Accept(100));
}

ENJIN_TEST(ReplayWindow, ResetClearsState) {
    ReplayWindow window;
    ENJIN_EXPECT_TRUE(window.Accept(50));
    ENJIN_EXPECT_TRUE(window.Accept(51));
    window.Reset();
    ENJIN_EXPECT_FALSE(window.initialized);
    ENJIN_EXPECT_EQ(window.lastSequence, 0u);
    ENJIN_EXPECT_EQ(window.receivedBitmask, 0ull);
    // After reset, first packet accepted again
    ENJIN_EXPECT_TRUE(window.Accept(50));
}

// ============================================================================
// INTERPOLATION BUFFER
// ============================================================================

ENJIN_TEST(InterpolationBuffer, PushAndCount) {
    InterpolationBuffer buffer;
    ENJIN_EXPECT_EQ(buffer.count, 0u);

    InterpolationState s;
    s.timestamp = 1.0f;
    s.position = Math::Vector3(0, 0, 0);
    buffer.Push(s);
    ENJIN_EXPECT_EQ(buffer.count, 1u);

    s.timestamp = 2.0f;
    s.position = Math::Vector3(1, 0, 0);
    buffer.Push(s);
    ENJIN_EXPECT_EQ(buffer.count, 2u);
}

ENJIN_TEST(InterpolationBuffer, GetInterpolationPairNeedsTwoStates) {
    InterpolationBuffer buffer;
    InterpolationState from, to;
    f32 t = 0.0f;

    // With 0 states, should return false
    ENJIN_EXPECT_FALSE(buffer.GetInterpolationPair(0.5f, from, to, t));

    // With 1 state, should still return false
    InterpolationState s;
    s.timestamp = 1.0f;
    buffer.Push(s);
    ENJIN_EXPECT_FALSE(buffer.GetInterpolationPair(1.0f, from, to, t));
}

ENJIN_TEST(InterpolationBuffer, InterpolationBetweenTwoStates) {
    InterpolationBuffer buffer;

    InterpolationState s1;
    s1.timestamp = 0.0f;
    s1.position = Math::Vector3(0, 0, 0);
    buffer.Push(s1);

    InterpolationState s2;
    s2.timestamp = 1.0f;
    s2.position = Math::Vector3(10, 0, 0);
    buffer.Push(s2);

    InterpolationState from, to;
    f32 t = 0.0f;

    // Midpoint interpolation
    bool found = buffer.GetInterpolationPair(0.5f, from, to, t);
    ENJIN_EXPECT_TRUE(found);
    ENJIN_EXPECT_FLOAT_EQ(t, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(from.position.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(to.position.x, 10.0f);
}

ENJIN_TEST(InterpolationBuffer, WrapAroundAtMaxCapacity) {
    InterpolationBuffer buffer;

    // Fill past capacity (INTERP_BUFFER_SIZE = 4)
    for (u32 i = 0; i < INTERP_BUFFER_SIZE + 2; i++) {
        InterpolationState s;
        s.timestamp = static_cast<f32>(i);
        s.position = Math::Vector3(static_cast<f32>(i), 0, 0);
        buffer.Push(s);
    }

    // Count should be capped at INTERP_BUFFER_SIZE
    ENJIN_EXPECT_EQ(buffer.count, INTERP_BUFFER_SIZE);
}

// ============================================================================
// FNV-1a HASH (RPC NAME HASHING)
// ============================================================================

ENJIN_TEST(FNV1a, ConsistentHashing) {
    u32 hash1 = FNV1aHash("SpawnProjectile");
    u32 hash2 = FNV1aHash("SpawnProjectile");
    ENJIN_EXPECT_EQ(hash1, hash2);
}

ENJIN_TEST(FNV1a, DifferentStringsProduceDifferentHashes) {
    u32 hash1 = FNV1aHash("SpawnProjectile");
    u32 hash2 = FNV1aHash("DealDamage");
    ENJIN_EXPECT_NE(hash1, hash2);
}

ENJIN_TEST(FNV1a, EmptyStringHasKnownOffset) {
    // FNV-1a of empty string should be the offset basis: 2166136261
    u32 hash = FNV1aHash("");
    ENJIN_EXPECT_EQ(hash, 2166136261u);
}

// ============================================================================
// SNAPSHOT FIELD MASK (DELTA COMPRESSION)
// ============================================================================

ENJIN_TEST(SnapshotDelta, FieldMaskBitsIndependent) {
    // Verify the bitmask values are distinct powers of two
    ENJIN_EXPECT_EQ(SnapPosition, 1);
    ENJIN_EXPECT_EQ(SnapRotation, 2);
    ENJIN_EXPECT_EQ(SnapScale, 4);
    ENJIN_EXPECT_EQ(SnapVelocity, 8);

    // Combining fields with OR produces expected mask
    u8 mask = SnapPosition | SnapVelocity;
    ENJIN_EXPECT_TRUE(mask & SnapPosition);
    ENJIN_EXPECT_FALSE(mask & SnapRotation);
    ENJIN_EXPECT_FALSE(mask & SnapScale);
    ENJIN_EXPECT_TRUE(mask & SnapVelocity);
}

ENJIN_TEST(SnapshotDelta, EntitySnapshotDefaults) {
    EntitySnapshot snap;
    ENJIN_EXPECT_EQ(snap.networkId, INVALID_NETWORK_ID);
    ENJIN_EXPECT_EQ(snap.fieldMask, 0);
    ENJIN_EXPECT_EQ(snap.tick, 0u);
    // Default scale should be (1,1,1)
    ENJIN_EXPECT_FLOAT_EQ(snap.scale.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(snap.scale.y, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(snap.scale.z, 1.0f);
}

// ============================================================================
// LOBBY STATE
// ============================================================================

ENJIN_TEST(Lobby, PlayerDefaults) {
    LobbyPlayer player;
    ENJIN_EXPECT_EQ(player.id, INVALID_PLAYER);
    ENJIN_EXPECT_TRUE(player.name.empty());
    ENJIN_EXPECT_FALSE(player.ready);
    ENJIN_EXPECT_FALSE(player.isHost);
}

ENJIN_TEST(Lobby, PlayerFieldAssignment) {
    LobbyPlayer player;
    player.id = 0;
    player.name = "PlayerOne";
    player.ready = true;
    player.isHost = true;

    ENJIN_EXPECT_EQ(player.id, 0);
    ENJIN_EXPECT_STR_EQ(player.name, "PlayerOne");
    ENJIN_EXPECT_TRUE(player.ready);
    ENJIN_EXPECT_TRUE(player.isHost);
}

// ============================================================================
// RELIABLE DELIVERY
// ============================================================================

ENJIN_TEST(ReliableDelivery, MessageStructDefaults) {
    ReliableMessage msg;
    ENJIN_EXPECT_EQ(msg.sequence, 0);
    ENJIN_EXPECT_FLOAT_EQ(msg.lastSendTime, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(msg.firstSendTime, 0.0f);
    ENJIN_EXPECT_EQ(msg.retryCount, 0);
    ENJIN_EXPECT_TRUE(msg.data.empty());
}

ENJIN_TEST(ReliableDelivery, SequenceNumberingOnConnectionInfo) {
    ConnectionInfo conn;
    ENJIN_EXPECT_EQ(conn.localSequence, 0);
    ENJIN_EXPECT_EQ(conn.remoteSequence, 0);
    ENJIN_EXPECT_EQ(conn.remoteAckBitfield, 0u);

    // Simulate incrementing local sequence as packets are sent
    conn.localSequence++;
    conn.localSequence++;
    conn.localSequence++;
    ENJIN_EXPECT_EQ(conn.localSequence, 3);

    // Record send timestamps and verify retrieval
    conn.RecordSendTime(0, 1.0f);
    conn.RecordSendTime(1, 1.05f);
    conn.RecordSendTime(2, 1.10f);
    ENJIN_EXPECT_FLOAT_EQ(conn.GetSendTime(0), 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(conn.GetSendTime(1), 1.05f);
    ENJIN_EXPECT_FLOAT_EQ(conn.GetSendTime(2), 1.10f);
}

// ============================================================================
// RATE LIMITER
// ============================================================================

ENJIN_TEST(RateLimiter, TokenConsumption) {
    RateLimiter limiter;
    limiter.Configure(10.0f, 5.0f, 0.0f, 10.0f);

    // Should have 10 tokens initially, consume 3
    ENJIN_EXPECT_TRUE(limiter.Consume(3.0f, 0.0f));
    // 7 tokens left, consume 7 more
    ENJIN_EXPECT_TRUE(limiter.Consume(7.0f, 0.0f));
    // 0 tokens left, can't consume 1
    ENJIN_EXPECT_FALSE(limiter.Consume(1.0f, 0.0f));
}

ENJIN_TEST(RateLimiter, TokenRefill) {
    RateLimiter limiter;
    // 10 max tokens, refill rate 10/sec, start with 0
    limiter.Configure(10.0f, 10.0f, 0.0f, 0.0f);

    // At time 0, no tokens
    ENJIN_EXPECT_FALSE(limiter.Consume(1.0f, 0.0f));

    // After 0.5 seconds, should have ~5 tokens
    ENJIN_EXPECT_TRUE(limiter.Consume(4.0f, 0.5f));

    // After another 2 seconds (time 2.5), should have refilled to max (10)
    // minus the 4 consumed = 6 + 20 = 26, capped at 10
    ENJIN_EXPECT_TRUE(limiter.Consume(10.0f, 2.5f));
}

// ============================================================================
// NETWORK ADDRESS
// ============================================================================

ENJIN_TEST(NetworkAddress, EqualityComparison) {
    NetworkAddress a{0x0100007F, 7777};  // 127.0.0.1:7777
    NetworkAddress b{0x0100007F, 7777};
    NetworkAddress c{0x0100007F, 7778};
    NetworkAddress d{0x0200007F, 7777};

    ENJIN_EXPECT_TRUE(a == b);
    ENJIN_EXPECT_TRUE(a != c);  // Different port
    ENJIN_EXPECT_TRUE(a != d);  // Different IP
}

ENJIN_TEST(NetworkAddress, HashDiffersForDifferentAddresses) {
    NetworkAddressHash hasher;
    NetworkAddress a{0x0100007F, 7777};
    NetworkAddress b{0x0100007F, 7778};
    NetworkAddress c{0x0200007F, 7777};

    // Different addresses should generally produce different hashes
    // (Not strictly guaranteed but overwhelmingly likely for distinct inputs)
    size_t h1 = hasher(a);
    size_t h2 = hasher(b);
    size_t h3 = hasher(c);

    // At least one pair should differ
    ENJIN_EXPECT_TRUE(h1 != h2 || h1 != h3 || h2 != h3);
}

// ============================================================================
// SHA-256 BASIC VALIDATION
// ============================================================================

ENJIN_TEST(SHA256, EmptyStringKnownHash) {
    // SHA-256("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
    u8 hash[32];
    SHA256::Hash(nullptr, 0, hash);

    // Check first 4 bytes
    ENJIN_EXPECT_EQ(hash[0], 0xe3);
    ENJIN_EXPECT_EQ(hash[1], 0xb0);
    ENJIN_EXPECT_EQ(hash[2], 0xc4);
    ENJIN_EXPECT_EQ(hash[3], 0x42);

    // Check last 4 bytes
    ENJIN_EXPECT_EQ(hash[28], 0x78);
    ENJIN_EXPECT_EQ(hash[29], 0x52);
    ENJIN_EXPECT_EQ(hash[30], 0xb8);
    ENJIN_EXPECT_EQ(hash[31], 0x55);
}

ENJIN_TEST(SHA256, ConsistentOutput) {
    const char* msg = "The quick brown fox jumps over the lazy dog";
    u8 hash1[32];
    u8 hash2[32];

    SHA256::Hash(reinterpret_cast<const u8*>(msg), static_cast<u32>(std::strlen(msg)), hash1);
    SHA256::Hash(reinterpret_cast<const u8*>(msg), static_cast<u32>(std::strlen(msg)), hash2);

    bool same = (std::memcmp(hash1, hash2, 32) == 0);
    ENJIN_EXPECT_TRUE(same);
}

ENJIN_TEST_MAIN()
