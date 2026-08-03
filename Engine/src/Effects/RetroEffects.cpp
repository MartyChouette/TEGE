#include "Enjin/Effects/RetroEffects.h"
#include "Enjin/Math/Math.h"

namespace Enjin {
namespace Effects {

void RetroEffects::SetColorPreset(ColorPreset preset) {
    m_ColorPreset = preset;
    // Color presets affect fog color, saturation, etc.
    // Actual color grading happens in shader
}

void RetroEffects::StartTransition(const TransitionSettings& settings) {
    m_Transition = settings;
    m_TransitionProgress = 0.0f;
}

void RetroEffects::UpdateTransition(f32 deltaTime) {
    if (m_TransitionProgress >= 1.0f) return;

    m_TransitionProgress += deltaTime / m_Transition.duration;
    if (m_TransitionProgress > 1.0f) {
        m_TransitionProgress = 1.0f;
    }
}

void RetroEffects::ApplyPS1Preset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;

    // Low resolution
    m_Resolution.renderWidth = 320;
    m_Resolution.renderHeight = 240;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;

    // PS1 dithering
    m_DitherPattern = DitherPattern::Bayer4x4;

    // 16-bit color
    m_ColorMode = ColorMode::HighColor;
    m_ColorPreset = ColorPreset::PS1;

    // Affine texture warping
    m_Affine.enabled = true;
    m_Affine.warpStrength = 1.0f;
    m_Affine.vertexSnapping = true;
    m_Affine.snapGridSize = 1.0f;
    m_Affine.texturePageSize = 64.0f; // PS1 VRAM pages were 64x64

    // Vertex jitter
    m_VertexJitter.enabled = true;
    m_VertexJitter.jitterAmount = 1.0f;
    m_VertexJitter.snapToGrid = true;
    m_VertexJitter.gridResolution = 160;
    m_VertexJitter.depthSortJitter = 0.003f; // PS1 ordering table imprecision

    // PS1 used Gouraud shading
    m_GouraudOnly = true;

    // No CRT by default (optional)
    m_CRT.enabled = false;

    // Classic fog
    m_Fog.enabled = true;
    m_Fog.color = Math::Vector3(0.4f, 0.4f, 0.5f);
    m_Fog.start = 5.0f;
    m_Fog.end = 30.0f;
    m_Fog.hardCutoff = false;
}

void RetroEffects::ApplyN64Preset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;

    // N64 resolution (slightly higher than PS1)
    m_Resolution.renderWidth = 320;
    m_Resolution.renderHeight = 240;
    m_Resolution.pointFiltering = false;  // N64 had bilinear
    m_Resolution.integerScaling = true;

    // Less aggressive dithering
    m_DitherPattern = DitherPattern::Bayer2x2;

    // 16-bit color
    m_ColorMode = ColorMode::HighColor;
    m_ColorPreset = ColorPreset::N64;

    // No affine warping (N64 had perspective correct textures)
    m_Affine.enabled = false;

    // Slight vertex jitter
    m_VertexJitter.enabled = true;
    m_VertexJitter.jitterAmount = 0.3f;
    m_VertexJitter.snapToGrid = false;

    // N64 used Gouraud shading
    m_GouraudOnly = true;

    // Heavy fog (N64 loved fog)
    m_Fog.enabled = true;
    m_Fog.color = Math::Vector3(0.6f, 0.6f, 0.7f);
    m_Fog.start = 10.0f;
    m_Fog.end = 40.0f;
}

void RetroEffects::ApplyPS2Preset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;

    // PS2 resolution (higher than PS1)
    m_Resolution.renderWidth = 512;
    m_Resolution.renderHeight = 448;
    m_Resolution.pointFiltering = false;
    m_Resolution.integerScaling = true;

    // Subtle dithering
    m_DitherPattern = DitherPattern::Bayer2x2;

    // Better color
    m_ColorMode = ColorMode::TrueColor;
    m_ColorPreset = ColorPreset::PS2;

    // No affine warping
    m_Affine.enabled = false;

    // No vertex jitter
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;

    // Light fog
    m_Fog.enabled = true;
    m_Fog.color = Math::Vector3(0.5f, 0.55f, 0.6f);
    m_Fog.start = 30.0f;
    m_Fog.end = 100.0f;
}

void RetroEffects::ApplyGameCubePreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;

    // GameCube had good resolution
    m_Resolution.renderWidth = 640;
    m_Resolution.renderHeight = 480;
    m_Resolution.pointFiltering = false;
    m_Resolution.integerScaling = true;

    // Clean image
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::TrueColor;
    m_ColorPreset = ColorPreset::GameCube;

    // No retro artifacts
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;

    // Optional fog
    m_Fog.enabled = true;
    m_Fog.color = Math::Vector3(0.7f, 0.75f, 0.8f);
    m_Fog.start = 50.0f;
    m_Fog.end = 150.0f;
}

void RetroEffects::ApplySNESPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;

    // SNES resolution
    m_Resolution.renderWidth = 256;
    m_Resolution.renderHeight = 224;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_Resolution.aspectRatio = 4.0f / 3.0f;

    // No dithering (SNES had good color)
    m_DitherPattern = DitherPattern::None;

    // 15-bit color (SNES had 32768 colors)
    m_ColorMode = ColorMode::HighColor;
    m_ColorPreset = ColorPreset::SNES;

    // No 3D effects
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;

    // No fog (2D focused)
    m_Fog.enabled = false;

    // Optional CRT for authenticity
    m_CRT.enabled = false;
}

void RetroEffects::ApplyDreamcastPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;

    // Dreamcast resolution
    m_Resolution.renderWidth = 640;
    m_Resolution.renderHeight = 480;
    m_Resolution.pointFiltering = false;
    m_Resolution.integerScaling = true;

    // Clean
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::TrueColor;
    m_ColorPreset = ColorPreset::Dreamcast;

    // No artifacts
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;

    // Dreamcast signature: spherical environment mapping
    m_SphereEnvMap = true;
    m_SphereEnvStrength = 0.4f;
    m_PosterizeLevels = 0.0f;  // Dreamcast had clean 24-bit color

    // Light fog
    m_Fog.enabled = true;
    m_Fog.color = Math::Vector3(0.6f, 0.65f, 0.75f);
    m_Fog.start = 40.0f;
    m_Fog.end = 120.0f;
}

void RetroEffects::ApplyNESPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 256;
    m_Resolution.renderHeight = 240;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::Palette16;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
}

void RetroEffects::ApplyGameBoyPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 160;
    m_Resolution.renderHeight = 144;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::Monochrome;
    m_ColorPreset = ColorPreset::GameBoy;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
}

void RetroEffects::ApplyGBAPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 240;
    m_Resolution.renderHeight = 160;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::HighColor;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
}

void RetroEffects::ApplyGenesisPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 320;
    m_Resolution.renderHeight = 224;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::Palette256;
    m_ColorPreset = ColorPreset::Genesis;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
}

void RetroEffects::ApplySaturnPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 352;
    m_Resolution.renderHeight = 240;
    m_Resolution.pointFiltering = false;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::Bayer4x4;
    m_ColorMode = ColorMode::HighColor;
    m_ColorPreset = ColorPreset::Saturn;
    m_Affine.enabled = true;
    m_Affine.warpStrength = 0.5f;
    m_Affine.vertexSnapping = true;
    m_Affine.snapGridSize = 1.0f;
    m_VertexJitter.enabled = true;
    m_VertexJitter.jitterAmount = 0.3f;
    m_VertexJitter.snapToGrid = true;
    m_VertexJitter.gridResolution = 200;
    m_GouraudOnly = true;
    m_CRT.enabled = false;
    m_Fog.enabled = true;
    m_Fog.color = Math::Vector3(0.5f, 0.5f, 0.55f);
    m_Fog.start = 20.0f;
    m_Fog.end = 60.0f;
}

void RetroEffects::ApplyMasterSystemPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 256;
    m_Resolution.renderHeight = 192;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::Palette16;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
}

void RetroEffects::ApplyPSPPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 480;
    m_Resolution.renderHeight = 272;
    m_Resolution.pointFiltering = false;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::Bayer2x2;
    m_ColorMode = ColorMode::TrueColor;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = true;
    m_Fog.color = Math::Vector3(0.55f, 0.55f, 0.6f);
    m_Fog.start = 25.0f;
    m_Fog.end = 80.0f;
}

void RetroEffects::ApplyDOSVGAPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 320;
    m_Resolution.renderHeight = 200;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::Ordered;
    m_ColorMode = ColorMode::Palette256;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
}

void RetroEffects::ApplyVirtualBoyPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 384;
    m_Resolution.renderHeight = 224;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::Monochrome;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
}

void RetroEffects::ApplyNeoGeoPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 320;
    m_Resolution.renderHeight = 224;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::TrueColor;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
}

void RetroEffects::Apply3DOPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 320;
    m_Resolution.renderHeight = 240;
    m_Resolution.pointFiltering = false;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::Bayer2x2;
    m_ColorMode = ColorMode::HighColor;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = true;
    m_Affine.warpStrength = 1.0f;
    m_Affine.vertexSnapping = true;
    m_Affine.snapGridSize = 1.0f;
    m_VertexJitter.enabled = true;
    m_VertexJitter.jitterAmount = 0.5f;
    m_VertexJitter.snapToGrid = true;
    m_VertexJitter.gridResolution = 160;
    m_GouraudOnly = true;
    m_CRT.enabled = false;
    m_Fog.enabled = true;
    m_Fog.color = Math::Vector3(0.5f, 0.5f, 0.55f);
    m_Fog.start = 15.0f;
    m_Fog.end = 50.0f;
}

void RetroEffects::ApplyXboxPreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 640;
    m_Resolution.renderHeight = 480;
    m_Resolution.pointFiltering = false;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::TrueColor;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = true;
    m_Fog.color = Math::Vector3(0.6f, 0.6f, 0.65f);
    m_Fog.start = 40.0f;
    m_Fog.end = 120.0f;
}

void RetroEffects::ApplyAtari2600Preset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 160;
    m_Resolution.renderHeight = 192;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::Palette16;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
}

void RetroEffects::ApplyPCEnginePreset() {
    ClearAllEffects(); // reset to neutral first so no field bleeds in from a prior preset
    m_Enabled = true;
    m_Resolution.renderWidth = 256;
    m_Resolution.renderHeight = 240;
    m_Resolution.pointFiltering = true;
    m_Resolution.integerScaling = true;
    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::Palette256;
    m_ColorPreset = ColorPreset::None;
    m_Affine.enabled = false;
    m_VertexJitter.enabled = false;
    m_GouraudOnly = false;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
}

void RetroEffects::ApplyCRTModelPreset(CRTModel model) {
    if (model == CRTModel::Custom || static_cast<u8>(model) >= static_cast<u8>(CRTModel::Count))
        return;

    const auto& spec = CRT_MODEL_SPECS[static_cast<u8>(model)];
    m_CRT.enabled = true;
    m_CRT.maskType = spec.maskType;
    m_CRT.maskPitch = spec.maskPitch;
    m_CRT.bloomRadius = spec.bloomRadius;
    m_CRT.bloomStrength = spec.bloomStrength;
    m_CRT.bloomSigma = spec.bloomSigma;
    m_CRT.scanlineIntensity = spec.scanlineIntensity;
    m_CRT.scanlineWidth = spec.scanlineWidth;
    m_CRT.curvature = spec.curvature;
    m_CRT.tvl = spec.tvl;
    m_CRT.curvedScreen = (spec.curvature > 0.0f);
    m_CRT.phosphorGlow = true;
}

void RetroEffects::ClearAllEffects() {
    m_Enabled = false;

    m_Resolution.renderWidth = 1920;
    m_Resolution.renderHeight = 1080;
    m_Resolution.pointFiltering = false;
    m_Resolution.integerScaling = false;

    m_DitherPattern = DitherPattern::None;
    m_ColorMode = ColorMode::TrueColor;
    m_ColorPreset = ColorPreset::None;

    m_Affine.enabled = false;
    m_Affine.warpStrength = 1.0f;
    m_Affine.vertexSnapping = false;
    m_Affine.snapGridSize = 1.0f;
    m_Affine.texturePageSize = 0.0f;
    m_VertexJitter.enabled = false;
    m_VertexJitter.jitterAmount = 0.5f;
    m_VertexJitter.snapToGrid = false;
    m_VertexJitter.gridResolution = 160;
    m_VertexJitter.depthSortJitter = 0.0f;
    m_Resolution.aspectRatio = 4.0f / 3.0f;
    m_GouraudOnly = false;
    m_SphereEnvMap = false;
    m_SphereEnvStrength = 0.5f;
    m_PosterizeLevels = 0.0f;
    m_CRT.enabled = false;
    m_Fog.enabled = false;
    m_Fog.hardCutoff = false;
    m_VHS.enabled = false;
}

} // namespace Effects
} // namespace Enjin
