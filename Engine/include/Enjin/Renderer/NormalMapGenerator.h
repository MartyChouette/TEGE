#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

// Generates normal maps from height/grayscale images using Sobel operator
class ENJIN_API NormalMapGenerator {
public:
    struct Options {
        f32 strength = 1.0f;   // Normal map intensity multiplier
        bool flipY = false;    // Flip green channel (DirectX vs OpenGL convention)
    };

    // Generate RGBA normal map from grayscale height pixels (single channel or RGBA)
    // Input: heightPixels in RGBA format (uses red channel as height)
    // Returns: RGBA normal map (flat = 128,128,255)
    static std::vector<u8> Generate(const u8* heightPixels, u32 w, u32 h,
                                    const Options& opts = Options{});

    // Load a height image, generate normal map, save as PNG
    static bool GenerateAndSave(const std::string& heightPath,
                                const std::string& normalPath,
                                const Options& opts = Options{});
};

} // namespace Renderer
} // namespace Enjin
