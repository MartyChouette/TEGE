#include "Enjin/Build/AssetPacker.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <fstream>
#include <filesystem>

namespace Enjin::Build {

// CRC32 lookup table (standard polynomial 0xEDB88320)
static u32 s_CRC32Table[256] = {0};
static bool s_CRC32Initialized = false;

static void InitCRC32Table() {
    if (s_CRC32Initialized) return;
    for (u32 i = 0; i < 256; i++) {
        u32 crc = i;
        for (u32 j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (~(crc & 1) + 1));
        }
        s_CRC32Table[i] = crc;
    }
    s_CRC32Initialized = true;
}

AssetPacker::~AssetPacker() {
    if (m_Active && m_File.is_open()) {
        m_File.close();
    }
}

bool AssetPacker::Begin(const std::string& outputPath, const std::string& key) {
    InitCRC32Table();

    m_Key = key.empty() ? "enjin_default_pack_key_2025" : key;
    m_Entries.clear();
    m_TotalOriginalSize = 0;
    m_TotalPackedSize = 0;

    m_File.open(outputPath, std::ios::binary | std::ios::trunc);
    if (!m_File.is_open()) {
        ENJIN_LOG_ERROR(Build, "Failed to create pack file: %s", outputPath.c_str());
        return false;
    }

    // Write header
    // Magic (8 bytes)
    m_File.write(ENJPAK_MAGIC, 8);
    // Flags (4 bytes) - obfuscated
    u32 flags = ENJPAK_FLAG_OBFUSCATED;
    m_File.write(reinterpret_cast<const char*>(&flags), sizeof(flags));
    // Entry count placeholder (4 bytes) - updated in Finalize
    u32 entryCount = 0;
    m_File.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));
    // Index offset placeholder (8 bytes) - updated in Finalize
    u64 indexOffset = 0;
    m_File.write(reinterpret_cast<const char*>(&indexOffset), sizeof(indexOffset));

    m_DataStartOffset = static_cast<u64>(m_File.tellp());
    m_Active = true;

    ENJIN_LOG_INFO(Build, "Packing assets to: %s", outputPath.c_str());
    return true;
}

bool AssetPacker::AddFile(const std::string& virtualPath, const std::string& diskPath) {
    if (!m_Active) return false;

    std::ifstream input(diskPath, std::ios::binary | std::ios::ate);
    if (!input.is_open()) {
        ENJIN_LOG_ERROR(Build, "Cannot read file: %s", diskPath.c_str());
        return false;
    }

    auto fileSize = input.tellg();
    input.seekg(0, std::ios::beg);

    std::vector<u8> fileData(static_cast<usize>(fileSize));
    input.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    input.close();

    return AddData(virtualPath, fileData.data(), fileData.size());
}

bool AssetPacker::AddData(const std::string& virtualPath, const void* data, usize size) {
    if (!m_Active) return false;

    // CRC32 of original data
    u32 crc = ComputeCRC32(data, size);

    // Compress (currently stores raw — compression can be added here later)
    std::vector<u8> packed = CompressData(data, size);

    // XOR obfuscate
    XorObfuscate(packed);

    // Record entry
    PakIndexEntry entry;
    entry.virtualPath = virtualPath;
    entry.dataOffset = static_cast<u64>(m_File.tellp());
    entry.compressedSize = packed.size();
    entry.originalSize = size;
    entry.crc32 = crc;

    // Write data block
    m_File.write(reinterpret_cast<const char*>(packed.data()),
                 static_cast<std::streamsize>(packed.size()));

    m_Entries.push_back(std::move(entry));
    m_TotalOriginalSize += size;
    m_TotalPackedSize += packed.size();

    return true;
}

bool AssetPacker::Finalize() {
    if (!m_Active) return false;

    u64 indexOffset = static_cast<u64>(m_File.tellp());

    // Build index as a byte buffer, then obfuscate
    std::vector<u8> indexBuf;

    auto appendBytes = [&](const void* src, usize len) {
        const u8* p = static_cast<const u8*>(src);
        indexBuf.insert(indexBuf.end(), p, p + len);
    };

    for (auto& entry : m_Entries) {
        u32 pathLen = static_cast<u32>(entry.virtualPath.size());
        appendBytes(&pathLen, sizeof(pathLen));
        indexBuf.insert(indexBuf.end(),
                        entry.virtualPath.begin(), entry.virtualPath.end());
        appendBytes(&entry.dataOffset, sizeof(entry.dataOffset));
        appendBytes(&entry.compressedSize, sizeof(entry.compressedSize));
        appendBytes(&entry.originalSize, sizeof(entry.originalSize));
        appendBytes(&entry.crc32, sizeof(entry.crc32));
    }

    // CRC32 of the raw index before obfuscation
    u32 indexCRC = ComputeCRC32(indexBuf.data(), indexBuf.size());

    // Obfuscate index
    XorObfuscate(indexBuf);

    // Write obfuscated index
    m_File.write(reinterpret_cast<const char*>(indexBuf.data()),
                 static_cast<std::streamsize>(indexBuf.size()));

    // Footer: index size (u32), index CRC32 (u32)
    u32 indexSize = static_cast<u32>(indexBuf.size());
    m_File.write(reinterpret_cast<const char*>(&indexSize), sizeof(indexSize));
    m_File.write(reinterpret_cast<const char*>(&indexCRC), sizeof(indexCRC));

    // Patch header with final values
    m_File.seekp(12);  // after magic(8) + flags(4)
    u32 entryCount = static_cast<u32>(m_Entries.size());
    m_File.write(reinterpret_cast<const char*>(&entryCount), sizeof(entryCount));
    m_File.write(reinterpret_cast<const char*>(&indexOffset), sizeof(indexOffset));

    m_File.close();
    m_Active = false;

    ENJIN_LOG_INFO(Build, "Pack finalized: %u files, %llu -> %llu bytes",
                   entryCount,
                   static_cast<unsigned long long>(m_TotalOriginalSize),
                   static_cast<unsigned long long>(m_TotalPackedSize));
    return true;
}

void AssetPacker::XorObfuscate(std::vector<u8>& data) const {
    if (m_Key.empty()) return;
    usize keyLen = m_Key.size();
    for (usize i = 0; i < data.size(); i++) {
        data[i] ^= static_cast<u8>(m_Key[i % keyLen]);
    }
}

u32 AssetPacker::ComputeCRC32(const void* data, usize size) {
    InitCRC32Table();
    const u8* bytes = static_cast<const u8*>(data);
    u32 crc = 0xFFFFFFFF;
    for (usize i = 0; i < size; i++) {
        crc = s_CRC32Table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

std::vector<u8> AssetPacker::CompressData(const void* data, usize size) const {
    // Currently stores raw. To add zlib/miniz compression later,
    // compress here and the format already supports it via
    // compressedSize != originalSize in the index.
    std::vector<u8> output(size);
    if (size > 0) {
        std::memcpy(output.data(), data, size);
    }
    return output;
}

} // namespace Enjin::Build
