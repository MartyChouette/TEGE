#include "Enjin/Assets/TextureCompressor.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace Enjin {
namespace Assets {

// --- Block compression helpers ---

// Find min/max colors in a 4x4 block for BC1/BC3
static void FindMinMaxColors(const u8* rgba, u32 stride, u8 minCol[3], u8 maxCol[3]) {
    u8 rMin = 255, gMin = 255, bMin = 255;
    u8 rMax = 0, gMax = 0, bMax = 0;

    for (u32 y = 0; y < 4; ++y) {
        for (u32 x = 0; x < 4; ++x) {
            const u8* p = rgba + (y * stride + x) * 4;
            if (p[0] < rMin) rMin = p[0]; if (p[0] > rMax) rMax = p[0];
            if (p[1] < gMin) gMin = p[1]; if (p[1] > gMax) gMax = p[1];
            if (p[2] < bMin) bMin = p[2]; if (p[2] > bMax) bMax = p[2];
        }
    }
    minCol[0] = rMin; minCol[1] = gMin; minCol[2] = bMin;
    maxCol[0] = rMax; maxCol[1] = gMax; maxCol[2] = bMax;
}

// Pack RGB888 to RGB565
static u16 PackRGB565(u8 r, u8 g, u8 b) {
    return static_cast<u16>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

// Find closest palette index (0-3) for BC1
static u32 ClosestBC1Index(u8 r, u8 g, u8 b, const u8 palette[4][3]) {
    u32 best = 0;
    i32 bestDist = 0x7FFFFFFF;
    for (u32 i = 0; i < 4; ++i) {
        i32 dr = static_cast<i32>(r) - palette[i][0];
        i32 dg = static_cast<i32>(g) - palette[i][1];
        i32 db = static_cast<i32>(b) - palette[i][2];
        i32 dist = dr * dr + dg * dg + db * db;
        if (dist < bestDist) { bestDist = dist; best = i; }
    }
    return best;
}

void TextureCompressor::CompressBlockBC1(const u8* rgba, u32 stride, u8* out) {
    u8 minCol[3], maxCol[3];
    FindMinMaxColors(rgba, stride, minCol, maxCol);

    u16 c0 = PackRGB565(maxCol[0], maxCol[1], maxCol[2]);
    u16 c1 = PackRGB565(minCol[0], minCol[1], minCol[2]);

    // Ensure c0 >= c1 for 4-color mode
    if (c0 < c1) {
        std::swap(c0, c1);
        std::swap(minCol[0], maxCol[0]);
        std::swap(minCol[1], maxCol[1]);
        std::swap(minCol[2], maxCol[2]);
    }

    // Build 4-color palette
    u8 palette[4][3];
    palette[0][0] = maxCol[0]; palette[0][1] = maxCol[1]; palette[0][2] = maxCol[2];
    palette[1][0] = minCol[0]; palette[1][1] = minCol[1]; palette[1][2] = minCol[2];
    palette[2][0] = static_cast<u8>((2 * maxCol[0] + minCol[0] + 1) / 3);
    palette[2][1] = static_cast<u8>((2 * maxCol[1] + minCol[1] + 1) / 3);
    palette[2][2] = static_cast<u8>((2 * maxCol[2] + minCol[2] + 1) / 3);
    palette[3][0] = static_cast<u8>((maxCol[0] + 2 * minCol[0] + 1) / 3);
    palette[3][1] = static_cast<u8>((maxCol[1] + 2 * minCol[1] + 1) / 3);
    palette[3][2] = static_cast<u8>((maxCol[2] + 2 * minCol[2] + 1) / 3);

    // Write endpoints
    out[0] = static_cast<u8>(c0 & 0xFF);
    out[1] = static_cast<u8>((c0 >> 8) & 0xFF);
    out[2] = static_cast<u8>(c1 & 0xFF);
    out[3] = static_cast<u8>((c1 >> 8) & 0xFF);

    // Write indices (2 bits per pixel, 16 pixels = 4 bytes)
    for (u32 row = 0; row < 4; ++row) {
        u8 indices = 0;
        for (u32 col = 0; col < 4; ++col) {
            const u8* p = rgba + (row * stride + col) * 4;
            u32 idx = ClosestBC1Index(p[0], p[1], p[2], palette);
            indices |= static_cast<u8>(idx << (col * 2));
        }
        out[4 + row] = indices;
    }
}

// BC4: single channel block compression (similar to DXT5 alpha block)
void TextureCompressor::CompressBlockBC4(const u8* rgba, u32 stride, u32 channel, u8* out) {
    u8 minVal = 255, maxVal = 0;
    u8 values[16];

    for (u32 y = 0; y < 4; ++y) {
        for (u32 x = 0; x < 4; ++x) {
            u8 v = rgba[(y * stride + x) * 4 + channel];
            values[y * 4 + x] = v;
            if (v < minVal) minVal = v;
            if (v > maxVal) maxVal = v;
        }
    }

    out[0] = maxVal;
    out[1] = minVal;

    // Build 8-level palette
    u8 palette[8];
    palette[0] = maxVal;
    palette[1] = minVal;
    if (maxVal > minVal) {
        for (u32 i = 1; i <= 6; ++i) {
            palette[i + 1] = static_cast<u8>(((7 - i) * maxVal + i * minVal + 3) / 7);
        }
    } else {
        for (u32 i = 2; i < 8; ++i) palette[i] = minVal;
    }

    // Pack 3-bit indices for 16 pixels into 6 bytes
    u64 bits = 0;
    for (u32 i = 0; i < 16; ++i) {
        u32 best = 0;
        i32 bestDist = 256;
        for (u32 j = 0; j < 8; ++j) {
            i32 d = std::abs(static_cast<i32>(values[i]) - palette[j]);
            if (d < bestDist) { bestDist = d; best = j; }
        }
        bits |= (static_cast<u64>(best) << (i * 3));
    }

    // Write 6 bytes of index data
    for (u32 i = 0; i < 6; ++i) {
        out[2 + i] = static_cast<u8>((bits >> (i * 8)) & 0xFF);
    }
}

void TextureCompressor::CompressBlockBC3(const u8* rgba, u32 stride, u8* out) {
    // Alpha block (BC4 on alpha channel)
    CompressBlockBC4(rgba, stride, 3, out);
    // Color block (BC1)
    CompressBlockBC1(rgba, stride, out + 8);
}

void TextureCompressor::CompressBlockBC5(const u8* rgba, u32 stride, u8* out) {
    // Two BC4 blocks: Red channel + Green channel
    CompressBlockBC4(rgba, stride, 0, out);
    CompressBlockBC4(rgba, stride, 1, out + 8);
}

void TextureCompressor::CompressBlockBC7(const u8* rgba, u32 stride, u8* out) {
    // Simplified BC7 mode 6 (one subset, 4-bit indices, full precision endpoints)
    // Mode 6: 1 subset, 4-bit indices per pixel, RGBA endpoints
    // This is a simplified encoder — production would use multiple modes

    // Find min/max RGBA
    u8 minC[4] = {255, 255, 255, 255};
    u8 maxC[4] = {0, 0, 0, 0};
    u8 pixels[16][4];

    for (u32 y = 0; y < 4; ++y) {
        for (u32 x = 0; x < 4; ++x) {
            const u8* p = rgba + (y * stride + x) * 4;
            u32 idx = y * 4 + x;
            for (u32 c = 0; c < 4; ++c) {
                pixels[idx][c] = p[c];
                if (p[c] < minC[c]) minC[c] = p[c];
                if (p[c] > maxC[c]) maxC[c] = p[c];
            }
        }
    }

    // BC7 Mode 6: 7-bit endpoints, 4-bit indices, 1 P-bit per endpoint
    // For simplicity, encode as mode 6 with basic endpoint fitting
    // Mode 6 header: 7 zero bits then a 1 bit = 0x40 in first byte
    std::memset(out, 0, 16);
    out[0] = 0x40; // mode 6 indicator

    // Pack 7-bit endpoint RGBA values (simplified — truncate to 7 bits)
    // In a real encoder, we'd do proper mode selection + endpoint optimization
    // For this implementation, we use a simple quantization
    u8 ep0[4], ep1[4];
    for (u32 c = 0; c < 4; ++c) {
        ep0[c] = maxC[c] >> 1;
        ep1[c] = minC[c] >> 1;
    }

    // Pack endpoints into bit stream after mode bits (bit 7)
    // Mode 6: 7+1 mode bits, then 2x(7bit x 4channels) = 56 bits endpoints,
    // 2 P-bits, then 4bit x 16 = 64 index bits
    // Total = 8 + 56 + 2 + 64 = 130 bits > 128, so simplified packing
    // We'll just store a reasonable approximation

    // Simple fallback: store as two-color interpolation with 4-bit indices
    // Pack endpoints (bytes 1-7)
    out[1] = ep0[0]; out[2] = ep0[1]; out[3] = ep0[2]; out[4] = ep0[3];
    out[5] = ep1[0]; out[6] = ep1[1]; out[7] = ep1[2]; out[8] = ep1[3];

    // Calculate and pack 4-bit indices (bytes 9-15, 64 bits)
    for (u32 i = 0; i < 16; ++i) {
        // Find interpolation index (0-15) that best matches pixel
        u32 best = 0;
        i32 bestErr = 0x7FFFFFFF;
        for (u32 t = 0; t < 16; ++t) {
            i32 err = 0;
            for (u32 c = 0; c < 4; ++c) {
                i32 interp = (maxC[c] * (15 - t) + minC[c] * t + 7) / 15;
                i32 d = static_cast<i32>(pixels[i][c]) - interp;
                err += d * d;
            }
            if (err < bestErr) { bestErr = err; best = t; }
        }
        u32 byteIdx = 9 + (i * 4) / 8;
        u32 bitIdx = (i * 4) % 8;
        if (byteIdx < 16) {
            out[byteIdx] |= static_cast<u8>(best << bitIdx);
            if (bitIdx > 4 && byteIdx + 1 < 16) {
                out[byteIdx + 1] |= static_cast<u8>(best >> (8 - bitIdx));
            }
        }
    }
}

// --- Mipmap generation ---

std::vector<u8> TextureCompressor::GenerateMipLevel(
    const u8* srcData, u32 srcWidth, u32 srcHeight, u32 channels) {

    u32 dstWidth = std::max(1u, srcWidth / 2);
    u32 dstHeight = std::max(1u, srcHeight / 2);
    std::vector<u8> dst(dstWidth * dstHeight * channels);

    for (u32 y = 0; y < dstHeight; ++y) {
        for (u32 x = 0; x < dstWidth; ++x) {
            for (u32 c = 0; c < channels; ++c) {
                u32 sx = x * 2;
                u32 sy = y * 2;
                u32 sx1 = std::min(sx + 1, srcWidth - 1);
                u32 sy1 = std::min(sy + 1, srcHeight - 1);

                u32 sum = srcData[(sy * srcWidth + sx) * channels + c]
                        + srcData[(sy * srcWidth + sx1) * channels + c]
                        + srcData[(sy1 * srcWidth + sx) * channels + c]
                        + srcData[(sy1 * srcWidth + sx1) * channels + c];

                dst[(y * dstWidth + x) * channels + c] = static_cast<u8>((sum + 2) / 4);
            }
        }
    }
    return dst;
}

// --- Main compression API ---

CompressedTextureData TextureCompressor::Compress(
    const u8* rgba8Data, u32 width, u32 height,
    const TextureCompressionSettings& settings) {

    CompressedTextureData result;
    result.format = settings.format;
    result.width = width;
    result.height = height;
    result.channels = 4;

    if (!rgba8Data || width == 0 || height == 0 || settings.format == CompressedFormat::None) {
        ENJIN_LOG_WARN(Assets, "TextureCompressor: invalid input or no format specified");
        return result;
    }

    // ASTC formats not yet implemented (require a full ASTC encoder)
    if (settings.format == CompressedFormat::ASTC_4x4 ||
        settings.format == CompressedFormat::ASTC_6x6 ||
        settings.format == CompressedFormat::ASTC_8x8) {
        ENJIN_LOG_WARN(Assets, "TextureCompressor: ASTC format not yet implemented, falling back to BC7");
        result.format = CompressedFormat::BC7;
    }

    // Generate mip chain
    std::vector<std::pair<u32, u32>> mipDims; // width, height per level
    std::vector<std::vector<u8>> mipPixels;

    // Level 0 is the original
    mipDims.push_back({width, height});
    mipPixels.emplace_back(rgba8Data, rgba8Data + width * height * 4);

    if (settings.generateMipmaps) {
        u32 mw = width, mh = height;
        u32 maxLevels = settings.maxMipLevels > 0
            ? settings.maxMipLevels
            : static_cast<u32>(std::floor(std::log2(std::max(width, height)))) + 1;

        while ((mw > 1 || mh > 1) && mipDims.size() < maxLevels) {
            auto mipData = GenerateMipLevel(mipPixels.back().data(), mw, mh, 4);
            mw = std::max(1u, mw / 2);
            mh = std::max(1u, mh / 2);
            mipDims.push_back({mw, mh});
            mipPixels.push_back(std::move(mipData));
        }
    }

    // Block size for BCn formats
    u32 blockBytes = 0;
    switch (result.format) {
        case CompressedFormat::BC1: blockBytes = 8; break;
        case CompressedFormat::BC3: blockBytes = 16; break;
        case CompressedFormat::BC4: blockBytes = 8; break;
        case CompressedFormat::BC5: blockBytes = 16; break;
        case CompressedFormat::BC7: blockBytes = 16; break;
        default: break;
    }

    if (blockBytes == 0) {
        ENJIN_LOG_ERROR(Assets, "TextureCompressor: unsupported format");
        return result;
    }

    // Compress each mip level
    for (usize level = 0; level < mipDims.size(); ++level) {
        u32 mw = mipDims[level].first;
        u32 mh = mipDims[level].second;
        const u8* src = mipPixels[level].data();

        // Pad to multiple of 4 if needed
        u32 bw = (mw + 3) / 4;
        u32 bh = (mh + 3) / 4;

        CompressedMipLevel mip;
        mip.width = mw;
        mip.height = mh;
        mip.data.resize(bw * bh * blockBytes);

        // Temporary padded block buffer
        u8 block[4 * 4 * 4]; // 4x4 RGBA

        for (u32 by = 0; by < bh; ++by) {
            for (u32 bx = 0; bx < bw; ++bx) {
                // Extract 4x4 block with edge clamping
                for (u32 py = 0; py < 4; ++py) {
                    for (u32 px = 0; px < 4; ++px) {
                        u32 sx = std::min(bx * 4 + px, mw - 1);
                        u32 sy = std::min(by * 4 + py, mh - 1);
                        const u8* pixel = src + (sy * mw + sx) * 4;
                        u8* dst = block + (py * 4 + px) * 4;
                        dst[0] = pixel[0];
                        dst[1] = pixel[1];
                        dst[2] = pixel[2];
                        dst[3] = pixel[3];
                    }
                }

                u8* outBlock = mip.data.data() + (by * bw + bx) * blockBytes;
                switch (result.format) {
                    case CompressedFormat::BC1:
                        CompressBlockBC1(block, 4, outBlock);
                        break;
                    case CompressedFormat::BC3:
                        CompressBlockBC3(block, 4, outBlock);
                        break;
                    case CompressedFormat::BC4:
                        CompressBlockBC4(block, 4, 0, outBlock);
                        break;
                    case CompressedFormat::BC5:
                        CompressBlockBC5(block, 4, outBlock);
                        break;
                    case CompressedFormat::BC7:
                        CompressBlockBC7(block, 4, outBlock);
                        break;
                    default:
                        break;
                }
            }
        }

        result.mipLevels.push_back(std::move(mip));
    }

    result.valid = true;

    usize totalCompressed = 0;
    for (const auto& mip : result.mipLevels) totalCompressed += mip.data.size();
    usize totalUncompressed = 0;
    for (const auto& dim : mipDims) totalUncompressed += dim.first * dim.second * 4;

    ENJIN_LOG_INFO(Assets, "Compressed texture %ux%u (%s): %zu -> %zu bytes (%.1fx), %zu mip levels",
        width, height, FormatName(result.format),
        totalUncompressed, totalCompressed,
        totalUncompressed > 0 ? static_cast<f32>(totalUncompressed) / totalCompressed : 1.0f,
        result.mipLevels.size());

    return result;
}

CompressedFormat TextureCompressor::RecommendFormat(
    u32 channels, bool hasAlpha, bool isNormalMap, bool isMobile) {

    if (isMobile) return CompressedFormat::ASTC_4x4;

    if (isNormalMap) {
        return CompressedFormat::BC5; // RG channels for XY normals
    }
    if (channels == 1) {
        return CompressedFormat::BC4; // Single channel
    }
    if (hasAlpha) {
        return CompressedFormat::BC7; // Best quality RGBA
    }
    return CompressedFormat::BC1; // RGB, no alpha needed
}

const char* TextureCompressor::FormatName(CompressedFormat format) {
    switch (format) {
        case CompressedFormat::None:     return "None";
        case CompressedFormat::BC1:      return "BC1 (DXT1)";
        case CompressedFormat::BC3:      return "BC3 (DXT5)";
        case CompressedFormat::BC4:      return "BC4 (R)";
        case CompressedFormat::BC5:      return "BC5 (RG)";
        case CompressedFormat::BC7:      return "BC7 (RGBA)";
        case CompressedFormat::ASTC_4x4: return "ASTC 4x4";
        case CompressedFormat::ASTC_6x6: return "ASTC 6x6";
        case CompressedFormat::ASTC_8x8: return "ASTC 8x8";
    }
    return "Unknown";
}

u32 TextureCompressor::BitsPerPixel(CompressedFormat format) {
    switch (format) {
        case CompressedFormat::None:     return 32; // RGBA8
        case CompressedFormat::BC1:      return 4;
        case CompressedFormat::BC3:      return 8;
        case CompressedFormat::BC4:      return 4;
        case CompressedFormat::BC5:      return 8;
        case CompressedFormat::BC7:      return 8;
        case CompressedFormat::ASTC_4x4: return 8;
        case CompressedFormat::ASTC_6x6: return 4; // ~3.56 rounded
        case CompressedFormat::ASTC_8x8: return 2;
    }
    return 32;
}

usize TextureCompressor::CalculateCompressedSize(CompressedFormat format, u32 width, u32 height) {
    u32 bw = (width + 3) / 4;
    u32 bh = (height + 3) / 4;
    u32 bpp = BitsPerPixel(format);
    // Each 4x4 block = blockBytes
    u32 blockBytes = (bpp * 16 + 7) / 8; // 16 pixels per block
    return static_cast<usize>(bw) * bh * blockBytes;
}

f32 TextureCompressor::CompressionRatio(CompressedFormat format) {
    return 32.0f / static_cast<f32>(BitsPerPixel(format));
}

} // namespace Assets
} // namespace Enjin
