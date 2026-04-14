#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/PostProcessing.h"
#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTReflections.h"
#include "Enjin/Renderer/RayTracing/RTAmbientOcclusion.h"
#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/RadianceCache.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/ReSTIR.h"
#include "Enjin/Renderer/RayTracing/RTTemporalReuse.h"
#include "Enjin/Renderer/RayTracing/SurfelRadianceCache.h"
#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"
#include "Enjin/Renderer/RayTracing/OIDNDenoiser.h"
#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#endif
#include <algorithm>
#include <nlohmann/json.hpp>
#include <cmath>

using json = nlohmann::json;

namespace Enjin {
namespace Renderer {

// Tolerant bool deserialization — handles both JSON booleans and numbers
// (needed for backward compat with scenes that used RF() on bool fields)
static bool JB(const json& val) {
    if (val.is_boolean()) return val.get<bool>();
    if (val.is_number()) return val.get<double>() != 0.0;
    return false;
}

// ---------------------------------------------------------------------------
// CaptureFromRuntime — read live rendering state into a config struct
// ---------------------------------------------------------------------------
SceneRenderSettings SceneRenderSettings::CaptureFromRuntime(ECS::RenderSystem* rs, PostProcessSettings* pp) {
    SceneRenderSettings s;

    if (rs) {
#if !ENJIN_RENDERER_WEBGPU
        s.shadowsEnabled       = rs->IsShadowsEnabled();
        s.shadowResolution     = rs->GetShadowResolution();
        s.shadowDistance       = rs->GetShadowDistance();
        s.shadowStrength       = rs->GetShadowStrength();
        s.shadowSoftness       = rs->GetShadowSoftness();
        s.cascadeProgressiveUpdate  = rs->IsCascadeProgressiveUpdate();
        s.cascadeFarUpdateInterval  = rs->GetCascadeFarUpdateInterval();
        s.wireframe            = rs->IsWireframeEnabled();
#endif
        s.backfaceCulling      = rs->IsBackfaceCullingEnabled();
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
        s.texturePageSize           = rs->GetTexturePageSize();
        s.depthSortJitter           = rs->GetDepthSortJitter();
        s.lightRampMode             = rs->GetLightRampMode();
        s.normalQuantizeSteps       = rs->GetNormalQuantizeSteps();
        s.celShadowMode             = rs->GetCelShadowMode();
        s.halfLambert               = rs->IsHalfLambert();

        // Shading Model
        s.shadingModel = rs->GetShadingModel();
        s.fresnelEnabled = rs->IsFresnelEnabled();
        s.energyConservation = rs->IsEnergyConservation();
        s.geometryTerm = rs->IsGeometryTerm();
        s.sphereEnvMapEnabled = rs->IsSphereEnvMapEnabled();
        s.sphereEnvStrength = rs->GetSphereEnvStrength();
        s.posterizeLevels = rs->GetPosterizeLevels();

        // Cel Shading
        s.celShadingEnabled = rs->IsCelShadingEnabled();
        s.celDiffuseBands = rs->GetCelDiffuseBands();
        s.celSpecularCutoff = rs->GetCelSpecularCutoff();

        // Geometry Outlines
        s.geometryOutlinesEnabled = rs->IsGeometryOutlinesEnabled();
        s.geometryOutlineWidth = rs->GetGeometryOutlineWidth();
        s.geometryOutlineColor = rs->GetGeometryOutlineColor();

        // Ray Tracing
#if !ENJIN_RENDERER_WEBGPU
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
            s.rtReflectionSDFFallback = rtReflect->GetConfig().sdfFallback;
            s.rtReflectionSDFMaxDistance = rtReflect->GetConfig().sdfMaxDistance;
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
        if (auto* rc = rs->GetRadianceCache()) {
            s.radianceCacheEnabled = rc->GetConfig().enabled;
            s.radianceCacheTileSize = rc->GetConfig().tileSize;
            s.radianceCacheMaxAge = rc->GetConfig().maxAge;
            s.radianceCacheDepthThreshold = rc->GetConfig().depthThreshold;
            s.radianceCacheNormalThreshold = rc->GetConfig().normalThreshold;
            s.radianceCacheHysteresis = rc->GetConfig().hysteresis;
            s.radianceCacheExcludeDirectional = rc->GetConfig().excludeDirectional;
        }
        if (auto* pathTracer = rs->GetPathTracer()) {
            s.rtPathTracerMaxBounces = pathTracer->GetConfig().maxBounces;
            s.rtPathTracerTargetSPP = pathTracer->GetConfig().targetSPP;
            s.rtPathTracerFireflyClamp = pathTracer->GetConfig().fireflyClampValue;
            s.rtPathTracerNEE = pathTracer->GetConfig().enableNEE;
            s.rtPathTracerMIS = pathTracer->GetConfig().enableMIS;
            s.rtPathTracerRRMinBounce = pathTracer->GetConfig().russianRouletteMinBounce;
            s.rtPathTracerRRMinProb = pathTracer->GetConfig().russianRouletteMinProb;
        }
        if (auto* denoiser = rs->GetSVGFDenoiser()) {
            s.rtDenoiserEnabled = true;
            s.rtDenoiserIterations = denoiser->GetConfig().atrousIterations;
            s.rtDenoiserTemporalAlpha = denoiser->GetConfig().temporalAlpha;
        }
        s.rtDenoiserType = rs->GetDenoiserType();
        if (auto* oidn = rs->GetOIDNDenoiser()) {
            s.rtOIDNQuality = static_cast<u32>(oidn->GetConfig().quality);
        }
        if (auto* restir = rs->GetReSTIR()) {
            s.restirEnabled = restir->GetConfig().enabled;
            s.restirInitialCandidates = restir->GetConfig().initialCandidates;
            s.restirDistanceBias = restir->GetConfig().distanceBias;
            s.restirTemporalReuse = restir->GetConfig().temporalReuse;
            s.restirTemporalMaxHistory = restir->GetConfig().temporalMaxHistory;
            s.restirTemporalDepthThreshold = restir->GetConfig().temporalDepthThreshold;
            s.restirTemporalNormalThreshold = restir->GetConfig().temporalNormalThreshold;
            s.restirSpatialReuse = restir->GetConfig().spatialReuse;
            s.restirSpatialNeighbors = restir->GetConfig().spatialNeighbors;
            s.restirSpatialRadius = restir->GetConfig().spatialRadius;
            s.restirSpatialDepthThreshold = restir->GetConfig().spatialDepthThreshold;
            s.restirSpatialNormalThreshold = restir->GetConfig().spatialNormalThreshold;
        }
        if (auto* tr = rs->GetRTTemporalReuse()) {
            s.rtTemporalReuseEnabled = tr->GetConfig().enabled;
            s.rtTemporalReuseHistoryLength = tr->GetConfig().historyLength;
            s.rtTemporalReuseDisocclusionThreshold = tr->GetConfig().disocclusionThreshold;
            s.rtTemporalReuseNormalThreshold = tr->GetConfig().normalThreshold;
            s.rtTemporalReuseShadows = tr->GetConfig().reuseShadows;
            s.rtTemporalReuseReflections = tr->GetConfig().reuseReflections;
            s.rtTemporalReuseAO = tr->GetConfig().reuseAO;
            s.rtTemporalReuseGI = tr->GetConfig().reuseGI;
        }
        if (auto* sc = rs->GetSurfelRadianceCache()) {
            s.surfelCacheEnabled = sc->GetConfig().enabled;
            s.surfelCacheMaxSurfels = sc->GetConfig().maxSurfels;
            s.surfelCacheRadius = sc->GetConfig().surfelRadius;
            s.surfelCacheUpdateFraction = sc->GetConfig().updateFraction;
            s.surfelCacheMaxAge = sc->GetConfig().maxAge;
            s.surfelCacheExcludeDirectional = sc->GetConfig().excludeDirectional;
            s.surfelCacheCameraRadius = sc->GetConfig().cameraRadius;
            s.surfelCacheBlendWeight = sc->GetConfig().blendWeight;
            s.surfelCacheNormalThreshold = sc->GetConfig().normalThreshold;
            s.surfelCachePlacementInterval = sc->GetConfig().placementInterval;
            s.surfelCacheRaysPerSurfel = sc->GetConfig().raysPerSurfel;
        }
        if (auto* compositor = rs->GetRTCompositor()) {
            s.rtShadowStrength = compositor->GetConfig().shadowStrength;
            s.rtReflectionStrength = compositor->GetConfig().reflectionStrength;
            s.rtAOStrength = compositor->GetConfig().aoStrength;
            s.rtGIStrength = compositor->GetConfig().giStrength;
        }
#endif // !ENJIN_RENDERER_WEBGPU
    }

    if (pp) {
        // HDR output
        s.hdrOutput                    = pp->hdrOutputMode != 0;

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

        // Anti-Aliasing
        s.aaMode                       = pp->aaMode;
        s.aaComparisonEnabled          = pp->aaComparisonEnabled != 0;
        s.aaComparisonModeLeft         = pp->aaComparisonModeLeft;
        s.aaComparisonModeRight        = pp->aaComparisonModeRight;
        s.aaComparisonDivider          = pp->aaComparisonDivider;
        s.fxaaEnabled                  = pp->fxaaEnabled != 0;
        s.fxaaSpanMax                  = pp->fxaaSpanMax;
        s.fxaaReduceMin                = pp->fxaaReduceMin;
        s.fxaaReduceMul                = pp->fxaaReduceMul;
        s.taaSharpness                 = pp->taaSharpness;
        s.taaJitterScale               = pp->taaJitterScale;
        s.taaFeedbackMin               = pp->taaFeedbackMin;
        s.taaFeedbackMax               = pp->taaFeedbackMax;

        // Temporal Upscaling
        s.upscalerType                 = pp->upscalerType;
        s.upscalerQuality              = pp->upscalerQuality;
        s.upscalerSharpness            = pp->upscalerSharpness;

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
        s.crtBloomSigma                = pp->crtBloomSigma;
        s.crtModelPreset               = pp->crtModelPreset;
        s.crtTVL                       = pp->crtTVL;

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
        s.celOutlineCurvatureWeight    = pp->celOutlineCurvatureWeight;
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

        // Screen-Space Effects
        s.godRaysEnabled               = pp->godRaysEnabled != 0;
        s.godRaysIntensity             = pp->godRaysIntensity;
        s.godRaysDecay                 = pp->godRaysDecay;
        s.godRaysDensity               = pp->godRaysDensity;
        s.godRaysSamples               = pp->godRaysSamples;
        s.godRaysWeight                = pp->godRaysWeight;

        s.ssaoEnabled                  = pp->ssaoEnabled != 0;
        s.ssaoRadius                   = pp->ssaoRadius;
        s.ssaoIntensity                = pp->ssaoIntensity;
        s.ssaoBias                     = pp->ssaoBias;
        s.ssaoSamples                  = pp->ssaoSamples;

        s.contactShadowsEnabled        = pp->contactShadowsEnabled != 0;
        s.contactShadowsLength         = pp->contactShadowsLength;
        s.contactShadowsSteps          = pp->contactShadowsSteps;
        s.contactShadowsIntensity      = pp->contactShadowsIntensity;

        s.causticsEnabled              = pp->causticsEnabled != 0;
        s.causticsIntensity            = pp->causticsIntensity;
        s.causticsScale                = pp->causticsScale;
        s.causticsSpeed                = pp->causticsSpeed;
        s.causticsWaterY               = pp->causticsWaterY;

        s.fogShaftsEnabled             = pp->fogShaftsEnabled != 0;
        s.fogShaftsIntensity           = pp->fogShaftsIntensity;
        s.fogShaftsDensity             = pp->fogShaftsDensity;
        s.fogShaftsDecay               = pp->fogShaftsDecay;
        s.fogShaftsSamples             = pp->fogShaftsSamples;
        s.fogShaftsMaxDistance          = pp->fogShaftsMaxDistance;
    }

    return s;
}

// ---------------------------------------------------------------------------
// ApplyToRuntime — write config values to live systems
// Preserves runtime-only fields: time, screenWidth/Height, colorblindMode/Strength
// ---------------------------------------------------------------------------
void SceneRenderSettings::ApplyToRuntime(ECS::RenderSystem* rs, PostProcessSettings* pp) const {
    if (rs) {
#if !ENJIN_RENDERER_WEBGPU
        rs->SetShadowsEnabled(shadowsEnabled);
        rs->SetShadowResolution(shadowResolution);
        rs->SetShadowDistance(shadowDistance);
        rs->SetShadowStrength(shadowStrength);
        rs->SetShadowSoftness(shadowSoftness);
        rs->SetCascadeProgressiveUpdate(cascadeProgressiveUpdate);
        rs->SetCascadeFarUpdateInterval(cascadeFarUpdateInterval);
        rs->SetWireframeEnabled(wireframe);
#endif
        rs->SetBackfaceCullingEnabled(backfaceCulling);
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
        rs->SetTexturePageSize(texturePageSize);
        rs->SetDepthSortJitter(depthSortJitter);
        rs->SetLightRampMode(lightRampMode);
        rs->SetNormalQuantizeSteps(normalQuantizeSteps);
        rs->SetCelShadowMode(celShadowMode);
        rs->SetHalfLambert(halfLambert);

        // Temporal Upscaling
#if !ENJIN_RENDERER_WEBGPU
        rs->SetUpscalerSharpness(upscalerSharpness);
        rs->SetUpscalerQuality(upscalerQuality);
        rs->SetUpscalerType(upscalerType);  // Must be last — triggers init with quality/sharpness
#endif

        // Shading Model
        rs->SetShadingModel(shadingModel);
        rs->SetFresnelEnabled(fresnelEnabled);
        rs->SetEnergyConservation(energyConservation);
        rs->SetGeometryTerm(geometryTerm);
        rs->SetSphereEnvMapEnabled(sphereEnvMapEnabled);
        rs->SetSphereEnvStrength(sphereEnvStrength);
        rs->SetPosterizeLevels(posterizeLevels);

        // Cel Shading
        rs->SetCelShadingEnabled(celShadingEnabled);
        rs->SetCelDiffuseBands(celDiffuseBands);
        rs->SetCelSpecularCutoff(celSpecularCutoff);

        // Geometry Outlines
        rs->SetGeometryOutlinesEnabled(geometryOutlinesEnabled);
        rs->SetGeometryOutlineWidth(geometryOutlineWidth);
        rs->SetGeometryOutlineColor(geometryOutlineColor);

        // Ray Tracing
#if !ENJIN_RENDERER_WEBGPU
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
            rtReflect->GetConfig().sdfFallback = rtReflectionSDFFallback;
            rtReflect->GetConfig().sdfMaxDistance = rtReflectionSDFMaxDistance;
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
        if (auto* rc = rs->GetRadianceCache()) {
            rc->GetConfig().enabled = radianceCacheEnabled;
            rc->GetConfig().tileSize = radianceCacheTileSize;
            rc->GetConfig().maxAge = radianceCacheMaxAge;
            rc->GetConfig().depthThreshold = radianceCacheDepthThreshold;
            rc->GetConfig().normalThreshold = radianceCacheNormalThreshold;
            rc->GetConfig().hysteresis = radianceCacheHysteresis;
            rc->GetConfig().excludeDirectional = radianceCacheExcludeDirectional;
        }
        if (auto* pathTracer = rs->GetPathTracer()) {
            pathTracer->GetConfig().maxBounces = rtPathTracerMaxBounces;
            pathTracer->GetConfig().targetSPP = rtPathTracerTargetSPP;
            pathTracer->GetConfig().fireflyClampValue = rtPathTracerFireflyClamp;
            pathTracer->GetConfig().enableNEE = rtPathTracerNEE;
            pathTracer->GetConfig().enableMIS = rtPathTracerMIS;
            pathTracer->GetConfig().russianRouletteMinBounce = rtPathTracerRRMinBounce;
            pathTracer->GetConfig().russianRouletteMinProb = rtPathTracerRRMinProb;
        }
        if (auto* denoiser = rs->GetSVGFDenoiser()) {
            denoiser->GetConfig().atrousIterations = rtDenoiserIterations;
            denoiser->GetConfig().temporalAlpha = rtDenoiserTemporalAlpha;
        }
        rs->SetDenoiserType(rtDenoiserType);
        if (auto* oidn = rs->GetOIDNDenoiser()) {
            oidn->GetConfig().quality = static_cast<Renderer::OIDNQuality>(std::min(rtOIDNQuality, 2u));
        }
        if (auto* restir = rs->GetReSTIR()) {
            restir->GetConfig().enabled = restirEnabled;
            restir->GetConfig().initialCandidates = restirInitialCandidates;
            restir->GetConfig().distanceBias = restirDistanceBias;
            restir->GetConfig().temporalReuse = restirTemporalReuse;
            restir->GetConfig().temporalMaxHistory = restirTemporalMaxHistory;
            restir->GetConfig().temporalDepthThreshold = restirTemporalDepthThreshold;
            restir->GetConfig().temporalNormalThreshold = restirTemporalNormalThreshold;
            restir->GetConfig().spatialReuse = restirSpatialReuse;
            restir->GetConfig().spatialNeighbors = restirSpatialNeighbors;
            restir->GetConfig().spatialRadius = restirSpatialRadius;
            restir->GetConfig().spatialDepthThreshold = restirSpatialDepthThreshold;
            restir->GetConfig().spatialNormalThreshold = restirSpatialNormalThreshold;
        }
        if (auto* tr = rs->GetRTTemporalReuse()) {
            tr->GetConfig().enabled = rtTemporalReuseEnabled;
            tr->GetConfig().historyLength = rtTemporalReuseHistoryLength;
            tr->GetConfig().disocclusionThreshold = rtTemporalReuseDisocclusionThreshold;
            tr->GetConfig().normalThreshold = rtTemporalReuseNormalThreshold;
            tr->GetConfig().reuseShadows = rtTemporalReuseShadows;
            tr->GetConfig().reuseReflections = rtTemporalReuseReflections;
            tr->GetConfig().reuseAO = rtTemporalReuseAO;
            tr->GetConfig().reuseGI = rtTemporalReuseGI;
        }
        if (auto* sc = rs->GetSurfelRadianceCache()) {
            sc->GetConfig().enabled = surfelCacheEnabled;
            sc->GetConfig().maxSurfels = surfelCacheMaxSurfels;
            sc->GetConfig().surfelRadius = surfelCacheRadius;
            sc->GetConfig().updateFraction = surfelCacheUpdateFraction;
            sc->GetConfig().maxAge = surfelCacheMaxAge;
            sc->GetConfig().excludeDirectional = surfelCacheExcludeDirectional;
            sc->GetConfig().cameraRadius = surfelCacheCameraRadius;
            sc->GetConfig().blendWeight = surfelCacheBlendWeight;
            sc->GetConfig().normalThreshold = surfelCacheNormalThreshold;
            sc->GetConfig().placementInterval = surfelCachePlacementInterval;
            sc->GetConfig().raysPerSurfel = surfelCacheRaysPerSurfel;
        }
        if (auto* compositor = rs->GetRTCompositor()) {
            compositor->GetConfig().shadowStrength = rtShadowStrength;
            compositor->GetConfig().reflectionStrength = rtReflectionStrength;
            compositor->GetConfig().aoStrength = rtAOStrength;
            compositor->GetConfig().giStrength = rtGIStrength;
        }
#endif // !ENJIN_RENDERER_WEBGPU
    }

    if (pp) {
        // Save runtime-only fields
        f32 savedTime = pp->time;
        u32 savedScreenW = pp->screenWidth;
        u32 savedScreenH = pp->screenHeight;
        u32 savedColorblindMode = pp->colorblindMode;
        f32 savedColorblindStrength = pp->colorblindStrength;

        // HDR output — mode is set by swapchain, we just store the intent
        // (actual mode depends on display capabilities; set via swapchain toggle)

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

        // Anti-Aliasing
        pp->aaMode                       = aaMode;
        pp->aaComparisonEnabled          = aaComparisonEnabled ? 1 : 0;
        pp->aaComparisonModeLeft         = aaComparisonModeLeft;
        pp->aaComparisonModeRight        = aaComparisonModeRight;
        pp->aaComparisonDivider          = aaComparisonDivider;
        pp->fxaaEnabled                  = (aaMode == 1) ? 1 : 0;
        pp->fxaaSpanMax                  = fxaaSpanMax;
        pp->fxaaReduceMin                = fxaaReduceMin;
        pp->fxaaReduceMul                = fxaaReduceMul;
        pp->taaSharpness                 = taaSharpness;
        pp->taaJitterScale               = taaJitterScale;
        pp->taaFeedbackMin               = taaFeedbackMin;
        pp->taaFeedbackMax               = taaFeedbackMax;

        // Temporal Upscaling
        pp->upscalerType                 = upscalerType;
        pp->upscalerQuality              = upscalerQuality;
        pp->upscalerSharpness            = upscalerSharpness;

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
        pp->crtBloomSigma                = crtBloomSigma;
        pp->crtModelPreset               = crtModelPreset;
        pp->crtTVL                       = crtTVL;

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
        pp->celOutlineCurvatureWeight    = celOutlineCurvatureWeight;
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

        // Screen-Space Effects
        pp->godRaysEnabled               = godRaysEnabled ? 1 : 0;
        pp->godRaysIntensity             = godRaysIntensity;
        pp->godRaysDecay                 = godRaysDecay;
        pp->godRaysDensity               = godRaysDensity;
        pp->godRaysSamples               = godRaysSamples;
        pp->godRaysWeight                = godRaysWeight;

        pp->ssaoEnabled                  = ssaoEnabled ? 1 : 0;
        pp->ssaoRadius                   = ssaoRadius;
        pp->ssaoIntensity                = ssaoIntensity;
        pp->ssaoBias                     = ssaoBias;
        pp->ssaoSamples                  = ssaoSamples;

        pp->contactShadowsEnabled        = contactShadowsEnabled ? 1 : 0;
        pp->contactShadowsLength         = contactShadowsLength;
        pp->contactShadowsSteps          = contactShadowsSteps;
        pp->contactShadowsIntensity      = contactShadowsIntensity;

        pp->causticsEnabled              = causticsEnabled ? 1 : 0;
        pp->causticsIntensity            = causticsIntensity;
        pp->causticsScale                = causticsScale;
        pp->causticsSpeed                = causticsSpeed;
        pp->causticsWaterY               = causticsWaterY;

        pp->fogShaftsEnabled             = fogShaftsEnabled ? 1 : 0;
        pp->fogShaftsIntensity           = fogShaftsIntensity;
        pp->fogShaftsDensity             = fogShaftsDensity;
        pp->fogShaftsDecay               = fogShaftsDecay;
        pp->fogShaftsSamples             = fogShaftsSamples;
        pp->fogShaftsMaxDistance          = fogShaftsMaxDistance;

        // Restore runtime-only fields
        pp->time = savedTime;
        pp->screenWidth = savedScreenW;
        pp->screenHeight = savedScreenH;
        pp->colorblindMode = savedColorblindMode;
        pp->colorblindStrength = savedColorblindStrength;
    }
}

// ---------------------------------------------------------------------------
// Art Style Presets
// ---------------------------------------------------------------------------

void ApplyArtStylePreset(SceneRenderSettings& s, u32 presetIndex) {
    s.artStylePreset = presetIndex;

    switch (presetIndex) {
    case 0: // Realistic PBR
        s.shadingModel = 1;             // PBR (GGX)
        s.fresnelEnabled = true;
        s.energyConservation = true;
        s.geometryTerm = true;
        s.halfLambert = false;
        s.celShadingEnabled = false;
        s.celOutlineEnabled = false;
        s.geometryOutlinesEnabled = false;
        s.globalFlatShading = false;
        s.globalAffineTexturing = false;
        s.globalVertexSnapping = false;
        s.globalGouraudOnly = false;
        s.globalUVQuantize = false;
        s.globalStippleTransparency = false;
        s.sphereEnvMapEnabled = false;
        s.posterizeLevels = 0.0f;
        s.lightRampMode = 0.0f;
        s.celShadowMode = 0.0f;
        s.resDownscaleEnabled = false;
        s.paletteEnabled = false;
        s.colorQuantEnabled = false;
        s.ditherEnabled = false;
        s.stippleEnabled = false;
        break;

    case 1: // Classic Blinn-Phong
        s.shadingModel = 0;             // Blinn-Phong
        s.fresnelEnabled = false;
        s.energyConservation = false;
        s.geometryTerm = false;
        s.halfLambert = false;
        s.celShadingEnabled = false;
        s.celOutlineEnabled = false;
        s.geometryOutlinesEnabled = false;
        s.globalFlatShading = false;
        s.globalAffineTexturing = false;
        s.globalVertexSnapping = false;
        s.globalGouraudOnly = false;
        s.globalUVQuantize = false;
        s.globalStippleTransparency = false;
        s.sphereEnvMapEnabled = false;
        s.posterizeLevels = 0.0f;
        s.lightRampMode = 0.0f;
        s.celShadowMode = 0.0f;
        s.resDownscaleEnabled = false;
        s.paletteEnabled = false;
        s.colorQuantEnabled = false;
        s.ditherEnabled = false;
        s.stippleEnabled = false;
        break;

    case 2: // Hand-Painted
        s.shadingModel = 0;             // Blinn-Phong
        s.fresnelEnabled = false;
        s.energyConservation = false;
        s.geometryTerm = false;
        s.halfLambert = true;
        s.celShadingEnabled = false;
        s.celOutlineEnabled = false;
        s.geometryOutlinesEnabled = false;
        s.globalFlatShading = false;
        s.globalAffineTexturing = false;
        s.globalVertexSnapping = false;
        s.globalGouraudOnly = false;
        s.globalUVQuantize = false;
        s.globalStippleTransparency = false;
        s.sphereEnvMapEnabled = false;
        s.posterizeLevels = 0.0f;
        s.lightRampMode = 2.0f;         // Warm shadow ramp
        s.celShadowMode = 0.0f;
        s.resDownscaleEnabled = false;
        s.paletteEnabled = false;
        s.colorQuantEnabled = false;
        s.ditherEnabled = false;
        s.stippleEnabled = false;
        break;

    case 3: // Toon/Anime
        s.shadingModel = 0;             // Blinn-Phong
        s.fresnelEnabled = false;
        s.energyConservation = false;
        s.geometryTerm = false;
        s.halfLambert = false;
        s.celShadingEnabled = true;
        s.celDiffuseBands = 4.0f;
        s.celSpecularCutoff = 0.5f;
        s.celShadowMode = 1.0f;         // Purple shadows
        s.celOutlineEnabled = false;
        s.geometryOutlinesEnabled = true;
        s.geometryOutlineWidth = 0.02f;
        s.geometryOutlineColor = Math::Vector3(0.0f, 0.0f, 0.0f);
        s.globalFlatShading = false;
        s.globalAffineTexturing = false;
        s.globalVertexSnapping = false;
        s.globalGouraudOnly = false;
        s.globalUVQuantize = false;
        s.globalStippleTransparency = false;
        s.sphereEnvMapEnabled = false;
        s.posterizeLevels = 0.0f;
        s.lightRampMode = 4.0f;         // Anime ramp
        s.resDownscaleEnabled = false;
        s.paletteEnabled = false;
        s.colorQuantEnabled = false;
        s.ditherEnabled = false;
        s.stippleEnabled = false;
        break;

    case 4: // Low-Poly Retro
        s.shadingModel = 0;             // Blinn-Phong
        s.fresnelEnabled = false;
        s.energyConservation = false;
        s.geometryTerm = false;
        s.halfLambert = false;
        s.celShadingEnabled = false;
        s.celOutlineEnabled = false;
        s.geometryOutlinesEnabled = false;
        s.globalFlatShading = true;
        s.globalAffineTexturing = true;
        s.globalVertexSnapping = true;
        s.globalVertexSnapResolution = 160;
        s.globalGouraudOnly = false;
        s.globalUVQuantize = false;
        s.globalStippleTransparency = false;
        s.sphereEnvMapEnabled = false;
        s.posterizeLevels = 16.0f;
        s.lightRampMode = 0.0f;
        s.celShadowMode = 0.0f;
        s.resDownscaleEnabled = false;
        s.paletteEnabled = false;
        s.colorQuantEnabled = false;
        s.ditherEnabled = false;
        s.stippleEnabled = false;
        break;

    case 5: // Pixel Art
        s.shadingModel = 0;             // Blinn-Phong
        s.fresnelEnabled = false;
        s.energyConservation = false;
        s.geometryTerm = false;
        s.halfLambert = false;
        s.celShadingEnabled = false;
        s.celOutlineEnabled = false;
        s.geometryOutlinesEnabled = false;
        s.globalFlatShading = false;
        s.globalAffineTexturing = false;
        s.globalVertexSnapping = false;
        s.globalGouraudOnly = false;
        s.globalUVQuantize = false;
        s.globalStippleTransparency = false;
        s.sphereEnvMapEnabled = false;
        s.posterizeLevels = 0.0f;
        s.lightRampMode = 0.0f;
        s.celShadowMode = 0.0f;
        s.resDownscaleEnabled = true;
        s.internalWidth = 320;
        s.internalHeight = 240;
        s.usePointFiltering = true;
        s.paletteEnabled = true;
        s.paletteColors = 16;
        s.colorQuantEnabled = false;
        s.ditherEnabled = true;
        s.ditherPattern = 0;            // Bayer 2x2
        s.ditherStrength = 1.0f;
        s.stippleEnabled = false;
        break;

    case 6: // NPR Sketch
        s.shadingModel = 0;             // Blinn-Phong
        s.fresnelEnabled = false;
        s.energyConservation = false;
        s.geometryTerm = false;
        s.halfLambert = false;
        s.celShadingEnabled = true;
        s.celDiffuseBands = 2.0f;
        s.celSpecularCutoff = 0.8f;
        s.celShadowMode = 0.0f;
        s.celOutlineEnabled = true;
        s.celOutlineThickness = 2.0f;
        s.celOutlineThreshold = 0.1f;
        s.celOutlineCurvatureWeight = 1.0f;   // Pen/ink style: thicker on curved edges
        s.celOutlineColor = Math::Vector3(0.0f, 0.0f, 0.0f);
        s.geometryOutlinesEnabled = false;
        s.globalFlatShading = false;
        s.globalAffineTexturing = false;
        s.globalVertexSnapping = false;
        s.globalGouraudOnly = false;
        s.globalUVQuantize = false;
        s.globalStippleTransparency = false;
        s.sphereEnvMapEnabled = false;
        s.posterizeLevels = 0.0f;
        s.lightRampMode = 0.0f;
        s.stippleEnabled = true;
        s.stipplePatternMask = 16;      // Crosshatch pattern (bit 4)
        s.stippleColorMode = 0;         // Monochrome
        s.stippleScale = 1.0f;
        s.stippleDensity = 0.5f;
        s.stippleStrength = 0.7f;
        s.stippleFgColor = Math::Vector3(0.0f, 0.0f, 0.0f);
        s.stippleBgColor = Math::Vector3(1.0f, 1.0f, 0.95f);
        s.resDownscaleEnabled = false;
        s.paletteEnabled = false;
        s.colorQuantEnabled = false;
        s.ditherEnabled = false;
        break;

    default:
        break;
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
    j["artStylePreset"]    = s.artStylePreset;

    // RenderSystem
    j["shadowsEnabled"]    = s.shadowsEnabled;
    j["shadowResolution"]  = s.shadowResolution;
    j["shadowDistance"]    = RF(s.shadowDistance);
    j["shadowStrength"]    = RF(s.shadowStrength);
    j["shadowSoftness"]    = RF(s.shadowSoftness);
    j["cascadeProgressiveUpdate"] = s.cascadeProgressiveUpdate;
    j["cascadeFarUpdateInterval"] = s.cascadeFarUpdateInterval;
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

    // HDR output
    j["hdrOutput"]         = s.hdrOutput;

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

    // Anti-Aliasing
    j["aaMode"]            = s.aaMode;
    j["aaComparisonEnabled"]    = s.aaComparisonEnabled;
    j["aaComparisonModeLeft"]   = s.aaComparisonModeLeft;
    j["aaComparisonModeRight"]  = s.aaComparisonModeRight;
    j["aaComparisonDivider"]    = RF(s.aaComparisonDivider);
    j["fxaaEnabled"]       = s.fxaaEnabled;
    j["fxaaSpanMax"]       = RF(s.fxaaSpanMax);
    j["fxaaReduceMin"]     = RF(s.fxaaReduceMin);
    j["fxaaReduceMul"]     = RF(s.fxaaReduceMul);
    j["taaSharpness"]      = RF(s.taaSharpness);
    j["taaJitterScale"]    = RF(s.taaJitterScale);
    j["taaFeedbackMin"]    = RF(s.taaFeedbackMin);
    j["taaFeedbackMax"]    = RF(s.taaFeedbackMax);

    // Temporal Upscaling
    j["upscalerType"]      = s.upscalerType;
    j["upscalerQuality"]   = s.upscalerQuality;
    j["upscalerSharpness"] = RF(s.upscalerSharpness);

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
    j["crtBloomSigma"]       = RF(s.crtBloomSigma);
    j["crtModelPreset"]      = RF(s.crtModelPreset);
    j["crtTVL"]              = RF(s.crtTVL);

    // VHS
    j["vhsEnabled"]            = s.vhsEnabled;
    j["vhsTrackingIntensity"]  = RF(s.vhsTrackingIntensity);
    j["vhsTrackingSpeed"]      = RF(s.vhsTrackingSpeed);
    j["vhsWobbleIntensity"]    = RF(s.vhsWobbleIntensity);
    j["vhsWobbleSpeed"]        = RF(s.vhsWobbleSpeed);
    j["vhsColorBleed"]         = RF(s.vhsColorBleed);
    j["vhsNoiseIntensity"]     = RF(s.vhsNoiseIntensity);
    j["vhsBlueShift"]          = RF(s.vhsBlueShift);
    j["vhsScreenTear"]         = s.vhsScreenTear;
    j["vhsTearOffset"]         = RF(s.vhsTearOffset);
    j["vhsInterlacing"]        = s.vhsInterlacing;

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
    j["texturePageSize"]            = RF(s.texturePageSize);
    j["depthSortJitter"]            = RF(s.depthSortJitter);
    j["lightRampMode"]              = RF(s.lightRampMode);
    j["normalQuantizeSteps"]        = RF(s.normalQuantizeSteps);
    j["celShadowMode"]              = RF(s.celShadowMode);
    j["halfLambert"]                = s.halfLambert;

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

    // Shading Model
    j["shadingModel"]          = s.shadingModel;
    j["fresnelEnabled"]        = s.fresnelEnabled;
    j["energyConservation"]    = s.energyConservation;
    j["geometryTerm"]          = s.geometryTerm;

    // Dreamcast-style effects
    j["sphereEnvMapEnabled"]   = s.sphereEnvMapEnabled;
    j["sphereEnvStrength"]     = RF(s.sphereEnvStrength);
    j["posterizeLevels"]       = RF(s.posterizeLevels);

    // Cel Shading
    j["celShadingEnabled"]     = s.celShadingEnabled;
    j["celDiffuseBands"]       = s.celDiffuseBands;
    j["celSpecularCutoff"]     = RF(s.celSpecularCutoff);
    j["celOutlineEnabled"]     = s.celOutlineEnabled;
    j["celOutlineThickness"]   = RF(s.celOutlineThickness);
    j["celOutlineThreshold"]   = RF(s.celOutlineThreshold);
    j["celOutlineCurvatureWeight"] = RF(s.celOutlineCurvatureWeight);
    j["celOutlineColor"]       = SerializeVec3(s.celOutlineColor);
    j["geometryOutlinesEnabled"] = s.geometryOutlinesEnabled;
    j["geometryOutlineWidth"]    = RF(s.geometryOutlineWidth);
    j["geometryOutlineColor"]    = SerializeVec3(s.geometryOutlineColor);

    // Screen-Space Effects
    j["godRaysEnabled"]          = s.godRaysEnabled;
    j["godRaysIntensity"]        = RF(s.godRaysIntensity);
    j["godRaysDecay"]            = RF(s.godRaysDecay);
    j["godRaysDensity"]          = RF(s.godRaysDensity);
    j["godRaysSamples"]          = s.godRaysSamples;
    j["godRaysWeight"]           = RF(s.godRaysWeight);
    j["ssaoEnabled"]             = s.ssaoEnabled;
    j["ssaoRadius"]              = RF(s.ssaoRadius);
    j["ssaoIntensity"]           = RF(s.ssaoIntensity);
    j["ssaoBias"]                = RF(s.ssaoBias);
    j["ssaoSamples"]             = s.ssaoSamples;
    j["contactShadowsEnabled"]   = s.contactShadowsEnabled;
    j["contactShadowsLength"]    = RF(s.contactShadowsLength);
    j["contactShadowsSteps"]     = s.contactShadowsSteps;
    j["contactShadowsIntensity"] = RF(s.contactShadowsIntensity);
    j["causticsEnabled"]         = s.causticsEnabled;
    j["causticsIntensity"]       = RF(s.causticsIntensity);
    j["causticsScale"]           = RF(s.causticsScale);
    j["causticsSpeed"]           = RF(s.causticsSpeed);
    j["causticsWaterY"]          = RF(s.causticsWaterY);
    j["fogShaftsEnabled"]        = s.fogShaftsEnabled;
    j["fogShaftsIntensity"]      = RF(s.fogShaftsIntensity);
    j["fogShaftsDensity"]        = RF(s.fogShaftsDensity);
    j["fogShaftsDecay"]          = RF(s.fogShaftsDecay);
    j["fogShaftsSamples"]        = s.fogShaftsSamples;
    j["fogShaftsMaxDistance"]     = RF(s.fogShaftsMaxDistance);

    // Ray Tracing
    j["rtEnabled"]                    = s.rtEnabled;
    j["rtMode"]                       = s.rtMode;
    j["rtShadowsEnabled"]             = s.rtShadowsEnabled;
    j["rtShadowMaxDistance"]           = RF(s.rtShadowMaxDistance);
    j["rtShadowRadius"]               = RF(s.rtShadowRadius);
    j["rtReflectionsEnabled"]          = s.rtReflectionsEnabled;
    j["rtReflectionMaxDistance"]       = RF(s.rtReflectionMaxDistance);
    j["rtReflectionRoughnessThreshold"] = RF(s.rtReflectionRoughnessThreshold);
    j["rtReflectionSDFFallback"]       = s.rtReflectionSDFFallback;
    j["rtReflectionSDFMaxDistance"]     = RF(s.rtReflectionSDFMaxDistance);
    j["rtAOEnabled"]                   = s.rtAOEnabled;
    j["rtAORadius"]                    = RF(s.rtAORadius);
    j["rtAOPower"]                     = RF(s.rtAOPower);
    j["rtGIEnabled"]                   = s.rtGIEnabled;
    j["rtGIMaxDistance"]               = RF(s.rtGIMaxDistance);
    j["rtGIIntensity"]                 = RF(s.rtGIIntensity);
    j["rtGIBounces"]                   = s.rtGIBounces;
    j["radianceCacheEnabled"]          = s.radianceCacheEnabled;
    j["radianceCacheTileSize"]         = s.radianceCacheTileSize;
    j["radianceCacheMaxAge"]           = RF(s.radianceCacheMaxAge);
    j["radianceCacheDepthThreshold"]   = RF(s.radianceCacheDepthThreshold);
    j["radianceCacheNormalThreshold"]  = RF(s.radianceCacheNormalThreshold);
    j["radianceCacheHysteresis"]       = RF(s.radianceCacheHysteresis);
    j["radianceCacheExcludeDirectional"] = s.radianceCacheExcludeDirectional;
    j["rtPathTracerMaxBounces"]        = s.rtPathTracerMaxBounces;
    j["rtPathTracerTargetSPP"]         = s.rtPathTracerTargetSPP;
    j["rtPathTracerFireflyClamp"]      = RF(s.rtPathTracerFireflyClamp);
    j["rtPathTracerNEE"]               = s.rtPathTracerNEE;
    j["rtPathTracerMIS"]               = s.rtPathTracerMIS;
    j["rtPathTracerRRMinBounce"]       = RF(s.rtPathTracerRRMinBounce);
    j["rtPathTracerRRMinProb"]         = RF(s.rtPathTracerRRMinProb);
    j["rtSimplifiedMaterials"]         = s.rtSimplifiedMaterials;
    j["rtSimplifyAfterBounce"]         = s.rtSimplifyAfterBounce;
    j["rtDenoiserEnabled"]             = s.rtDenoiserEnabled;
    j["rtDenoiserType"]                = s.rtDenoiserType;
    j["rtDenoiserIterations"]          = s.rtDenoiserIterations;
    j["rtDenoiserTemporalAlpha"]       = RF(s.rtDenoiserTemporalAlpha);
    j["rtOIDNQuality"]                 = s.rtOIDNQuality;
    j["restirEnabled"]                 = s.restirEnabled;
    j["restirInitialCandidates"]       = s.restirInitialCandidates;
    j["restirDistanceBias"]            = RF(s.restirDistanceBias);
    j["restirTemporalReuse"]           = s.restirTemporalReuse;
    j["restirTemporalMaxHistory"]      = s.restirTemporalMaxHistory;
    j["restirTemporalDepthThreshold"]  = RF(s.restirTemporalDepthThreshold);
    j["restirTemporalNormalThreshold"] = RF(s.restirTemporalNormalThreshold);
    j["restirSpatialReuse"]            = s.restirSpatialReuse;
    j["restirSpatialNeighbors"]        = s.restirSpatialNeighbors;
    j["restirSpatialRadius"]           = RF(s.restirSpatialRadius);
    j["restirSpatialDepthThreshold"]   = RF(s.restirSpatialDepthThreshold);
    j["restirSpatialNormalThreshold"]  = RF(s.restirSpatialNormalThreshold);
    // RT Temporal Reuse
    j["rtTemporalReuseEnabled"]                = s.rtTemporalReuseEnabled;
    j["rtTemporalReuseHistoryLength"]          = RF(s.rtTemporalReuseHistoryLength);
    j["rtTemporalReuseDisocclusionThreshold"]  = RF(s.rtTemporalReuseDisocclusionThreshold);
    j["rtTemporalReuseNormalThreshold"]        = RF(s.rtTemporalReuseNormalThreshold);
    j["rtTemporalReuseShadows"]                = s.rtTemporalReuseShadows;
    j["rtTemporalReuseReflections"]            = s.rtTemporalReuseReflections;
    j["rtTemporalReuseAO"]                     = s.rtTemporalReuseAO;
    j["rtTemporalReuseGI"]                     = s.rtTemporalReuseGI;

    // Surfel Radiance Cache
    j["surfelCacheEnabled"]              = s.surfelCacheEnabled;
    j["surfelCacheMaxSurfels"]           = s.surfelCacheMaxSurfels;
    j["surfelCacheRadius"]               = RF(s.surfelCacheRadius);
    j["surfelCacheUpdateFraction"]       = RF(s.surfelCacheUpdateFraction);
    j["surfelCacheMaxAge"]               = RF(s.surfelCacheMaxAge);
    j["surfelCacheExcludeDirectional"]   = s.surfelCacheExcludeDirectional;
    j["surfelCacheCameraRadius"]         = RF(s.surfelCacheCameraRadius);
    j["surfelCacheBlendWeight"]          = RF(s.surfelCacheBlendWeight);
    j["surfelCacheNormalThreshold"]      = RF(s.surfelCacheNormalThreshold);
    j["surfelCachePlacementInterval"]    = s.surfelCachePlacementInterval;
    j["surfelCacheRaysPerSurfel"]        = s.surfelCacheRaysPerSurfel;

    j["rtShadowStrength"]              = RF(s.rtShadowStrength);
    j["rtReflectionStrength"]          = RF(s.rtReflectionStrength);
    j["rtAOStrength"]                  = RF(s.rtAOStrength);
    j["rtGIStrength"]                  = RF(s.rtGIStrength);

    return j;
}

SceneRenderSettings DeserializeRenderSettings(const json& j) {
    SceneRenderSettings s;

    if (j.contains("useProjectDefaults")) s.useProjectDefaults = JB(j["useProjectDefaults"]);
    if (j.contains("artStylePreset"))    s.artStylePreset    = j["artStylePreset"].get<u32>();

    // RenderSystem
    if (j.contains("shadowsEnabled"))    s.shadowsEnabled    = JB(j["shadowsEnabled"]);
    if (j.contains("shadowResolution")) s.shadowResolution  = j["shadowResolution"].get<u32>();
    if (j.contains("shadowDistance"))   s.shadowDistance     = j["shadowDistance"].get<f32>();
    if (j.contains("shadowStrength"))   s.shadowStrength     = j["shadowStrength"].get<f32>();
    if (j.contains("shadowSoftness"))   s.shadowSoftness     = j["shadowSoftness"].get<f32>();
    if (j.contains("cascadeProgressiveUpdate")) s.cascadeProgressiveUpdate = JB(j["cascadeProgressiveUpdate"]);
    if (j.contains("cascadeFarUpdateInterval")) s.cascadeFarUpdateInterval = std::clamp(j["cascadeFarUpdateInterval"].get<u32>(), 2u, 8u);
    if (j.contains("backfaceCulling"))   s.backfaceCulling   = JB(j["backfaceCulling"]);
    if (j.contains("wireframe"))         s.wireframe         = JB(j["wireframe"]);
    if (j.contains("ambientIntensity"))  s.ambientIntensity  = j["ambientIntensity"].get<f32>();
    if (j.contains("ambientColor"))      s.ambientColor      = DeserializeVec3(j["ambientColor"], s.ambientColor);
    if (j.contains("fogDensity"))        s.fogDensity        = j["fogDensity"].get<f32>();
    if (j.contains("fogStart"))          s.fogStart          = j["fogStart"].get<f32>();
    if (j.contains("fogEnd"))            s.fogEnd            = j["fogEnd"].get<f32>();
    if (j.contains("fogHeightFalloff"))  s.fogHeightFalloff  = j["fogHeightFalloff"].get<f32>();
    if (j.contains("fogColor"))          s.fogColor          = DeserializeVec3(j["fogColor"], s.fogColor);
    if (j.contains("snowIntensity"))     s.snowIntensity     = j["snowIntensity"].get<f32>();
    if (j.contains("worldCurvature"))    s.worldCurvature    = j["worldCurvature"].get<f32>();
    if (j.contains("rainActive"))        s.rainActive        = JB(j["rainActive"]);

    // HDR output
    if (j.contains("hdrOutput"))         s.hdrOutput         = JB(j["hdrOutput"]);

    // Tone mapping
    if (j.contains("toneMappingMode"))   s.toneMappingMode   = j["toneMappingMode"].get<u32>();
    if (j.contains("exposure"))          s.exposure          = j["exposure"].get<f32>();
    if (j.contains("gamma"))             s.gamma             = j["gamma"].get<f32>();
    if (j.contains("whitePoint"))        s.whitePoint        = j["whitePoint"].get<f32>();

    // Bloom
    if (j.contains("bloomEnabled"))      s.bloomEnabled      = JB(j["bloomEnabled"]);
    if (j.contains("bloomThreshold"))    s.bloomThreshold    = j["bloomThreshold"].get<f32>();
    if (j.contains("bloomIntensity"))    s.bloomIntensity    = j["bloomIntensity"].get<f32>();
    if (j.contains("bloomRadius"))       s.bloomRadius       = j["bloomRadius"].get<f32>();

    // Vignette
    if (j.contains("vignetteEnabled"))     s.vignetteEnabled     = JB(j["vignetteEnabled"]);
    if (j.contains("vignetteIntensity"))   s.vignetteIntensity   = j["vignetteIntensity"].get<f32>();
    if (j.contains("vignetteSmoothness"))  s.vignetteSmoothness  = j["vignetteSmoothness"].get<f32>();

    // Chromatic aberration
    if (j.contains("chromaticAberrationEnabled"))   s.chromaticAberrationEnabled   = JB(j["chromaticAberrationEnabled"]);
    if (j.contains("chromaticAberrationIntensity")) s.chromaticAberrationIntensity = j["chromaticAberrationIntensity"].get<f32>();

    // Color grading
    if (j.contains("colorFilter"))       s.colorFilter       = DeserializeVec3(j["colorFilter"], s.colorFilter);
    if (j.contains("saturation"))        s.saturation        = j["saturation"].get<f32>();
    if (j.contains("contrast"))          s.contrast          = j["contrast"].get<f32>();
    if (j.contains("brightness"))        s.brightness        = j["brightness"].get<f32>();

    // Film grain
    if (j.contains("filmGrainEnabled"))    s.filmGrainEnabled    = JB(j["filmGrainEnabled"]);
    if (j.contains("filmGrainIntensity"))  s.filmGrainIntensity  = j["filmGrainIntensity"].get<f32>();

    // Anti-Aliasing
    if (j.contains("aaMode"))                 s.aaMode                 = j["aaMode"].get<u32>();
    if (j.contains("aaComparisonEnabled"))    s.aaComparisonEnabled    = JB(j["aaComparisonEnabled"]);
    if (j.contains("aaComparisonModeLeft"))   s.aaComparisonModeLeft   = j["aaComparisonModeLeft"].get<u32>();
    if (j.contains("aaComparisonModeRight"))  s.aaComparisonModeRight  = j["aaComparisonModeRight"].get<u32>();
    if (j.contains("aaComparisonDivider"))    s.aaComparisonDivider    = j["aaComparisonDivider"].get<f32>();
    if (j.contains("fxaaEnabled"))       s.fxaaEnabled       = JB(j["fxaaEnabled"]);
    if (j.contains("fxaaSpanMax"))       s.fxaaSpanMax       = j["fxaaSpanMax"].get<f32>();
    if (j.contains("fxaaReduceMin"))     s.fxaaReduceMin     = j["fxaaReduceMin"].get<f32>();
    if (j.contains("fxaaReduceMul"))     s.fxaaReduceMul     = j["fxaaReduceMul"].get<f32>();
    if (j.contains("taaSharpness"))      s.taaSharpness      = j["taaSharpness"].get<f32>();
    if (j.contains("taaJitterScale"))    s.taaJitterScale    = j["taaJitterScale"].get<f32>();
    if (j.contains("taaFeedbackMin"))    s.taaFeedbackMin    = j["taaFeedbackMin"].get<f32>();
    if (j.contains("taaFeedbackMax"))    s.taaFeedbackMax    = j["taaFeedbackMax"].get<f32>();

    // Temporal Upscaling
    if (j.contains("upscalerType"))      s.upscalerType      = j["upscalerType"].get<u32>();
    if (j.contains("upscalerQuality"))   s.upscalerQuality   = j["upscalerQuality"].get<u32>();
    if (j.contains("upscalerSharpness")) s.upscalerSharpness = j["upscalerSharpness"].get<f32>();

    // Retro: Dithering
    if (j.contains("ditherEnabled"))     s.ditherEnabled     = JB(j["ditherEnabled"]);
    if (j.contains("ditherPattern"))     s.ditherPattern     = j["ditherPattern"].get<u32>();
    if (j.contains("ditherStrength"))    s.ditherStrength    = j["ditherStrength"].get<f32>();

    // Retro: Color quantization
    if (j.contains("colorQuantEnabled")) s.colorQuantEnabled = JB(j["colorQuantEnabled"]);
    if (j.contains("colorBitDepth"))     s.colorBitDepth     = j["colorBitDepth"].get<u32>();

    // Retro: Resolution downscaling
    if (j.contains("resDownscaleEnabled")) s.resDownscaleEnabled = JB(j["resDownscaleEnabled"]);
    if (j.contains("internalWidth"))       s.internalWidth       = j["internalWidth"].get<u32>();
    if (j.contains("internalHeight"))      s.internalHeight      = j["internalHeight"].get<u32>();
    if (j.contains("usePointFiltering"))   s.usePointFiltering   = JB(j["usePointFiltering"]);

    // CRT scanlines
    if (j.contains("crtEnabled"))          s.crtEnabled          = JB(j["crtEnabled"]);
    if (j.contains("scanlineIntensity"))   s.scanlineIntensity   = j["scanlineIntensity"].get<f32>();
    if (j.contains("scanlineWidth"))       s.scanlineWidth       = j["scanlineWidth"].get<f32>();
    if (j.contains("crtCurvature"))        s.crtCurvature        = j["crtCurvature"].get<f32>();

    // LUT
    if (j.contains("lutEnabled"))        s.lutEnabled        = JB(j["lutEnabled"]);
    if (j.contains("lutStrength"))       s.lutStrength       = j["lutStrength"].get<f32>();
    if (j.contains("lutSize"))           s.lutSize           = j["lutSize"].get<u32>();

    // CRT Phosphor
    if (j.contains("crtPhosphorEnabled"))  s.crtPhosphorEnabled  = JB(j["crtPhosphorEnabled"]);
    if (j.contains("crtMaskType"))         s.crtMaskType         = j["crtMaskType"].get<u32>();
    if (j.contains("crtMaskPitch"))        s.crtMaskPitch        = j["crtMaskPitch"].get<f32>();
    if (j.contains("crtBloomRadius"))      s.crtBloomRadius      = j["crtBloomRadius"].get<f32>();
    if (j.contains("crtBloomStrength"))    s.crtBloomStrength    = j["crtBloomStrength"].get<f32>();
    if (j.contains("crtBloomSigma"))       s.crtBloomSigma       = j["crtBloomSigma"].get<f32>();
    if (j.contains("crtModelPreset"))      s.crtModelPreset      = j["crtModelPreset"].get<f32>();
    if (j.contains("crtTVL"))              s.crtTVL              = j["crtTVL"].get<f32>();

    // VHS
    if (j.contains("vhsEnabled"))            s.vhsEnabled            = JB(j["vhsEnabled"]);
    if (j.contains("vhsTrackingIntensity"))  s.vhsTrackingIntensity  = j["vhsTrackingIntensity"].get<f32>();
    if (j.contains("vhsTrackingSpeed"))      s.vhsTrackingSpeed      = j["vhsTrackingSpeed"].get<f32>();
    if (j.contains("vhsWobbleIntensity"))    s.vhsWobbleIntensity    = j["vhsWobbleIntensity"].get<f32>();
    if (j.contains("vhsWobbleSpeed"))        s.vhsWobbleSpeed        = j["vhsWobbleSpeed"].get<f32>();
    if (j.contains("vhsColorBleed"))         s.vhsColorBleed         = j["vhsColorBleed"].get<f32>();
    if (j.contains("vhsNoiseIntensity"))     s.vhsNoiseIntensity     = j["vhsNoiseIntensity"].get<f32>();
    if (j.contains("vhsBlueShift"))          s.vhsBlueShift          = j["vhsBlueShift"].get<f32>();
    if (j.contains("vhsScreenTear"))         s.vhsScreenTear         = JB(j["vhsScreenTear"]);
    if (j.contains("vhsTearOffset"))         s.vhsTearOffset         = j["vhsTearOffset"].get<f32>();
    if (j.contains("vhsInterlacing"))        s.vhsInterlacing        = JB(j["vhsInterlacing"]);

    // Palette lock
    if (j.contains("paletteEnabled"))    s.paletteEnabled    = JB(j["paletteEnabled"]);
    if (j.contains("paletteColors"))     s.paletteColors     = j["paletteColors"].get<u32>();

    // Stipple
    if (j.contains("stippleEnabled"))    s.stippleEnabled    = JB(j["stippleEnabled"]);
    if (j.contains("stipplePatternMask")) s.stipplePatternMask = j["stipplePatternMask"].get<u32>();
    if (j.contains("stipplePattern"))     s.stipplePatternMask = 1u << j["stipplePattern"].get<u32>(); // migrate old single-pattern
    if (j.contains("stippleColorMode"))  s.stippleColorMode  = j["stippleColorMode"].get<u32>();
    if (j.contains("stippleScale"))      s.stippleScale      = j["stippleScale"].get<f32>();
    if (j.contains("stippleDensity"))    s.stippleDensity    = j["stippleDensity"].get<f32>();
    if (j.contains("stippleStrength"))   s.stippleStrength   = j["stippleStrength"].get<f32>();
    if (j.contains("stippleFgColor"))    s.stippleFgColor    = DeserializeVec3(j["stippleFgColor"], s.stippleFgColor);
    if (j.contains("stippleBgColor"))    s.stippleBgColor    = DeserializeVec3(j["stippleBgColor"], s.stippleBgColor);

    // Global retro overrides
    if (j.contains("globalFlatShading"))          s.globalFlatShading          = JB(j["globalFlatShading"]);
    if (j.contains("globalAffineTexturing"))      s.globalAffineTexturing      = JB(j["globalAffineTexturing"]);
    if (j.contains("globalVertexSnapping"))       s.globalVertexSnapping       = JB(j["globalVertexSnapping"]);
    if (j.contains("globalStippleTransparency"))  s.globalStippleTransparency  = JB(j["globalStippleTransparency"]);
    if (j.contains("globalUVQuantize"))           s.globalUVQuantize           = JB(j["globalUVQuantize"]);
    if (j.contains("globalGouraudOnly"))          s.globalGouraudOnly          = JB(j["globalGouraudOnly"]);
    if (j.contains("globalVertexSnapResolution")) s.globalVertexSnapResolution = j["globalVertexSnapResolution"].get<u32>();
    if (j.contains("texturePageSize"))            s.texturePageSize            = j["texturePageSize"].get<f32>();
    if (j.contains("depthSortJitter"))            s.depthSortJitter            = j["depthSortJitter"].get<f32>();
    if (j.contains("lightRampMode"))              s.lightRampMode              = j["lightRampMode"].get<f32>();
    if (j.contains("normalQuantizeSteps"))        s.normalQuantizeSteps        = j["normalQuantizeSteps"].get<f32>();
    if (j.contains("celShadowMode"))              s.celShadowMode              = j["celShadowMode"].get<f32>();
    if (j.contains("halfLambert"))                s.halfLambert                = JB(j["halfLambert"]);

    // Depth of Field
    if (j.contains("dofEnabled"))            s.dofEnabled            = JB(j["dofEnabled"]);
    if (j.contains("dofFocalDistance"))       s.dofFocalDistance      = j["dofFocalDistance"].get<f32>();
    if (j.contains("dofFocalRange"))         s.dofFocalRange         = j["dofFocalRange"].get<f32>();
    if (j.contains("dofNearBlurStrength"))   s.dofNearBlurStrength   = j["dofNearBlurStrength"].get<f32>();
    if (j.contains("dofFarBlurStrength"))    s.dofFarBlurStrength    = j["dofFarBlurStrength"].get<f32>();
    if (j.contains("dofBokehSize"))          s.dofBokehSize          = j["dofBokehSize"].get<f32>();
    if (j.contains("dofApertureShape"))      s.dofApertureShape      = j["dofApertureShape"].get<u32>();
    if (j.contains("dofDebugCoC"))           s.dofDebugCoC           = JB(j["dofDebugCoC"]);

    // Tilt-Shift
    if (j.contains("tiltShiftEnabled"))      s.tiltShiftEnabled      = JB(j["tiltShiftEnabled"]);
    if (j.contains("tiltShiftFocusY"))       s.tiltShiftFocusY       = j["tiltShiftFocusY"].get<f32>();
    if (j.contains("tiltShiftBandWidth"))    s.tiltShiftBandWidth    = j["tiltShiftBandWidth"].get<f32>();
    if (j.contains("tiltShiftBlurAmount"))   s.tiltShiftBlurAmount   = j["tiltShiftBlurAmount"].get<f32>();

    // Shading Model
    if (j.contains("shadingModel"))          s.shadingModel          = j["shadingModel"].get<u32>();
    if (j.contains("fresnelEnabled"))        s.fresnelEnabled        = JB(j["fresnelEnabled"]);
    if (j.contains("energyConservation"))    s.energyConservation    = JB(j["energyConservation"]);
    if (j.contains("geometryTerm"))          s.geometryTerm          = JB(j["geometryTerm"]);

    // Dreamcast-style effects
    if (j.contains("sphereEnvMapEnabled"))   s.sphereEnvMapEnabled   = JB(j["sphereEnvMapEnabled"]);
    if (j.contains("sphereEnvStrength"))     s.sphereEnvStrength     = j["sphereEnvStrength"].get<f32>();
    if (j.contains("posterizeLevels"))       s.posterizeLevels       = j["posterizeLevels"].get<f32>();

    // Cel Shading
    if (j.contains("celShadingEnabled"))     s.celShadingEnabled     = JB(j["celShadingEnabled"]);
    if (j.contains("celDiffuseBands"))       s.celDiffuseBands       = j["celDiffuseBands"].get<f32>();
    if (j.contains("celSpecularCutoff"))     s.celSpecularCutoff     = j["celSpecularCutoff"].get<f32>();
    if (j.contains("celOutlineEnabled"))     s.celOutlineEnabled     = JB(j["celOutlineEnabled"]);
    if (j.contains("celOutlineThickness"))   s.celOutlineThickness   = j["celOutlineThickness"].get<f32>();
    if (j.contains("celOutlineThreshold"))   s.celOutlineThreshold   = j["celOutlineThreshold"].get<f32>();
    if (j.contains("celOutlineCurvatureWeight")) s.celOutlineCurvatureWeight = j["celOutlineCurvatureWeight"].get<f32>();
    if (j.contains("celOutlineColor"))       s.celOutlineColor       = DeserializeVec3(j["celOutlineColor"], s.celOutlineColor);
    if (j.contains("geometryOutlinesEnabled")) s.geometryOutlinesEnabled = JB(j["geometryOutlinesEnabled"]);
    if (j.contains("geometryOutlineWidth"))    s.geometryOutlineWidth    = j["geometryOutlineWidth"].get<f32>();
    if (j.contains("geometryOutlineColor"))    s.geometryOutlineColor    = DeserializeVec3(j["geometryOutlineColor"], s.geometryOutlineColor);

    // Screen-Space Effects
    if (j.contains("godRaysEnabled"))          s.godRaysEnabled          = JB(j["godRaysEnabled"]);
    if (j.contains("godRaysIntensity"))        s.godRaysIntensity        = j["godRaysIntensity"].get<f32>();
    if (j.contains("godRaysDecay"))            s.godRaysDecay            = j["godRaysDecay"].get<f32>();
    if (j.contains("godRaysDensity"))          s.godRaysDensity          = j["godRaysDensity"].get<f32>();
    if (j.contains("godRaysSamples"))          s.godRaysSamples          = j["godRaysSamples"].get<u32>();
    if (j.contains("godRaysWeight"))           s.godRaysWeight           = j["godRaysWeight"].get<f32>();
    if (j.contains("ssaoEnabled"))             s.ssaoEnabled             = JB(j["ssaoEnabled"]);
    if (j.contains("ssaoRadius"))              s.ssaoRadius              = j["ssaoRadius"].get<f32>();
    if (j.contains("ssaoIntensity"))           s.ssaoIntensity           = j["ssaoIntensity"].get<f32>();
    if (j.contains("ssaoBias"))                s.ssaoBias                = j["ssaoBias"].get<f32>();
    if (j.contains("ssaoSamples"))             s.ssaoSamples             = j["ssaoSamples"].get<u32>();
    if (j.contains("contactShadowsEnabled"))   s.contactShadowsEnabled   = JB(j["contactShadowsEnabled"]);
    if (j.contains("contactShadowsLength"))    s.contactShadowsLength    = j["contactShadowsLength"].get<f32>();
    if (j.contains("contactShadowsSteps"))     s.contactShadowsSteps     = j["contactShadowsSteps"].get<u32>();
    if (j.contains("contactShadowsIntensity")) s.contactShadowsIntensity = j["contactShadowsIntensity"].get<f32>();
    if (j.contains("causticsEnabled"))         s.causticsEnabled         = JB(j["causticsEnabled"]);
    if (j.contains("causticsIntensity"))       s.causticsIntensity       = j["causticsIntensity"].get<f32>();
    if (j.contains("causticsScale"))           s.causticsScale           = j["causticsScale"].get<f32>();
    if (j.contains("causticsSpeed"))           s.causticsSpeed           = j["causticsSpeed"].get<f32>();
    if (j.contains("causticsWaterY"))          s.causticsWaterY          = j["causticsWaterY"].get<f32>();
    if (j.contains("fogShaftsEnabled"))        s.fogShaftsEnabled        = JB(j["fogShaftsEnabled"]);
    if (j.contains("fogShaftsIntensity"))      s.fogShaftsIntensity      = j["fogShaftsIntensity"].get<f32>();
    if (j.contains("fogShaftsDensity"))        s.fogShaftsDensity        = j["fogShaftsDensity"].get<f32>();
    if (j.contains("fogShaftsDecay"))          s.fogShaftsDecay          = j["fogShaftsDecay"].get<f32>();
    if (j.contains("fogShaftsSamples"))        s.fogShaftsSamples        = j["fogShaftsSamples"].get<u32>();
    if (j.contains("fogShaftsMaxDistance"))     s.fogShaftsMaxDistance     = j["fogShaftsMaxDistance"].get<f32>();

    // Ray Tracing
    if (j.contains("rtEnabled"))                      s.rtEnabled                      = JB(j["rtEnabled"]);
    if (j.contains("rtMode"))                         s.rtMode                         = j["rtMode"].get<u32>();
    if (j.contains("rtShadowsEnabled"))               s.rtShadowsEnabled               = JB(j["rtShadowsEnabled"]);
    if (j.contains("rtShadowMaxDistance"))             s.rtShadowMaxDistance             = j["rtShadowMaxDistance"].get<f32>();
    if (j.contains("rtShadowRadius"))                 s.rtShadowRadius                 = j["rtShadowRadius"].get<f32>();
    if (j.contains("rtReflectionsEnabled"))            s.rtReflectionsEnabled            = JB(j["rtReflectionsEnabled"]);
    if (j.contains("rtReflectionMaxDistance"))         s.rtReflectionMaxDistance         = j["rtReflectionMaxDistance"].get<f32>();
    if (j.contains("rtReflectionRoughnessThreshold")) s.rtReflectionRoughnessThreshold = j["rtReflectionRoughnessThreshold"].get<f32>();
    if (j.contains("rtReflectionSDFFallback"))         s.rtReflectionSDFFallback         = JB(j["rtReflectionSDFFallback"]);
    if (j.contains("rtReflectionSDFMaxDistance"))       s.rtReflectionSDFMaxDistance       = j["rtReflectionSDFMaxDistance"].get<f32>();
    if (j.contains("rtAOEnabled"))                    s.rtAOEnabled                    = JB(j["rtAOEnabled"]);
    if (j.contains("rtAORadius"))                     s.rtAORadius                     = j["rtAORadius"].get<f32>();
    if (j.contains("rtAOPower"))                      s.rtAOPower                      = j["rtAOPower"].get<f32>();
    if (j.contains("rtGIEnabled"))                    s.rtGIEnabled                    = JB(j["rtGIEnabled"]);
    if (j.contains("rtGIMaxDistance"))                 s.rtGIMaxDistance                 = j["rtGIMaxDistance"].get<f32>();
    if (j.contains("rtGIIntensity"))                  s.rtGIIntensity                  = j["rtGIIntensity"].get<f32>();
    if (j.contains("rtGIBounces"))                    s.rtGIBounces                    = j["rtGIBounces"].get<u32>();
    if (j.contains("radianceCacheEnabled"))           s.radianceCacheEnabled           = JB(j["radianceCacheEnabled"]);
    if (j.contains("radianceCacheTileSize"))          s.radianceCacheTileSize          = j["radianceCacheTileSize"].get<u32>();
    if (j.contains("radianceCacheMaxAge"))            s.radianceCacheMaxAge            = j["radianceCacheMaxAge"].get<f32>();
    if (j.contains("radianceCacheDepthThreshold"))    s.radianceCacheDepthThreshold    = j["radianceCacheDepthThreshold"].get<f32>();
    if (j.contains("radianceCacheNormalThreshold"))   s.radianceCacheNormalThreshold   = j["radianceCacheNormalThreshold"].get<f32>();
    if (j.contains("radianceCacheHysteresis"))        s.radianceCacheHysteresis        = j["radianceCacheHysteresis"].get<f32>();
    if (j.contains("radianceCacheExcludeDirectional")) s.radianceCacheExcludeDirectional = JB(j["radianceCacheExcludeDirectional"]);
    if (j.contains("rtPathTracerMaxBounces"))         s.rtPathTracerMaxBounces         = j["rtPathTracerMaxBounces"].get<u32>();
    if (j.contains("rtPathTracerTargetSPP"))          s.rtPathTracerTargetSPP          = j["rtPathTracerTargetSPP"].get<u32>();
    if (j.contains("rtPathTracerFireflyClamp"))       s.rtPathTracerFireflyClamp       = j["rtPathTracerFireflyClamp"].get<f32>();
    if (j.contains("rtPathTracerNEE"))                s.rtPathTracerNEE                = JB(j["rtPathTracerNEE"]);
    if (j.contains("rtPathTracerMIS"))                s.rtPathTracerMIS                = JB(j["rtPathTracerMIS"]);
    if (j.contains("rtPathTracerRRMinBounce"))        s.rtPathTracerRRMinBounce        = j["rtPathTracerRRMinBounce"].get<f32>();
    if (j.contains("rtPathTracerRRMinProb"))          s.rtPathTracerRRMinProb          = j["rtPathTracerRRMinProb"].get<f32>();
    if (j.contains("rtSimplifiedMaterials"))          s.rtSimplifiedMaterials          = JB(j["rtSimplifiedMaterials"]);
    if (j.contains("rtSimplifyAfterBounce"))          s.rtSimplifyAfterBounce          = j["rtSimplifyAfterBounce"].get<u32>();
    if (j.contains("rtDenoiserEnabled"))              s.rtDenoiserEnabled              = JB(j["rtDenoiserEnabled"]);
    if (j.contains("rtDenoiserType"))                 s.rtDenoiserType                 = j["rtDenoiserType"].get<u32>();
    if (j.contains("rtDenoiserIterations"))           s.rtDenoiserIterations           = j["rtDenoiserIterations"].get<u32>();
    if (j.contains("rtDenoiserTemporalAlpha"))        s.rtDenoiserTemporalAlpha        = j["rtDenoiserTemporalAlpha"].get<f32>();
    if (j.contains("rtOIDNQuality"))                  s.rtOIDNQuality                  = j["rtOIDNQuality"].get<u32>();
    if (j.contains("restirEnabled"))                  s.restirEnabled                  = JB(j["restirEnabled"]);
    if (j.contains("restirInitialCandidates"))        s.restirInitialCandidates        = j["restirInitialCandidates"].get<u32>();
    if (j.contains("restirDistanceBias"))             s.restirDistanceBias             = j["restirDistanceBias"].get<f32>();
    if (j.contains("restirTemporalReuse"))            s.restirTemporalReuse            = JB(j["restirTemporalReuse"]);
    if (j.contains("restirTemporalMaxHistory"))       s.restirTemporalMaxHistory       = j["restirTemporalMaxHistory"].get<u32>();
    if (j.contains("restirTemporalDepthThreshold"))   s.restirTemporalDepthThreshold   = j["restirTemporalDepthThreshold"].get<f32>();
    if (j.contains("restirTemporalNormalThreshold"))  s.restirTemporalNormalThreshold  = j["restirTemporalNormalThreshold"].get<f32>();
    if (j.contains("restirSpatialReuse"))             s.restirSpatialReuse             = JB(j["restirSpatialReuse"]);
    if (j.contains("restirSpatialNeighbors"))         s.restirSpatialNeighbors         = j["restirSpatialNeighbors"].get<u32>();
    if (j.contains("restirSpatialRadius"))            s.restirSpatialRadius            = j["restirSpatialRadius"].get<f32>();
    if (j.contains("restirSpatialDepthThreshold"))    s.restirSpatialDepthThreshold    = j["restirSpatialDepthThreshold"].get<f32>();
    if (j.contains("restirSpatialNormalThreshold"))   s.restirSpatialNormalThreshold   = j["restirSpatialNormalThreshold"].get<f32>();

    // RT Temporal Reuse
    if (j.contains("rtTemporalReuseEnabled"))               s.rtTemporalReuseEnabled               = JB(j["rtTemporalReuseEnabled"]);
    if (j.contains("rtTemporalReuseHistoryLength"))         s.rtTemporalReuseHistoryLength         = j["rtTemporalReuseHistoryLength"].get<f32>();
    if (j.contains("rtTemporalReuseDisocclusionThreshold")) s.rtTemporalReuseDisocclusionThreshold = j["rtTemporalReuseDisocclusionThreshold"].get<f32>();
    if (j.contains("rtTemporalReuseNormalThreshold"))       s.rtTemporalReuseNormalThreshold       = j["rtTemporalReuseNormalThreshold"].get<f32>();
    if (j.contains("rtTemporalReuseShadows"))               s.rtTemporalReuseShadows               = JB(j["rtTemporalReuseShadows"]);
    if (j.contains("rtTemporalReuseReflections"))           s.rtTemporalReuseReflections           = JB(j["rtTemporalReuseReflections"]);
    if (j.contains("rtTemporalReuseAO"))                    s.rtTemporalReuseAO                    = JB(j["rtTemporalReuseAO"]);
    if (j.contains("rtTemporalReuseGI"))                    s.rtTemporalReuseGI                    = JB(j["rtTemporalReuseGI"]);

    // Surfel Radiance Cache
    if (j.contains("surfelCacheEnabled"))              s.surfelCacheEnabled              = JB(j["surfelCacheEnabled"]);
    if (j.contains("surfelCacheMaxSurfels"))           s.surfelCacheMaxSurfels           = j["surfelCacheMaxSurfels"].get<u32>();
    if (j.contains("surfelCacheRadius"))               s.surfelCacheRadius               = j["surfelCacheRadius"].get<f32>();
    if (j.contains("surfelCacheUpdateFraction"))       s.surfelCacheUpdateFraction       = j["surfelCacheUpdateFraction"].get<f32>();
    if (j.contains("surfelCacheMaxAge"))               s.surfelCacheMaxAge               = j["surfelCacheMaxAge"].get<f32>();
    if (j.contains("surfelCacheExcludeDirectional"))   s.surfelCacheExcludeDirectional   = JB(j["surfelCacheExcludeDirectional"]);
    if (j.contains("surfelCacheCameraRadius"))         s.surfelCacheCameraRadius         = j["surfelCacheCameraRadius"].get<f32>();
    if (j.contains("surfelCacheBlendWeight"))          s.surfelCacheBlendWeight          = j["surfelCacheBlendWeight"].get<f32>();
    if (j.contains("surfelCacheNormalThreshold"))      s.surfelCacheNormalThreshold      = j["surfelCacheNormalThreshold"].get<f32>();
    if (j.contains("surfelCachePlacementInterval"))    s.surfelCachePlacementInterval    = j["surfelCachePlacementInterval"].get<u32>();
    if (j.contains("surfelCacheRaysPerSurfel"))        s.surfelCacheRaysPerSurfel        = j["surfelCacheRaysPerSurfel"].get<u32>();

    if (j.contains("rtShadowStrength"))               s.rtShadowStrength               = j["rtShadowStrength"].get<f32>();
    if (j.contains("rtReflectionStrength"))            s.rtReflectionStrength            = j["rtReflectionStrength"].get<f32>();
    if (j.contains("rtAOStrength"))                   s.rtAOStrength                   = j["rtAOStrength"].get<f32>();
    if (j.contains("rtGIStrength"))                   s.rtGIStrength                   = j["rtGIStrength"].get<f32>();

    return s;
}

} // namespace Renderer
} // namespace Enjin
