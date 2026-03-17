#include "Enjin/Build/AssetPacker.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <fstream>
#include <filesystem>
#include <mutex>

static_assert(sizeof(std::streamoff) >= 8, "Asset packs require 64-bit file offsets");

namespace Enjin::Build {

// CRC32 lookup table (standard polynomial 0xEDB88320)
static u32 s_CRC32Table[256] = {0};

static void InitCRC32Table() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        for (u32 i = 0; i < 256; i++) {
            u32 crc = i;
            for (u32 j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & (~(crc & 1) + 1));
            }
            s_CRC32Table[i] = crc;
        }
    });
}

AssetPacker::~AssetPacker() {
    if (m_Active && m_File.is_open()) {
        m_File.close();
    }
}

bool AssetPacker::Begin(const std::string& outputPath, const std::string& key) {
    InitCRC32Table();

    if (key.empty()) {
        ENJIN_LOG_WARN(Build, "No encryption key provided — using default obfuscation key");
    }
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
    // Format version (2 bytes)
    u16 version = ENJPAK_FORMAT_VERSION;
    m_File.write(reinterpret_cast<const char*>(&version), sizeof(version));
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
    if (fileSize < 0) {
        ENJIN_LOG_ERROR(Build, "Cannot determine file size: %s", diskPath.c_str());
        return false;
    }
    input.seekg(0, std::ios::beg);

    std::vector<u8> fileData(static_cast<usize>(fileSize));
    input.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    if (!input.good() || input.gcount() != fileSize) {
        ENJIN_LOG_ERROR(Build, "Incomplete read for file: %s (expected %lld, got %lld)",
                        diskPath.c_str(), static_cast<long long>(fileSize),
                        static_cast<long long>(input.gcount()));
        input.close();
        return false;
    }
    input.close();

    return AddData(virtualPath, fileData.data(), fileData.size());
}

bool AssetPacker::AddData(const std::string& virtualPath, const void* data, usize size) {
    if (!m_Active) return false;

    // Reject excessively large individual files (max 1 GB)
    static constexpr usize MAX_SINGLE_FILE = 1024ull * 1024ull * 1024ull;
    if (size > MAX_SINGLE_FILE) {
        ENJIN_LOG_ERROR(Build, "File too large to pack (%zu bytes): %s", size, virtualPath.c_str());
        return false;
    }

    // Cap total file count to prevent unbounded index growth
    static constexpr usize MAX_ENTRIES = 100000;
    if (m_Entries.size() >= MAX_ENTRIES) {
        ENJIN_LOG_ERROR(Build, "Max file count (%zu) reached, cannot add: %s", MAX_ENTRIES, virtualPath.c_str());
        return false;
    }

    // CRC32 of original data
    u32 crc = ComputeCRC32(data, size);

    // Compress (currently stores raw — compression can be added here later)
    std::vector<u8> packed = CompressData(data, size);

    // XOR obfuscate
    XorObfuscate(packed);

    // Record entry
    PakIndexEntry entry;
    entry.virtualPath = virtualPath;
    auto pos = m_File.tellp();
    if (pos < 0) {
        ENJIN_LOG_ERROR(Build, "Cannot determine write position for: %s", virtualPath.c_str());
        return false;
    }
    entry.dataOffset = static_cast<u64>(pos);
    entry.compressedSize = packed.size();
    entry.originalSize = size;
    entry.crc32 = crc;

    // Write data block
    m_File.write(reinterpret_cast<const char*>(packed.data()),
                 static_cast<std::streamsize>(packed.size()));
    if (!m_File.good()) {
        ENJIN_LOG_ERROR(Build, "Failed to write packed data for: %s", virtualPath.c_str());
        return false;
    }

    m_Entries.push_back(std::move(entry));
    m_TotalOriginalSize += size;
    m_TotalPackedSize += packed.size();

    return true;
}

bool AssetPacker::Finalize() {
    if (!m_Active) return false;

    auto pos = m_File.tellp();
    if (pos < 0) {
        ENJIN_LOG_ERROR(Build, "Cannot determine file position for index write");
        return false;
    }
    u64 indexOffset = static_cast<u64>(pos);

    // Build index as a byte buffer, then obfuscate
    std::vector<u8> indexBuf;

    auto appendBytes = [&](const void* src, usize len) {
        const u8* p = static_cast<const u8*>(src);
        indexBuf.insert(indexBuf.end(), p, p + len);
    };

    for (auto& entry : m_Entries) {
        if (entry.virtualPath.size() > UINT32_MAX) {
            ENJIN_LOG_ERROR(Build, "Path too long to pack: %s", entry.virtualPath.c_str());
            continue;
        }
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
    if (indexBuf.size() > UINT32_MAX) {
        ENJIN_LOG_ERROR(Build, "Index buffer too large (%zu bytes) for u32 size field", indexBuf.size());
        m_File.close();
        m_Active = false;
        return false;
    }
    u32 indexSize = static_cast<u32>(indexBuf.size());
    m_File.write(reinterpret_cast<const char*>(&indexSize), sizeof(indexSize));
    m_File.write(reinterpret_cast<const char*>(&indexCRC), sizeof(indexCRC));

    // Patch header with final values
    m_File.seekp(14);  // after magic(8) + flags(4) + version(2)
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
