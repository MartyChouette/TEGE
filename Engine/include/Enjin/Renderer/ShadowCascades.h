#pragma once
// Cascaded shadow map fitting, shared by both backends.
//
// This lived inside ShadowMap::UpdateCascades, which is Vulkan-only, so the web
// renderer grew its own single-map fit instead: one orthographic box around the
// casters, each caster approximated as a unit cube, the ground assumed to be at
// y = 0, and wide flat meshes skipped as casters. Different pictures from the
// same scene. Both backends now fit their cascades here (WP-16).
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Renderer {

struct ShadowCascadeInput {
    Math::Matrix4 cameraView;
    Math::Matrix4 cameraProj;
    f32 cameraNear = 0.1f;
    f32 cameraFar = 1000.0f;
    f32 shadowDistance = 80.0f;   // cascades end here, or at cameraFar if nearer
    f32 splitLambda = 0.75f;      // 0 = linear splits, 1 = logarithmic
    u32 cascadeCount = 4;
    u32 resolution = 2048;        // texels per cascade edge, for texel snapping
    Math::Vector3 lightDir;       // direction the light travels
};

// Fills outSplits[i] (view-space far distance of cascade i) and outViewProj[i]
// (light proj * view, depth 0..1) for i < cascadeCount. Each cascade is fitted
// to the bounding sphere of its slice of the camera frustum and snapped to its
// own texel grid, so the map holds still under camera rotation and translation.
ENJIN_API void ComputeShadowCascades(const ShadowCascadeInput& in, f32* outSplits, Math::Matrix4* outViewProj);

} // namespace Renderer
} // namespace Enjin
