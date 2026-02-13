#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTReflections.h"
#include "Enjin/Renderer/RayTracing/RTAmbientOcclusion.h"
#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"
#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#include <nlohmann/json.hpp>
#include <cmath>

using json = nlohmann::json;

namespace Enjin {
namespace Renderer {

// ---------------------------------------------------------------------------
// CaptureFromRuntime — read live rendering state into a config struct
// ---------------------------------------------------------------------------
SceneRenderSettings SceneRenderSettings::CaptureFromRuntime(ECS::RenderSystem* rs, PostProcessSettings* pp) {
    SceneRenderSettings s;

    if (rs) {
        s.shadowsEnabled       = rs->IsShadowsEnabled();
        s.shadowResolution     = rs->GetShadowResolution();
        s.shadowDistance       = rs->GetShadowDistance();
        s.shadowStrength       = rs->GetShadowStrength();
        s.shadowSoftness       = rs->GetShadowSoftness();
        s.backfaceCulling      = rs->IsBackfaceCullingEnabled();
        s.wireframe            = rs->IsWireframeEnabled();
        s.ambientIntensity     = rs->GetAmbientIntensity();
        s.ambientColor         = rs->GetAmbientColor();
        s.fogDensity           = rs->GetFogDensity();
        s.fogStart             = rs->GetFogStart();
        s.fogEnd               = rs->GetFogEnd();
        s.fogHeightFalloff     = rs->GetFogHeightFalloff();
        s.fogColor             = rs->GetFogColor();
        s.snowIntensity        = rs->GetSnowIntensity();
        s.worldCurvature       = rs->GetWorldCurvature();
        s.rainActive           = rs->IsRainActive();
        // Global retro overrides
        s.globalFlatShading          = rs->GetGlobalFlatShading();
        s.globalAffineTexturing      = rs->GetGlobalAffineTexturing();
        s.globalVertexSnapping       = rs->GetGlobalVertexSnapping();
        s.globalStippleTransparency  = rs->GetGlobalStippleTransparency();
        s.globalUVQuantize           = rs->GetGlobalUVQuantize();
        s.globalGouraudOnly          = rs->GetGlobalGouraudOnly();
        s.globalVertexSnapResolution = rs->GetGlobalVertexSnapResolution();

        // Cel Shading
        s.celShadingEnabled = rs->IsCelShadingEnabled();
        s.celDiffuseBands = rs->GetCelDiffuseBands();
        s.celSpecularCutoff = rs->GetCelSpecularCutoff();

        // Ray Tracing
        s.rtEnabled = rs->IsRayTracingEnabled();
        s.rtMode = rs->GetRTMode();
        if (auto* rtShadows = rs->GetRTShadows()) {
            s.rtShadowsEnabled = rtShadows->GetConfig().enabled;
            s.rtShadowMaxDistance = rtShadows->GetConfig().maxDistance;
            s.rtShadowRadius = rtShadows->GetConfig().radius;
        }
        if (auto* rtReflect = rs->GetRTReflections()) {
            s.rtReflectionsEnabled = rtReflect->GetConfig().enabled;
            s.rtReflectionMaxDistance = rtReflect->GetConfig().maxDistance;
            s.rtReflectionRoughnessThreshold = rtReflect->GetConfig().roughnessThreshold;
        }
        if (auto* rtAO = rs->GetRTAO()) {
            s.rtAOEnabled = rtAO->GetConfig().enabled;
            s.rtAORadius = rtAO->GetConfig().radius;
            s.rtAOPower = rtAO->GetConfig().power;
        }
        if (auto* rtGI = rs->GetRTGI()) {
            s.rtGIEnabled = rtGI->GetConfig().enabled;
            s.rtGIMaxDistance = rtGI->GetConfig().maxDistance;
            s.rtGIIntensity = rtGI->GetConfig().intensity;
            s.rtGIBounces = rtGI->GetConfig().bounces;
        }
        if (auto* pathTracer = rs->GetPathTracer()) {
            s.rtPathTracerMaxBounces = pathTracer->GetConfig().maxBounces;
            s.rtPathTracerTargetSPP = pathTracer->GetConfig().targetSPP;
        }
        if (auto* denoiser = rs->GetSVGFDenoiser()) {
            s.rtDenoiserEnabled = true;
            s.rtDenoiserIterations = denoiser->GetConfig().atrousIterations;
            s.rtDenoiserTemporalAlpha = denoiser->GetConfig().temporalAlpha;
        }
        if (auto* compositor = rs->GetRTCompositor()) {
            s.rtShadowStrength = compositor->GetConfig().shadowStrength;
            s.rtReflectionStrength = compositor->GetConfig().reflectionStrength;
            s.rtAOStrength = compositor->GetConfig().aoStrength;
            s.rtGIStrength = compositor->GetConfig().giStrength;
        }
    }

    if (pp) {
        // Tone mapping
        s.toneMappingMode              = pp->toneMappingMode;
        s.exposure                     = pp->exposure;
        s.gamma                        = pp->gamma;
        s.whitePoint                   = pp->whitePoint;

        // Bloom
        s.bloomEnabled                 = pp->bloomEnabled != 0;
        s.bloomThreshold               = pp->bloomThreshold;
        s.bloomIntensity               = pp->bloomIntensity;
        s.bloomRadius                  = pp->bloomRadius;

        // Vignette
        s.vignetteEnabled              = pp->vignetteEnabled != 0;
        s.vignetteIntensity            = pp->vignetteIntensity;
        s.vignetteSmoothness           = pp->vignetteSmoothness;

        // Chromatic aberration
        s.chromaticAberrationEnabled   = pp->chromaticAberrationEnabled != 0;
        s.chromaticAberrationIntensity = pp->chromaticAberrationIntensity;

        // Color grading
        s.colorFilter                  = pp->colorFilter;
        s.saturation                   = pp->saturation;
        s.contrast                     = pp->contrast;
        s.brightness                   = pp->brightness;

        // Film grain
        s.filmGrainEnabled             = pp->filmGrainEnabled != 0;
        s.filmGrainIntensity           = pp->filmGrainIntensity;

        // FXAA
        s.fxaaEnabled                  = pp->fxaaEnabled != 0;
        s.fxaaSpanMax                  = pp->fxaaSpanMax;
        s.fxaaReduceMin                = pp->fxaaReduceMin;
        s.fxaaReduceMul                = pp->fxaaReduceMul;

        // Retro: Dithering
        s.ditherEnabled                = pp->ditherEnabled != 0;
        s.ditherPattern                = pp->ditherPattern;
        s.ditherStrength               = pp->ditherStrength;

        // Retro: Color quantization
        s.colorQuantEnabled            = pp->colorQuantEnabled != 0;
        s.colorBitDepth                = pp->colorBitDepth;

        // Retro: Resolution downscaling
        s.resDownscaleEnabled          = pp->resDownscaleEnabled != 0;
        s.internalWidth                = pp->internalWidth;
        s.internalHeight               = pp->internalHeight;
        s.usePointFiltering            = pp->usePointFiltering != 0;

        // CRT scanlines
        s.crtEnabled                   = pp->crtEnabled != 0;
        s.scanlineIntensity            = pp->scanlineIntensity;
        s.scanlineWidth                = pp->scanlineWidth;
        s.crtCurvature                 = pp->crtCurvature;

        // LUT
        s.lutEnabled                   = pp->lutEnabled != 0;
        s.lutStrength                  = pp->lutStrength;
        s.lutSize                      = pp->lutSize;

        // CRT Phosphor
        s.crtPhosphorEnabled           = pp->crtPhosphorEnabled != 0;
        s.crtMaskType                  = pp->crtMaskType;
        s.crtMaskPitch                 = pp->crtMaskPitch;
        s.crtBloomRadius               = pp->crtBloomRadius;
        s.crtBloomStrength             = pp->crtBloomStrength;

        // VHS
        s.vhsEnabled                   = pp->vhsEnabled != 0;
        s.vhsTrackingIntensity         = pp->vhsTrackingIntensity;
        s.vhsTrackingSpeed             = pp->vhsTrackingSpeed;
        s.vhsWobbleIntensity           = pp->vhsWobbleIntensity;
        s.vhsWobbleSpeed               = pp->vhsWobbleSpeed;
        s.vhsColorBleed                = pp->vhsColorBleed;
        s.vhsNoiseIntensity            = pp->vhsNoiseIntensity;
        s.vhsBlueShift                 = pp->vhsBlueShift;
        s.vhsScreenTear                = pp->vhsScreenTear != 0;
        s.vhsTearOffset                = pp->vhsTearOffset;
        s.vhsInterlacing               = pp->vhsInterlacing != 0;

        // Palette lock
        s.paletteEnabled               = pp->paletteEnabled != 0;
        s.paletteColors                = pp->paletteColors;

        // Depth of Field
        s.dofEnabled                   = pp->dofEnabled != 0;
        s.dofFocalDistance             = pp->dofFocalDistance;
        s.dofFocalRange               = pp->dofFocalRange;
        s.dofNearBlurStrength         = pp->dofNearBlurStrength;
        s.dofFarBlurStrength          = pp->dofFarBlurStrength;
        s.dofBokehSize                = pp->dofBokehSize;
        s.dofApertureShape            = pp->dofApertureShape;
        s.dofDebugCoC                 = pp->dofDebugCoC != 0;

        // Tilt-Shift
        s.tiltShiftEnabled            = pp->tiltShiftEnabled != 0;
        s.tiltShiftFocusY             = pp->tiltShiftFocusY;
        s.tiltShiftBandWidth          = pp->tiltShiftBandWidth;
        s.tiltShiftBlurAmount         = pp->tiltShiftBlurAmount;

        // Cel outline
        s.celOutlineEnabled            = pp->celOutlineEnabled != 0;
        s.celOutlineThickness          = pp->celOutlineThickness;
        s.celOutlineThreshold          = pp->celOutlineThreshold;
        s.celOutlineColor              = pp->celOutlineColor;

        // Stipple
        s.stippleEnabled               = pp->stippleEnabled != 0;
        s.stipplePatternMask           = pp->stipplePatternMask;
        s.stippleColorMode             = pp->stippleColorMode;
        s.stippleScale                 = pp->stippleScale;
        s.stippleDensity               = pp->stippleDensity;
        s.stippleStrength              = pp->stippleStrength;
        s.stippleFgColor               = pp->stippleFgColor;
        s.stippleBgColor               = pp->stippleBgColor;
    }

    return s;
}

// ---------------------------------------------------------------------------
// ApplyToRuntime — write config values to live systems
// Preserves runtime-only fields: time, screenWidth/Height, colorblindMode/Strength
// ---------------------------------------------------------------------------
void SceneRenderSettings::ApplyToRuntime(ECS::RenderSystem* rs, PostProcessSettings* pp) const {
    if (rs) {
        rs->SetShadowsEnabled(shadowsEnabled);
        rs->SetShadowResolution(shadowResolution);
        rs->SetShadowDistance(shadowDistance);
        rs->SetShadowStrength(shadowStrength);
        rs->SetShadowSoftness(shadowSoftness);
        rs->SetBackfaceCullingEnabled(backfaceCulling);
        rs->SetWireframeEnabled(wireframe);
        rs->SetAmbientIntensity(ambientIntensity);
        rs->SetAmbientColor(ambientColor);
        rs->SetFogParams(fogDensity, fogStart, fogEnd, fogHeightFalloff);
        rs->SetFogColor(fogColor);
        rs->SetSnowIntensity(snowIntensity);
        rs->SetWorldCurvature(worldCurvature);
        rs->SetRainActive(rainActive);
        // Global retro overrides
        rs->SetGlobalFlatShading(globalFlatShading);
        rs->SetGlobalAffineTexturing(globalAffineTexturing);
        rs->SetGlobalVertexSnapping(globalVertexSnapping);
        rs->SetGlobalStippleTransparency(globalStippleTransparency);
        rs->SetGlobalUVQuantize(globalUVQuantize);
        rs->SetGlobalGouraudOnly(globalGouraudOnly);
        rs->SetGlobalVertexSnapResolution(static_cast<u8>(globalVertexSnapResolution));

        // Cel Shading
        rs->SetCelShadingEnabled(celShadingEnabled);
        rs->SetCelDiffuseBands(celDiffuseBands);
        rs->SetCelSpecularCutoff(celSpecularCutoff);

        // Ray Tracing
        rs->SetRayTracingEnabled(rtEnabled);
        rs->SetRTMode(rtMode);
        if (auto* rtShadows = rs->GetRTShadows()) {
            rtShadows->GetConfig().enabled = rtShadowsEnabled;
            rtShadows->GetConfig().maxDistance = rtShadowMaxDistance;
            rtShadows->GetConfig().radius = rtShadowRadius;
        }
        if (auto* rtReflect = rs->GetRTReflections()) {
            rtReflect->GetConfig().enabled = rtReflectionsEnabled;
            rtReflect->GetConfig().maxDistance = rtReflectionMaxDistance;
            rtReflect->GetConfig().roughnessThreshold = rtReflectionRoughnessThreshold;
        }
        if (auto* rtAO = rs->GetRTAO()) {
            rtAO->GetConfig().enabled = rtAOEnabled;
            rtAO->GetConfig().radius = rtAORadius;
            rtAO->GetConfig().power = rtAOPower;
        }
        if (auto* rtGI = rs->GetRTGI()) {
            rtGI->GetConfig().enabled = rtGIEnabled;
            rtGI->GetConfig().maxDistance = rtGIMaxDistance;
            rtGI->GetConfig().intensity = rtGIIntensity;
            rtGI->GetConfig().bounces = rtGIBounces;
        }
        if (auto* pathTracer = rs->GetPathTracer()) {
            pathTracer->GetConfig().maxBounces = rtPathTracerMaxBounces;
            pathTracer->GetConfig().targetSPP = rtPathTracerTargetSPP;
        }
        if (auto* denoiser = rs->GetSVGFDenoiser()) {
            denoiser->GetConfig().atrousIterations = rtDenoiserIterations;
            denoiser->GetConfig().temporalAlpha = rtDenoiserTemporalAlpha;
        }
        if (auto* compositor = rs->GetRTCompositor()) {
            compositor->GetConfig().shadowStrength = rtShadowStrength;
            compositor->GetConfig().reflectionStrength = rtReflectionStrength;
            compositor->GetConfig().aoStrength = rtAOStrength;
            compositor->GetConfig().giStrength = rtGIStrength;
        }
    }

    if (pp) {
        // Save runtime-only fields
        f32 savedTime = pp->time;
        u32 savedScreenW = pp->screenWidth;
        u32 savedScreenH = pp->screenHeight;
        u32 savedColorblindMode = pp->colorblindMode;
        f32 savedColorblindStrength = pp->colorblindStrength;

        // Tone mapping
        pp->toneMappingMode              = toneMappingMode;
        pp->exposure                     = exposure;
        pp->gamma                        = gamma;
        pp->whitePoint                   = whitePoint;

        // Bloom
        pp->bloomEnabled                 = bloomEnabled ? 1 : 0;
        pp->bloomThreshold               = bloomThreshold;
        pp->bloomIntensity               = bloomIntensity;
        pp->bloomRadius                  = bloomRadius;

        // Vignette
        pp->vignetteEnabled              = vignetteEnabled ? 1 : 0;
        pp->vignetteIntensity            = vignetteIntensity;
        pp->vignetteSmoothness           = vignetteSmoothness;

        // Chromatic aberration
        pp->chromaticAberrationEnabled   = chromaticAberrationEnabled ? 1 : 0;
        pp->chromaticAberrationIntensity = chromaticAberrationIntensity;

        // Color grading
        pp->colorFilter                  = colorFilter;
        pp->saturation                   = saturation;
        pp->contrast                     = contrast;
        pp->brightness                   = brightness;

        // Film grain
        pp->filmGrainEnabled             = filmGrainEnabled ? 1 : 0;
        pp->filmGrainIntensity           = filmGrainIntensity;

        // FXAA
        pp->fxaaEnabled                  = fxaaEnabled ? 1 : 0;
        pp->fxaaSpanMax                  = fxaaSpanMax;
        pp->fxaaReduceMin                = fxaaReduceMin;
        pp->fxaaReduceMul                = fxaaReduceMul;

        // Retro: Dithering
        pp->ditherEnabled                = ditherEnabled ? 1 : 0;
        pp->ditherPattern                = ditherPattern;
        pp->ditherStrength               = ditherStrength;

        // Retro: Color quantization
        pp->colorQuantEnabled            = colorQuantEnabled ? 1 : 0;
        pp->colorBitDepth                = colorBitDepth;

        // Retro: Resolution downscaling
        pp->resDownscaleEnabled          = resDownscaleEnabled ? 1 : 0;
        pp->internalWidth                = internalWidth;
        pp->internalHeight               = internalHeight;
        pp->usePointFiltering            = usePointFiltering ? 1 : 0;

        // CRT scanlines
        pp->crtEnabled                   = crtEnabled ? 1 : 0;
        pp->scanlineIntensity            = scanlineIntensity;
        pp->scanlineWidth                = scanlineWidth;
        pp->crtCurvature                 = crtCurvature;

        // LUT
        pp->lutEnabled                   = lutEnabled ? 1 : 0;
        pp->lutStrength                  = lutStrength;
        pp->lutSize                      = lutSize;

        // CRT Phosphor
        pp->crtPhosphorEnabled           = crtPhosphorEnabled ? 1 : 0;
        pp->crtMaskType                  = crtMaskType;
        pp->crtMaskPitch                 = crtMaskPitch;
        pp->crtBloomRadius               = crtBloomRadius;
        pp->crtBloomStrength             = crtBloomStrength;

        // VHS
        pp->vhsEnabled                   = vhsEnabled ? 1 : 0;
        pp->vhsTrackingIntensity         = vhsTrackingIntensity;
        pp->vhsTrackingSpeed             = vhsTrackingSpeed;
        pp->vhsWobbleIntensity           = vhsWobbleIntensity;
        pp->vhsWobbleSpeed               = vhsWobbleSpeed;
        pp->vhsColorBleed                = vhsColorBleed;
        pp->vhsNoiseIntensity            = vhsNoiseIntensity;
        pp->vhsBlueShift                 = vhsBlueShift;
        pp->vhsScreenTear                = vhsScreenTear ? 1 : 0;
        pp->vhsTearOffset                = vhsTearOffset;
        pp->vhsInterlacing               = vhsInterlacing ? 1 : 0;

        // Palette lock
        pp->paletteEnabled               = paletteEnabled ? 1 : 0;
        pp->paletteColors                = paletteColors;

        // Depth of Field
        pp->dofEnabled                   = dofEnabled ? 1 : 0;
        pp->dofFocalDistance             = dofFocalDistance;
        pp->dofFocalRange               = dofFocalRange;
        pp->dofNearBlurStrength         = dofNearBlurStrength;
        pp->dofFarBlurStrength          = dofFarBlurStrength;
        pp->dofBokehSize                = dofBokehSize;
        pp->dofApertureShape            = dofApertureShape;
        pp->dofDebugCoC                 = dofDebugCoC ? 1 : 0;

        // Tilt-Shift
        pp->tiltShiftEnabled            = tiltShiftEnabled ? 1 : 0;
        pp->tiltShiftFocusY             = tiltShiftFocusY;
        pp->tiltShiftBandWidth          = tiltShiftBandWidth;
        pp->tiltShiftBlurAmount         = tiltShiftBlurAmount;

        // Cel outline
        pp->celOutlineEnabled            = celOutlineEnabled ? 1 : 0;
        pp->celOutlineThickness          = celOutlineThickness;
        pp->celOutlineThreshold          = celOutlineThreshold;
        pp->celOutlineColor              = celOutlineColor;

        // Stipple
        pp->stippleEnabled               = stippleEnabled ? 1 : 0;
        pp->stipplePatternMask           = stipplePatternMask;
        pp->stippleColorMode             = stippleColorMode;
        pp->stippleScale                 = stippleScale;
        pp->stippleDensity               = stippleDensity;
        pp->stippleStrength              = stippleStrength;
        pp->stippleFgColor               = stippleFgColor;
        pp->stippleBgColor               = stippleBgColor;

        // Restore runtime-only fields
        pp->time = savedTime;
        pp->screenWidth = savedScreenW;
        pp->screenHeight = savedScreenH;
        pp->colorblindMode = savedColorblindMode;
        pp->colorblindStrength = savedColorblindStrength;
    }
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------

// Round float to 6 decimal places for deterministic serialization
static f32 RF(f32 val) {
    if (std::isnan(val) || std::isinf(val)) return val;
    if (std::fabs(val) < 1e-6f) return 0.0f;
    constexpr f32 mult = 1000000.0f;
    return std::round(val * mult) / mult;
}

static json SerializeVec3(const Math::Vector3& v) {
    return json::array({RF(v.x), RF(v.y), RF(v.z)});
}

static Math::Vector3 DeserializeVec3(const json& j, const Math::Vector3& def = Math::Vector3(0, 0, 0)) {
    if (!j.is_array() || j.size() < 3) return def;
    return Math::Vector3(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>());
}

json SerializeRenderSettings(const SceneRenderSettings& s) {
    json j;

    j["useProjectDefaults"] = s.useProjectDefaults;

    // RenderSystem
    j["shadowsEnabled"]    = s.shadowsEnabled;
    j["shadowResolution"]  = s.shadowResolution;
    j["shadowDistance"]    = RF(s.shadowDistance);
    j["shadowStrength"]    = RF(s.shadowStrength);
    j["shadowSoftness"]    = RF(s.shadowSoftness);
    j["backfaceCulling"]   = s.backfaceCulling;
    j["wireframe"]         = s.wireframe;
    j["ambientIntensity"]  = RF(s.ambientIntensity);
    j["ambientColor"]      = SerializeVec3(s.ambientColor);
    j["fogDensity"]        = RF(s.fogDensity);
    j["fogStart"]          = RF(s.fogStart);
    j["fogEnd"]            = RF(s.fogEnd);
    j["fogHeightFalloff"]  = RF(s.fogHeightFalloff);
    j["fogColor"]          = SerializeVec3(s.fogColor);
    j["snowIntensity"]     = RF(s.snowIntensity);
    j["worldCurvature"]    = RF(s.worldCurvature);
    j["rainActive"]        = s.rainActive;

    // Tone mapping
    j["toneMappingMode"]   = s.toneMappingMode;
    j["exposure"]          = RF(s.exposure);
    j["gamma"]             = RF(s.gamma);
    j["whitePoint"]        = RF(s.whitePoint);

    // Bloom
    j["bloomEnabled"]      = s.bloomEnabled;
    j["bloomThreshold"]    = RF(s.bloomThreshold);
    j["bloomIntensity"]    = RF(s.bloomIntensity);
    j["bloomRadius"]       = RF(s.bloomRadius);

    // Vignette
    j["vignetteEnabled"]     = s.vignetteEnabled;
    j["vignetteIntensity"]   = RF(s.vignetteIntensity);
    j["vignetteSmoothness"]  = RF(s.vignetteSmoothness);

    // Chromatic aberration
    j["chromaticAberrationEnabled"]   = s.chromaticAberrationEnabled;
    j["chromaticAberrationIntensity"] = RF(s.chromaticAberrationIntensity);

    // Color grading
    j["colorFilter"]       = SerializeVec3(s.colorFilter);
    j["saturation"]        = RF(s.saturation);
    j["contrast"]          = RF(s.contrast);
    j["brightness"]        = RF(s.brightness);

    // Film grain
    j["filmGrainEnabled"]    = s.filmGrainEnabled;
    j["filmGrainIntensity"]  = RF(s.filmGrainIntensity);

    // FXAA
    j["fxaaEnabled"]       = s.fxaaEnabled;
    j["fxaaSpanMax"]       = RF(s.fxaaSpanMax);
    j["fxaaReduceMin"]     = RF(s.fxaaReduceMin);
    j["fxaaReduceMul"]     = RF(s.fxaaReduceMul);

    // Retro: Dithering
    j["ditherEnabled"]     = s.ditherEnabled;
    j["ditherPattern"]     = s.ditherPattern;
    j["ditherStrength"]    = RF(s.ditherStrength);

    // Retro: Color quantization
    j["colorQuantEnabled"] = s.colorQuantEnabled;
    j["colorBitDepth"]     = s.colorBitDepth;

    // Retro: Resolution downscaling
    j["resDownscaleEnabled"] = s.resDownscaleEnabled;
    j["internalWidth"]       = s.internalWidth;
    j["internalHeight"]      = s.internalHeight;
    j["usePointFiltering"]   = s.usePointFiltering;

    // CRT scanlines
    j["crtEnabled"]          = s.crtEnabled;
    j["scanlineIntensity"]   = RF(s.scanlineIntensity);
    j["scanlineWidth"]       = RF(s.scanlineWidth);
    j["crtCurvature"]        = RF(s.crtCurvature);

    // LUT
    j["lutEnabled"]        = s.lutEnabled;
    j["lutStrength"]       = RF(s.lutStrength);
    j["lutSize"]           = s.lutSize;

    // CRT Phosphor
    j["crtPhosphorEnabled"]  = s.crtPhosphorEnabled;
    j["crtMaskType"]         = s.crtMaskType;
    j["crtMaskPitch"]        = RF(s.crtMaskPitch);
    j["crtBloomRadius"]      = RF(s.crtBloomRadius);
    j["crtBloomStrength"]    = RF(s.crtBloomStrength);

    // VHS
    j["vhsEnabled"]            = s.vhsEnabled;
    j["vhsTrackingIntensity"]  = RF(s.vhsTrackingIntensity);
    j["vhsTrackingSpeed"]      = RF(s.vhsTrackingSpeed);
    j["vhsWobbleIntensity"]    = RF(s.vhsWobbleIntensity);
    j["vhsWobbleSpeed"]        = RF(s.vhsWobbleSpeed);
    j["vhsColorBleed"]         = RF(s.vhsColorBleed);
    j["vhsNoiseIntensity"]     = RF(s.vhsNoiseIntensity);
    j["vhsBlueShift"]          = RF(s.vhsBlueShift);
    j["vhsScreenTear"]         = RF(s.vhsScreenTear);
    j["vhsTearOffset"]         = RF(s.vhsTearOffset);
    j["vhsInterlacing"]        = RF(s.vhsInterlacing);

    // Palette lock
    j["paletteEnabled"]    = s.paletteEnabled;
    j["paletteColors"]     = s.paletteColors;

    // Stipple
    j["stippleEnabled"]    = s.stippleEnabled;
    j["stipplePatternMask"] = s.stipplePatternMask;
    j["stippleColorMode"]  = s.stippleColorMode;
    j["stippleScale"]      = RF(s.stippleScale);
    j["stippleDensity"]    = RF(s.stippleDensity);
    j["stippleStrength"]   = RF(s.stippleStrength);
    j["stippleFgColor"]    = SerializeVec3(s.stippleFgColor);
    j["stippleBgColor"]    = SerializeVec3(s.stippleBgColor);

    // Global retro overrides
    j["globalFlatShading"]          = s.globalFlatShading;
    j["globalAffineTexturing"]      = s.globalAffineTexturing;
    j["globalVertexSnapping"]       = s.globalVertexSnapping;
    j["globalStippleTransparency"]  = s.globalStippleTransparency;
    j["globalUVQuantize"]           = s.globalUVQuantize;
    j["globalGouraudOnly"]          = s.globalGouraudOnly;
    j["globalVertexSnapResolution"] = s.globalVertexSnapResolution;

    // Depth of Field
    j["dofEnabled"]            = s.dofEnabled;
    j["dofFocalDistance"]      = RF(s.dofFocalDistance);
    j["dofFocalRange"]         = RF(s.dofFocalRange);
    j["dofNearBlurStrength"]   = RF(s.dofNearBlurStrength);
    j["dofFarBlurStrength"]    = RF(s.dofFarBlurStrength);
    j["dofBokehSize"]          = RF(s.dofBokehSize);
    j["dofApertureShape"]      = s.dofApertureShape;
    j["dofDebugCoC"]           = s.dofDebugCoC;

    // Tilt-Shift
    j["tiltShiftEnabled"]      = s.tiltShiftEnabled;
    j["tiltShiftFocusY"]       = RF(s.tiltShiftFocusY);
    j["tiltShiftBandWidth"]    = RF(s.tiltShiftBandWidth);
    j["tiltShiftBlurAmount"]   = RF(s.tiltShiftBlurAmount);

    // Cel Shading
    j["celShadingEnabled"]     = s.celShadingEnabled;
    j["celDiffuseBands"]       = s.celDiffuseBands;
    j["celSpecularCutoff"]     = RF(s.celSpecularCutoff);
    j["celOutlineEnabled"]     = s.celOutlineEnabled;
    j["celOutlineThickness"]   = RF(s.celOutlineThickness);
    j["celOutlineThreshold"]   = RF(s.celOutlineThreshold);
    j["celOutlineColor"]       = SerializeVec3(s.celOutlineColor);

    // Ray Tracing
    j["rtEnabled"]                    = s.rtEnabled;
    j["rtMode"]                       = s.rtMode;
    j["rtShadowsEnabled"]             = s.rtShadowsEnabled;
    j["rtShadowMaxDistance"]           = RF(s.rtShadowMaxDistance);
    j["rtShadowRadius"]               = RF(s.rtShadowRadius);
    j["rtReflectionsEnabled"]          = s.rtReflectionsEnabled;
    j["rtReflectionMaxDistance"]       = RF(s.rtReflectionMaxDistance);
    j["rtReflectionRoughnessThreshold"] = RF(s.rtReflectionRoughnessThreshold);
    j["rtAOEnabled"]                   = s.rtAOEnabled;
    j["rtAORadius"]                    = RF(s.rtAORadius);
    j["rtAOPower"]                     = RF(s.rtAOPower);
    j["rtGIEnabled"]                   = s.rtGIEnabled;
    j["rtGIMaxDistance"]               = RF(s.rtGIMaxDistance);
    j["rtGIIntensity"]                 = RF(s.rtGIIntensity);
    j["rtGIBounces"]                   = s.rtGIBounces;
    j["rtPathTracerMaxBounces"]        = s.rtPathTracerMaxBounces;
    j["rtPathTracerTargetSPP"]         = s.rtPathTracerTargetSPP;
    j["rtDenoiserEnabled"]             = s.rtDenoiserEnabled;
    j["rtDenoiserIterations"]          = s.rtDenoiserIterations;
    j["rtDenoiserTemporalAlpha"]       = RF(s.rtDenoiserTemporalAlpha);
    j["rtShadowStrength"]              = RF(s.rtShadowStrength);
    j["rtReflectionStrength"]          = RF(s.rtReflectionStrength);
    j["rtAOStrength"]                  = RF(s.rtAOStrength);
    j["rtGIStrength"]                  = RF(s.rtGIStrength);

    return j;
}

SceneRenderSettings DeserializeRenderSettings(const json& j) {
    SceneRenderSettings s;

    if (j.contains("useProjectDefaults")) s.useProjectDefaults = j["useProjectDefaults"].get<bool>();

    // RenderSystem
    if (j.contains("shadowsEnabled"))    s.shadowsEnabled    = j["shadowsEnabled"].get<bool>();
    if (j.contains("shadowResolution")) s.shadowResolution  = j["shadowResolution"].get<u32>();
    if (j.contains("shadowDistance"))   s.shadowDistance     = j["shadowDistance"].get<f32>();
    if (j.contains("shadowStrength"))   s.shadowStrength     = j["shadowStrength"].get<f32>();
    if (j.contains("shadowSoftness"))   s.shadowSoftness     = j["shadowSoftness"].get<f32>();
    if (j.contains("backfaceCulling"))   s.backfaceCulling   = j["backfaceCulling"].get<bool>();
    if (j.contains("wireframe"))         s.wireframe         = j["wireframe"].get<bool>();
    if (j.contains("ambientIntensity"))  s.ambientIntensity  = j["ambientIntensity"].get<f32>();
    if (j.contains("ambientColor"))      s.ambientColor      = DeserializeVec3(j["ambientColor"], s.ambientColor);
    if (j.contains("fogDensity"))        s.fogDensity        = j["fogDensity"].get<f32>();
    if (j.contains("fogStart"))          s.fogStart          = j["fogStart"].get<f32>();
    if (j.contains("fogEnd"))            s.fogEnd            = j["fogEnd"].get<f32>();
    if (j.contains("fogHeightFalloff"))  s.fogHeightFalloff  = j["fogHeightFalloff"].get<f32>();
    if (j.contains("fogColor"))          s.fogColor          = DeserializeVec3(j["fogColor"], s.fogColor);
    if (j.contains("snowIntensity"))     s.snowIntensity     = j["snowIntensity"].get<f32>();
    if (j.contains("worldCurvature"))    s.worldCurvature    = j["worldCurvature"].get<f32>();
    if (j.contains("rainActive"))        s.rainActive        = j["rainActive"].get<bool>();

    // Tone mapping
    if (j.contains("toneMappingMode"))   s.toneMappingMode   = j["toneMappingMode"].get<u32>();
    if (j.contains("exposure"))          s.exposure          = j["exposure"].get<f32>();
    if (j.contains("gamma"))             s.gamma             = j["gamma"].get<f32>();
    if (j.contains("whitePoint"))        s.whitePoint        = j["whitePoint"].get<f32>();

    // Bloom
    if (j.contains("bloomEnabled"))      s.bloomEnabled      = j["bloomEnabled"].get<bool>();
    if (j.contains("bloomThreshold"))    s.bloomThreshold    = j["bloomThreshold"].get<f32>();
    if (j.contains("bloomIntensity"))    s.bloomIntensity    = j["bloomIntensity"].get<f32>();
    if (j.contains("bloomRadius"))       s.bloomRadius       = j["bloomRadius"].get<f32>();

    // Vignette
    if (j.contains("vignetteEnabled"))     s.vignetteEnabled     = j["vignetteEnabled"].get<bool>();
    if (j.contains("vignetteIntensity"))   s.vignetteIntensity   = j["vignetteIntensity"].get<f32>();
    if (j.contains("vignetteSmoothness"))  s.vignetteSmoothness  = j["vignetteSmoothness"].get<f32>();

    // Chromatic aberration
    if (j.contains("chromaticAberrationEnabled"))   s.chromaticAberrationEnabled   = j["chromaticAberrationEnabled"].get<bool>();
    if (j.contains("chromaticAberrationIntensity")) s.chromaticAberrationIntensity = j["chromaticAberrationIntensity"].get<f32>();

    // Color grading
    if (j.contains("colorFilter"))       s.colorFilter       = DeserializeVec3(j["colorFilter"], s.colorFilter);
    if (j.contains("saturation"))        s.saturation        = j["saturation"].get<f32>();
    if (j.contains("contrast"))          s.contrast          = j["contrast"].get<f32>();
    if (j.contains("brightness"))        s.brightness        = j["brightness"].get<f32>();

    // Film grain
    if (j.contains("filmGrainEnabled"))    s.filmGrainEnabled    = j["filmGrainEnabled"].get<bool>();
    if (j.contains("filmGrainIntensity"))  s.filmGrainIntensity  = j["filmGrainIntensity"].get<f32>();

    // FXAA
    if (j.contains("fxaaEnabled"))       s.fxaaEnabled       = j["fxaaEnabled"].get<bool>();
    if (j.contains("fxaaSpanMax"))       s.fxaaSpanMax       = j["fxaaSpanMax"].get<f32>();
    if (j.contains("fxaaReduceMin"))     s.fxaaReduceMin     = j["fxaaReduceMin"].get<f32>();
    if (j.contains("fxaaReduceMul"))     s.fxaaReduceMul     = j["fxaaReduceMul"].get<f32>();

    // Retro: Dithering
    if (j.contains("ditherEnabled"))     s.ditherEnabled     = j["ditherEnabled"].get<bool>();
    if (j.contains("ditherPattern"))     s.ditherPattern     = j["ditherPattern"].get<u32>();
    if (j.contains("ditherStrength"))    s.ditherStrength    = j["ditherStrength"].get<f32>();

    // Retro: Color quantization
    if (j.contains("colorQuantEnabled")) s.colorQuantEnabled = j["colorQuantEnabled"].get<bool>();
    if (j.contains("colorBitDepth"))     s.colorBitDepth     = j["colorBitDepth"].get<u32>();

    // Retro: Resolution downscaling
    if (j.contains("resDownscaleEnabled")) s.resDownscaleEnabled = j["resDownscaleEnabled"].get<bool>();
    if (j.contains("internalWidth"))       s.internalWidth       = j["internalWidth"].get<u32>();
    if (j.contains("internalHeight"))      s.internalHeight      = j["internalHeight"].get<u32>();
    if (j.contains("usePointFiltering"))   s.usePointFiltering   = j["usePointFiltering"].get<bool>();

    // CRT scanlines
    if (j.contains("crtEnabled"))          s.crtEnabled          = j["crtEnabled"].get<bool>();
    if (j.contains("scanlineIntensity"))   s.scanlineIntensity   = j["scanlineIntensity"].get<f32>();
    if (j.contains("scanlineWidth"))       s.scanlineWidth       = j["scanlineWidth"].get<f32>();
    if (j.contains("crtCurvature"))        s.crtCurvature        = j["crtCurvature"].get<f32>();

    // LUT
    if (j.contains("lutEnabled"))        s.lutEnabled        = j["lutEnabled"].get<bool>();
    if (j.contains("lutStrength"))       s.lutStrength       = j["lutStrength"].get<f32>();
    if (j.contains("lutSize"))           s.lutSize           = j["lutSize"].get<u32>();

    // CRT Phosphor
    if (j.contains("crtPhosphorEnabled"))  s.crtPhosphorEnabled  = j["crtPhosphorEnabled"].get<bool>();
    if (j.contains("crtMaskType"))         s.crtMaskType         = j["crtMaskType"].get<u32>();
    if (j.contains("crtMaskPitch"))        s.crtMaskPitch        = j["crtMaskPitch"].get<f32>();
    if (j.contains("crtBloomRadius"))      s.crtBloomRadius      = j["crtBloomRadius"].get<f32>();
    if (j.contains("crtBloomStrength"))    s.crtBloomStrength    = j["crtBloomStrength"].get<f32>();

    // VHS
    if (j.contains("vhsEnabled"))            s.vhsEnabled            = j["vhsEnabled"].get<bool>();
    if (j.contains("vhsTrackingIntensity"))  s.vhsTrackingIntensity  = j["vhsTrackingIntensity"].get<f32>();
    if (j.contains("vhsTrackingSpeed"))      s.vhsTrackingSpeed      = j["vhsTrackingSpeed"].get<f32>();
    if (j.contains("vhsWobbleIntensity"))    s.vhsWobbleIntensity    = j["vhsWobbleIntensity"].get<f32>();
    if (j.contains("vhsWobbleSpeed"))        s.vhsWobbleSpeed        = j["vhsWobbleSpeed"].get<f32>();
    if (j.contains("vhsColorBleed"))         s.vhsColorBleed         = j["vhsColorBleed"].get<f32>();
    if (j.contains("vhsNoiseIntensity"))     s.vhsNoiseIntensity     = j["vhsNoiseIntensity"].get<f32>();
    if (j.contains("vhsBlueShift"))          s.vhsBlueShift          = j["vhsBlueShift"].get<f32>();
    if (j.contains("vhsScreenTear"))         s.vhsScreenTear         = j["vhsScreenTear"].get<bool>();
    if (j.contains("vhsTearOffset"))         s.vhsTearOffset         = j["vhsTearOffset"].get<f32>();
    if (j.contains("vhsInterlacing"))        s.vhsInterlacing        = j["vhsInterlacing"].get<bool>();

    // Palette lock
    if (j.contains("paletteEnabled"))    s.paletteEnabled    = j["paletteEnabled"].get<bool>();
    if (j.contains("paletteColors"))     s.paletteColors     = j["paletteColors"].get<u32>();

    // Stipple
    if (j.contains("stippleEnabled"))    s.stippleEnabled    = j["stippleEnabled"].get<bool>();
    if (j.contains("stipplePatternMask")) s.stipplePatternMask = j["stipplePatternMask"].get<u32>();
    if (j.contains("stipplePattern"))     s.stipplePatternMask = 1u << j["stipplePattern"].get<u32>(); // migrate old single-pattern
    if (j.contains("stippleColorMode"))  s.stippleColorMode  = j["stippleColorMode"].get<u32>();
    if (j.contains("stippleScale"))      s.stippleScale      = j["stippleScale"].get<f32>();
    if (j.contains("stippleDensity"))    s.stippleDensity    = j["stippleDensity"].get<f32>();
    if (j.contains("stippleStrength"))   s.stippleStrength   = j["stippleStrength"].get<f32>();
    if (j.contains("stippleFgColor"))    s.stippleFgColor    = DeserializeVec3(j["stippleFgColor"], s.stippleFgColor);
    if (j.contains("stippleBgColor"))    s.stippleBgColor    = DeserializeVec3(j["stippleBgColor"], s.stippleBgColor);

    // Global retro overrides
    if (j.contains("globalFlatShading"))          s.globalFlatShading          = j["globalFlatShading"].get<bool>();
    if (j.contains("globalAffineTexturing"))      s.globalAffineTexturing      = j["globalAffineTexturing"].get<bool>();
    if (j.contains("globalVertexSnapping"))       s.globalVertexSnapping       = j["globalVertexSnapping"].get<bool>();
    if (j.contains("globalStippleTransparency"))  s.globalStippleTransparency  = j["globalStippleTransparency"].get<bool>();
    if (j.contains("globalUVQuantize"))           s.globalUVQuantize           = j["globalUVQuantize"].get<bool>();
    if (j.contains("globalGouraudOnly"))          s.globalGouraudOnly          = j["globalGouraudOnly"].get<bool>();
    if (j.contains("globalVertexSnapResolution")) s.globalVertexSnapResolution = j["globalVertexSnapResolution"].get<u32>();

    // Depth of Field
    if (j.contains("dofEnabled"))            s.dofEnabled            = j["dofEnabled"].get<bool>();
    if (j.contains("dofFocalDistance"))       s.dofFocalDistance      = j["dofFocalDistance"].get<f32>();
    if (j.contains("dofFocalRange"))         s.dofFocalRange         = j["dofFocalRange"].get<f32>();
    if (j.contains("dofNearBlurStrength"))   s.dofNearBlurStrength   = j["dofNearBlurStrength"].get<f32>();
    if (j.contains("dofFarBlurStrength"))    s.dofFarBlurStrength    = j["dofFarBlurStrength"].get<f32>();
    if (j.contains("dofBokehSize"))          s.dofBokehSize          = j["dofBokehSize"].get<f32>();
    if (j.contains("dofApertureShape"))      s.dofApertureShape      = j["dofApertureShape"].get<u32>();
    if (j.contains("dofDebugCoC"))           s.dofDebugCoC           = j["dofDebugCoC"].get<bool>();

    // Tilt-Shift
    if (j.contains("tiltShiftEnabled"))      s.tiltShiftEnabled      = j["tiltShiftEnabled"].get<bool>();
    if (j.contains("tiltShiftFocusY"))       s.tiltShiftFocusY       = j["tiltShiftFocusY"].get<f32>();
    if (j.contains("tiltShiftBandWidth"))    s.tiltShiftBandWidth    = j["tiltShiftBandWidth"].get<f32>();
    if (j.contains("tiltShiftBlurAmount"))   s.tiltShiftBlurAmount   = j["tiltShiftBlurAmount"].get<f32>();

    // Cel Shading
    if (j.contains("celShadingEnabled"))     s.celShadingEnabled     = j["celShadingEnabled"].get<bool>();
    if (j.contains("celDiffuseBands"))       s.celDiffuseBands       = j["celDiffuseBands"].get<f32>();
    if (j.contains("celSpecularCutoff"))     s.celSpecularCutoff     = j["celSpecularCutoff"].get<f32>();
    if (j.contains("celOutlineEnabled"))     s.celOutlineEnabled     = j["celOutlineEnabled"].get<bool>();
    if (j.contains("celOutlineThickness"))   s.celOutlineThickness   = j["celOutlineThickness"].get<f32>();
    if (j.contains("celOutlineThreshold"))   s.celOutlineThreshold   = j["celOutlineThreshold"].get<f32>();
    if (j.contains("celOutlineColor"))       s.celOutlineColor       = DeserializeVec3(j["celOutlineColor"], s.celOutlineColor);

    // Ray Tracing
    if (j.contains("rtEnabled"))                      s.rtEnabled                      = j["rtEnabled"].get<bool>();
    if (j.contains("rtMode"))                         s.rtMode                         = j["rtMode"].get<u32>();
    if (j.contains("rtShadowsEnabled"))               s.rtShadowsEnabled               = j["rtShadowsEnabled"].get<bool>();
    if (j.contains("rtShadowMaxDistance"))             s.rtShadowMaxDistance             = j["rtShadowMaxDistance"].get<f32>();
    if (j.contains("rtShadowRadius"))                 s.rtShadowRadius                 = j["rtShadowRadius"].get<f32>();
    if (j.contains("rtReflectionsEnabled"))            s.rtReflectionsEnabled            = j["rtReflectionsEnabled"].get<bool>();
    if (j.contains("rtReflectionMaxDistance"))         s.rtReflectionMaxDistance         = j["rtReflectionMaxDistance"].get<f32>();
    if (j.contains("rtReflectionRoughnessThreshold")) s.rtReflectionRoughnessThreshold = j["rtReflectionRoughnessThreshold"].get<f32>();
    if (j.contains("rtAOEnabled"))                    s.rtAOEnabled                    = j["rtAOEnabled"].get<bool>();
    if (j.contains("rtAORadius"))                     s.rtAORadius                     = j["rtAORadius"].get<f32>();
    if (j.contains("rtAOPower"))                      s.rtAOPower                      = j["rtAOPower"].get<f32>();
    if (j.contains("rtGIEnabled"))                    s.rtGIEnabled                    = j["rtGIEnabled"].get<bool>();
    if (j.contains("rtGIMaxDistance"))                 s.rtGIMaxDistance                 = j["rtGIMaxDistance"].get<f32>();
    if (j.contains("rtGIIntensity"))                  s.rtGIIntensity                  = j["rtGIIntensity"].get<f32>();
    if (j.contains("rtGIBounces"))                    s.rtGIBounces                    = j["rtGIBounces"].get<u32>();
    if (j.contains("rtPathTracerMaxBounces"))         s.rtPathTracerMaxBounces         = j["rtPathTracerMaxBounces"].get<u32>();
    if (j.contains("rtPathTracerTargetSPP"))          s.rtPathTracerTargetSPP          = j["rtPathTracerTargetSPP"].get<u32>();
    if (j.contains("rtDenoiserEnabled"))              s.rtDenoiserEnabled              = j["rtDenoiserEnabled"].get<bool>();
    if (j.contains("rtDenoiserIterations"))           s.rtDenoiserIterations           = j["rtDenoiserIterations"].get<u32>();
    if (j.contains("rtDenoiserTemporalAlpha"))        s.rtDenoiserTemporalAlpha        = j["rtDenoiserTemporalAlpha"].get<f32>();
    if (j.contains("rtShadowStrength"))               s.rtShadowStrength               = j["rtShadowStrength"].get<f32>();
    if (j.contains("rtReflectionStrength"))            s.rtReflectionStrength            = j["rtReflectionStrength"].get<f32>();
    if (j.contains("rtAOStrength"))                   s.rtAOStrength                   = j["rtAOStrength"].get<f32>();
    if (j.contains("rtGIStrength"))                   s.rtGIStrength                   = j["rtGIStrength"].get<f32>();

    return s;
}

} // namespace Renderer
} // namespace Enjin
