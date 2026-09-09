#include "Enjin/Renderer/DepthPlate.h"

#include <cmath>

namespace Enjin {
namespace Renderer {

namespace {
f32 Clamp01(f32 v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
} // namespace

u32 PackPlateDepth24(f32 normalized) {
    const f32 scaled = Clamp01(normalized) * kPlateDepthMaxValue;
    // Rounded, not truncated. Truncation biases every distance toward the
    // camera by up to one step, and a systematic bias in a depth plate reads as
    // characters standing slightly inside walls.
    const u32 v = static_cast<u32>(scaled + 0.5f);
    return v > 0xFFFFFFu ? 0xFFFFFFu : v;
}

f32 UnpackPlateDepth24(u8 r, u8 g, u8 b) {
    const u32 v = (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) | static_cast<u32>(b);
    return static_cast<f32>(v) / kPlateDepthMaxValue;
}

f32 NormalizePlateDistance(f32 distance, f32 nearDistance, f32 farDistance) {
    const f32 span = farDistance - nearDistance;
    if (!(span > 0.0f)) return 0.0f;   // also catches NaN
    return Clamp01((distance - nearDistance) / span);
}

f32 DenormalizePlateDistance(f32 normalized, f32 nearDistance, f32 farDistance) {
    const f32 span = farDistance - nearDistance;
    if (!(span > 0.0f)) return nearDistance;
    return nearDistance + Clamp01(normalized) * span;
}

f32 PlateDepthMapping::Evaluate(f32 distance) const {
    if (inverse) {
        if (!(std::fabs(distance) > 1e-6f)) return 1.0f;
        return a + b / distance;
    }
    return a + b * distance;
}

f32 ProjectViewDistanceToDepth(const Math::Matrix4& proj, f32 distance) {
    // Column-major, element (row, col) = m[col * 4 + row]. The view-space point
    // is (0, 0, -distance, 1): this engine's cameras look down -Z.
    const f32 z = -distance;
    const f32 zClip = proj.m[10] * z + proj.m[14];
    const f32 wClip = proj.m[11] * z + proj.m[15];
    if (!(std::fabs(wClip) > 1e-9f)) return 1.0f;
    return zClip / wClip;
}

PlateDepthMapping SolvePlateDepthMapping(const Math::Matrix4& proj,
                                         f32 nearDistance, f32 farDistance) {
    PlateDepthMapping out;
    // A degenerate range cannot describe anything. Everything lands on the far
    // plane, which draws the plate behind all live geometry -- visibly wrong,
    // and visibly wrong is the right failure for authored data.
    if (!(nearDistance > 0.0f) || !(farDistance > nearDistance)) {
        out.inverse = false;
        out.a = 1.0f;
        out.b = 0.0f;
        return out;
    }

    const f32 d1 = nearDistance;
    const f32 d2 = farDistance;
    const f32 z1 = ProjectViewDistanceToDepth(proj, d1);
    const f32 z2 = ProjectViewDistanceToDepth(proj, d2);

    // Both candidate forms, fitted through the same two points.
    PlateDepthMapping inv;
    inv.inverse = true;
    const f32 invSpan = (1.0f / d1) - (1.0f / d2);
    if (std::fabs(invSpan) > 1e-9f) {
        inv.b = (z1 - z2) / invSpan;
        inv.a = z1 - inv.b / d1;
    }

    PlateDepthMapping lin;
    lin.inverse = false;
    lin.b = (z1 - z2) / (d1 - d2);
    lin.a = z1 - lin.b * d1;

    // Decided by which one actually reproduces the matrix at a THIRD point,
    // rather than by inspecting the matrix for a perspective-looking element.
    // Two points fit either form exactly, so the midpoint is what separates
    // them, and a projection this does not recognise still gets the better of
    // the two rather than a guess.
    const f32 dm = 0.5f * (d1 + d2);
    const f32 truth = ProjectViewDistanceToDepth(proj, dm);
    const f32 errInv = std::fabs(inv.Evaluate(dm) - truth);
    const f32 errLin = std::fabs(lin.Evaluate(dm) - truth);
    return (errInv <= errLin) ? inv : lin;
}

f32 InvertPlateDepth(const PlateDepthMapping& mapping, f32 depth) {
    if (mapping.inverse) {
        const f32 denom = depth - mapping.a;
        // Depth equal to `a` is the projection's horizon: the distance is
        // infinite. A cleared depth buffer sits there, so this is the ordinary
        // case for background pixels, not an error.
        if (!(std::fabs(denom) > 1e-9f)) return 0.0f;
        const f32 dist = mapping.b / denom;
        return dist > 0.0f ? dist : 0.0f;
    }
    if (!(std::fabs(mapping.b) > 1e-9f)) return 0.0f;
    const f32 dist = (depth - mapping.a) / mapping.b;
    return dist > 0.0f ? dist : 0.0f;
}

} // namespace Renderer
} // namespace Enjin
