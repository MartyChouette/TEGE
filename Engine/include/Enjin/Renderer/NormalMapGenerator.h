#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

struct NormalMapOptions {
    f32 strength = 1.0f;   // Normal map intensity multiplier
    bool flipY = false;    // Flip green channel (DirectX vs OpenGL convention)
};

// Generates normal maps from height/grayscale images using Sobel operator
class ENJIN_API NormalMapGenerator {
public:
    using Options = NormalMapOptions;

    static std::vector<u8> Generate(const u8* heightPixels, u32 w, u32 h,
                                    const NormalMapOptions& opts = {});

    static bool GenerateAndSave(const std::string& heightPath,
                                const std::string& normalPath,
                                const NormalMapOptions& opts = {});
};

} // namespace Renderer
} // namespace Enjin
