#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Platform/Platform.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

static ECS::RenderSystem* s_BindingsRenderSystem = nullptr;
static Renderer::PostProcessing* s_BindingsPostProcessing = nullptr;
extern ECS::World* s_BindingsWorld;

namespace Enjin {
namespace Scripting {

void SetBindingsRenderSystem(ECS::RenderSystem* renderSystem) {
    s_BindingsRenderSystem = renderSystem;
}

void SetBindingsPostProcessing(Renderer::PostProcessing* postProcessing) {
    s_BindingsPostProcessing = postProcessing;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// RenderSystem wrappers — Shadows
// ============================================================================

static void Render_SetShadowsEnabled(bool enabled) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetShadowsEnabled(enabled);
}

static bool Render_IsShadowsEnabled() {
    if (!s_BindingsRenderSystem) return false;
    return s_BindingsRenderSystem->IsShadowsEnabled();
}

static void Render_SetShadowDistance(f32 distance) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetShadowDistance(distance);
}

static f32 Render_GetShadowDistance() {
    if (!s_BindingsRenderSystem) return 100.0f;
    return s_BindingsRenderSystem->GetShadowDistance();
}

static void Render_SetShadowStrength(f32 strength) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetShadowStrength(strength);
}

static f32 Render_GetShadowStrength() {
    if (!s_BindingsRenderSystem) return 1.0f;
    return s_BindingsRenderSystem->GetShadowStrength();
}

// ============================================================================
// RenderSystem wrappers — Ambient
// ============================================================================

static void Render_SetAmbientIntensity(f32 intensity) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetAmbientIntensity(intensity);
}

static f32 Render_GetAmbientIntensity() {
    if (!s_BindingsRenderSystem) return 0.3f;
    return s_BindingsRenderSystem->GetAmbientIntensity();
}

static void Render_SetAmbientColor(const Vector3& color) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetAmbientColor(color);
}

static Vector3 Render_GetAmbientColor() {
    if (!s_BindingsRenderSystem) return Vector3(1.0f);
    return s_BindingsRenderSystem->GetAmbientColor();
}

// ============================================================================
// RenderSystem wrappers — Fog
// ============================================================================

static void Render_SetFogDensity(f32 density) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetFogParams(
        density,
        s_BindingsRenderSystem->GetFogStart(),
        s_BindingsRenderSystem->GetFogEnd(),
        s_BindingsRenderSystem->GetFogHeightFalloff());
}

static f32 Render_GetFogDensity() {
    if (!s_BindingsRenderSystem) return 0.0f;
    return s_BindingsRenderSystem->GetFogDensity();
}

static void Render_SetFogColor(const Vector3& color) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetFogColor(color);
}

static Vector3 Render_GetFogColor() {
    if (!s_BindingsRenderSystem) return Vector3(0.5f);
    return s_BindingsRenderSystem->GetFogColor();
}

static void Render_SetFogStart(f32 start) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetFogParams(
        s_BindingsRenderSystem->GetFogDensity(),
        start,
        s_BindingsRenderSystem->GetFogEnd(),
        s_BindingsRenderSystem->GetFogHeightFalloff());
}

static f32 Render_GetFogStart() {
    if (!s_BindingsRenderSystem) return 10.0f;
    return s_BindingsRenderSystem->GetFogStart();
}

static void Render_SetFogEnd(f32 end) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetFogParams(
        s_BindingsRenderSystem->GetFogDensity(),
        s_BindingsRenderSystem->GetFogStart(),
        end,
        s_BindingsRenderSystem->GetFogHeightFalloff());
}

static f32 Render_GetFogEnd() {
    if (!s_BindingsRenderSystem) return 100.0f;
    return s_BindingsRenderSystem->GetFogEnd();
}

static void Render_SetFogHeightFalloff(f32 falloff) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetFogParams(
        s_BindingsRenderSystem->GetFogDensity(),
        s_BindingsRenderSystem->GetFogStart(),
        s_BindingsRenderSystem->GetFogEnd(),
        falloff);
}

static f32 Render_GetFogHeightFalloff() {
    if (!s_BindingsRenderSystem) return 0.0f;
    return s_BindingsRenderSystem->GetFogHeightFalloff();
}

// ============================================================================
// RenderSystem wrappers — Snow, Curvature, Wireframe, Rain
// ============================================================================

static void Render_SetSnowIntensity(f32 intensity) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetSnowIntensity(intensity);
}

static f32 Render_GetSnowIntensity() {
    if (!s_BindingsRenderSystem) return 0.0f;
    return s_BindingsRenderSystem->GetSnowIntensity();
}

static void Render_SetWorldCurvature(f32 curvature) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetWorldCurvature(curvature);
}

static f32 Render_GetWorldCurvature() {
    if (!s_BindingsRenderSystem) return 0.0f;
    return s_BindingsRenderSystem->GetWorldCurvature();
}

static void Render_SetWireframeEnabled(bool enabled) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetWireframeEnabled(enabled);
}

static bool Render_IsWireframeEnabled() {
    if (!s_BindingsRenderSystem) return false;
    return s_BindingsRenderSystem->IsWireframeEnabled();
}

static void Render_SetRainActive(bool active) {
    if (!s_BindingsRenderSystem) return;
    s_BindingsRenderSystem->SetRainActive(active);
}

static bool Render_IsRainActive() {
    if (!s_BindingsRenderSystem) return false;
    return s_BindingsRenderSystem->IsRainActive();
}

// ============================================================================
// PostProcessing wrappers — Tone Mapping & Exposure
// ============================================================================

static void PostProcess_SetToneMapping(i32 mode) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().toneMappingMode = static_cast<u32>(mode);
}

static i32 PostProcess_GetToneMapping() {
    if (!s_BindingsPostProcessing) return 0;
    return static_cast<i32>(s_BindingsPostProcessing->GetSettings().toneMappingMode);
}

static void PostProcess_SetExposure(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().exposure = value;
}

static f32 PostProcess_GetExposure() {
    if (!s_BindingsPostProcessing) return 1.0f;
    return s_BindingsPostProcessing->GetSettings().exposure;
}

static void PostProcess_SetGamma(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().gamma = value;
}

static f32 PostProcess_GetGamma() {
    if (!s_BindingsPostProcessing) return 1.0f;
    return s_BindingsPostProcessing->GetSettings().gamma;
}

// ============================================================================
// PostProcessing wrappers — Bloom
// ============================================================================

static void PostProcess_SetBloomEnabled(bool enabled) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().bloomEnabled = enabled ? 1u : 0u;
}

static bool PostProcess_IsBloomEnabled() {
    if (!s_BindingsPostProcessing) return false;
    return s_BindingsPostProcessing->GetSettings().bloomEnabled != 0;
}

static void PostProcess_SetBloomThreshold(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().bloomThreshold = value;
}

static f32 PostProcess_GetBloomThreshold() {
    if (!s_BindingsPostProcessing) return 1.0f;
    return s_BindingsPostProcessing->GetSettings().bloomThreshold;
}

static void PostProcess_SetBloomIntensity(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().bloomIntensity = value;
}

static f32 PostProcess_GetBloomIntensity() {
    if (!s_BindingsPostProcessing) return 0.5f;
    return s_BindingsPostProcessing->GetSettings().bloomIntensity;
}

// ============================================================================
// PostProcessing wrappers — Vignette
// ============================================================================

static void PostProcess_SetVignetteEnabled(bool enabled) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().vignetteEnabled = enabled ? 1u : 0u;
}

static bool PostProcess_IsVignetteEnabled() {
    if (!s_BindingsPostProcessing) return false;
    return s_BindingsPostProcessing->GetSettings().vignetteEnabled != 0;
}

static void PostProcess_SetVignetteIntensity(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().vignetteIntensity = value;
}

static f32 PostProcess_GetVignetteIntensity() {
    if (!s_BindingsPostProcessing) return 0.3f;
    return s_BindingsPostProcessing->GetSettings().vignetteIntensity;
}

static void PostProcess_SetVignetteSmoothness(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().vignetteSmoothness = value;
}

static f32 PostProcess_GetVignetteSmoothness() {
    if (!s_BindingsPostProcessing) return 0.5f;
    return s_BindingsPostProcessing->GetSettings().vignetteSmoothness;
}

// ============================================================================
// PostProcessing wrappers — Chromatic Aberration
// ============================================================================

static void PostProcess_SetChromaticAberrationEnabled(bool enabled) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().chromaticAberrationEnabled = enabled ? 1u : 0u;
}

static bool PostProcess_IsChromaticAberrationEnabled() {
    if (!s_BindingsPostProcessing) return false;
    return s_BindingsPostProcessing->GetSettings().chromaticAberrationEnabled != 0;
}

static void PostProcess_SetChromaticAberrationIntensity(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().chromaticAberrationIntensity = value;
}

static f32 PostProcess_GetChromaticAberrationIntensity() {
    if (!s_BindingsPostProcessing) return 0.005f;
    return s_BindingsPostProcessing->GetSettings().chromaticAberrationIntensity;
}

// ============================================================================
// PostProcessing wrappers — Color Grading
// ============================================================================

static void PostProcess_SetColorFilter(const Vector3& color) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().colorFilter = color;
}

static Vector3 PostProcess_GetColorFilter() {
    if (!s_BindingsPostProcessing) return Vector3(1.0f);
    return s_BindingsPostProcessing->GetSettings().colorFilter;
}

static void PostProcess_SetSaturation(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().saturation = value;
}

static f32 PostProcess_GetSaturation() {
    if (!s_BindingsPostProcessing) return 1.0f;
    return s_BindingsPostProcessing->GetSettings().saturation;
}

static void PostProcess_SetContrast(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().contrast = value;
}

static f32 PostProcess_GetContrast() {
    if (!s_BindingsPostProcessing) return 1.0f;
    return s_BindingsPostProcessing->GetSettings().contrast;
}

static void PostProcess_SetBrightness(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().brightness = value;
}

static f32 PostProcess_GetBrightness() {
    if (!s_BindingsPostProcessing) return 0.0f;
    return s_BindingsPostProcessing->GetSettings().brightness;
}

// ============================================================================
// PostProcessing wrappers — Film Grain
// ============================================================================

static void PostProcess_SetFilmGrainEnabled(bool enabled) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().filmGrainEnabled = enabled ? 1u : 0u;
}

static bool PostProcess_IsFilmGrainEnabled() {
    if (!s_BindingsPostProcessing) return false;
    return s_BindingsPostProcessing->GetSettings().filmGrainEnabled != 0;
}

static void PostProcess_SetFilmGrainIntensity(f32 value) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().filmGrainIntensity = value;
}

static f32 PostProcess_GetFilmGrainIntensity() {
    if (!s_BindingsPostProcessing) return 0.05f;
    return s_BindingsPostProcessing->GetSettings().filmGrainIntensity;
}

// ============================================================================
// PostProcessing wrappers — FXAA
// ============================================================================

static void PostProcess_SetFXAAEnabled(bool enabled) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().fxaaEnabled = enabled ? 1u : 0u;
}

static bool PostProcess_IsFXAAEnabled() {
    if (!s_BindingsPostProcessing) return true;
    return s_BindingsPostProcessing->GetSettings().fxaaEnabled != 0;
}

// ============================================================================
// Screen-Space Effects wrappers — God Rays
// ============================================================================

static void PostProcess_SetGodRaysEnabled(bool enabled) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().godRaysEnabled = enabled ? 1u : 0u;
}
static bool PostProcess_IsGodRaysEnabled() {
    if (!s_BindingsPostProcessing) return false;
    return s_BindingsPostProcessing->GetSettings().godRaysEnabled != 0;
}
static void PostProcess_SetGodRaysIntensity(f32 v) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().godRaysIntensity = v;
}
static f32 PostProcess_GetGodRaysIntensity() {
    if (!s_BindingsPostProcessing) return 0.5f;
    return s_BindingsPostProcessing->GetSettings().godRaysIntensity;
}
static void PostProcess_SetGodRaysSamples(i32 v) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().godRaysSamples = static_cast<u32>(std::max(v, 1));
}
static i32 PostProcess_GetGodRaysSamples() {
    if (!s_BindingsPostProcessing) return 64;
    return static_cast<i32>(s_BindingsPostProcessing->GetSettings().godRaysSamples);
}

// ============================================================================
// Screen-Space Effects wrappers — SSAO
// ============================================================================

static void PostProcess_SetSSAOEnabled(bool enabled) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().ssaoEnabled = enabled ? 1u : 0u;
}
static bool PostProcess_IsSSAOEnabled() {
    if (!s_BindingsPostProcessing) return false;
    return s_BindingsPostProcessing->GetSettings().ssaoEnabled != 0;
}
static void PostProcess_SetSSAORadius(f32 v) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().ssaoRadius = v;
}
static f32 PostProcess_GetSSAORadius() {
    if (!s_BindingsPostProcessing) return 0.5f;
    return s_BindingsPostProcessing->GetSettings().ssaoRadius;
}
static void PostProcess_SetSSAOIntensity(f32 v) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().ssaoIntensity = v;
}
static f32 PostProcess_GetSSAOIntensity() {
    if (!s_BindingsPostProcessing) return 1.5f;
    return s_BindingsPostProcessing->GetSettings().ssaoIntensity;
}

// ============================================================================
// Screen-Space Effects wrappers — Contact Shadows
// ============================================================================

static void PostProcess_SetContactShadowsEnabled(bool enabled) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().contactShadowsEnabled = enabled ? 1u : 0u;
}
static bool PostProcess_IsContactShadowsEnabled() {
    if (!s_BindingsPostProcessing) return false;
    return s_BindingsPostProcessing->GetSettings().contactShadowsEnabled != 0;
}
static void PostProcess_SetContactShadowsIntensity(f32 v) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().contactShadowsIntensity = v;
}
static f32 PostProcess_GetContactShadowsIntensity() {
    if (!s_BindingsPostProcessing) return 1.0f;
    return s_BindingsPostProcessing->GetSettings().contactShadowsIntensity;
}

// ============================================================================
// Screen-Space Effects wrappers — Fake Caustics
// ============================================================================

static void PostProcess_SetCausticsEnabled(bool enabled) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().causticsEnabled = enabled ? 1u : 0u;
}
static bool PostProcess_IsCausticsEnabled() {
    if (!s_BindingsPostProcessing) return false;
    return s_BindingsPostProcessing->GetSettings().causticsEnabled != 0;
}
static void PostProcess_SetCausticsIntensity(f32 v) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().causticsIntensity = v;
}
static f32 PostProcess_GetCausticsIntensity() {
    if (!s_BindingsPostProcessing) return 0.3f;
    return s_BindingsPostProcessing->GetSettings().causticsIntensity;
}
static void PostProcess_SetCausticsWaterY(f32 v) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().causticsWaterY = v;
}
static f32 PostProcess_GetCausticsWaterY() {
    if (!s_BindingsPostProcessing) return 0.0f;
    return s_BindingsPostProcessing->GetSettings().causticsWaterY;
}

// ============================================================================
// Screen-Space Effects wrappers — Fog Shafts
// ============================================================================

static void PostProcess_SetFogShaftsEnabled(bool enabled) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().fogShaftsEnabled = enabled ? 1u : 0u;
}
static bool PostProcess_IsFogShaftsEnabled() {
    if (!s_BindingsPostProcessing) return false;
    return s_BindingsPostProcessing->GetSettings().fogShaftsEnabled != 0;
}
static void PostProcess_SetFogShaftsIntensity(f32 v) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().fogShaftsIntensity = v;
}
static f32 PostProcess_GetFogShaftsIntensity() {
    if (!s_BindingsPostProcessing) return 0.3f;
    return s_BindingsPostProcessing->GetSettings().fogShaftsIntensity;
}
static void PostProcess_SetFogShaftsMaxDistance(f32 v) {
    if (!s_BindingsPostProcessing) return;
    s_BindingsPostProcessing->GetSettings().fogShaftsMaxDistance = v;
}
static f32 PostProcess_GetFogShaftsMaxDistance() {
    if (!s_BindingsPostProcessing) return 50.0f;
    return s_BindingsPostProcessing->GetSettings().fogShaftsMaxDistance;
}

// ============================================================================
// Post-Process Volume wrappers
// ============================================================================

static void PPVolume_SetActive(u64 entity, bool active) {
    if (!s_BindingsWorld) return;
    auto* vol = s_BindingsWorld->GetComponent<ECS::PostProcessVolumeComponent>(static_cast<ECS::Entity>(entity));
    if (vol) vol->isActive = active;
}

static bool PPVolume_IsActive(u64 entity) {
    if (!s_BindingsWorld) return false;
    auto* vol = s_BindingsWorld->GetComponent<ECS::PostProcessVolumeComponent>(static_cast<ECS::Entity>(entity));
    return vol ? vol->isActive : false;
}

static void PPVolume_SetWeight(u64 entity, f32 weight) {
    if (!s_BindingsWorld) return;
    auto* vol = s_BindingsWorld->GetComponent<ECS::PostProcessVolumeComponent>(static_cast<ECS::Entity>(entity));
    if (vol) vol->weight = std::clamp(weight, 0.0f, 1.0f);
}

static f32 PPVolume_GetWeight(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* vol = s_BindingsWorld->GetComponent<ECS::PostProcessVolumeComponent>(static_cast<ECS::Entity>(entity));
    return vol ? vol->weight : 0.0f;
}

static void PPVolume_SetBlendRadius(u64 entity, f32 radius) {
    if (!s_BindingsWorld) return;
    auto* vol = s_BindingsWorld->GetComponent<ECS::PostProcessVolumeComponent>(static_cast<ECS::Entity>(entity));
    if (vol) vol->blendRadius = std::max(radius, 0.0f);
}

static f32 PPVolume_GetBlendRadius(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* vol = s_BindingsWorld->GetComponent<ECS::PostProcessVolumeComponent>(static_cast<ECS::Entity>(entity));
    return vol ? vol->blendRadius : 0.0f;
}

static void PPVolume_SetPriority(u64 entity, i32 priority) {
    if (!s_BindingsWorld) return;
    auto* vol = s_BindingsWorld->GetComponent<ECS::PostProcessVolumeComponent>(static_cast<ECS::Entity>(entity));
    if (vol) vol->priority = priority;
}

static i32 PPVolume_GetPriority(u64 entity) {
    if (!s_BindingsWorld) return 0;
    auto* vol = s_BindingsWorld->GetComponent<ECS::PostProcessVolumeComponent>(static_cast<ECS::Entity>(entity));
    return vol ? vol->priority : 0;
}

static void PPVolume_SetGlobal(u64 entity, bool global) {
    if (!s_BindingsWorld) return;
    auto* vol = s_BindingsWorld->GetComponent<ECS::PostProcessVolumeComponent>(static_cast<ECS::Entity>(entity));
    if (vol) vol->isGlobal = global;
}

static bool PPVolume_IsGlobal(u64 entity) {
    if (!s_BindingsWorld) return false;
    auto* vol = s_BindingsWorld->GetComponent<ECS::PostProcessVolumeComponent>(static_cast<ECS::Entity>(entity));
    return vol ? vol->isGlobal : false;
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterRenderBindings(asIScriptEngine* engine) {
    // ---- Shadows ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetShadowsEnabled(bool)",
        asFUNCTION(Render_SetShadowsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Render_IsShadowsEnabled()",
        asFUNCTION(Render_IsShadowsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetShadowDistance(float)",
        asFUNCTION(Render_SetShadowDistance), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetShadowDistance()",
        asFUNCTION(Render_GetShadowDistance), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetShadowStrength(float)",
        asFUNCTION(Render_SetShadowStrength), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetShadowStrength()",
        asFUNCTION(Render_GetShadowStrength), asCALL_CDECL));

    // ---- Ambient ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetAmbientIntensity(float)",
        asFUNCTION(Render_SetAmbientIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetAmbientIntensity()",
        asFUNCTION(Render_GetAmbientIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetAmbientColor(const Vector3 &in)",
        asFUNCTION(Render_SetAmbientColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Render_GetAmbientColor()",
        asFUNCTION(Render_GetAmbientColor), asCALL_CDECL));

    // ---- Fog ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetFogDensity(float)",
        asFUNCTION(Render_SetFogDensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetFogDensity()",
        asFUNCTION(Render_GetFogDensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetFogColor(const Vector3 &in)",
        asFUNCTION(Render_SetFogColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Render_GetFogColor()",
        asFUNCTION(Render_GetFogColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetFogStart(float)",
        asFUNCTION(Render_SetFogStart), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetFogStart()",
        asFUNCTION(Render_GetFogStart), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetFogEnd(float)",
        asFUNCTION(Render_SetFogEnd), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetFogEnd()",
        asFUNCTION(Render_GetFogEnd), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetFogHeightFalloff(float)",
        asFUNCTION(Render_SetFogHeightFalloff), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetFogHeightFalloff()",
        asFUNCTION(Render_GetFogHeightFalloff), asCALL_CDECL));

    // ---- Snow, Curvature, Wireframe, Rain ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetSnowIntensity(float)",
        asFUNCTION(Render_SetSnowIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetSnowIntensity()",
        asFUNCTION(Render_GetSnowIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetWorldCurvature(float)",
        asFUNCTION(Render_SetWorldCurvature), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetWorldCurvature()",
        asFUNCTION(Render_GetWorldCurvature), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetWireframeEnabled(bool)",
        asFUNCTION(Render_SetWireframeEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Render_IsWireframeEnabled()",
        asFUNCTION(Render_IsWireframeEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetRainActive(bool)",
        asFUNCTION(Render_SetRainActive), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Render_IsRainActive()",
        asFUNCTION(Render_IsRainActive), asCALL_CDECL));

    // ---- PostProcess: Tone Mapping & Exposure ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetToneMapping(int)",
        asFUNCTION(PostProcess_SetToneMapping), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int PostProcess_GetToneMapping()",
        asFUNCTION(PostProcess_GetToneMapping), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetExposure(float)",
        asFUNCTION(PostProcess_SetExposure), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetExposure()",
        asFUNCTION(PostProcess_GetExposure), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetGamma(float)",
        asFUNCTION(PostProcess_SetGamma), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetGamma()",
        asFUNCTION(PostProcess_GetGamma), asCALL_CDECL));

    // ---- PostProcess: Bloom ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetBloomEnabled(bool)",
        asFUNCTION(PostProcess_SetBloomEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsBloomEnabled()",
        asFUNCTION(PostProcess_IsBloomEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetBloomThreshold(float)",
        asFUNCTION(PostProcess_SetBloomThreshold), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetBloomThreshold()",
        asFUNCTION(PostProcess_GetBloomThreshold), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetBloomIntensity(float)",
        asFUNCTION(PostProcess_SetBloomIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetBloomIntensity()",
        asFUNCTION(PostProcess_GetBloomIntensity), asCALL_CDECL));

    // ---- PostProcess: Vignette ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetVignetteEnabled(bool)",
        asFUNCTION(PostProcess_SetVignetteEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsVignetteEnabled()",
        asFUNCTION(PostProcess_IsVignetteEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetVignetteIntensity(float)",
        asFUNCTION(PostProcess_SetVignetteIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetVignetteIntensity()",
        asFUNCTION(PostProcess_GetVignetteIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetVignetteSmoothness(float)",
        asFUNCTION(PostProcess_SetVignetteSmoothness), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetVignetteSmoothness()",
        asFUNCTION(PostProcess_GetVignetteSmoothness), asCALL_CDECL));

    // ---- PostProcess: Chromatic Aberration ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetChromaticAberrationEnabled(bool)",
        asFUNCTION(PostProcess_SetChromaticAberrationEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsChromaticAberrationEnabled()",
        asFUNCTION(PostProcess_IsChromaticAberrationEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetChromaticAberrationIntensity(float)",
        asFUNCTION(PostProcess_SetChromaticAberrationIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetChromaticAberrationIntensity()",
        asFUNCTION(PostProcess_GetChromaticAberrationIntensity), asCALL_CDECL));

    // ---- PostProcess: Color Grading ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetColorFilter(const Vector3 &in)",
        asFUNCTION(PostProcess_SetColorFilter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 PostProcess_GetColorFilter()",
        asFUNCTION(PostProcess_GetColorFilter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetSaturation(float)",
        asFUNCTION(PostProcess_SetSaturation), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetSaturation()",
        asFUNCTION(PostProcess_GetSaturation), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetContrast(float)",
        asFUNCTION(PostProcess_SetContrast), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetContrast()",
        asFUNCTION(PostProcess_GetContrast), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetBrightness(float)",
        asFUNCTION(PostProcess_SetBrightness), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetBrightness()",
        asFUNCTION(PostProcess_GetBrightness), asCALL_CDECL));

    // ---- PostProcess: Film Grain ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFilmGrainEnabled(bool)",
        asFUNCTION(PostProcess_SetFilmGrainEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsFilmGrainEnabled()",
        asFUNCTION(PostProcess_IsFilmGrainEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFilmGrainIntensity(float)",
        asFUNCTION(PostProcess_SetFilmGrainIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetFilmGrainIntensity()",
        asFUNCTION(PostProcess_GetFilmGrainIntensity), asCALL_CDECL));

    // ---- PostProcess: FXAA ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFXAAEnabled(bool)",
        asFUNCTION(PostProcess_SetFXAAEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsFXAAEnabled()",
        asFUNCTION(PostProcess_IsFXAAEnabled), asCALL_CDECL));

    // ---- Screen-Space Effects: God Rays ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetGodRaysEnabled(bool)",
        asFUNCTION(PostProcess_SetGodRaysEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsGodRaysEnabled()",
        asFUNCTION(PostProcess_IsGodRaysEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetGodRaysIntensity(float)",
        asFUNCTION(PostProcess_SetGodRaysIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetGodRaysIntensity()",
        asFUNCTION(PostProcess_GetGodRaysIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetGodRaysSamples(int)",
        asFUNCTION(PostProcess_SetGodRaysSamples), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int PostProcess_GetGodRaysSamples()",
        asFUNCTION(PostProcess_GetGodRaysSamples), asCALL_CDECL));

    // ---- Screen-Space Effects: SSAO ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetSSAOEnabled(bool)",
        asFUNCTION(PostProcess_SetSSAOEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsSSAOEnabled()",
        asFUNCTION(PostProcess_IsSSAOEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetSSAORadius(float)",
        asFUNCTION(PostProcess_SetSSAORadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetSSAORadius()",
        asFUNCTION(PostProcess_GetSSAORadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetSSAOIntensity(float)",
        asFUNCTION(PostProcess_SetSSAOIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetSSAOIntensity()",
        asFUNCTION(PostProcess_GetSSAOIntensity), asCALL_CDECL));

    // ---- Screen-Space Effects: Contact Shadows ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetContactShadowsEnabled(bool)",
        asFUNCTION(PostProcess_SetContactShadowsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsContactShadowsEnabled()",
        asFUNCTION(PostProcess_IsContactShadowsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetContactShadowsIntensity(float)",
        asFUNCTION(PostProcess_SetContactShadowsIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetContactShadowsIntensity()",
        asFUNCTION(PostProcess_GetContactShadowsIntensity), asCALL_CDECL));

    // ---- Screen-Space Effects: Caustics ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetCausticsEnabled(bool)",
        asFUNCTION(PostProcess_SetCausticsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsCausticsEnabled()",
        asFUNCTION(PostProcess_IsCausticsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetCausticsIntensity(float)",
        asFUNCTION(PostProcess_SetCausticsIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetCausticsIntensity()",
        asFUNCTION(PostProcess_GetCausticsIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetCausticsWaterY(float)",
        asFUNCTION(PostProcess_SetCausticsWaterY), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetCausticsWaterY()",
        asFUNCTION(PostProcess_GetCausticsWaterY), asCALL_CDECL));

    // ---- Screen-Space Effects: Fog Shafts ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFogShaftsEnabled(bool)",
        asFUNCTION(PostProcess_SetFogShaftsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsFogShaftsEnabled()",
        asFUNCTION(PostProcess_IsFogShaftsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFogShaftsIntensity(float)",
        asFUNCTION(PostProcess_SetFogShaftsIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetFogShaftsIntensity()",
        asFUNCTION(PostProcess_GetFogShaftsIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFogShaftsMaxDistance(float)",
        asFUNCTION(PostProcess_SetFogShaftsMaxDistance), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetFogShaftsMaxDistance()",
        asFUNCTION(PostProcess_GetFogShaftsMaxDistance), asCALL_CDECL));

    // ---- Post-Process Volumes ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PPVolume_SetActive(uint64, bool)",
        asFUNCTION(PPVolume_SetActive), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PPVolume_IsActive(uint64)",
        asFUNCTION(PPVolume_IsActive), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PPVolume_SetWeight(uint64, float)",
        asFUNCTION(PPVolume_SetWeight), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PPVolume_GetWeight(uint64)",
        asFUNCTION(PPVolume_GetWeight), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PPVolume_SetBlendRadius(uint64, float)",
        asFUNCTION(PPVolume_SetBlendRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PPVolume_GetBlendRadius(uint64)",
        asFUNCTION(PPVolume_GetBlendRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PPVolume_SetPriority(uint64, int)",
        asFUNCTION(PPVolume_SetPriority), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int PPVolume_GetPriority(uint64)",
        asFUNCTION(PPVolume_GetPriority), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PPVolume_SetGlobal(uint64, bool)",
        asFUNCTION(PPVolume_SetGlobal), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PPVolume_IsGlobal(uint64)",
        asFUNCTION(PPVolume_IsGlobal), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
