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
    f32 texturePageSize = 0.0f; // PS1 VRAM page size in texels (0=off, 64/128 typical)
};

// Vertex jitter/wobble (PS1 lack of sub-pixel precision)
struct VertexJitterSettings {
    bool enabled = false;
    f32 jitterAmount = 0.5f;    // Pixels of jitter
    bool snapToGrid = false;    // Snap to pixel grid
    u32 gridResolution = 160;   // Virtual resolution for snapping
    f32 depthSortJitter = 0.0f; // PS1 ordering table jitter (0=off, 0.001-0.01 typical)
};

// CRT hardware model presets
enum class CRTModel : u8 {
    Custom = 0,
    SonyTrinitronKV27V42,   // Consumer TV: aperture grille, warm bloom
    SonyPVM20M4U,           // Pro monitor: tight phosphors, sharp scanlines
    JVCTMH150CG,            // Broadcast monitor: excellent geometry
    Toshiba14AF46,          // Consumer flat CRT: shadow mask
    SonyGDMFW900,           // High-end PC CRT: extremely fine pitch
    ViewSonicG810,          // PC monitor: good all-rounder
    NECMultiSyncFE2111SB,   // PC monitor: flat screen, fine pitch
    Generic15kHzArcade,     // Arcade: coarse phosphors, heavy bloom
    WellsGardnerK7000,      // Arcade standard: slot mask
    Commodore1084S,         // Home computer: warm, forgiving
    Count
};

// Hardware-accurate CRT model specification
struct CRTModelSpec {
    u32 maskType;           // 0=aperture grille, 1=shadow mask, 2=slot mask
    f32 maskPitch;
    f32 bloomRadius;
    f32 bloomStrength;
    f32 bloomSigma;
    f32 scanlineIntensity;
    f32 scanlineWidth;
    f32 curvature;
    f32 tvl;                // TV lines (horizontal resolution measure)
};

// Static lookup table of real CRT hardware specs
static constexpr CRTModelSpec CRT_MODEL_SPECS[] = {
    // Custom (defaults)
    { 0, 1.0f, 1.5f, 0.3f, 0.8f, 0.3f, 1.0f, 0.0f, 400.0f },
    // Sony Trinitron KV-27V42
    { 0, 0.65f, 2.0f, 0.35f, 0.9f, 0.35f, 1.0f, 0.03f, 500.0f },
    // Sony PVM-20M4U
    { 0, 0.31f, 1.5f, 0.2f, 0.6f, 0.4f, 0.8f, 0.0f, 800.0f },
    // JVC TM-H150CG
    { 0, 0.28f, 1.5f, 0.22f, 0.65f, 0.38f, 0.85f, 0.0f, 750.0f },
    // Toshiba 14AF46
    { 1, 0.58f, 2.5f, 0.4f, 1.1f, 0.3f, 1.2f, 0.02f, 400.0f },
    // Sony GDM-FW900
    { 0, 0.23f, 1.2f, 0.15f, 0.5f, 0.2f, 0.6f, 0.0f, 1100.0f },
    // ViewSonic G810
    { 1, 0.25f, 1.3f, 0.18f, 0.55f, 0.25f, 0.7f, 0.01f, 900.0f },
    // NEC MultiSync FE2111SB
    { 1, 0.24f, 1.2f, 0.16f, 0.55f, 0.22f, 0.65f, 0.0f, 950.0f },
    // Generic 15kHz Arcade
    { 2, 0.83f, 3.0f, 0.45f, 1.0f, 0.45f, 1.5f, 0.04f, 300.0f },
    // Wells Gardner K7000
    { 2, 0.75f, 2.8f, 0.42f, 0.95f, 0.42f, 1.4f, 0.035f, 330.0f },
    // Commodore 1084S
    { 2, 0.42f, 2.5f, 0.38f, 1.0f, 0.35f, 1.1f, 0.025f, 450.0f },
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
    f32 bloomSigma = 0.8f;   // Gaussian spread control
    f32 tvl = 400.0f;        // TV lines resolution
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
    f32 tapeDropout = 0.0f;  // Signal loss band intensity (0=off, 0.1-0.5 typical)
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

    // Dreamcast-style effects
    bool GetSphereEnvMap() const { return m_SphereEnvMap; }
    void SetSphereEnvMap(bool enabled) { m_SphereEnvMap = enabled; }
    f32 GetSphereEnvStrength() const { return m_SphereEnvStrength; }
    void SetSphereEnvStrength(f32 v) { m_SphereEnvStrength = v; }
    f32 GetPosterizeLevels() const { return m_PosterizeLevels; }
    void SetPosterizeLevels(f32 v) { m_PosterizeLevels = v; }

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
    void ApplyCRTModelPreset(CRTModel model);
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
    bool m_SphereEnvMap = false;
    f32 m_SphereEnvStrength = 0.5f;
    f32 m_PosterizeLevels = 0.0f;

    TransitionSettings m_Transition;
    f32 m_TransitionProgress = 1.0f;
};

} // namespace Effects
} // namespace Enjin
