#include "Enjin/Build/AssetReader.h"
#include "Enjin/Build/AssetPacker.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Paths.h"
#include <algorithm>
#include <cstring>
#include <fstream>

static_assert(sizeof(std::streamoff) >= 8, "Asset packs require 64-bit file offsets");

namespace Enjin::Build {

AssetReader::~AssetReader() {
    Close();
}

bool AssetReader::Open(const std::string& pakPath, const std::string& key) {
    Close();

    if (key.empty()) {
        ENJIN_LOG_WARN(Build, "No encryption key provided — using default obfuscation key");
    }
    m_Key = key.empty() ? "enjin_default_pack_key_2025" : key;
    m_PakPath = pakPath;

    std::ifstream file(pakPath, std::ios::binary);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Build, "Cannot open pack file: %s", pakPath.c_str());
        return false;
    }

    // Read and verify magic
    char magic[8];
    file.read(magic, 8);
    if (std::memcmp(magic, ENJPAK_MAGIC, 8) != 0) {
        ENJIN_LOG_ERROR(Build, "Invalid pack file (bad magic): %s", pakPath.c_str());
        return false;
    }

    // Read flags
    u32 flags = 0;
    file.read(reinterpret_cast<char*>(&flags), sizeof(flags));

    // Read and validate format version
    u16 formatVersion = 0;
    file.read(reinterpret_cast<char*>(&formatVersion), sizeof(formatVersion));
    if (formatVersion == 0 || formatVersion > ENJPAK_FORMAT_VERSION) {
        ENJIN_LOG_ERROR(Build, "Unsupported pack format version %u (max supported: %u): %s",
                        formatVersion, ENJPAK_FORMAT_VERSION, pakPath.c_str());
        return false;
    }

    // Read entry count
    u32 entryCount = 0;
    file.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));

    // Read index offset
    u64 indexOffset = 0;
    file.read(reinterpret_cast<char*>(&indexOffset), sizeof(indexOffset));

    // Seek to end to get file size, then read footer
    file.seekg(0, std::ios::end);
    u64 fileSize = static_cast<u64>(file.tellg());

    // Guard against integer overflow: indexOffset + 8 could wrap
    if (indexOffset > fileSize || fileSize - indexOffset < 8) {
        ENJIN_LOG_ERROR(Build, "Pack file truncated: %s", pakPath.c_str());
        return false;
    }

    // Read footer (last 8 bytes): index size (u32) + index CRC32 (u32)
    file.seekg(static_cast<std::streamoff>(fileSize - 8));
    u32 indexSize = 0;
    u32 indexCRC = 0;
    file.read(reinterpret_cast<char*>(&indexSize), sizeof(indexSize));
    file.read(reinterpret_cast<char*>(&indexCRC), sizeof(indexCRC));

    // Validate footer: index data + footer must exactly fill [indexOffset, fileSize)
    if (indexOffset + static_cast<u64>(indexSize) + 8 != fileSize) {
        ENJIN_LOG_ERROR(Build, "Pack footer size mismatch (indexOffset=%llu + indexSize=%u + 8 != fileSize=%llu): %s",
                        static_cast<unsigned long long>(indexOffset), indexSize,
                        static_cast<unsigned long long>(fileSize), pakPath.c_str());
        return false;
    }

    // Cap index size to prevent excessive allocation (max 64 MB)
    static constexpr u32 MAX_INDEX_SIZE = 64u * 1024u * 1024u;
    if (indexSize > MAX_INDEX_SIZE) {
        ENJIN_LOG_ERROR(Build, "Pack index too large (%u bytes, max %u): %s",
                        indexSize, MAX_INDEX_SIZE, pakPath.c_str());
        return false;
    }

    // Verify index fits within the file
    if (static_cast<u64>(indexSize) > fileSize - indexOffset) {
        ENJIN_LOG_ERROR(Build, "Pack index size exceeds available data: %s", pakPath.c_str());
        return false;
    }

    // Read obfuscated index
    file.seekg(static_cast<std::streamoff>(indexOffset));
    std::vector<u8> indexBuf(indexSize);
    file.read(reinterpret_cast<char*>(indexBuf.data()), indexSize);
    if (!file.good()) {
        ENJIN_LOG_ERROR(Build, "Failed to read pack index: %s", pakPath.c_str());
        return false;
    }

    // Deobfuscate
    XorDeobfuscate(indexBuf);

    // Verify index CRC
    u32 computedCRC = AssetPacker::ComputeCRC32(indexBuf.data(), indexBuf.size());
    if (computedCRC != indexCRC) {
        ENJIN_LOG_ERROR(Build, "Pack index CRC mismatch (file corrupted or wrong key): %s", pakPath.c_str());
        return false;
    }

    // Parse index entries
    usize pos = 0;
    auto readVal = [&](void* dst, usize len) -> bool {
        if (pos + len > indexBuf.size()) return false;
        std::memcpy(dst, indexBuf.data() + pos, len);
        pos += len;
        return true;
    };

    // Cap entry count to prevent excessive iteration (max 1M entries)
    static constexpr u32 MAX_ENTRY_COUNT = 1000000u;
    if (entryCount > MAX_ENTRY_COUNT) {
        ENJIN_LOG_ERROR(Build, "Pack entry count too large (%u, max %u): %s",
                        entryCount, MAX_ENTRY_COUNT, pakPath.c_str());
        return false;
    }

    u32 parsedCount = 0;
    for (u32 i = 0; i < entryCount; i++) {
        u32 pathLen = 0;
        if (!readVal(&pathLen, sizeof(pathLen))) break;

        // Cap path length to prevent overflow and excessive allocation
        static constexpr u32 MAX_PATH_LEN = 4096u;
        if (pathLen > MAX_PATH_LEN || pos > indexBuf.size() || pathLen > indexBuf.size() - pos) break;
        std::string vpath(reinterpret_cast<const char*>(indexBuf.data() + pos), pathLen);
        pos += pathLen;

        // Reject path traversal and absolute paths
        if (!Platform::IsSafeRelativePath(vpath)) {
            ENJIN_LOG_WARN(Build, "Rejecting virtual path with traversal/absolute: %s", vpath.c_str());
            // Still need to read the entry fields to advance pos
            Entry skip;
            if (!readVal(&skip.offset, sizeof(skip.offset))) break;
            if (!readVal(&skip.compressedSize, sizeof(skip.compressedSize))) break;
            if (!readVal(&skip.originalSize, sizeof(skip.originalSize))) break;
            if (!readVal(&skip.crc32, sizeof(skip.crc32))) break;
            parsedCount++;
            continue;
        }

        Entry entry;
        if (!readVal(&entry.offset, sizeof(entry.offset))) break;
        if (!readVal(&entry.compressedSize, sizeof(entry.compressedSize))) break;
        if (!readVal(&entry.originalSize, sizeof(entry.originalSize))) break;
        if (!readVal(&entry.crc32, sizeof(entry.crc32))) break;

        // Validate that entry data region fits within the file
        if (entry.offset > fileSize || entry.compressedSize > fileSize - entry.offset) {
            ENJIN_LOG_WARN(Build, "Entry '%s' data region exceeds file bounds, skipping", vpath.c_str());
            continue;
        }

        // Reject entries whose data overlaps the index+footer region
        if (entry.compressedSize > 0 && entry.offset + entry.compressedSize > indexOffset) {
            ENJIN_LOG_WARN(Build, "Entry '%s' data region overlaps index, skipping", vpath.c_str());
            continue;
        }

        m_Index[vpath] = entry;
        parsedCount++;
    }

    // Post-parse: detect overlapping entries
    {
        std::vector<std::pair<u64, u64>> regions; // (offset, end)
        regions.reserve(m_Index.size());
        for (auto& [vp, e] : m_Index) {
            if (e.compressedSize > 0)
                regions.emplace_back(e.offset, e.offset + e.compressedSize);
        }
        std::sort(regions.begin(), regions.end());
        for (usize i = 1; i < regions.size(); i++) {
            if (regions[i].first < regions[i - 1].second) {
                ENJIN_LOG_ERROR(Build, "Pack has overlapping data entries — file corrupt: %s", pakPath.c_str());
                m_Index.clear();
                return false;
            }
        }
    }

    if (parsedCount != entryCount) {
        ENJIN_LOG_ERROR(Build, "Pack index truncated: expected %u entries, parsed %u — rejecting pack: %s",
                        entryCount, parsedCount, pakPath.c_str());
        m_Index.clear();
        return false;
    }

    m_Open = true;
    ENJIN_LOG_INFO(Build, "Opened pack: %s (%u files)", pakPath.c_str(), entryCount);
    return true;
}

void AssetReader::Close() {
    m_Index.clear();
    m_PakPath.clear();
    m_Key.clear();
    m_Open = false;
}

bool AssetReader::HasFile(const std::string& virtualPath) const {
    return m_Index.find(virtualPath) != m_Index.end();
}

std::vector<u8> AssetReader::ReadFile(const std::string& virtualPath) const {
    auto it = m_Index.find(virtualPath);
    if (it == m_Index.end()) {
        ENJIN_LOG_WARN(Build, "File not found in pack: %s", virtualPath.c_str());
        return {};
    }

    const Entry& entry = it->second;

    // Reject unreasonable sizes (max 512 MB per file)
    static constexpr u64 MAX_FILE_SIZE = 512ull * 1024ull * 1024ull;
    if (entry.compressedSize > MAX_FILE_SIZE || entry.originalSize > MAX_FILE_SIZE) {
        ENJIN_LOG_ERROR(Build, "File too large in pack (%s): compressed=%llu, original=%llu",
                        virtualPath.c_str(),
                        static_cast<unsigned long long>(entry.compressedSize),
                        static_cast<unsigned long long>(entry.originalSize));
        return {};
    }

    std::ifstream file(m_PakPath, std::ios::binary);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Build, "Cannot reopen pack file: %s", m_PakPath.c_str());
        return {};
    }

    // Verify offset + size won't exceed file bounds
    file.seekg(0, std::ios::end);
    u64 fileSz = static_cast<u64>(file.tellg());
    if (entry.offset > fileSz || entry.compressedSize > fileSz - entry.offset) {
        ENJIN_LOG_ERROR(Build, "Entry '%s' data region [%llu + %llu] exceeds file size %llu",
                        virtualPath.c_str(),
                        static_cast<unsigned long long>(entry.offset),
                        static_cast<unsigned long long>(entry.compressedSize),
                        static_cast<unsigned long long>(fileSz));
        return {};
    }

    // Read compressed+obfuscated data
    file.seekg(static_cast<std::streamoff>(entry.offset));
    if (!file.good()) {
        ENJIN_LOG_ERROR(Build, "Seek failed for %s at offset %llu",
                        virtualPath.c_str(), static_cast<unsigned long long>(entry.offset));
        return {};
    }
    std::vector<u8> compressed(static_cast<usize>(entry.compressedSize));
    file.read(reinterpret_cast<char*>(compressed.data()),
              static_cast<std::streamsize>(entry.compressedSize));
    if (!file.good() || static_cast<u64>(file.gcount()) != entry.compressedSize) {
        ENJIN_LOG_ERROR(Build, "Read failed for %s (expected %llu bytes, got %lld)",
                        virtualPath.c_str(), static_cast<unsigned long long>(entry.compressedSize),
                        static_cast<long long>(file.gcount()));
        return {};
    }

    // Deobfuscate
    XorDeobfuscate(compressed);

    // Decompress
    std::vector<u8> original = DecompressData(compressed, entry.originalSize);
    if (original.empty() && entry.originalSize > 0) {
        ENJIN_LOG_ERROR(Build, "Decompression failed for %s", virtualPath.c_str());
        return {};
    }

    // Verify CRC
    u32 crc = AssetPacker::ComputeCRC32(original.data(), original.size());
    if (crc != entry.crc32) {
        ENJIN_LOG_ERROR(Build, "CRC mismatch for %s (expected 0x%08X, got 0x%08X)",
                        virtualPath.c_str(), entry.crc32, crc);
        return {};
    }

    return original;
}

std::vector<std::string> AssetReader::ListFiles() const {
    std::vector<std::string> files;
    files.reserve(m_Index.size());
    for (auto& [path, _] : m_Index) {
        files.push_back(path);
    }
    return files;
}

bool AssetReader::VerifyIntegrity() const {
    if (!m_Open) return false;

    std::ifstream file(m_PakPath, std::ios::binary);
    if (!file.is_open()) return false;

    for (auto& [vpath, entry] : m_Index) {
        static constexpr u64 MAX_VERIFY_SIZE = 512ull * 1024ull * 1024ull;
        if (entry.compressedSize > MAX_VERIFY_SIZE) {
            ENJIN_LOG_ERROR(Build, "Integrity check skipped (too large): %s", vpath.c_str());
            return false;
        }
        file.seekg(static_cast<std::streamoff>(entry.offset));
        if (!file.good()) {
            ENJIN_LOG_ERROR(Build, "Integrity check seek failed: %s", vpath.c_str());
            return false;
        }
        std::vector<u8> compressed(static_cast<usize>(entry.compressedSize));
        file.read(reinterpret_cast<char*>(compressed.data()),
                  static_cast<std::streamsize>(entry.compressedSize));
        if (!file.good() || static_cast<u64>(file.gcount()) != entry.compressedSize) {
            ENJIN_LOG_ERROR(Build, "Integrity check read failed: %s", vpath.c_str());
            return false;
        }

        // Deobfuscate
        if (!m_Key.empty()) {
            usize keyLen = m_Key.size();
            for (usize i = 0; i < compressed.size(); i++) {
                compressed[i] ^= static_cast<u8>(m_Key[i % keyLen]);
            }
        }

        // Decompress
        std::vector<u8> original = DecompressData(compressed, entry.originalSize);

        u32 crc = AssetPacker::ComputeCRC32(original.data(), original.size());
        if (crc != entry.crc32) {
            ENJIN_LOG_ERROR(Build, "Integrity check failed: %s", vpath.c_str());
            return false;
        }
    }

    ENJIN_LOG_INFO(Build, "Pack integrity verified: all %u files OK", static_cast<u32>(m_Index.size()));
    return true;
}

void AssetReader::XorDeobfuscate(std::vector<u8>& data) const {
    // XOR is symmetric — same operation as obfuscation
    if (m_Key.empty()) return;
    usize keyLen = m_Key.size();
    for (usize i = 0; i < data.size(); i++) {
        data[i] ^= static_cast<u8>(m_Key[i % keyLen]);
    }
}

std::vector<u8> AssetReader::DecompressData(const std::vector<u8>& compressed, u64 originalSize) const {
    // Currently data is stored raw (no compression in AssetPacker yet).
    // When compression is added to AssetPacker, add matching decompression here.
    // The format distinguishes via compressedSize vs originalSize.
    if (compressed.size() == originalSize) {
        return compressed;  // stored raw
    }

    // Size mismatch means data is compressed but we have no decompressor yet.
    // Return empty to avoid silently returning corrupt (compressed) data.
    ENJIN_LOG_ERROR(Build, "Compressed data detected (compressed=%zu, original=%llu) but decompression not implemented",
                    compressed.size(), static_cast<unsigned long long>(originalSize));
    return {};
}

} // namespace Enjin::Build
