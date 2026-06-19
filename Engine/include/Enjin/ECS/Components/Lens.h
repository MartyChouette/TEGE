#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"  // f32, u8

namespace Enjin {
namespace ECS {

// Lens type presets. A preset is just a starting set of parameters; every value
// stays tweakable afterward (that is the "custom lens"). Selecting a type calls
// ApplyPreset to seed the parameters.
enum class LensType : u8 {
    Standard = 0,   // neutral, no distortion
    WideAngle,      // mild barrel + light vignette
    Fisheye,        // strong barrel bend
    Telephoto,      // mild pincushion + tighter vignette
    Anamorphic,     // horizontal stretch + blue-ish dispersion
    Vintage,        // soft barrel + heavy vignette + dispersion
    Security,       // CCTV: strong barrel + heavy vignette
    Dreamy,         // soft, gentle dispersion + vignette
    Custom,         // user-tweaked, ApplyPreset leaves values alone
    COUNT
};

// Per-camera lens. When present on the active camera, its parameters are pushed
// into the post-process pass each frame (distortion, dispersion, vignette,
// anamorphic squeeze). Cheap: all single-pass, UV-domain effects.
struct LensComponent {
    bool enabled = true;
    LensType type = LensType::Standard;

    // Bends: negative = barrel (edges bulge out), positive = pincushion
    // (edges pinch in). Roughly -0.6 .. 0.4 useful range.
    f32 distortion = 0.0f;

    // Anamorphic horizontal squeeze. 1.0 = none, <1 squeezes horizontally,
    // >1 stretches (the classic wide cinema look).
    f32 anamorphicSqueeze = 1.0f;

    // Chromatic aberration / dispersion — the color fringing real glass produces
    // toward the edges (thickness/index difference). 0 = off, ~0.002-0.01 typical.
    f32 chromaticAberration = 0.0f;

    // Vignette (corner darkening). intensity 0 = off.
    f32 vignetteIntensity = 0.0f;
    f32 vignetteSoftness = 0.5f;

    // Seed parameters from a lens-type preset. Leaves Custom untouched.
    void ApplyPreset(LensType t) {
        switch (t) {
            case LensType::Standard:
                distortion = 0.0f;  anamorphicSqueeze = 1.0f;  chromaticAberration = 0.0f;
                vignetteIntensity = 0.0f;  vignetteSoftness = 0.5f;  break;
            case LensType::WideAngle:
                distortion = -0.15f; anamorphicSqueeze = 1.0f;  chromaticAberration = 0.002f;
                vignetteIntensity = 0.15f; vignetteSoftness = 0.5f;  break;
            case LensType::Fisheye:
                distortion = -0.50f; anamorphicSqueeze = 1.0f;  chromaticAberration = 0.003f;
                vignetteIntensity = 0.20f; vignetteSoftness = 0.45f; break;
            case LensType::Telephoto:
                distortion = 0.08f;  anamorphicSqueeze = 1.0f;  chromaticAberration = 0.0f;
                vignetteIntensity = 0.35f; vignetteSoftness = 0.6f;  break;
            case LensType::Anamorphic:
                distortion = -0.05f; anamorphicSqueeze = 1.33f; chromaticAberration = 0.004f;
                vignetteIntensity = 0.20f; vignetteSoftness = 0.5f;  break;
            case LensType::Vintage:
                distortion = -0.10f; anamorphicSqueeze = 1.0f;  chromaticAberration = 0.006f;
                vignetteIntensity = 0.45f; vignetteSoftness = 0.55f; break;
            case LensType::Security:
                distortion = -0.35f; anamorphicSqueeze = 1.0f;  chromaticAberration = 0.003f;
                vignetteIntensity = 0.50f; vignetteSoftness = 0.4f;  break;
            case LensType::Dreamy:
                distortion = 0.0f;   anamorphicSqueeze = 1.0f;  chromaticAberration = 0.002f;
                vignetteIntensity = 0.25f; vignetteSoftness = 0.7f;  break;
            case LensType::Custom:
            default:
                break;  // leave user values
        }
        type = t;
    }
};

} // namespace ECS
} // namespace Enjin
