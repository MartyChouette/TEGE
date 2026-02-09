#include "Enjin/Renderer/NormalMapGenerator.h"
#include "Enjin/Logging/Log.h"
#include <stb_image.h>
#include <stb_image_write.h>
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Renderer {

// Sample height (red channel) from RGBA with clamped bounds
static f32 SampleHeight(const u8* pixels, u32 w, u32 h, i32 x, i32 y) {
    x = std::clamp(x, 0, (i32)w - 1);
    y = std::clamp(y, 0, (i32)h - 1);
    return static_cast<f32>(pixels[((u32)y * w + (u32)x) * 4]) / 255.0f;
}

std::vector<u8> NormalMapGenerator::Generate(const u8* heightPixels, u32 w, u32 h,
                                             const Options& opts) {
    std::vector<u8> result(w * h * 4, 255);
    if (!heightPixels || w == 0 || h == 0) return result;

    f32 strength = opts.strength;

    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            // Sobel kernels for Gx and Gy
            f32 tl = SampleHeight(heightPixels, w, h, (i32)x - 1, (i32)y - 1);
            f32 t  = SampleHeight(heightPixels, w, h, (i32)x,     (i32)y - 1);
            f32 tr = SampleHeight(heightPixels, w, h, (i32)x + 1, (i32)y - 1);
            f32 l  = SampleHeight(heightPixels, w, h, (i32)x - 1, (i32)y);
            f32 r  = SampleHeight(heightPixels, w, h, (i32)x + 1, (i32)y);
            f32 bl = SampleHeight(heightPixels, w, h, (i32)x - 1, (i32)y + 1);
            f32 b  = SampleHeight(heightPixels, w, h, (i32)x,     (i32)y + 1);
            f32 br = SampleHeight(heightPixels, w, h, (i32)x + 1, (i32)y + 1);

            // Sobel X: [-1 0 1; -2 0 2; -1 0 1]
            f32 gx = (-tl - 2.0f * l - bl) + (tr + 2.0f * r + br);
            // Sobel Y: [-1 -2 -1; 0 0 0; 1 2 1]
            f32 gy = (-tl - 2.0f * t - tr) + (bl + 2.0f * b + br);

            if (opts.flipY) gy = -gy;

            // Normal = normalize(-gx * strength, -gy * strength, 1.0)
            f32 nx = -gx * strength;
            f32 ny = -gy * strength;
            f32 nz = 1.0f;
            f32 len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 0.0f) {
                nx /= len;
                ny /= len;
                nz /= len;
            }

            // Encode to [0, 255]: n * 0.5 + 0.5
            u32 idx = (y * w + x) * 4;
            result[idx + 0] = static_cast<u8>(std::clamp((nx * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
            result[idx + 1] = static_cast<u8>(std::clamp((ny * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
            result[idx + 2] = static_cast<u8>(std::clamp((nz * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));
            result[idx + 3] = 255;
        }
    }

    return result;
}

bool NormalMapGenerator::GenerateAndSave(const std::string& heightPath,
                                         const std::string& normalPath,
                                         const Options& opts) {
    int w, h, channels;
    u8* pixels = stbi_load(heightPath.c_str(), &w, &h, &channels, 4);
    if (!pixels) {
        ENJIN_LOG_ERROR(Renderer, "NormalMapGenerator: Failed to load height image: %s", heightPath.c_str());
        return false;
    }

    auto normalData = Generate(pixels, (u32)w, (u32)h, opts);
    stbi_image_free(pixels);

    if (!stbi_write_png(normalPath.c_str(), w, h, 4, normalData.data(), w * 4)) {
        ENJIN_LOG_ERROR(Renderer, "NormalMapGenerator: Failed to write normal map: %s", normalPath.c_str());
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "NormalMapGenerator: Generated normal map %s -> %s (%dx%d)",
                   heightPath.c_str(), normalPath.c_str(), w, h);
    return true;
}

} // namespace Renderer
} // namespace Enjin
