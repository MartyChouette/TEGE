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

    // --- Art Style Preset ---
    // 0=Realistic PBR, 1=Classic Blinn-Phong, 2=Hand-Painted, 3=Toon/Anime,
    // 4=Low-Poly Retro, 5=Pixel Art, 6=NPR Sketch
    u32 artStylePreset = 0;

    // --- RenderSystem fields ---
    bool shadowsEnabled = true;
    u32 shadowResolution = 2048;    // 512/1024/2048/4096
    f32 shadowDistance = 100.0f;    // Max shadow distance
    f32 shadowStrength = 1.0f;     // 0..1
    f32 shadowSoftness = 0.0f;     // 0 = hard, 1-5 = soft (Poisson disk texel radius)
    bool cascadeProgressiveUpdate = false;  // Far CSM cascades update every N frames
    u32 cascadeFarUpdateInterval = 2;       // Far cascade update frequency (2-8)
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
    // HDR output
    bool hdrOutput = false;        // Enable HDR swapchain output

    // Tone mapping
    u32 toneMappingMode = 3;  // Default to ACES (0=None, 1=Reinhard, 2=ReinhardExt, 3=ACES)
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

    // Anti-Aliasing mode: 0=None, 1=FXAA, 2=TAA, 3=SMAA, 4=MSAA 2x, 5=MSAA 4x, 6=MSAA 8x
    u32 aaMode = 1;  // Default: FXAA

    // AA Comparison Mode (split-screen side-by-side AA comparison)
    bool aaComparisonEnabled = false;
    u32 aaComparisonModeLeft = 0;     // AA mode for left side (0=None, 1=FXAA, 2=TAA, 3=SMAA)
    u32 aaComparisonModeRight = 1;    // AA mode for right side
    f32 aaComparisonDivider = 0.5f;   // Divider position (0=left edge, 1=right edge)

    // FXAA
    bool fxaaEnabled = true;
    f32 fxaaSpanMax = 8.0f;
    f32 fxaaReduceMin = 1.0f / 128.0f;
    f32 fxaaReduceMul = 1.0f / 8.0f;

    // TAA (Temporal Anti-Aliasing)
    f32 taaSharpness = 0.1f;       // Sharpening strength applied after TAA resolve (0 = off)
    f32 taaJitterScale = 1.0f;     // Jitter magnitude multiplier (1.0 = standard Halton)
    f32 taaFeedbackMin = 0.88f;    // Min history blend weight (low = more responsive, more flicker)
    f32 taaFeedbackMax = 0.97f;    // Max history blend weight (high = smoother, more ghosting)

    // Temporal Upscaling (FSR 2 / DLSS / XeSS — replaces TAA when active)
    u32 upscalerType = 0;             // 0=None, 1=FSR2, 2=DLSS, 3=XeSS
    u32 upscalerQuality = 2;          // 0=Performance, 1=Balanced, 2=Quality, 3=UltraQuality
    f32 upscalerSharpness = 0.0f;     // Additional sharpening (0 = upscaler default)

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
    f32 crtBloomSigma = 0.8f;
    f32 crtModelPreset = 0.0f;
    f32 crtTVL = 400.0f;

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
    f32 texturePageSize = 0.0f;       // PS1 VRAM page size (0=off, 64/128)
    f32 depthSortJitter = 0.0f;       // PS1 ordering table jitter (0=off)
    f32 lightRampMode = 0.0f;         // 0=off, 1=smooth, 2=warm, 3=cool, 4=anime
    f32 normalQuantizeSteps = 0.0f;   // Normal quantization (0=off, 4-16)
    f32 celShadowMode = 0.0f;         // 0=off, 1=purple, 2=blue, 3=warm, 4=neutral cool
    bool halfLambert = false;          // Half-Lambert soft light falloff

    // Full-screen stipple / dither
    bool stippleEnabled = false;
    u32 stipplePatternMask = 1;   // Bitmask: bit0=Bayer4x4..bit7=FloydSteinberg
    u32 stippleColorMode = 0;     // 0=Mono, 1=DuoTone, 2=FullColor
    f32 stippleScale = 1.0f;
    f32 stippleDensity = 0.5f;
    f32 stippleStrength = 1.0f;
    Math::Vector3 stippleFgColor = Math::Vector3(0.0f, 0.0f, 0.0f);
    Math::Vector3 stippleBgColor = Math::Vector3(1.0f, 1.0f, 1.0f);

    // --- Depth of Field ---
    bool dofEnabled = false;
    f32 dofFocalDistance = 10.0f;
    f32 dofFocalRange = 5.0f;
    f32 dofNearBlurStrength = 1.0f;
    f32 dofFarBlurStrength = 1.0f;
    f32 dofBokehSize = 4.0f;
    u32 dofApertureShape = 0;
    bool dofDebugCoC = false;

    // --- Tilt-Shift ---
    bool tiltShiftEnabled = false;
    f32 tiltShiftFocusY = 0.5f;
    f32 tiltShiftBandWidth = 0.3f;
    f32 tiltShiftBlurAmount = 3.0f;

    // --- Shading Model ---
    u32 shadingModel = 0;              // 0=Blinn-Phong, 1=PBR (GGX)
    bool fresnelEnabled = false;       // Fresnel-Schlick edge reflections
    bool energyConservation = false;   // Diffuse/specular energy balance
    bool geometryTerm = false;         // Smith GGX microfacet self-shadowing

    // --- Dreamcast-style effects ---
    bool sphereEnvMapEnabled = false;  // Spherical environment mapping (matcap sheen)
    f32 sphereEnvStrength = 0.5f;      // Intensity of sphere env contribution
    f32 posterizeLevels = 0.0f;        // 0=disabled, 4-256=color levels per channel

    // --- Cel Shading ---
    bool celShadingEnabled = false;
    f32 celDiffuseBands = 3.0f;
    f32 celSpecularCutoff = 0.5f;
    bool celOutlineEnabled = false;
    f32 celOutlineThickness = 1.0f;
    f32 celOutlineThreshold = 0.1f;
    f32 celOutlineCurvatureWeight = 0.0f;  // Curvature-driven thickness (0=off, 0-2 typical)
    Math::Vector3 celOutlineColor = Math::Vector3(0.0f, 0.0f, 0.0f);

    // --- Geometry Outlines (inverted-hull) ---
    bool geometryOutlinesEnabled = false;
    f32 geometryOutlineWidth = 0.02f;  // World-space extrusion distance
    Math::Vector3 geometryOutlineColor = Math::Vector3(0.0f, 0.0f, 0.0f); // Black default

    // --- Screen-Space Effects ---
    // God Rays
    bool godRaysEnabled = false;
    f32 godRaysIntensity = 0.5f;
    f32 godRaysDecay = 0.97f;
    f32 godRaysDensity = 1.0f;
    u32 godRaysSamples = 64;
    f32 godRaysWeight = 0.01f;

    // SSAO
    bool ssaoEnabled = false;
    f32 ssaoRadius = 0.5f;
    f32 ssaoIntensity = 1.5f;
    f32 ssaoBias = 0.025f;
    u32 ssaoSamples = 16;

    // Contact Shadows
    bool contactShadowsEnabled = false;
    f32 contactShadowsLength = 0.1f;
    u32 contactShadowsSteps = 16;
    f32 contactShadowsIntensity = 1.0f;

    // Fake Caustics
    bool causticsEnabled = false;
    f32 causticsIntensity = 0.3f;
    f32 causticsScale = 1.0f;
    f32 causticsSpeed = 1.0f;
    f32 causticsWaterY = 0.0f;

    // Fog Shafts
    bool fogShaftsEnabled = false;
    f32 fogShaftsIntensity = 0.3f;
    f32 fogShaftsDensity = 0.05f;
    f32 fogShaftsDecay = 0.95f;
    u32 fogShaftsSamples = 16;
    f32 fogShaftsMaxDistance = 50.0f;

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
    bool rtReflectionSDFFallback = true;
    f32 rtReflectionSDFMaxDistance = 500.0f;

    // RT Ambient Occlusion
    bool rtAOEnabled = true;
    f32 rtAORadius = 2.0f;
    f32 rtAOPower = 1.5f;

    // RT Global Illumination
    bool rtGIEnabled = false;
    f32 rtGIMaxDistance = 50.0f;
    f32 rtGIIntensity = 1.0f;
    u32 rtGIBounces = 1;

    // Radiance Cache (screen-space irradiance caching for GI)
    bool radianceCacheEnabled = false;
    u32 radianceCacheTileSize = 32;
    f32 radianceCacheMaxAge = 8.0f;
    f32 radianceCacheDepthThreshold = 0.1f;
    f32 radianceCacheNormalThreshold = 0.85f;
    f32 radianceCacheHysteresis = 0.9f;
    bool radianceCacheExcludeDirectional = true;

    // Path Tracer
    u32 rtPathTracerMaxBounces = 4;
    u32 rtPathTracerTargetSPP = 1024;
    f32 rtPathTracerFireflyClamp = 10.0f;  // Max radiance per sample
    bool rtPathTracerNEE = true;           // Next Event Estimation
    bool rtPathTracerMIS = true;           // Multiple Importance Sampling
    f32 rtPathTracerRRMinBounce = 3.0f;    // Russian Roulette min bounce
    f32 rtPathTracerRRMinProb = 0.05f;     // Russian Roulette min survival probability

    // Simplified RT Materials (pre-baked to reduce hit shader divergence)
    bool rtSimplifiedMaterials = true;
    u32 rtSimplifyAfterBounce = 1;  // Simplify material evaluation after this bounce depth

    // Denoiser
    bool rtDenoiserEnabled = true;
    u32 rtDenoiserType = 0;             // 0=SVGF, 1=OIDN
    u32 rtDenoiserIterations = 5;       // SVGF: a-trous iterations
    f32 rtDenoiserTemporalAlpha = 0.05f; // SVGF: temporal blend factor
    u32 rtOIDNQuality = 1;              // OIDN: 0=Fast, 1=Default, 2=High

    // ReSTIR (Reservoir-based Spatiotemporal Importance Resampling)
    bool restirEnabled = false;
    u32 restirInitialCandidates = 8;     // N — random light candidates per pixel (1-32)
    f32 restirDistanceBias = 0.1f;       // Minimum distance for falloff (prevents div-by-zero)
    bool restirTemporalReuse = false;    // Reproject previous frame's reservoirs via motion vectors
    u32 restirTemporalMaxHistory = 20;   // M_max cap to prevent stale sample dominance
    f32 restirTemporalDepthThreshold = 0.1f;   // Relative depth ratio for reprojection validity
    f32 restirTemporalNormalThreshold = 0.9f;  // Normal dot product for reprojection validity
    bool restirSpatialReuse = false;     // Share reservoirs with similar neighbors
    u32 restirSpatialNeighbors = 5;      // K — number of neighbors to sample
    f32 restirSpatialRadius = 30.0f;     // Screen-space neighbor search radius (pixels)
    f32 restirSpatialDepthThreshold = 0.1f;    // Relative depth threshold for neighbor similarity
    f32 restirSpatialNormalThreshold = 0.9f;   // Normal dot product for neighbor similarity

    // RT Temporal Reuse
    bool rtTemporalReuseEnabled = false;
    f32 rtTemporalReuseHistoryLength = 0.9f;
    f32 rtTemporalReuseDisocclusionThreshold = 0.1f;
    f32 rtTemporalReuseNormalThreshold = 0.9f;
    bool rtTemporalReuseShadows = true;
    bool rtTemporalReuseReflections = true;
    bool rtTemporalReuseAO = true;
    bool rtTemporalReuseGI = true;

    // Surfel Radiance Cache
    bool surfelCacheEnabled = false;
    u32 surfelCacheMaxSurfels = 65536;
    f32 surfelCacheRadius = 0.5f;
    f32 surfelCacheUpdateFraction = 0.125f;
    f32 surfelCacheMaxAge = 32.0f;
    bool surfelCacheExcludeDirectional = true;
    f32 surfelCacheCameraRadius = 50.0f;
    f32 surfelCacheBlendWeight = 0.5f;
    f32 surfelCacheNormalThreshold = 0.7f;
    u32 surfelCachePlacementInterval = 4;
    u32 surfelCacheRaysPerSurfel = 2;

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

// Apply a named art style preset to the given settings struct.
// presetIndex: 0=Realistic PBR, 1=Classic Blinn-Phong, 2=Hand-Painted,
//              3=Toon/Anime, 4=Low-Poly Retro, 5=Pixel Art, 6=NPR Sketch
void ApplyArtStylePreset(SceneRenderSettings& s, u32 presetIndex);

// JSON serialization (free functions — usable from SceneSerializer and SceneManager)
nlohmann::json SerializeRenderSettings(const SceneRenderSettings& s);
SceneRenderSettings DeserializeRenderSettings(const nlohmann::json& j);

} // namespace Renderer
} // namespace Enjin
