#include "Enjin/Networking/NetworkSerializer.h"
#include <cstring>

namespace Enjin {
namespace Networking {

// ============================================================================
// WRITE HELPERS
// ============================================================================

void WriteU8(std::vector<u8>& buf, u8 val) {
    buf.push_back(val);
}

void WriteU16(std::vector<u8>& buf, u16 val) {
    // Network byte order (big-endian)
    buf.push_back(static_cast<u8>((val >> 8) & 0xFF));
    buf.push_back(static_cast<u8>(val & 0xFF));
}

void WriteU32(std::vector<u8>& buf, u32 val) {
    buf.push_back(static_cast<u8>((val >> 24) & 0xFF));
    buf.push_back(static_cast<u8>((val >> 16) & 0xFF));
    buf.push_back(static_cast<u8>((val >> 8) & 0xFF));
    buf.push_back(static_cast<u8>(val & 0xFF));
}

void WriteI32(std::vector<u8>& buf, i32 val) {
    u32 uval;
    std::memcpy(&uval, &val, sizeof(u32));
    WriteU32(buf, uval);
}

void WriteF32(std::vector<u8>& buf, f32 val) {
    u32 uval;
    std::memcpy(&uval, &val, sizeof(u32));
    WriteU32(buf, uval);
}

void WriteString(std::vector<u8>& buf, const std::string& str) {
    u16 len = static_cast<u16>(str.size());
    if (len > 255) len = 255;  // Cap string length for safety
    WriteU8(buf, static_cast<u8>(len));
    for (u16 i = 0; i < len; i++) {
        buf.push_back(static_cast<u8>(str[i]));
    }
}

void WriteVector3(std::vector<u8>& buf, const Math::Vector3& v) {
    WriteF32(buf, v.x);
    WriteF32(buf, v.y);
    WriteF32(buf, v.z);
}

void WriteQuaternion(std::vector<u8>& buf, const Math::Quaternion& q) {
    WriteF32(buf, q.x);
    WriteF32(buf, q.y);
    WriteF32(buf, q.z);
    WriteF32(buf, q.w);
}

void WritePacketHeader(std::vector<u8>& buf, const PacketHeader& header) {
    WriteU8(buf, header.type);
    WriteU16(buf, header.sequence);
    WriteU16(buf, header.ackSequence);
    WriteU32(buf, header.ackBitfield);
    WriteU8(buf, header.senderId);
    WriteU16(buf, header.payloadSize);
}

// ============================================================================
// READ HELPERS
// ============================================================================

u8 ReadU8(const u8* data, u32& offset, u32 maxSize) {
    if (offset >= maxSize) return 0;
    return data[offset++];
}

u16 ReadU16(const u8* data, u32& offset, u32 maxSize) {
    if (offset + 2 > maxSize) { offset = maxSize; return 0; }
    u16 val = (static_cast<u16>(data[offset]) << 8) | data[offset + 1];
    offset += 2;
    return val;
}

u32 ReadU32(const u8* data, u32& offset, u32 maxSize) {
    if (offset + 4 > maxSize) { offset = maxSize; return 0; }
    u32 val = (static_cast<u32>(data[offset]) << 24) |
              (static_cast<u32>(data[offset + 1]) << 16) |
              (static_cast<u32>(data[offset + 2]) << 8) |
              data[offset + 3];
    offset += 4;
    return val;
}

i32 ReadI32(const u8* data, u32& offset, u32 maxSize) {
    u32 uval = ReadU32(data, offset, maxSize);
    i32 val;
    std::memcpy(&val, &uval, sizeof(i32));
    return val;
}

f32 ReadF32(const u8* data, u32& offset, u32 maxSize) {
    u32 uval = ReadU32(data, offset, maxSize);
    f32 val;
    std::memcpy(&val, &uval, sizeof(f32));
    return val;
}

std::string ReadString(const u8* data, u32& offset, u32 maxSize) {
    u8 len = ReadU8(data, offset, maxSize);
    if (offset + len > maxSize) { offset = maxSize; return ""; }
    std::string str(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return str;
}

Math::Vector3 ReadVector3(const u8* data, u32& offset, u32 maxSize) {
    f32 x = ReadF32(data, offset, maxSize);
    f32 y = ReadF32(data, offset, maxSize);
    f32 z = ReadF32(data, offset, maxSize);
    return Math::Vector3(x, y, z);
}

Math::Quaternion ReadQuaternion(const u8* data, u32& offset, u32 maxSize) {
    f32 x = ReadF32(data, offset, maxSize);
    f32 y = ReadF32(data, offset, maxSize);
    f32 z = ReadF32(data, offset, maxSize);
    f32 w = ReadF32(data, offset, maxSize);
    return Math::Quaternion(x, y, z, w);
}

PacketHeader ReadPacketHeader(const u8* data, u32& offset, u32 maxSize) {
    PacketHeader h;
    h.type = ReadU8(data, offset, maxSize);
    h.sequence = ReadU16(data, offset, maxSize);
    h.ackSequence = ReadU16(data, offset, maxSize);
    h.ackBitfield = ReadU32(data, offset, maxSize);
    h.senderId = ReadU8(data, offset, maxSize);
    h.payloadSize = ReadU16(data, offset, maxSize);
    return h;
}

} // namespace Networking
} // namespace Enjin
