#pragma once

#include "Enjin/Platform/Platform.h"
#include <cstdint>
#include <vector>

namespace Enjin {
namespace Renderer {

// Generates signed distance fields from alpha images using 8SSEDT
// (8-points Signed Sequential Euclidean Distance Transform)
class ENJIN_API SDFGenerator {
public:
    struct SDFResult {
        std::vector<float> distances; // Signed distances (negative inside, positive outside)
        uint32_t width = 0;
        uint32_t height = 0;
    };

    // Generate SDF from alpha channel of RGBA pixels
    // spread: max distance in pixels to compute (clamp beyond this)
    static SDFResult Generate(const uint8_t* alphaPixels, uint32_t w, uint32_t h, float spread = 8.0f);

    // Convert SDF result to RGBA image (distance encoded in alpha channel)
    // Alpha = 255 at center (0 distance), 0 at max spread
    static std::vector<uint8_t> ToRGBA(const SDFResult& sdf);

private:
    static void ComputeDistanceField(const std::vector<bool>& inside,
                                     uint32_t w, uint32_t h,
                                     std::vector<float>& outDistances);
};

} // namespace Renderer
} // namespace Enjin
