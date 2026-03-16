#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Networking/NetworkTypes.h"
#include "Enjin/Networking/NetworkSerializer.h"

#include <cstring>
#include <vector>
#include <string>
#include <limits>
#include <cmath>

using namespace Enjin;
using namespace Enjin::Networking;

// ===========================================================================
// Buffer underrun: truncated header (< 12 bytes)
// ===========================================================================

ENJIN_TEST(BufferUnderrun, EmptyBuffer) {
    u32 offset = 0;
    u8 data[1] = {0};
    // Reading from an empty buffer (maxSize=0) should return defaults
    u8 val = ReadU8(data, offset, 0);
    ENJIN_EXPECT_EQ(val, 0);
    ENJIN_EXPECT_EQ(offset, 0u); // offset clamped to maxSize
}

ENJIN_TEST(BufferUnderrun, ReadU16FromOneByte) {
    u8 data[1] = {0xFF};
    u32 offset = 0;
    u16 val = ReadU16(data, offset, 1);
    ENJIN_EXPECT_EQ(val, 0u); // insufficient bytes
    ENJIN_EXPECT_EQ(offset, 1u); // offset advanced to maxSize
}

ENJIN_TEST(BufferUnderrun, ReadU32FromTwoBytes) {
    u8 data[2] = {0xAB, 0xCD};
    u32 offset = 0;
    u32 val = ReadU32(data, offset, 2);
    ENJIN_EXPECT_EQ(val, 0u);
    ENJIN_EXPECT_EQ(offset, 2u);
}

ENJIN_TEST(BufferUnderrun, ReadF32FromThreeBytes) {
    u8 data[3] = {0, 0, 0};
    u32 offset = 0;
    f32 val = ReadF32(data, offset, 3);
    ENJIN_EXPECT_FLOAT_EQ(val, 0.0f);
    ENJIN_EXPECT_EQ(offset, 3u);
}

ENJIN_TEST(BufferUnderrun, TruncatedPacketHeader) {
    // PacketHeader needs 12 bytes. Provide only 5.
    u8 data[5] = {0x01, 0x02, 0x00, 0x03, 0x00};
    u32 offset = 0;
    PacketHeader hdr = ReadPacketHeader(data, offset, 5);
    // First field (type, 1 byte) should read OK
    ENJIN_EXPECT_EQ(hdr.type, 0x01);
    // Remaining fields will be zeroed due to bounds check
    // Offset should be at maxSize (5)
    ENJIN_EXPECT_EQ(offset, 5u);
}

ENJIN_TEST(BufferUnderrun, ReadVector3FromEightBytes) {
    u8 data[8] = {0};
    u32 offset = 0;
    Math::Vector3 v = ReadVector3(data, offset, 8);
    // Needs 12 bytes (3 x f32). Should partially read then fail.
    ENJIN_EXPECT_EQ(offset, 8u);
}

ENJIN_TEST(BufferUnderrun, ReadQuaternionFromTwelveBytes) {
    u8 data[12] = {0};
    u32 offset = 0;
    Math::Quaternion q = ReadQuaternion(data, offset, 12);
    // Needs 16 bytes (4 x f32). Should partially read then fail.
    ENJIN_EXPECT_EQ(offset, 12u);
}

// ===========================================================================
// String length overflow
// ===========================================================================

ENJIN_TEST(StringOverflow, DeclaredLengthExceedsBuffer) {
    // Write a length byte of 200, but only 5 bytes of data follow
    u8 data[6];
    data[0] = 200; // claimed length
    data[1] = 'A'; data[2] = 'B'; data[3] = 'C'; data[4] = 'D'; data[5] = 'E';
    u32 offset = 0;
    std::string result = ReadString(data, offset, 6);
    // Should return empty string (bounds check: offset+len > maxSize)
    ENJIN_EXPECT_TRUE(result.empty());
    ENJIN_EXPECT_EQ(offset, 6u); // advanced to maxSize
}

ENJIN_TEST(StringOverflow, ExactFitString) {
    // Length byte = 5, plus exactly 5 data bytes
    u8 data[6];
    data[0] = 5;
    data[1] = 'H'; data[2] = 'e'; data[3] = 'l'; data[4] = 'l'; data[5] = 'o';
    u32 offset = 0;
    std::string result = ReadString(data, offset, 6);
    ENJIN_EXPECT_STR_EQ(result.c_str(), "Hello");
    ENJIN_EXPECT_EQ(offset, 6u);
}

// ===========================================================================
// Empty and max-length strings
// ===========================================================================

ENJIN_TEST(StringEdge, EmptyStringRoundTrip) {
    std::vector<u8> buf;
    WriteString(buf, "");
    ENJIN_EXPECT_EQ(buf.size(), (size_t)1); // just the length byte (0)

    u32 offset = 0;
    std::string result = ReadString(buf.data(), offset, (u32)buf.size());
    ENJIN_EXPECT_TRUE(result.empty());
    ENJIN_EXPECT_EQ(offset, 1u);
}

ENJIN_TEST(StringEdge, MaxLengthStringRoundTrip) {
    // 255 bytes is the max wire length
    std::string longStr(255, 'X');
    std::vector<u8> buf;
    WriteString(buf, longStr);
    ENJIN_EXPECT_EQ(buf.size(), (size_t)256); // 1 byte len + 255 data

    u32 offset = 0;
    std::string result = ReadString(buf.data(), offset, (u32)buf.size());
    ENJIN_EXPECT_EQ(result.size(), (size_t)255);
    ENJIN_EXPECT_EQ(result[0], 'X');
    ENJIN_EXPECT_EQ(result[254], 'X');
}

ENJIN_TEST(StringEdge, OverMaxTruncatedTo255) {
    // Write a 500-char string — should be truncated to 255
    std::string hugeStr(500, 'Z');
    std::vector<u8> buf;
    WriteString(buf, hugeStr);
    ENJIN_EXPECT_EQ(buf.size(), (size_t)256); // truncated: 1 + 255

    u32 offset = 0;
    std::string result = ReadString(buf.data(), offset, (u32)buf.size());
    ENJIN_EXPECT_EQ(result.size(), (size_t)255);
}

// ===========================================================================
// Offset already exhausted
// ===========================================================================

ENJIN_TEST(ExhaustedBuffer, ReadAfterEnd) {
    u8 data[4] = {0x01, 0x02, 0x03, 0x04};
    u32 offset = 4; // already at end
    u8 v1 = ReadU8(data, offset, 4);
    ENJIN_EXPECT_EQ(v1, 0);
    ENJIN_EXPECT_EQ(offset, 4u);

    u16 v2 = ReadU16(data, offset, 4);
    ENJIN_EXPECT_EQ(v2, 0u);

    u32 v3 = ReadU32(data, offset, 4);
    ENJIN_EXPECT_EQ(v3, 0u);
}

ENJIN_TEST(ExhaustedBuffer, ReadStringAfterEnd) {
    u8 data[4] = {0};
    u32 offset = 4;
    std::string result = ReadString(data, offset, 4);
    ENJIN_EXPECT_TRUE(result.empty());
}

// ===========================================================================
// Primitive round-trip edge cases
// ===========================================================================

ENJIN_TEST(PrimitiveEdge, MaxU32RoundTrip) {
    std::vector<u8> buf;
    WriteU32(buf, 0xFFFFFFFF);
    u32 offset = 0;
    u32 val = ReadU32(buf.data(), offset, (u32)buf.size());
    ENJIN_EXPECT_EQ(val, 0xFFFFFFFFu);
}

ENJIN_TEST(PrimitiveEdge, NegativeI32RoundTrip) {
    std::vector<u8> buf;
    WriteI32(buf, -12345);
    u32 offset = 0;
    i32 val = ReadI32(buf.data(), offset, (u32)buf.size());
    ENJIN_EXPECT_EQ(val, -12345);
}

ENJIN_TEST(PrimitiveEdge, ZeroF32RoundTrip) {
    std::vector<u8> buf;
    WriteF32(buf, 0.0f);
    u32 offset = 0;
    f32 val = ReadF32(buf.data(), offset, (u32)buf.size());
    ENJIN_EXPECT_FLOAT_EQ(val, 0.0f);
}

ENJIN_TEST(PrimitiveEdge, NaNF32RoundTrip) {
    std::vector<u8> buf;
    f32 nan = std::nanf("");
    WriteF32(buf, nan);
    u32 offset = 0;
    f32 val = ReadF32(buf.data(), offset, (u32)buf.size());
    ENJIN_EXPECT_TRUE(std::isnan(val));
}

ENJIN_TEST(PrimitiveEdge, InfF32RoundTrip) {
    std::vector<u8> buf;
    WriteF32(buf, std::numeric_limits<f32>::infinity());
    u32 offset = 0;
    f32 val = ReadF32(buf.data(), offset, (u32)buf.size());
    ENJIN_EXPECT_TRUE(std::isinf(val));
}

// ===========================================================================
// Vector3/Quaternion partial reads
// ===========================================================================

ENJIN_TEST(MathTypeEdge, Vector3RoundTrip) {
    std::vector<u8> buf;
    WriteVector3(buf, Math::Vector3(1.5f, -2.5f, 3.5f));
    ENJIN_EXPECT_EQ(buf.size(), (size_t)12);

    u32 offset = 0;
    Math::Vector3 v = ReadVector3(buf.data(), offset, (u32)buf.size());
    ENJIN_EXPECT_FLOAT_EQ(v.x, 1.5f);
    ENJIN_EXPECT_FLOAT_EQ(v.y, -2.5f);
    ENJIN_EXPECT_FLOAT_EQ(v.z, 3.5f);
}

ENJIN_TEST(MathTypeEdge, QuaternionRoundTrip) {
    std::vector<u8> buf;
    WriteQuaternion(buf, Math::Quaternion(0.1f, 0.2f, 0.3f, 0.9f));
    ENJIN_EXPECT_EQ(buf.size(), (size_t)16);

    u32 offset = 0;
    Math::Quaternion q = ReadQuaternion(buf.data(), offset, (u32)buf.size());
    ENJIN_EXPECT_FLOAT_EQ(q.x, 0.1f);
    ENJIN_EXPECT_FLOAT_EQ(q.y, 0.2f);
    ENJIN_EXPECT_FLOAT_EQ(q.z, 0.3f);
    ENJIN_EXPECT_FLOAT_EQ(q.w, 0.9f);
}

// ===========================================================================
// Sequential reads from one buffer
// ===========================================================================

ENJIN_TEST(Sequential, MultiFieldParsing) {
    std::vector<u8> buf;
    WriteU8(buf, 42);
    WriteU16(buf, 1000);
    WriteString(buf, "test");
    WriteF32(buf, 3.14f);

    u32 offset = 0;
    u32 maxSize = (u32)buf.size();

    u8 a = ReadU8(buf.data(), offset, maxSize);
    u16 b = ReadU16(buf.data(), offset, maxSize);
    std::string c = ReadString(buf.data(), offset, maxSize);
    f32 d = ReadF32(buf.data(), offset, maxSize);

    ENJIN_EXPECT_EQ(a, 42);
    ENJIN_EXPECT_EQ(b, 1000u);
    ENJIN_EXPECT_STR_EQ(c.c_str(), "test");
    ENJIN_EXPECT_FLOAT_NEAR(d, 3.14f, 0.001f);
    ENJIN_EXPECT_EQ(offset, maxSize); // consumed everything
}

ENJIN_TEST_MAIN()
