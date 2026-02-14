#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <functional>

namespace Enjin {
namespace Assets {

// Compressed texture format
enum class CompressedFormat {
    None = 0,
    BC1,        // RGB (DXT1) — 4bpp, opaque or 1-bit alpha
    BC3,        // RGBA (DXT5) — 8bpp, smooth alpha
    BC4,        // R only — 4bpp, single channel (normal maps, AO)
    BC5,        // RG — 8bpp, two channels (normal maps XY)
    BC7,        // RGBA — 8bpp, highest quality
    ASTC_4x4,   // 8bpp, mobile
    ASTC_6x6,   // 3.56bpp, mobile
    ASTC_8x8    // 2bpp, mobile
};

// Quality preset for compression
enum class CompressionQuality {
    Fast = 0,
    Normal,
    High
};

// Compressed mip level data
struct CompressedMipLevel {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> data;
};

// Result of texture compression
struct CompressedTextureData {
    CompressedFormat format = CompressedFormat::None;
    u32 width = 0;
    u32 height = 0;
    u32 channels = 0;
    std::vector<CompressedMipLevel> mipLevels;
    bool valid = false;
};

// Texture compression settings for import pipeline
struct TextureCompressionSettings {
    CompressedFormat format = CompressedFormat::None;
    CompressionQuality quality = CompressionQuality::Normal;
    bool generateMipmaps = true;
    u32 maxMipLevels = 0; // 0 = full chain
    bool sRGB = true;
    bool premultiplyAlpha = false;
};

// CPU-side texture compressor using block compression algorithms
// Compresses RGBA8 pixel data to BCn/ASTC format blocks on the CPU
class ENJIN_API TextureCompressor {
public:
    // Compress RGBA8 pixel data to the specified format
    static CompressedTextureData Compress(
        const u8* rgba8Data,
        u32 width,
        u32 height,
        const TextureCompressionSettings& settings);

    // Get a recommended format based on texture properties
    static CompressedFormat RecommendFormat(
        u32 channels,
        bool hasAlpha,
        bool isNormalMap,
        bool isMobile);

    // Get format string name
    static const char* FormatName(CompressedFormat format);

    // Get bits per pixel for a format
    static u32 BitsPerPixel(CompressedFormat format);

    // Calculate compressed data size for one mip level
    static usize CalculateCompressedSize(CompressedFormat format, u32 width, u32 height);

    // Estimate compression ratio vs uncompressed RGBA8
    static f32 CompressionRatio(CompressedFormat format);

private:
    // Block compression helpers (4x4 blocks)
    static void CompressBlockBC1(const u8* rgba, u32 stride, u8* out);
    static void CompressBlockBC3(const u8* rgba, u32 stride, u8* out);
    static void CompressBlockBC4(const u8* rgba, u32 stride, u32 channel, u8* out);
    static void CompressBlockBC5(const u8* rgba, u32 stride, u8* out);
    static void CompressBlockBC7(const u8* rgba, u32 stride, u8* out);

    // ASTC block compression (NxN blocks, 128-bit output)
    // Single-partition mode with direct endpoint encoding and interpolation weights
    static void CompressBlockASTC(const u8* rgba, u32 blockW, u32 blockH, u32 stride, u8* out);

    // Generate mip level from previous level (box filter)
    static std::vector<u8> GenerateMipLevel(
        const u8* srcData, u32 srcWidth, u32 srcHeight, u32 channels);
};

} // namespace Assets
} // namespace Enjin
