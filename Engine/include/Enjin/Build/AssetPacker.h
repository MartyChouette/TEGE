#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <fstream>

namespace Enjin::Build {

// .enjpak file format constants
constexpr const char ENJPAK_MAGIC[8] = {'E','N','J','P','A','K','1','0'};
constexpr u32 ENJPAK_FLAG_OBFUSCATED = 1 << 0;

struct PakIndexEntry {
    std::string virtualPath;
    u64 dataOffset = 0;
    u64 compressedSize = 0;
    u64 originalSize = 0;
    u32 crc32 = 0;
};

// Writes .enjpak archive files (compress + obfuscate + CRC)
class ENJIN_API AssetPacker {
public:
    AssetPacker() = default;
    ~AssetPacker();

    bool Begin(const std::string& outputPath, const std::string& key);
    bool AddFile(const std::string& virtualPath, const std::string& diskPath);
    bool AddData(const std::string& virtualPath, const void* data, usize size);
    bool Finalize();  // writes index + footer, closes file

    u32 GetFileCount() const { return static_cast<u32>(m_Entries.size()); }
    u64 GetTotalOriginalSize() const { return m_TotalOriginalSize; }
    u64 GetTotalPackedSize() const { return m_TotalPackedSize; }

    // CRC32 utility (also used by AssetReader for verification)
    static u32 ComputeCRC32(const void* data, usize size);

private:
    void XorObfuscate(std::vector<u8>& data) const;
    std::vector<u8> CompressData(const void* data, usize size) const;

    std::ofstream m_File;
    std::string m_Key;
    std::vector<PakIndexEntry> m_Entries;
    u64 m_DataStartOffset = 0;
    u64 m_TotalOriginalSize = 0;
    u64 m_TotalPackedSize = 0;
    bool m_Active = false;
};

} // namespace Enjin::Build
