#include "Enjin/Renderer/ShadowCascades.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Renderer {

// Moved verbatim from ShadowMap::UpdateCascades so the web renderer fits the
// same cascades; ShadowMap now calls this. The comments are the originals.
void ComputeShadowCascades(const ShadowCascadeInput& in, f32* outSplits, Math::Matrix4* outViewProj) {
    const Math::Matrix4& cameraView = in.cameraView;
    const Math::Matrix4& cameraProj = in.cameraProj;
    const f32 cameraNear = in.cameraNear;
    const f32 cameraFar = in.cameraFar;
    const Math::Vector3& lightDir = in.lightDir;

    // Clamp camera far to shadow distance
    f32 effectiveFar = std::min(in.cameraFar, in.shadowDistance);

    // Compute split distances using practical split scheme
    // (blend between logarithmic and linear split)
    f32 lambda = in.splitLambda;
    f32 ratio = effectiveFar / cameraNear;
    f32 range = effectiveFar - cameraNear;

    for (u32 i = 0; i < in.cascadeCount; ++i) {
        f32 p = static_cast<f32>(i + 1) / static_cast<f32>(in.cascadeCount);
        f32 logSplit = cameraNear * std::pow(ratio, p);
        f32 linearSplit = cameraNear + range * p;
        outSplits[i] = lambda * logSplit + (1.0f - lambda) * linearSplit;
    }

    // Compute inverse view-projection to get frustum corners in world space
    Math::Matrix4 invViewProj = (cameraProj * cameraView).Inverse();

    // Unproject the 4 frustum rays at the actual near (z_ndc=0) and far (z_ndc=1) planes.
    // We then interpolate linearly in world space for each cascade's split distances,
    // which is correct because depth varies linearly along each camera ray.
    // (Using linear depth fractions as NDC z is WRONG — perspective maps depth non-linearly.)
    Math::Vector3 nearCorners[4], farCorners[4];
    {
        int idx = 0;
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                Math::Vector4 nearNdc(x * 2.0f - 1.0f, y * 2.0f - 1.0f, 0.0f, 1.0f);
                Math::Vector4 farNdc(x * 2.0f - 1.0f, y * 2.0f - 1.0f, 1.0f, 1.0f);
                Math::Vector4 nw = invViewProj * nearNdc;
                Math::Vector4 fw = invViewProj * farNdc;
                nearCorners[idx] = Math::Vector3(nw.x / nw.w, nw.y / nw.w, nw.z / nw.w);
                farCorners[idx]  = Math::Vector3(fw.x / fw.w, fw.y / fw.w, fw.z / fw.w);
                idx++;
            }
        }
    }

    for (u32 cascade = 0; cascade < in.cascadeCount; ++cascade) {
        f32 splitNear = (cascade == 0) ? cameraNear : outSplits[cascade - 1];
        f32 splitFar = outSplits[cascade];

        // Linear interpolation fractions along the camera rays (world space)
        f32 nearT = (splitNear - cameraNear) / (cameraFar - cameraNear);
        f32 farT  = (splitFar  - cameraNear) / (cameraFar - cameraNear);

        // 8 world-space corners of the cascade sub-frustum
        Math::Vector3 corners[8];
        for (int i = 0; i < 4; ++i) {
            Math::Vector3 ray = farCorners[i] - nearCorners[i];
            corners[i]     = nearCorners[i] + ray * nearT;
            corners[i + 4] = nearCorners[i] + ray * farT;
        }

        // Compute frustum center
        Math::Vector3 center(0.0f);
        for (int i = 0; i < 8; ++i) {
            center = center + corners[i];
        }
        center = center * (1.0f / 8.0f);

        // Fit the cascade to the BOUNDING SPHERE of the slice, not the
        // light-space AABB of its corners (stable CSM). The AABB changes size
        // as the camera ROTATES, which rescales worldUnitsPerTexel — the
        // snapping grid itself — every frame, so the old center-snap below
        // only ever stabilized translation: rotating the camera re-quantized
        // every cascade per frame (shadow edges shimmer/crawl, borderline
        // surfaces pop in and out of shadow). The sphere's radius depends only
        // on the slice shape (FOV/aspect/splits), so the ortho extent and
        // texel size are constant and snapping holds under rotation too. The
        // slightly looser fit is the standard stable-CSM tradeoff.
        f32 radius = 0.0f;
        for (int i = 0; i < 8; ++i) {
            radius = std::max(radius, (corners[i] - center).Length());
        }
        // Quantize the radius so FP noise in the corner math can't wobble the fit
        radius = std::ceil(radius * 16.0f) / 16.0f;

        // Build light view matrix (eye pulled back far enough that the whole
        // sphere plus zPad worth of behind-frustum casters sits in front of it)
        const f32 zPad = 50.0f;
        Math::Vector3 lightDirN = lightDir.Normalized();
        Math::Vector3 lightUp(0.0f, 1.0f, 0.0f);
        if (std::abs(lightDirN.Dot(lightUp)) > 0.99f) {
            lightUp = Math::Vector3(0.0f, 0.0f, 1.0f);
        }
        Math::Matrix4 lightView = Math::Matrix4::LookAt(center - lightDirN * (radius + zPad), center, lightUp);

        // Constant-size ortho bounds from the sphere. Z still comes from the
        // light-space corners: it affects only the depth mapping, which the
        // shadow render and the main-pass sample share, so it doesn't need to
        // be rotation-stable — X/Y extent is what texel stability requires.
        f32 minX = -radius, maxX = radius;
        f32 minY = -radius, maxY = radius;
        f32 minZ = 1e9f, maxZ = -1e9f;
        for (int i = 0; i < 8; ++i) {
            Math::Vector4 lsCorner = lightView * Math::Vector4(corners[i].x, corners[i].y, corners[i].z, 1.0f);
            minZ = std::min(minZ, lsCorner.z);
            maxZ = std::max(maxZ, lsCorner.z);
        }
        // Z padding so shadow casters behind the frustum are captured
        minZ -= zPad;
        maxZ += zPad;

        // Build orthographic projection directly from light-space AABB bounds.
        // We bypass Math::Matrix4::Orthographic() because it applies a Vulkan Y-flip
        // (m[5] negated) without adjusting the m[13] translation, which clips geometry
        // for non-centered AABBs. Shadow maps are offscreen and don't need Y-flip.
        // Depth maps to Vulkan [0,1]: z_view=maxZ (closest to light) → 0, z_view=minZ → 1.
        Math::Matrix4 lightProj = Math::Matrix4::Identity();
        lightProj.m[0]  =  2.0f / (maxX - minX);
        lightProj.m[5]  =  2.0f / (maxY - minY);
        lightProj.m[10] = -1.0f / (maxZ - minZ);
        lightProj.m[12] = -(maxX + minX) / (maxX - minX);
        lightProj.m[13] = -(maxY + minY) / (maxY - minY);
        lightProj.m[14] =  maxZ / (maxZ - minZ);

        // Texel snapping, stable form: the ortho size is constant, so the
        // texel grid never rescales — snapping the projection of a fixed
        // world point (the origin) to texel increments pins the whole map to
        // the grid under any camera motion, rotation included.
        {
            Math::Matrix4 lightViewProj = lightProj * lightView;
            f32 halfRes = static_cast<f32>(in.resolution) * 0.5f;
            Math::Vector4 shadowOrigin = lightViewProj * Math::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
            f32 ox = shadowOrigin.x * halfRes;
            f32 oy = shadowOrigin.y * halfRes;
            lightProj.m[12] += (std::round(ox) - ox) / halfRes;
            lightProj.m[13] += (std::round(oy) - oy) / halfRes;
        }

        outViewProj[cascade] = lightProj * lightView;
    }
}

} // namespace Renderer
} // namespace Enjin
