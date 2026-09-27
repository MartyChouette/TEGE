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
// The pack's own key follows the fixed header (u16 length, then the bytes).
// A build with a custom Pack Key produced a game neither player could open:
// they only ever tried the default key (EP-17). The key travels with the pack
// so the reader needs nothing else. This is obfuscation, not encryption; the
// default key is in the source for anyone to read, and a custom key stored
// beside the data protects no more than that.
constexpr u32 ENJPAK_FLAG_KEY_EMBEDDED = 1 << 1;
constexpr u16 ENJPAK_FORMAT_VERSION = 1;

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

    // `obfuscate = false` writes an open pak: the flag bit is off and nothing
    // is XORed, so any reader can take its entries apart (they are still
    // compressed). That is what PackagingMode::PackedOpen promises.
    bool Begin(const std::string& outputPath, const std::string& key, bool obfuscate = true);
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
