#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace Effects {

// Retro rendering techniques from 5th/6th gen consoles
// These can be enabled/disabled per-project for that nostalgic look

// Dithering patterns (PS1 style)
enum class DitherPattern : u8 {
    None,
    Bayer2x2,       // Simple 2x2 pattern
    Bayer4x4,       // Classic PS1 dither
    Bayer8x8,       // Finer dither
    BlueNoise,      // More organic look
    Ordered         // Horizontal lines
};

// Color depth reduction
enum class ColorMode : u8 {
    TrueColor,      // 24-bit (modern)
    HighColor,      // 16-bit (PS1/Saturn)
    Palette256,     // 8-bit indexed
    Palette16,      // 4-bit (SNES)
    Monochrome      // 1-bit (Game Boy)
};

// Screen resolution modes
struct ResolutionSettings {
    u32 renderWidth = 320;      // Internal render resolution
    u32 renderHeight = 240;
    bool pointFiltering = true; // Nearest neighbor (crispy pixels)
    bool integerScaling = true; // Only scale by whole numbers
    f32 aspectRatio = 4.0f / 3.0f;  // Classic 4:3
};

// PS1-style affine texture warping
struct AffineSettings {
    bool enabled = false;
    f32 warpStrength = 1.0f;    // How much texture warps
    bool vertexSnapping = false; // Snap vertices to grid
    f32 snapGridSize = 1.0f;    // Grid size for vertex snap
};

// Vertex jitter/wobble (PS1 lack of sub-pixel precision)
struct VertexJitterSettings {
    bool enabled = false;
    f32 jitterAmount = 0.5f;    // Pixels of jitter
    bool snapToGrid = false;    // Snap to pixel grid
    u32 gridResolution = 160;   // Virtual resolution for snapping
};

// CRT/Scanline filter
struct CRTSettings {
    bool enabled = false;
    f32 scanlineIntensity = 0.3f;
    f32 scanlineWidth = 1.0f;
    bool curvedScreen = false;
    f32 curvature = 0.1f;
    f32 vignette = 0.3f;
    bool phosphorGlow = false;
    f32 glowStrength = 0.2f;
    Math::Vector3 phosphorMask = Math::Vector3(1.0f, 0.8f, 1.0f);  // RGB mask

    // Phosphor subpixel blending
    u32 maskType = 0;        // 0=aperture grille, 1=shadow mask, 2=slot mask
    f32 maskPitch = 1.0f;    // Subpixel spacing (pixels between RGB triplets)
    f32 bloomRadius = 1.5f;  // Phosphor bloom spread radius
    f32 bloomStrength = 0.3f; // How much phosphors bleed into neighbors
};

// VHS filter settings
struct VHSSettings {
    bool enabled = false;
    f32 trackingIntensity = 0.3f;
    f32 trackingSpeed = 1.0f;
    f32 wobbleIntensity = 0.002f;
    f32 wobbleSpeed = 2.0f;
    f32 colorBleedAmount = 0.003f;
    f32 noiseIntensity = 0.05f;
    f32 blueShift = 0.05f;
    bool screenTear = false;
    f32 tearOffset = 0.0f;
    bool interlacing = false;
};

// Color grading presets
enum class ColorPreset : u8 {
    None,
    PS1,            // Slightly washed out, blue tint
    N64,            // Warm, slightly blurry
    Saturn,         // High contrast, saturated
    Dreamcast,      // Clean, slightly cool
    PS2,            // Natural, good contrast
    GameCube,       // Vibrant, clean
    GameBoy,        // 4 shades of green
    SNES,           // Rich, saturated
    Genesis         // High contrast, limited palette
};

// Screen transitions (very retro!)
enum class TransitionType : u8 {
    None,
    Fade,           // Simple fade to black/white
    Wipe,           // Horizontal/vertical wipe
    Iris,           // Circle close/open (Mario 64)
    Pixelate,       // Pixelate in/out
    Dissolve,       // Random pixel dissolve
    Slide,          // Slide in from edge
    Mosaic          // Growing/shrinking tiles
};

struct TransitionSettings {
    TransitionType type = TransitionType::Fade;
    f32 duration = 0.5f;
    Math::Vector3 color = Math::Vector3(0, 0, 0);  // Fade color
    bool reversed = false;  // In vs out
};

// Fog settings (very important for PS1/N64)
struct RetroFogSettings {
    bool enabled = true;
    Math::Vector3 color = Math::Vector3(0.5f, 0.5f, 0.6f);

    // Linear fog (PS1/N64 style - simple and fast)
    f32 start = 10.0f;
    f32 end = 50.0f;

    // For that classic "draw distance" look
    bool hardCutoff = false;    // Objects just pop in
    f32 cutoffDistance = 60.0f;
};

// Sprite billboard settings
struct BillboardSettings {
    bool faceCamera = true;
    bool lockYAxis = true;      // Trees, etc.
    bool useCardboard = false;  // 2 crossed quads (Doom style)
    u32 animationFrames = 1;
    f32 animationSpeed = 10.0f;
};

// Main retro effects controller
class ENJIN_API RetroEffects {
public:
    RetroEffects() = default;

    // Enable/disable the whole system
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // Resolution
    void SetResolution(const ResolutionSettings& settings) { m_Resolution = settings; }
    ResolutionSettings& GetResolution() { return m_Resolution; }

    // Dithering
    void SetDitherPattern(DitherPattern pattern) { m_DitherPattern = pattern; }
    DitherPattern GetDitherPattern() const { return m_DitherPattern; }

    // Color
    void SetColorMode(ColorMode mode) { m_ColorMode = mode; }
    ColorMode GetColorMode() const { return m_ColorMode; }
    void SetColorPreset(ColorPreset preset);
    ColorPreset GetColorPreset() const { return m_ColorPreset; }

    // PS1 effects
    void SetAffineSettings(const AffineSettings& settings) { m_Affine = settings; }
    AffineSettings& GetAffineSettings() { return m_Affine; }

    void SetVertexJitter(const VertexJitterSettings& settings) { m_VertexJitter = settings; }
    VertexJitterSettings& GetVertexJitter() { return m_VertexJitter; }

    // CRT
    void SetCRTSettings(const CRTSettings& settings) { m_CRT = settings; }
    CRTSettings& GetCRTSettings() { return m_CRT; }

    // VHS
    void SetVHSSettings(const VHSSettings& settings) { m_VHS = settings; }
    VHSSettings& GetVHSSettings() { return m_VHS; }

    // Global retro flags
    bool GetGouraudOnly() const { return m_GouraudOnly; }
    void SetGouraudOnly(bool enabled) { m_GouraudOnly = enabled; }

    // Fog
    void SetFogSettings(const RetroFogSettings& settings) { m_Fog = settings; }
    RetroFogSettings& GetFogSettings() { return m_Fog; }

    // Transitions
    void StartTransition(const TransitionSettings& settings);
    void UpdateTransition(f32 deltaTime);
    bool IsTransitioning() const { return m_TransitionProgress < 1.0f; }
    f32 GetTransitionProgress() const { return m_TransitionProgress; }
    const TransitionSettings& GetTransitionSettings() const { return m_Transition; }

    // Quick presets
    void ApplyPS1Preset();
    void ApplyN64Preset();
    void ApplyPS2Preset();
    void ApplyGameCubePreset();
    void ApplySNESPreset();
    void ApplyDreamcastPreset();
    void ApplyNESPreset();
    void ApplyGameBoyPreset();
    void ApplyGBAPreset();
    void ApplyGenesisPreset();
    void ApplySaturnPreset();
    void ApplyMasterSystemPreset();
    void ApplyPSPPreset();
    void ApplyDOSVGAPreset();
    void ApplyVirtualBoyPreset();
    void ApplyNeoGeoPreset();
    void Apply3DOPreset();
    void ApplyXboxPreset();
    void ApplyAtari2600Preset();
    void ApplyPCEnginePreset();
    void ClearAllEffects();

private:
    bool m_Enabled = false;

    ResolutionSettings m_Resolution;
    DitherPattern m_DitherPattern = DitherPattern::None;
    ColorMode m_ColorMode = ColorMode::TrueColor;
    ColorPreset m_ColorPreset = ColorPreset::None;
    AffineSettings m_Affine;
    VertexJitterSettings m_VertexJitter;
    CRTSettings m_CRT;
    VHSSettings m_VHS;
    RetroFogSettings m_Fog;
    bool m_GouraudOnly = false;

    TransitionSettings m_Transition;
    f32 m_TransitionProgress = 1.0f;
};

} // namespace Effects
} // namespace Enjin
