#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/PostProcessing.h"
#include <nlohmann/json.hpp>

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
static json SerializeVec3(const Math::Vector3& v) {
    return json::array({v.x, v.y, v.z});
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
    j["shadowDistance"]    = s.shadowDistance;
    j["shadowStrength"]    = s.shadowStrength;
    j["backfaceCulling"]   = s.backfaceCulling;
    j["wireframe"]         = s.wireframe;
    j["ambientIntensity"]  = s.ambientIntensity;
    j["ambientColor"]      = SerializeVec3(s.ambientColor);
    j["fogDensity"]        = s.fogDensity;
    j["fogStart"]          = s.fogStart;
    j["fogEnd"]            = s.fogEnd;
    j["fogHeightFalloff"]  = s.fogHeightFalloff;
    j["fogColor"]          = SerializeVec3(s.fogColor);
    j["snowIntensity"]     = s.snowIntensity;
    j["worldCurvature"]    = s.worldCurvature;
    j["rainActive"]        = s.rainActive;

    // Tone mapping
    j["toneMappingMode"]   = s.toneMappingMode;
    j["exposure"]          = s.exposure;
    j["gamma"]             = s.gamma;
    j["whitePoint"]        = s.whitePoint;

    // Bloom
    j["bloomEnabled"]      = s.bloomEnabled;
    j["bloomThreshold"]    = s.bloomThreshold;
    j["bloomIntensity"]    = s.bloomIntensity;
    j["bloomRadius"]       = s.bloomRadius;

    // Vignette
    j["vignetteEnabled"]     = s.vignetteEnabled;
    j["vignetteIntensity"]   = s.vignetteIntensity;
    j["vignetteSmoothness"]  = s.vignetteSmoothness;

    // Chromatic aberration
    j["chromaticAberrationEnabled"]   = s.chromaticAberrationEnabled;
    j["chromaticAberrationIntensity"] = s.chromaticAberrationIntensity;

    // Color grading
    j["colorFilter"]       = SerializeVec3(s.colorFilter);
    j["saturation"]        = s.saturation;
    j["contrast"]          = s.contrast;
    j["brightness"]        = s.brightness;

    // Film grain
    j["filmGrainEnabled"]    = s.filmGrainEnabled;
    j["filmGrainIntensity"]  = s.filmGrainIntensity;

    // FXAA
    j["fxaaEnabled"]       = s.fxaaEnabled;
    j["fxaaSpanMax"]       = s.fxaaSpanMax;
    j["fxaaReduceMin"]     = s.fxaaReduceMin;
    j["fxaaReduceMul"]     = s.fxaaReduceMul;

    // Retro: Dithering
    j["ditherEnabled"]     = s.ditherEnabled;
    j["ditherPattern"]     = s.ditherPattern;
    j["ditherStrength"]    = s.ditherStrength;

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
    j["scanlineIntensity"]   = s.scanlineIntensity;
    j["scanlineWidth"]       = s.scanlineWidth;
    j["crtCurvature"]        = s.crtCurvature;

    // LUT
    j["lutEnabled"]        = s.lutEnabled;
    j["lutStrength"]       = s.lutStrength;
    j["lutSize"]           = s.lutSize;

    // CRT Phosphor
    j["crtPhosphorEnabled"]  = s.crtPhosphorEnabled;
    j["crtMaskType"]         = s.crtMaskType;
    j["crtMaskPitch"]        = s.crtMaskPitch;
    j["crtBloomRadius"]      = s.crtBloomRadius;
    j["crtBloomStrength"]    = s.crtBloomStrength;

    // VHS
    j["vhsEnabled"]            = s.vhsEnabled;
    j["vhsTrackingIntensity"]  = s.vhsTrackingIntensity;
    j["vhsTrackingSpeed"]      = s.vhsTrackingSpeed;
    j["vhsWobbleIntensity"]    = s.vhsWobbleIntensity;
    j["vhsWobbleSpeed"]        = s.vhsWobbleSpeed;
    j["vhsColorBleed"]         = s.vhsColorBleed;
    j["vhsNoiseIntensity"]     = s.vhsNoiseIntensity;
    j["vhsBlueShift"]          = s.vhsBlueShift;
    j["vhsScreenTear"]         = s.vhsScreenTear;
    j["vhsTearOffset"]         = s.vhsTearOffset;
    j["vhsInterlacing"]        = s.vhsInterlacing;

    // Palette lock
    j["paletteEnabled"]    = s.paletteEnabled;
    j["paletteColors"]     = s.paletteColors;

    // Global retro overrides
    j["globalFlatShading"]          = s.globalFlatShading;
    j["globalAffineTexturing"]      = s.globalAffineTexturing;
    j["globalVertexSnapping"]       = s.globalVertexSnapping;
    j["globalStippleTransparency"]  = s.globalStippleTransparency;
    j["globalUVQuantize"]           = s.globalUVQuantize;
    j["globalGouraudOnly"]          = s.globalGouraudOnly;
    j["globalVertexSnapResolution"] = s.globalVertexSnapResolution;

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

    // Global retro overrides
    if (j.contains("globalFlatShading"))          s.globalFlatShading          = j["globalFlatShading"].get<bool>();
    if (j.contains("globalAffineTexturing"))      s.globalAffineTexturing      = j["globalAffineTexturing"].get<bool>();
    if (j.contains("globalVertexSnapping"))       s.globalVertexSnapping       = j["globalVertexSnapping"].get<bool>();
    if (j.contains("globalStippleTransparency"))  s.globalStippleTransparency  = j["globalStippleTransparency"].get<bool>();
    if (j.contains("globalUVQuantize"))           s.globalUVQuantize           = j["globalUVQuantize"].get<bool>();
    if (j.contains("globalGouraudOnly"))          s.globalGouraudOnly          = j["globalGouraudOnly"].get<bool>();
    if (j.contains("globalVertexSnapResolution")) s.globalVertexSnapResolution = j["globalVertexSnapResolution"].get<u32>();

    return s;
}

} // namespace Renderer
} // namespace Enjin
