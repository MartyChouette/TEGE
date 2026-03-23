#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace ECS {

// ============================================================================
// Art Style Component
// ============================================================================
// Per-entity or per-scene art style override. When attached to an entity, its
// rendering style overrides the scene-level art style preset for that entity
// only. When no ArtStyleComponent is present, the entity uses the scene default.
//
// The 8 supported visual styles map to both forward-pass shader modifications
// (via push constants / material SSBO flags) and post-process adjustments.
// Some styles are purely forward-pass (Pre-PBR, Hand-Painted), some are purely
// post-process (Analog), and some are hybrid (Cel/Toon, Retro, Pixel Art).

// Visual art style enum — the 8 canonical styles plus "Inherit" (use scene default)
enum class ArtStyleType : u8 {
    Inherit = 0,        // Use the scene-level art style preset (no per-entity override)
    PrePBR,             // Style 1: Flat/Gouraud shading, no PBR, specular maps
    HandPainted,        // Style 2: Soft diffuse, painterly look, warm shadow ramp
    CelToon,            // Style 3: Hard edges, color banding, outlines
    NPR,                // Style 4: Non-photorealistic, sketch/watercolor/hatching
    Retro,              // Style 5: PS1/N64 vertex jitter, low-res textures, affine
    PixelArt,           // Style 6: Nearest-neighbor sampling, limited palette
    MaterialExpression, // Style 7: Custom shader effects, matcap, surface noise
    Analog,             // Style 8: Film grain, chromatic aberration, VHS, CRT
    Count
};

// Returns human-readable name for an art style type
inline const char* ArtStyleTypeName(ArtStyleType type) {
    switch (type) {
        case ArtStyleType::Inherit:            return "Inherit (Scene Default)";
        case ArtStyleType::PrePBR:             return "Pre-PBR";
        case ArtStyleType::HandPainted:        return "Hand-Painted";
        case ArtStyleType::CelToon:            return "Cel / Toon";
        case ArtStyleType::NPR:                return "NPR Illustrative";
        case ArtStyleType::Retro:              return "Retro 3D";
        case ArtStyleType::PixelArt:           return "Pixel Art";
        case ArtStyleType::MaterialExpression: return "Material Expression";
        case ArtStyleType::Analog:             return "Analog / Degraded";
        default:                               return "Unknown";
    }
}

// Per-entity art style component with parameters for each style.
// Only the parameters relevant to the active style are used at runtime.
// The component is intentionally flat (no unions) for serialization simplicity.
struct ArtStyleComponent {
    // Which style to apply (Inherit = use scene default)
    ArtStyleType style = ArtStyleType::Inherit;

    // Whether this entity's style should propagate to child entities in the hierarchy
    bool propagateToChildren = false;

    // === Pre-PBR Parameters (Style 1) ===
    // Classic fixed-function lighting without PBR energy conservation
    bool prePBR_halfLambert = false;        // Half-Lambert wrap lighting (NdotL * 0.5 + 0.5)
    bool prePBR_flatShading = false;        // Flat shading (face normals, no interpolation)
    bool prePBR_gouraudOnly = false;        // Gouraud: per-vertex lighting, no per-pixel
    f32  prePBR_specularStrength = 0.5f;    // Scene-level specular intensity multiplier (0-2)

    // === Hand-Painted Parameters (Style 2) ===
    // Painterly look with soft light wrapping and artist-authored light ramps
    f32  handPainted_lightWrapAmount = 0.5f; // Light wrapping (0-1, higher = softer falloff)
    u8   handPainted_lightRampMode = 2;      // 0=off, 1=smooth, 2=warm, 3=cool, 4=anime
    f32  handPainted_saturationBoost = 0.0f; // Extra saturation on diffuse (-0.5 to 0.5)

    // === Cel/Toon Parameters (Style 3) ===
    // Hard-edged shading with color banding and outlines
    f32  cel_diffuseBands = 4.0f;           // Number of shading bands (2-8)
    f32  cel_specularCutoff = 0.5f;         // Hard specular threshold (0-1)
    f32  cel_outlineWidth = 0.02f;          // Geometry outline width in world units
    Math::Vector3 cel_outlineColor = Math::Vector3(0.0f, 0.0f, 0.0f); // Outline color
    f32  cel_rimStrength = 0.0f;            // Rim/edge glow intensity (0-3)
    u8   cel_shadowMode = 1;                // 0=dark, 1=purple, 2=blue, 3=warm, 4=neutral cool
    u8   cel_lightRampMode = 4;             // Light ramp override (0=off, 1-4 = smooth/warm/cool/anime)

    // === NPR Illustrative Parameters (Style 4) ===
    // Sketch, watercolor, pen-and-ink, hatching effects
    bool npr_celOutline = true;             // Enable Sobel edge detection outlines
    f32  npr_outlineThickness = 2.0f;       // Sobel outline thickness
    f32  npr_curvatureWeight = 0.5f;        // Curvature-driven outline variation (0-2)
    u32  npr_stipplePatternMask = 1;        // Bitmask for stipple patterns (bit0=Bayer4x4, etc.)
    f32  npr_stippleDensity = 0.5f;         // Stipple density threshold (0-1)
    f32  npr_stippleStrength = 0.8f;        // Blend strength with original (0-1)
    u8   npr_diffuseBands = 2;              // Cel shading bands for NPR (2-4)

    // === Retro 3D Parameters (Style 5) ===
    // PS1/N64 era vertex jitter, affine textures, limited precision
    bool retro_vertexSnapping = true;       // Snap vertices to grid (PS1 look)
    u8   retro_snapResolution = 160;        // Grid resolution (80-320, lower = more jitter)
    bool retro_affineTexturing = true;      // Affine (non-perspective-correct) texture mapping
    bool retro_uvQuantize = true;           // UV coordinate quantization
    bool retro_flatShading = true;          // Force flat shading
    f32  retro_texturePageSize = 0.0f;      // PS1 VRAM page warping (0=off, 64/128 typical)
    f32  retro_posterizeLevels = 16.0f;     // Color posterization levels (4-256)

    // === Pixel Art Parameters (Style 6) ===
    // Nearest-neighbor upscaling, limited palettes, crispy pixels
    u32  pixel_paletteColors = 16;          // Number of palette colors (2-256)
    u32  pixel_paletteMode = 0;             // 0=per-channel, 1=PICO-8, 2=GameBoy, 3=NES, 4=CGA, 5=C64
    bool pixel_pointFiltering = true;       // Nearest-neighbor texture sampling
    u32  pixel_normalQuantizeSteps = 0;     // Normal quantization (0=off, 4-16)

    // === Material Expression Parameters (Style 7) ===
    // Matcap textures, procedural surface noise, SSS
    f32  matExpr_surfaceNoiseScale = 5.0f;     // Noise frequency in world units
    f32  matExpr_surfaceNoiseStrength = 0.15f;  // Noise influence on diffuse (0-1)
    f32  matExpr_sssIntensity = 0.0f;           // Subsurface scattering intensity
    f32  matExpr_sssRadius = 1.0f;              // Scatter radius in world units
    Math::Vector3 matExpr_sssColor = Math::Vector3(1.0f, 0.2f, 0.1f); // Scatter tint

    // === Analog / Degraded Parameters (Style 8) ===
    // Film grain, chromatic aberration, VHS, CRT effects
    bool analog_filmGrain = true;           // Film grain noise
    f32  analog_filmGrainIntensity = 0.08f; // Grain strength
    bool analog_chromaticAberration = true; // Chromatic aberration
    f32  analog_chromaticIntensity = 0.005f;// Aberration strength
    bool analog_vhsEnabled = false;         // VHS filter (tracking, wobble, color bleed)
    f32  analog_vhsTrackingIntensity = 0.3f;
    bool analog_crtEnabled = false;         // CRT scanlines
    f32  analog_scanlineIntensity = 0.3f;
    bool analog_filmGateWeave = false;      // Film gate jitter
    f32  analog_gateWeaveIntensity = 0.002f;
    bool analog_lightLeaks = false;         // Film light leak overlay
    f32  analog_lightLeakIntensity = 0.15f;
};

} // namespace ECS
} // namespace Enjin
