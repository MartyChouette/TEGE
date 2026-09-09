#pragma once

// Pre-rendered backgrounds with a depth plate.
//
// The technique: render a room offline at any quality you like, ship the
// finished image plus a per-pixel DEPTH image, and then draw live characters
// into it. Because the plate carries depth, a character walking behind a pillar
// is occluded by that pillar even though the pillar is just pixels. Resident
// Evil, Final Fantasy VII and Alone in the Dark all worked this way, and the
// result is a world visibly more detailed than the hardware could ever draw
// live.
//
// It also passes the filter the rest of that list is sorted by: nothing here is
// screen-space. The plate is a WORLD, authored from a fixed camera, and it does
// not stop existing at the frame edge -- it stops existing where the room does.
//
// This header is the part with no GPU in it: how a distance is packed into
// image bytes, and how a distance becomes the exact depth value the rasterizer
// would have written. Both are pure, so they can be tested without a device and
// reasoned about without a frame capture.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Matrix.h"

namespace Enjin {
namespace Renderer {

// --- Packing -----------------------------------------------------------------
//
// A depth plate is stored as an ordinary 8-bit image with the distance spread
// across R, G and B (most significant byte first). Eight bits alone is 256
// depth steps, which makes a character's feet visibly pop between layers; 24
// bits is 16.7 million and the banding disappears.
//
// This is why a depth plate must be loaded UNORM and sampled NEAREST. These
// bytes are a NUMBER, not a colour: an sRGB decode would rescale them and
// linear filtering would average two unrelated bytes into a distance that
// exists nowhere in the scene.

inline constexpr f32 kPlateDepthMaxValue = 16777215.0f;   // 2^24 - 1

// Normalized [0,1] -> packed 0xRRGGBB.
u32 PackPlateDepth24(f32 normalized);

// Packed bytes -> normalized [0,1].
f32 UnpackPlateDepth24(u8 r, u8 g, u8 b);

// Where a world distance sits between the plate's near and far, and back again.
// Anything outside the range clamps: a plate cannot describe what it did not
// render, and pretending otherwise would put geometry at a made-up distance.
f32 NormalizePlateDistance(f32 distance, f32 nearDistance, f32 farDistance);
f32 DenormalizePlateDistance(f32 normalized, f32 nearDistance, f32 farDistance);

// --- Depth mapping -----------------------------------------------------------
//
// The plate stores a LINEAR distance, and the depth buffer holds whatever
// non-linear value the projection produces. Getting between them wrong does not
// fail loudly -- the character simply sinks into the floor or floats in front of
// walls -- so the mapping is solved from the projection matrix ITSELF rather
// than from a remembered formula.
//
// That matters here more than usual. This engine's Matrix4::Perspective is the
// OpenGL form (near maps to -1, not to 0), and Vulkan clips everything below 0,
// so the effective near plane is the harmonic mean of near and far rather than
// the near plane. Any hand-derived formula that assumed the Vulkan convention
// would be wrong by exactly that much, and would look like a bias problem.
struct PlateDepthMapping {
    // depth = inverse ? (a + b / distance) : (a + b * distance)
    f32 a = 0.0f;
    f32 b = 0.0f;
    // Perspective is a + b/distance; orthographic is linear in distance. Which
    // one applies is decided by fitting both and keeping the one that actually
    // reproduces the matrix, so an ortho shot is not silently wrong.
    bool inverse = true;

    f32 Evaluate(f32 distance) const;
};

// The depth the rasterizer would write for a point `distance` units in front of
// the camera, straight down its forward axis. Evaluates the matrix rather than
// assuming a convention: view-space point (0, 0, -distance), clip z over clip w.
f32 ProjectViewDistanceToDepth(const Math::Matrix4& proj, f32 distance);

// Fit a mapping to the projection over the plate's range.
PlateDepthMapping SolvePlateDepthMapping(const Math::Matrix4& proj,
                                         f32 nearDistance, f32 farDistance);

// The other direction, used when BAKING a plate: the depth buffer hands back
// projected depth and the plate has to store linear distance. Returns 0 when
// the mapping cannot be inverted at that value (an empty depth buffer reads as
// the far plane, which inverts to infinity).
f32 InvertPlateDepth(const PlateDepthMapping& mapping, f32 depth);

} // namespace Renderer
} // namespace Enjin
