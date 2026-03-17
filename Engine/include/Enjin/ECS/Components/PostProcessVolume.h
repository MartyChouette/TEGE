#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Renderer/PostProcessing.h"
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace ECS {

// Post-process volume shape
enum class PPVolumeShape : u32 {
    Box = 0,     // Axis-aligned bounding box
    Sphere = 1   // Spherical region (radius = halfExtents.x)
};

// Post-process volume component - defines a region with custom post-processing settings.
// When the camera enters this volume, its settings blend with the global/default settings.
// Multiple volumes can overlap; blending is priority-sorted with distance-based weight falloff.
// A "global" volume applies everywhere (ignores shape/position) and serves as a base layer.
struct ENJIN_API PostProcessVolumeComponent {
    // Volume shape (ignored when isGlobal = true)
    PPVolumeShape shape = PPVolumeShape::Box;

    // Bounding box half-extents (or radius for sphere, using halfExtents.x)
    Math::Vector3 halfExtents = Math::Vector3(10.0f, 10.0f, 10.0f);

    // Higher priority volumes override lower ones when overlapping (0 = default)
    i32 priority = 0;

    // Whether this volume is active
    bool isActive = true;

    // Global volume: applies everywhere, ignores shape/position. Use as a scene-wide base.
    bool isGlobal = false;

    // Blend radius: distance over which the volume fades in at its edges (world units).
    // 0 = hard boundary (instant on/off), >0 = smooth gradient at volume edges.
    f32 blendRadius = 2.0f;

    // Blend weight: maximum influence of this volume (0 = none, 1 = full override).
    f32 weight = 1.0f;

    // Which settings to override (bitmask). Allows partial overrides — only modify bloom
    // without touching tone mapping, etc. 0xFFFFFFFF = override everything.
    u32 overrideMask = 0xFFFFFFFF;

    // Override flag bits (which groups of settings this volume controls)
    static constexpr u32 OverrideToneMapping       = 1 << 0;
    static constexpr u32 OverrideBloom             = 1 << 1;
    static constexpr u32 OverrideVignette          = 1 << 2;
    static constexpr u32 OverrideChromaticAberr    = 1 << 3;
    static constexpr u32 OverrideColorGrading      = 1 << 4;
    static constexpr u32 OverrideFilmGrain         = 1 << 5;
    static constexpr u32 OverrideFXAA              = 1 << 6;
    static constexpr u32 OverrideDither            = 1 << 7;
    static constexpr u32 OverrideColorQuant        = 1 << 8;
    static constexpr u32 OverrideResDownscale      = 1 << 9;
    static constexpr u32 OverrideCRT               = 1 << 10;
    static constexpr u32 OverrideLUT               = 1 << 11;
    static constexpr u32 OverrideVHS               = 1 << 12;
    static constexpr u32 OverridePalette           = 1 << 13;
    static constexpr u32 OverrideDoF               = 1 << 14;
    static constexpr u32 OverrideTiltShift         = 1 << 15;
    static constexpr u32 OverrideCelOutline        = 1 << 16;
    static constexpr u32 OverrideStipple           = 1 << 17;
    static constexpr u32 OverrideCRTPhosphor       = 1 << 18;
    static constexpr u32 OverrideGodRays           = 1 << 19;
    static constexpr u32 OverrideSSAO              = 1 << 20;
    static constexpr u32 OverrideContactShadows    = 1 << 21;
    static constexpr u32 OverrideCaustics          = 1 << 22;
    static constexpr u32 OverrideFogShafts         = 1 << 23;
    static constexpr u32 OverrideAll               = 0xFFFFFFFF;

    // The post-process settings this volume wants to apply.
    Renderer::PostProcessSettings settings;

    // Check if a point is inside this volume (ignoring blend radius)
    bool ContainsPoint(const Math::Vector3& center, const Math::Vector3& point) const {
        if (isGlobal) return true;
        if (shape == PPVolumeShape::Sphere) {
            Math::Vector3 diff = point - center;
            f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
            f32 radius = halfExtents.x;
            return distSq <= radius * radius;
        }
        // Box
        return std::abs(point.x - center.x) <= halfExtents.x &&
               std::abs(point.y - center.y) <= halfExtents.y &&
               std::abs(point.z - center.z) <= halfExtents.z;
    }

    // Get the blend weight at a given point (0 = outside, 1 = fully inside).
    // Smoothly transitions over blendRadius at the volume edges.
    f32 GetBlendWeight(const Math::Vector3& center, const Math::Vector3& point) const {
        if (isGlobal) return weight;
        if (!isActive) return 0.0f;

        f32 distance = 0.0f;
        if (shape == PPVolumeShape::Sphere) {
            Math::Vector3 diff = point - center;
            f32 dist = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
            f32 radius = halfExtents.x;
            distance = dist - radius; // Negative = inside, positive = outside
        } else {
            // Signed distance to box surface (negative = inside)
            f32 dx = std::abs(point.x - center.x) - halfExtents.x;
            f32 dy = std::abs(point.y - center.y) - halfExtents.y;
            f32 dz = std::abs(point.z - center.z) - halfExtents.z;
            distance = std::max({dx, dy, dz}); // Negative inside, positive outside
        }

        if (distance >= blendRadius) return 0.0f; // Fully outside blend range
        if (distance <= 0.0f) return weight;       // Fully inside

        // Smooth blend in the transition zone (0..blendRadius)
        f32 t = 1.0f - (distance / std::max(blendRadius, 0.001f));
        // Smoothstep for nice falloff
        t = t * t * (3.0f - 2.0f * t);
        return t * weight;
    }
};

// Blend two PostProcessSettings using lerp on float fields.
// Only blends groups enabled in overrideMask.
inline void BlendPostProcessSettings(
    Renderer::PostProcessSettings& out,
    const Renderer::PostProcessSettings& a,
    const Renderer::PostProcessSettings& b,
    f32 t, u32 overrideMask)
{
    auto lerpF = [](f32 x, f32 y, f32 t) { return x + (y - x) * t; };
    auto lerpU = [](u32 x, u32 y, f32 t) -> u32 { return t >= 0.5f ? y : x; };
    auto lerpV = [&](Math::Vector3 x, Math::Vector3 y, f32 t) {
        return Math::Vector3(lerpF(x.x, y.x, t), lerpF(x.y, y.y, t), lerpF(x.z, y.z, t));
    };

    if (overrideMask & PostProcessVolumeComponent::OverrideToneMapping) {
        out.toneMappingMode = lerpU(a.toneMappingMode, b.toneMappingMode, t);
        out.exposure = lerpF(a.exposure, b.exposure, t);
        out.gamma = lerpF(a.gamma, b.gamma, t);
        out.whitePoint = lerpF(a.whitePoint, b.whitePoint, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideBloom) {
        out.bloomEnabled = lerpU(a.bloomEnabled, b.bloomEnabled, t);
        out.bloomThreshold = lerpF(a.bloomThreshold, b.bloomThreshold, t);
        out.bloomIntensity = lerpF(a.bloomIntensity, b.bloomIntensity, t);
        out.bloomRadius = lerpF(a.bloomRadius, b.bloomRadius, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideVignette) {
        out.vignetteEnabled = lerpU(a.vignetteEnabled, b.vignetteEnabled, t);
        out.vignetteIntensity = lerpF(a.vignetteIntensity, b.vignetteIntensity, t);
        out.vignetteSmoothness = lerpF(a.vignetteSmoothness, b.vignetteSmoothness, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideChromaticAberr) {
        out.chromaticAberrationEnabled = lerpU(a.chromaticAberrationEnabled, b.chromaticAberrationEnabled, t);
        out.chromaticAberrationIntensity = lerpF(a.chromaticAberrationIntensity, b.chromaticAberrationIntensity, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideColorGrading) {
        out.colorFilter = lerpV(a.colorFilter, b.colorFilter, t);
        out.saturation = lerpF(a.saturation, b.saturation, t);
        out.contrast = lerpF(a.contrast, b.contrast, t);
        out.brightness = lerpF(a.brightness, b.brightness, t);
        out.colorblindMode = lerpU(a.colorblindMode, b.colorblindMode, t);
        out.colorblindStrength = lerpF(a.colorblindStrength, b.colorblindStrength, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideFilmGrain) {
        out.filmGrainEnabled = lerpU(a.filmGrainEnabled, b.filmGrainEnabled, t);
        out.filmGrainIntensity = lerpF(a.filmGrainIntensity, b.filmGrainIntensity, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideFXAA) {
        out.fxaaEnabled = lerpU(a.fxaaEnabled, b.fxaaEnabled, t);
        out.fxaaSpanMax = lerpF(a.fxaaSpanMax, b.fxaaSpanMax, t);
        out.fxaaReduceMin = lerpF(a.fxaaReduceMin, b.fxaaReduceMin, t);
        out.fxaaReduceMul = lerpF(a.fxaaReduceMul, b.fxaaReduceMul, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideDither) {
        out.ditherEnabled = lerpU(a.ditherEnabled, b.ditherEnabled, t);
        out.ditherPattern = lerpU(a.ditherPattern, b.ditherPattern, t);
        out.ditherStrength = lerpF(a.ditherStrength, b.ditherStrength, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideColorQuant) {
        out.colorQuantEnabled = lerpU(a.colorQuantEnabled, b.colorQuantEnabled, t);
        out.colorBitDepth = lerpU(a.colorBitDepth, b.colorBitDepth, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideResDownscale) {
        out.resDownscaleEnabled = lerpU(a.resDownscaleEnabled, b.resDownscaleEnabled, t);
        out.internalWidth = lerpU(a.internalWidth, b.internalWidth, t);
        out.internalHeight = lerpU(a.internalHeight, b.internalHeight, t);
        out.usePointFiltering = lerpU(a.usePointFiltering, b.usePointFiltering, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideCRT) {
        out.crtEnabled = lerpU(a.crtEnabled, b.crtEnabled, t);
        out.scanlineIntensity = lerpF(a.scanlineIntensity, b.scanlineIntensity, t);
        out.scanlineWidth = lerpF(a.scanlineWidth, b.scanlineWidth, t);
        out.crtCurvature = lerpF(a.crtCurvature, b.crtCurvature, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideLUT) {
        out.lutEnabled = lerpU(a.lutEnabled, b.lutEnabled, t);
        out.lutStrength = lerpF(a.lutStrength, b.lutStrength, t);
        out.lutSize = lerpU(a.lutSize, b.lutSize, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideCRTPhosphor) {
        out.crtPhosphorEnabled = lerpU(a.crtPhosphorEnabled, b.crtPhosphorEnabled, t);
        out.crtMaskType = lerpU(a.crtMaskType, b.crtMaskType, t);
        out.crtMaskPitch = lerpF(a.crtMaskPitch, b.crtMaskPitch, t);
        out.crtBloomRadius = lerpF(a.crtBloomRadius, b.crtBloomRadius, t);
        out.crtBloomStrength = lerpF(a.crtBloomStrength, b.crtBloomStrength, t);
        out.crtBloomSigma = lerpF(a.crtBloomSigma, b.crtBloomSigma, t);
        out.crtModelPreset = lerpF(a.crtModelPreset, b.crtModelPreset, t);
        out.crtTVL = lerpF(a.crtTVL, b.crtTVL, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideVHS) {
        out.vhsEnabled = lerpU(a.vhsEnabled, b.vhsEnabled, t);
        out.vhsTrackingIntensity = lerpF(a.vhsTrackingIntensity, b.vhsTrackingIntensity, t);
        out.vhsTrackingSpeed = lerpF(a.vhsTrackingSpeed, b.vhsTrackingSpeed, t);
        out.vhsWobbleIntensity = lerpF(a.vhsWobbleIntensity, b.vhsWobbleIntensity, t);
        out.vhsWobbleSpeed = lerpF(a.vhsWobbleSpeed, b.vhsWobbleSpeed, t);
        out.vhsColorBleed = lerpF(a.vhsColorBleed, b.vhsColorBleed, t);
        out.vhsNoiseIntensity = lerpF(a.vhsNoiseIntensity, b.vhsNoiseIntensity, t);
        out.vhsBlueShift = lerpF(a.vhsBlueShift, b.vhsBlueShift, t);
        out.vhsScreenTear = lerpU(a.vhsScreenTear, b.vhsScreenTear, t);
        out.vhsInterlacing = lerpU(a.vhsInterlacing, b.vhsInterlacing, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverridePalette) {
        out.paletteEnabled = lerpU(a.paletteEnabled, b.paletteEnabled, t);
        out.paletteColors = lerpU(a.paletteColors, b.paletteColors, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideDoF) {
        out.dofEnabled = lerpU(a.dofEnabled, b.dofEnabled, t);
        out.dofFocalDistance = lerpF(a.dofFocalDistance, b.dofFocalDistance, t);
        out.dofFocalRange = lerpF(a.dofFocalRange, b.dofFocalRange, t);
        out.dofNearBlurStrength = lerpF(a.dofNearBlurStrength, b.dofNearBlurStrength, t);
        out.dofFarBlurStrength = lerpF(a.dofFarBlurStrength, b.dofFarBlurStrength, t);
        out.dofBokehSize = lerpF(a.dofBokehSize, b.dofBokehSize, t);
        out.dofApertureShape = lerpU(a.dofApertureShape, b.dofApertureShape, t);
        out.dofDebugCoC = lerpU(a.dofDebugCoC, b.dofDebugCoC, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideTiltShift) {
        out.tiltShiftEnabled = lerpU(a.tiltShiftEnabled, b.tiltShiftEnabled, t);
        out.tiltShiftFocusY = lerpF(a.tiltShiftFocusY, b.tiltShiftFocusY, t);
        out.tiltShiftBandWidth = lerpF(a.tiltShiftBandWidth, b.tiltShiftBandWidth, t);
        out.tiltShiftBlurAmount = lerpF(a.tiltShiftBlurAmount, b.tiltShiftBlurAmount, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideCelOutline) {
        out.celOutlineEnabled = lerpU(a.celOutlineEnabled, b.celOutlineEnabled, t);
        out.celOutlineThickness = lerpF(a.celOutlineThickness, b.celOutlineThickness, t);
        out.celOutlineThreshold = lerpF(a.celOutlineThreshold, b.celOutlineThreshold, t);
        out.celOutlineColor = lerpV(a.celOutlineColor, b.celOutlineColor, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideStipple) {
        out.stippleEnabled = lerpU(a.stippleEnabled, b.stippleEnabled, t);
        out.stipplePatternMask = lerpU(a.stipplePatternMask, b.stipplePatternMask, t);
        out.stippleColorMode = lerpU(a.stippleColorMode, b.stippleColorMode, t);
        out.stippleScale = lerpF(a.stippleScale, b.stippleScale, t);
        out.stippleDensity = lerpF(a.stippleDensity, b.stippleDensity, t);
        out.stippleStrength = lerpF(a.stippleStrength, b.stippleStrength, t);
        out.stippleFgColor = lerpV(a.stippleFgColor, b.stippleFgColor, t);
        out.stippleBgColor = lerpV(a.stippleBgColor, b.stippleBgColor, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideGodRays) {
        out.godRaysEnabled = lerpU(a.godRaysEnabled, b.godRaysEnabled, t);
        out.godRaysIntensity = lerpF(a.godRaysIntensity, b.godRaysIntensity, t);
        out.godRaysDecay = lerpF(a.godRaysDecay, b.godRaysDecay, t);
        out.godRaysDensity = lerpF(a.godRaysDensity, b.godRaysDensity, t);
        out.godRaysSamples = lerpU(a.godRaysSamples, b.godRaysSamples, t);
        out.godRaysWeight = lerpF(a.godRaysWeight, b.godRaysWeight, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideSSAO) {
        out.ssaoEnabled = lerpU(a.ssaoEnabled, b.ssaoEnabled, t);
        out.ssaoRadius = lerpF(a.ssaoRadius, b.ssaoRadius, t);
        out.ssaoIntensity = lerpF(a.ssaoIntensity, b.ssaoIntensity, t);
        out.ssaoBias = lerpF(a.ssaoBias, b.ssaoBias, t);
        out.ssaoSamples = lerpU(a.ssaoSamples, b.ssaoSamples, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideContactShadows) {
        out.contactShadowsEnabled = lerpU(a.contactShadowsEnabled, b.contactShadowsEnabled, t);
        out.contactShadowsLength = lerpF(a.contactShadowsLength, b.contactShadowsLength, t);
        out.contactShadowsSteps = lerpU(a.contactShadowsSteps, b.contactShadowsSteps, t);
        out.contactShadowsIntensity = lerpF(a.contactShadowsIntensity, b.contactShadowsIntensity, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideCaustics) {
        out.causticsEnabled = lerpU(a.causticsEnabled, b.causticsEnabled, t);
        out.causticsIntensity = lerpF(a.causticsIntensity, b.causticsIntensity, t);
        out.causticsScale = lerpF(a.causticsScale, b.causticsScale, t);
        out.causticsSpeed = lerpF(a.causticsSpeed, b.causticsSpeed, t);
        out.causticsWaterY = lerpF(a.causticsWaterY, b.causticsWaterY, t);
    }
    if (overrideMask & PostProcessVolumeComponent::OverrideFogShafts) {
        out.fogShaftsEnabled = lerpU(a.fogShaftsEnabled, b.fogShaftsEnabled, t);
        out.fogShaftsIntensity = lerpF(a.fogShaftsIntensity, b.fogShaftsIntensity, t);
        out.fogShaftsDensity = lerpF(a.fogShaftsDensity, b.fogShaftsDensity, t);
        out.fogShaftsDecay = lerpF(a.fogShaftsDecay, b.fogShaftsDecay, t);
        out.fogShaftsSamples = lerpU(a.fogShaftsSamples, b.fogShaftsSamples, t);
        out.fogShaftsMaxDistance = lerpF(a.fogShaftsMaxDistance, b.fogShaftsMaxDistance, t);
    }
}

} // namespace ECS
} // namespace Enjin
