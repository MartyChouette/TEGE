#pragma once

// Radiosity normal mapping: baked light that still responds to normal maps.
//
// An ordinary lightmap stores one irradiance value per texel, which is a
// surface's answer to "how much light arrives here" and has no answer at all to
// "from which direction". Put a normal map on it and nothing happens -- the
// bumps are lit identically to the flat surface, because the lighting was
// resolved before the bumps existed.
//
// Half-Life 2 solved it by storing THREE values per texel, one for each vector
// of a fixed basis sitting in tangent space, and blending them at run time by
// how much the normal-mapped normal faces each one. Baked global illumination
// that reacts to per-pixel detail, for three texture reads and no rays.
//
// It went away when the industry moved to fully dynamic deferred lighting. For
// a stylized engine aimed at weak hardware and the web it is still the best deal
// in graphics: the expensive part happens once, offline, at any quality, and
// what ships is three textures.
//
// This header is the part with no GPU and no bake in it -- the basis itself and
// the blend -- so it can be tested exactly and matched byte for byte by the
// shader that has to agree with it.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Renderer {

// The three basis directions, in TANGENT space, as Half-Life 2 defined them:
// evenly spread around the tangent-space Z axis, each tilted the same amount
// away from it. Any normal in the hemisphere faces at least one of them.
//
// These exact numbers are load-bearing. The bake integrates light against them
// and the shader blends by them; if the two ever disagree the lighting does not
// fail, it just leans -- surfaces pick up a directional bias nobody authored,
// which reads as a bad bake rather than as a mismatch.
inline constexpr u32 kRNMBasisCount = 3;

const Math::Vector3* RNMBasis();

// How much a tangent-space normal faces each basis direction.
//
// Clamped at zero (a basis direction the normal turns away from contributes
// nothing) and normalized to sum to one, which is what keeps a normal-mapped
// surface as bright as the flat lightmap says it should be. Without that
// normalization the total light changes with the bump angle and flat regions
// come out darker than the value that was baked for them.
Math::Vector3 RNMWeights(const Math::Vector3& tangentSpaceNormal);

// Blend three baked values by those weights.
Math::Vector3 RNMResolve(const Math::Vector3& weights,
                         const Math::Vector3& basis0,
                         const Math::Vector3& basis1,
                         const Math::Vector3& basis2);

} // namespace Renderer
} // namespace Enjin
