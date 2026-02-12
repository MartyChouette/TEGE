#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <nlohmann/json_fwd.hpp>

namespace Enjin {

// Forward declarations
namespace ECS { class RenderSystem; }
namespace Renderer {
    struct PostProcessSettings;
}

namespace Renderer {

// Pure config struct that captures ALL scene-level rendering state.
// NOT GPU-aligned — this is for serialization / editor storage only.
// Conversion helpers translate bool <-> u32 when talking to the GPU struct.
struct SceneRenderSettings {
    // --- Override flag ---
    bool useProjectDefaults = true;

    // --- RenderSystem fields ---
    bool shadowsEnabled = true;
    u32 shadowResolution = 2048;    // 512/1024/2048/4096
    f32 shadowDistance = 100.0f;    // Max shadow distance
    f32 shadowStrength = 1.0f;     // 0..1
    f32 shadowSoftness = 0.0f;     // 0 = hard, 1-5 = soft (Poisson disk texel radius)
    bool backfaceCulling = false;
    bool wireframe = false;
    f32 ambientIntensity = 1.0f;
    Math::Vector3 ambientColor = Math::Vector3(0.1f, 0.1f, 0.15f);
    f32 fogDensity = 0.0f;
    f32 fogStart = 20.0f;
    f32 fogEnd = 100.0f;
    f32 fogHeightFalloff = 0.1f;
    Math::Vector3 fogColor = Math::Vector3(0.5f, 0.5f, 0.6f);
    f32 snowIntensity = 0.0f;
    f32 worldCurvature = 0.0f;
    bool rainActive = false;

    // --- PostProcessSettings fields ---
    // Tone mapping
    u32 toneMappingMode = 0;
    f32 exposure = 1.0f;
    f32 gamma = 1.0f;
    f32 whitePoint = 4.0f;

    // Bloom
    bool bloomEnabled = false;
    f32 bloomThreshold = 1.0f;
    f32 bloomIntensity = 0.5f;
    f32 bloomRadius = 0.005f;

    // Vignette
    bool vignetteEnabled = false;
    f32 vignetteIntensity = 0.3f;
    f32 vignetteSmoothness = 0.5f;

    // Chromatic aberration
    bool chromaticAberrationEnabled = false;
    f32 chromaticAberrationIntensity = 0.005f;

    // Color grading
    Math::Vector3 colorFilter = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 saturation = 1.0f;
    f32 contrast = 1.0f;
    f32 brightness = 0.0f;

    // Film grain
    bool filmGrainEnabled = false;
    f32 filmGrainIntensity = 0.05f;

    // FXAA
    bool fxaaEnabled = true;
    f32 fxaaSpanMax = 8.0f;
    f32 fxaaReduceMin = 1.0f / 128.0f;
    f32 fxaaReduceMul = 1.0f / 8.0f;

    // Retro: Dithering
    bool ditherEnabled = false;
    u32 ditherPattern = 0;
    f32 ditherStrength = 1.0f;

    // Retro: Color quantization
    bool colorQuantEnabled = false;
    u32 colorBitDepth = 8;

    // Retro: Resolution downscaling
    bool resDownscaleEnabled = false;
    u32 internalWidth = 320;
    u32 internalHeight = 240;
    bool usePointFiltering = true;

    // CRT scanlines
    bool crtEnabled = false;
    f32 scanlineIntensity = 0.3f;
    f32 scanlineWidth = 1.0f;
    f32 crtCurvature = 0.0f;

    // LUT color grading
    bool lutEnabled = false;
    f32 lutStrength = 1.0f;
    u32 lutSize = 32;

    // CRT Phosphor
    bool crtPhosphorEnabled = false;
    u32 crtMaskType = 0;
    f32 crtMaskPitch = 1.0f;
    f32 crtBloomRadius = 1.5f;
    f32 crtBloomStrength = 0.3f;

    // VHS filter
    bool vhsEnabled = false;
    f32 vhsTrackingIntensity = 0.3f;
    f32 vhsTrackingSpeed = 1.0f;
    f32 vhsWobbleIntensity = 0.002f;
    f32 vhsWobbleSpeed = 2.0f;
    f32 vhsColorBleed = 0.003f;
    f32 vhsNoiseIntensity = 0.05f;
    f32 vhsBlueShift = 0.05f;
    bool vhsScreenTear = false;
    f32 vhsTearOffset = 0.0f;
    bool vhsInterlacing = false;

    // Color palette lock
    bool paletteEnabled = false;
    u32 paletteColors = 16;

    // Global retro shader overrides (force per-object flags on all entities)
    bool globalFlatShading = false;
    bool globalAffineTexturing = false;
    bool globalVertexSnapping = false;
    bool globalStippleTransparency = false;
    bool globalUVQuantize = false;
    bool globalGouraudOnly = false;
    u32 globalVertexSnapResolution = 160;

    // Full-screen stipple / dither
    bool stippleEnabled = false;
    u32 stipplePattern = 0;       // 0=Bayer4x4..7=FloydSteinberg
    u32 stippleColorMode = 0;     // 0=Mono, 1=DuoTone, 2=FullColor
    f32 stippleScale = 1.0f;
    f32 stippleDensity = 0.5f;
    f32 stippleStrength = 1.0f;
    Math::Vector3 stippleFgColor = Math::Vector3(0.0f, 0.0f, 0.0f);
    Math::Vector3 stippleBgColor = Math::Vector3(1.0f, 1.0f, 1.0f);

    // --- Cel Shading ---
    bool celShadingEnabled = false;
    f32 celDiffuseBands = 3.0f;
    f32 celSpecularCutoff = 0.5f;
    bool celOutlineEnabled = false;
    f32 celOutlineThickness = 1.0f;
    f32 celOutlineThreshold = 0.1f;
    Math::Vector3 celOutlineColor = Math::Vector3(0.0f, 0.0f, 0.0f);

    // --- Ray Tracing ---
    bool rtEnabled = false;
    u32 rtMode = 0;                    // 0=Hybrid, 1=PathTrace

    // RT Shadows
    bool rtShadowsEnabled = true;
    f32 rtShadowMaxDistance = 100.0f;
    f32 rtShadowRadius = 0.01f;

    // RT Reflections
    bool rtReflectionsEnabled = true;
    f32 rtReflectionMaxDistance = 50.0f;
    f32 rtReflectionRoughnessThreshold = 0.5f;

    // RT Ambient Occlusion
    bool rtAOEnabled = true;
    f32 rtAORadius = 2.0f;
    f32 rtAOPower = 1.5f;

    // RT Global Illumination
    bool rtGIEnabled = false;
    f32 rtGIMaxDistance = 50.0f;
    f32 rtGIIntensity = 1.0f;
    u32 rtGIBounces = 1;

    // Path Tracer
    u32 rtPathTracerMaxBounces = 4;
    u32 rtPathTracerTargetSPP = 1024;

    // Denoiser (SVGF)
    bool rtDenoiserEnabled = true;
    u32 rtDenoiserIterations = 5;
    f32 rtDenoiserTemporalAlpha = 0.05f;

    // Composite strengths
    f32 rtShadowStrength = 1.0f;
    f32 rtReflectionStrength = 0.5f;
    f32 rtAOStrength = 1.0f;
    f32 rtGIStrength = 0.5f;

    // --- Conversion helpers ---
    static SceneRenderSettings CaptureFromRuntime(ECS::RenderSystem* rs, PostProcessSettings* pp);
    void ApplyToRuntime(ECS::RenderSystem* rs, PostProcessSettings* pp) const;
    static SceneRenderSettings Defaults() { return SceneRenderSettings{}; }
};

// JSON serialization (free functions — usable from SceneSerializer and SceneManager)
nlohmann::json SerializeRenderSettings(const SceneRenderSettings& s);
SceneRenderSettings DeserializeRenderSettings(const nlohmann::json& j);

} // namespace Renderer
} // namespace Enjin
