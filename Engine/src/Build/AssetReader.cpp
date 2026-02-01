#include "Enjin/Build/AssetReader.h"
#include "Enjin/Build/AssetPacker.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <fstream>

namespace Enjin::Build {

AssetReader::~AssetReader() {
    Close();
}

bool AssetReader::Open(const std::string& pakPath, const std::string& key) {
    Close();

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

    // Read entry count
    u32 entryCount = 0;
    file.read(reinterpret_cast<char*>(&entryCount), sizeof(entryCount));

    // Read index offset
    u64 indexOffset = 0;
    file.read(reinterpret_cast<char*>(&indexOffset), sizeof(indexOffset));

    // Seek to end to get file size, then read footer
    file.seekg(0, std::ios::end);
    u64 fileSize = static_cast<u64>(file.tellg());

    if (fileSize < indexOffset + 8) {
        ENJIN_LOG_ERROR(Build, "Pack file truncated: %s", pakPath.c_str());
        return false;
    }

    // Read footer (last 8 bytes): index size (u32) + index CRC32 (u32)
    file.seekg(static_cast<std::streamoff>(fileSize - 8));
    u32 indexSize = 0;
    u32 indexCRC = 0;
    file.read(reinterpret_cast<char*>(&indexSize), sizeof(indexSize));
    file.read(reinterpret_cast<char*>(&indexCRC), sizeof(indexCRC));

    // Read obfuscated index
    file.seekg(static_cast<std::streamoff>(indexOffset));
    std::vector<u8> indexBuf(indexSize);
    file.read(reinterpret_cast<char*>(indexBuf.data()), indexSize);

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

    for (u32 i = 0; i < entryCount; i++) {
        u32 pathLen = 0;
        if (!readVal(&pathLen, sizeof(pathLen))) break;

        if (pos + pathLen > indexBuf.size()) break;
        std::string vpath(reinterpret_cast<const char*>(indexBuf.data() + pos), pathLen);
        pos += pathLen;

        Entry entry;
        if (!readVal(&entry.offset, sizeof(entry.offset))) break;
        if (!readVal(&entry.compressedSize, sizeof(entry.compressedSize))) break;
        if (!readVal(&entry.originalSize, sizeof(entry.originalSize))) break;
        if (!readVal(&entry.crc32, sizeof(entry.crc32))) break;

        m_Index[vpath] = entry;
    }

    if (m_Index.size() != entryCount) {
        ENJIN_LOG_WARN(Build, "Pack index partial read: expected %u entries, got %zu", entryCount, m_Index.size());
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

    std::ifstream file(m_PakPath, std::ios::binary);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Build, "Cannot reopen pack file: %s", m_PakPath.c_str());
        return {};
    }

    // Read compressed+obfuscated data
    file.seekg(static_cast<std::streamoff>(entry.offset));
    std::vector<u8> compressed(static_cast<usize>(entry.compressedSize));
    file.read(reinterpret_cast<char*>(compressed.data()),
              static_cast<std::streamsize>(entry.compressedSize));

    // Deobfuscate
    XorDeobfuscate(compressed);

    // Decompress
    std::vector<u8> original = DecompressData(compressed, entry.originalSize);

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
        file.seekg(static_cast<std::streamoff>(entry.offset));
        std::vector<u8> compressed(static_cast<usize>(entry.compressedSize));
        file.read(reinterpret_cast<char*>(compressed.data()),
                  static_cast<std::streamsize>(entry.compressedSize));

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

    // If sizes differ, this would be compressed data — for now treat as raw
    ENJIN_LOG_WARN(Build, "Compressed data detected but decompression not yet implemented, returning raw");
    return compressed;
}

} // namespace Enjin::Build
