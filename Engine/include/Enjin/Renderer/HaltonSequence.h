#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

namespace Enjin::Renderer {

// Number of Halton samples before the sequence repeats
constexpr u32 kHaltonSampleCount = 16;

// Compute element `index` of the Halton sequence for the given `base`.
// Returns a value in [0, 1).
inline f32 HaltonSequence(u32 index, u32 base) {
    f32 result = 0.0f;
    f32 fraction = 1.0f;
    u32 i = index;
    while (i > 0) {
        fraction /= static_cast<f32>(base);
        result += fraction * static_cast<f32>(i % base);
        i /= base;
    }
    return result;
}

// Compute a per-frame sub-pixel jitter offset in NDC space for TAA.
// Uses Halton bases 2 and 3, cycling through kHaltonSampleCount samples.
// The returned offset should be added to the projection matrix:
//   projection[2][0] += jitter.x
//   projection[2][1] += jitter.y
inline Math::Vector2 HaltonJitter(u32 frameIndex, u32 width, u32 height) {
    // Start at index 1 to avoid the (0,0) sample at index 0
    u32 index = (frameIndex % kHaltonSampleCount) + 1;

    // Raw Halton values in [0, 1), centered to [-0.5, 0.5)
    f32 jitterX = HaltonSequence(index, 2) - 0.5f;
    f32 jitterY = HaltonSequence(index, 3) - 0.5f;

    // Scale to sub-pixel size in NDC (2/dimension because NDC spans [-1, 1])
    jitterX *= 2.0f / static_cast<f32>(width);
    jitterY *= 2.0f / static_cast<f32>(height);

    return Math::Vector2(jitterX, jitterY);
}

} // namespace Enjin::Renderer
