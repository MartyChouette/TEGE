#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Build/AssetPacker.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Enjin::Build {

// Reads from .enjpak archives at runtime (deobfuscate + verify CRC)
class ENJIN_API AssetReader {
public:
    AssetReader() = default;
    ~AssetReader();

    bool Open(const std::string& pakPath, const std::string& key);
    void Close();

    bool IsOpen() const { return m_Open; }
    bool HasFile(const std::string& virtualPath) const;
    std::vector<u8> ReadFile(const std::string& virtualPath) const;
    std::vector<std::string> ListFiles() const;
    bool VerifyIntegrity() const;  // check all CRC32s

    u32 GetFileCount() const { return static_cast<u32>(m_Index.size()); }
    u64 GetFileSize(const std::string& virtualPath) const;   // decoded size, 0 if absent

private:
    struct Entry {
        u64 offset = 0;
        u64 compressedSize = 0;
        u64 originalSize = 0;
        u32 crc32 = 0;
    };

    void XorDeobfuscate(std::vector<u8>& data) const;
    std::vector<u8> DecompressData(const std::vector<u8>& compressed, u64 originalSize) const;

    std::unordered_map<std::string, Entry> m_Index;
    std::string m_PakPath;
    std::string m_Key;
    bool m_Open = false;
};

} // namespace Enjin::Build
