#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vector>

namespace Enjin {
namespace Editor {

// Traces the contour of a sprite's opaque region using marching squares
// and simplifies the result with Douglas-Peucker
class ENJIN_API SpriteContourTracer {
public:
    // Trace the outer contour of non-transparent pixels
    // Returns vertices in pixel coordinates (0..w, 0..h)
    static std::vector<Math::Vector2> TraceContour(
        const u8* pixels, u32 w, u32 h, u8 alphaThreshold = 10);

    // Simplify a polyline using Douglas-Peucker algorithm
    // epsilon controls the maximum deviation in pixels
    static std::vector<Math::Vector2> Simplify(
        const std::vector<Math::Vector2>& points, f32 epsilon = 1.0f);

private:
    // Marching squares lookup for edge direction
    static void MarchingSquares(const u8* pixels, u32 w, u32 h,
                                u8 alphaThreshold,
                                std::vector<Math::Vector2>& outContour);

    // Point-to-segment perpendicular distance
    static f32 PointLineDistance(const Math::Vector2& p,
                                const Math::Vector2& a,
                                const Math::Vector2& b);

    // Recursive Douglas-Peucker
    static void DouglasPeucker(const std::vector<Math::Vector2>& points,
                               usize start, usize end, f32 epsilon,
                               std::vector<bool>& keep);
};

} // namespace Editor
} // namespace Enjin
