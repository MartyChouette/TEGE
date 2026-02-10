#pragma once

#include "Enjin/Networking/NetworkTypes.h"
#include <vector>

namespace Enjin {
namespace Networking {

// ============================================================================
// BINARY WRITE HELPERS — Append to buffer
// ============================================================================

void WriteU8(std::vector<u8>& buf, u8 val);
void WriteU16(std::vector<u8>& buf, u16 val);
void WriteU32(std::vector<u8>& buf, u32 val);
void WriteI32(std::vector<u8>& buf, i32 val);
void WriteF32(std::vector<u8>& buf, f32 val);
void WriteString(std::vector<u8>& buf, const std::string& str);
void WriteVector3(std::vector<u8>& buf, const Math::Vector3& v);
void WriteQuaternion(std::vector<u8>& buf, const Math::Quaternion& q);
void WritePacketHeader(std::vector<u8>& buf, const PacketHeader& header);

// ============================================================================
// BINARY READ HELPERS — Read from data at offset, advance offset
// ============================================================================

u8 ReadU8(const u8* data, u32& offset, u32 maxSize);
u16 ReadU16(const u8* data, u32& offset, u32 maxSize);
u32 ReadU32(const u8* data, u32& offset, u32 maxSize);
i32 ReadI32(const u8* data, u32& offset, u32 maxSize);
f32 ReadF32(const u8* data, u32& offset, u32 maxSize);
std::string ReadString(const u8* data, u32& offset, u32 maxSize);
Math::Vector3 ReadVector3(const u8* data, u32& offset, u32 maxSize);
Math::Quaternion ReadQuaternion(const u8* data, u32& offset, u32 maxSize);
PacketHeader ReadPacketHeader(const u8* data, u32& offset, u32 maxSize);

} // namespace Networking
} // namespace Enjin
