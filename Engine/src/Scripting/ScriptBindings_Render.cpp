#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Effects/GPUParticleTypes.h"
#include "Enjin/Renderer/PostProcessing.h"
#include <cctype>
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Platform/Platform.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;

#if ENJIN_RENDERER_WEBGPU
// The Vulkan PostProcessing manager class doesn't exist on web builds (only
// PostProcessSettings does), but this file MUST compile there: every script-
// visible symbol has to be registered on every platform or an .as module that
// mentions one fails to compile wholesale (this exact failure shipped — the
// Playground script died on web with "No matching symbol 'Render_SetRainActive'").
// The wrappers only touch GetSettings(), and SetBindingsPostProcessing is never
// wired on web (pointer stays null), so this stand-in exists purely to satisfy
// the compiler. The settings type is the REAL struct, so there is no field drift.
namespace Enjin { namespace Renderer {
class PostProcessing {
public:
    PostProcessSettings& GetSettings() { return m_Settings; }
private:
    PostProcessSettings m_Settings;
};
}}
#endif

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

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

// ============================================================================
// Script render targets (FR-4) — live camera→texture from script.
// Vulkan-only for now; on web these return 0/false so scripts degrade cleanly.
// ============================================================================

static u64 RenderTarget_Create(int width, int height) {
#if !ENJIN_RENDERER_WEBGPU
    if (!s_BindingsRenderSystem || width <= 0 || height <= 0) return 0;
    return s_BindingsRenderSystem->CreateScriptRenderTarget(
        static_cast<u32>(width), static_cast<u32>(height));
#else
    (void)width; (void)height;
    return 0;
#endif
}

static void RenderTarget_Destroy(u64 handle) {
#if !ENJIN_RENDERER_WEBGPU
    if (s_BindingsRenderSystem) s_BindingsRenderSystem->DestroyScriptRenderTarget(handle);
#else
    (void)handle;
#endif
}

static void RenderTarget_SetCamera(u64 handle, u64 cameraEntity) {
#if !ENJIN_RENDERER_WEBGPU
    if (s_BindingsRenderSystem)
        s_BindingsRenderSystem->SetScriptRenderTargetCamera(handle, cameraEntity);
#else
    (void)handle; (void)cameraEntity;
#endif
}

static bool RenderTarget_BindToEntity(u64 handle, u64 entity) {
#if !ENJIN_RENDERER_WEBGPU
    if (!s_BindingsRenderSystem) return false;
    return s_BindingsRenderSystem->BindScriptRenderTargetToEntity(
        handle, static_cast<ECS::Entity>(entity));
#else
    (void)handle; (void)entity;
    return false;
#endif
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
    s_BindingsPostProcessing->GetSettings().godRaysSamples = static_cast<u32>(std::clamp(v, 1, 256));
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
// Fire-and-forget particle one-shots (sparks, hits, pickups) at a world point.
static Effects::GPUParticlePreset ParseParticlePreset(const std::string& name) {
    std::string s; s.reserve(name.size());
    for (char c : name) s += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    using P = Effects::GPUParticlePreset;
    if (s == "spark" || s == "sparks")                 return P::Sparks;
    if (s == "impact" || s == "hit" || s == "collision") return P::Impact;
    if (s == "pickup" || s == "collect" || s == "coin")  return P::Pickup;
    if (s == "smoke")  return P::Smoke;
    if (s == "fire")   return P::Fire;
    if (s == "blood")  return P::Blood;
    if (s == "mist")   return P::Mist;
    if (s == "spray")  return P::Spray;
    if (s == "dust")   return P::Dust;
    if (s == "magic")  return P::Magic;
    if (s == "snow")   return P::Snow;
    if (s == "liquid") return P::Liquid;
    return P::Sparks;
}

// Drop `count` particles styled by `preset` at (x,y,z). No entity, no cleanup —
// they die by their own lifetime. Great for hits, sparks, and item pickups.
static void Particles_OneShot(const std::string& preset, float x, float y, float z, int count) {
    if (!s_BindingsRenderSystem || count <= 0) return;
    s_BindingsRenderSystem->SpawnGPUParticlePreset(
        static_cast<u32>(count), Math::Vector3(x, y, z), Math::Vector3(0.0f, 1.0f, 0.0f),
        ParseParticlePreset(preset));   // web: RenderSystem stub, no-op
}

// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterRenderBindings(asIScriptEngine* engine) {
    // ---- Shadows ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetShadowsEnabled(bool)",
        ENJIN_AS_FN(Render_SetShadowsEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Render_IsShadowsEnabled()",
        ENJIN_AS_FN(Render_IsShadowsEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetShadowDistance(float)",
        ENJIN_AS_FN(Render_SetShadowDistance), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetShadowDistance()",
        ENJIN_AS_FN(Render_GetShadowDistance), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetShadowStrength(float)",
        ENJIN_AS_FN(Render_SetShadowStrength), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetShadowStrength()",
        ENJIN_AS_FN(Render_GetShadowStrength), ENJIN_AS_CALL_CDECL));

    // ---- Script render targets (FR-4) ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 RenderTarget_Create(int width, int height)",
        ENJIN_AS_FN(RenderTarget_Create), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void RenderTarget_Destroy(uint64 handle)",
        ENJIN_AS_FN(RenderTarget_Destroy), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void RenderTarget_SetCamera(uint64 handle, uint64 cameraEntity)",
        ENJIN_AS_FN(RenderTarget_SetCamera), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool RenderTarget_BindToEntity(uint64 handle, uint64 entity)",
        ENJIN_AS_FN(RenderTarget_BindToEntity), ENJIN_AS_CALL_CDECL));

    // ---- Ambient ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetAmbientIntensity(float)",
        ENJIN_AS_FN(Render_SetAmbientIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetAmbientIntensity()",
        ENJIN_AS_FN(Render_GetAmbientIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetAmbientColor(const Vector3 &in)",
        ENJIN_AS_FN(Render_SetAmbientColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Render_GetAmbientColor()",
        ENJIN_AS_FN(Render_GetAmbientColor), ENJIN_AS_CALL_CDECL));

    // ---- Fog ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetFogDensity(float)",
        ENJIN_AS_FN(Render_SetFogDensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetFogDensity()",
        ENJIN_AS_FN(Render_GetFogDensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetFogColor(const Vector3 &in)",
        ENJIN_AS_FN(Render_SetFogColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Render_GetFogColor()",
        ENJIN_AS_FN(Render_GetFogColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetFogStart(float)",
        ENJIN_AS_FN(Render_SetFogStart), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetFogStart()",
        ENJIN_AS_FN(Render_GetFogStart), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetFogEnd(float)",
        ENJIN_AS_FN(Render_SetFogEnd), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetFogEnd()",
        ENJIN_AS_FN(Render_GetFogEnd), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetFogHeightFalloff(float)",
        ENJIN_AS_FN(Render_SetFogHeightFalloff), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetFogHeightFalloff()",
        ENJIN_AS_FN(Render_GetFogHeightFalloff), ENJIN_AS_CALL_CDECL));

    // ---- Snow, Curvature, Wireframe, Rain ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetSnowIntensity(float)",
        ENJIN_AS_FN(Render_SetSnowIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetSnowIntensity()",
        ENJIN_AS_FN(Render_GetSnowIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetWorldCurvature(float)",
        ENJIN_AS_FN(Render_SetWorldCurvature), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Render_GetWorldCurvature()",
        ENJIN_AS_FN(Render_GetWorldCurvature), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetWireframeEnabled(bool)",
        ENJIN_AS_FN(Render_SetWireframeEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Render_IsWireframeEnabled()",
        ENJIN_AS_FN(Render_IsWireframeEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Render_SetRainActive(bool)",
        ENJIN_AS_FN(Render_SetRainActive), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Render_IsRainActive()",
        ENJIN_AS_FN(Render_IsRainActive), ENJIN_AS_CALL_CDECL));

    // ---- PostProcess: Tone Mapping & Exposure ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetToneMapping(int)",
        ENJIN_AS_FN(PostProcess_SetToneMapping), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int PostProcess_GetToneMapping()",
        ENJIN_AS_FN(PostProcess_GetToneMapping), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetExposure(float)",
        ENJIN_AS_FN(PostProcess_SetExposure), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetExposure()",
        ENJIN_AS_FN(PostProcess_GetExposure), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetGamma(float)",
        ENJIN_AS_FN(PostProcess_SetGamma), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetGamma()",
        ENJIN_AS_FN(PostProcess_GetGamma), ENJIN_AS_CALL_CDECL));

    // ---- PostProcess: Bloom ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetBloomEnabled(bool)",
        ENJIN_AS_FN(PostProcess_SetBloomEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsBloomEnabled()",
        ENJIN_AS_FN(PostProcess_IsBloomEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetBloomThreshold(float)",
        ENJIN_AS_FN(PostProcess_SetBloomThreshold), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetBloomThreshold()",
        ENJIN_AS_FN(PostProcess_GetBloomThreshold), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetBloomIntensity(float)",
        ENJIN_AS_FN(PostProcess_SetBloomIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetBloomIntensity()",
        ENJIN_AS_FN(PostProcess_GetBloomIntensity), ENJIN_AS_CALL_CDECL));

    // ---- PostProcess: Vignette ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetVignetteEnabled(bool)",
        ENJIN_AS_FN(PostProcess_SetVignetteEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsVignetteEnabled()",
        ENJIN_AS_FN(PostProcess_IsVignetteEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetVignetteIntensity(float)",
        ENJIN_AS_FN(PostProcess_SetVignetteIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetVignetteIntensity()",
        ENJIN_AS_FN(PostProcess_GetVignetteIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetVignetteSmoothness(float)",
        ENJIN_AS_FN(PostProcess_SetVignetteSmoothness), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetVignetteSmoothness()",
        ENJIN_AS_FN(PostProcess_GetVignetteSmoothness), ENJIN_AS_CALL_CDECL));

    // ---- PostProcess: Chromatic Aberration ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetChromaticAberrationEnabled(bool)",
        ENJIN_AS_FN(PostProcess_SetChromaticAberrationEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsChromaticAberrationEnabled()",
        ENJIN_AS_FN(PostProcess_IsChromaticAberrationEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetChromaticAberrationIntensity(float)",
        ENJIN_AS_FN(PostProcess_SetChromaticAberrationIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetChromaticAberrationIntensity()",
        ENJIN_AS_FN(PostProcess_GetChromaticAberrationIntensity), ENJIN_AS_CALL_CDECL));

    // ---- PostProcess: Color Grading ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetColorFilter(const Vector3 &in)",
        ENJIN_AS_FN(PostProcess_SetColorFilter), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 PostProcess_GetColorFilter()",
        ENJIN_AS_FN(PostProcess_GetColorFilter), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetSaturation(float)",
        ENJIN_AS_FN(PostProcess_SetSaturation), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetSaturation()",
        ENJIN_AS_FN(PostProcess_GetSaturation), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetContrast(float)",
        ENJIN_AS_FN(PostProcess_SetContrast), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetContrast()",
        ENJIN_AS_FN(PostProcess_GetContrast), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetBrightness(float)",
        ENJIN_AS_FN(PostProcess_SetBrightness), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetBrightness()",
        ENJIN_AS_FN(PostProcess_GetBrightness), ENJIN_AS_CALL_CDECL));

    // ---- PostProcess: Film Grain ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFilmGrainEnabled(bool)",
        ENJIN_AS_FN(PostProcess_SetFilmGrainEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsFilmGrainEnabled()",
        ENJIN_AS_FN(PostProcess_IsFilmGrainEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFilmGrainIntensity(float)",
        ENJIN_AS_FN(PostProcess_SetFilmGrainIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetFilmGrainIntensity()",
        ENJIN_AS_FN(PostProcess_GetFilmGrainIntensity), ENJIN_AS_CALL_CDECL));

    // ---- PostProcess: FXAA ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFXAAEnabled(bool)",
        ENJIN_AS_FN(PostProcess_SetFXAAEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsFXAAEnabled()",
        ENJIN_AS_FN(PostProcess_IsFXAAEnabled), ENJIN_AS_CALL_CDECL));

    // ---- Screen-Space Effects: God Rays ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetGodRaysEnabled(bool)",
        ENJIN_AS_FN(PostProcess_SetGodRaysEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsGodRaysEnabled()",
        ENJIN_AS_FN(PostProcess_IsGodRaysEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetGodRaysIntensity(float)",
        ENJIN_AS_FN(PostProcess_SetGodRaysIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetGodRaysIntensity()",
        ENJIN_AS_FN(PostProcess_GetGodRaysIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetGodRaysSamples(int)",
        ENJIN_AS_FN(PostProcess_SetGodRaysSamples), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int PostProcess_GetGodRaysSamples()",
        ENJIN_AS_FN(PostProcess_GetGodRaysSamples), ENJIN_AS_CALL_CDECL));

    // ---- Screen-Space Effects: SSAO ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetSSAOEnabled(bool)",
        ENJIN_AS_FN(PostProcess_SetSSAOEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsSSAOEnabled()",
        ENJIN_AS_FN(PostProcess_IsSSAOEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetSSAORadius(float)",
        ENJIN_AS_FN(PostProcess_SetSSAORadius), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetSSAORadius()",
        ENJIN_AS_FN(PostProcess_GetSSAORadius), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetSSAOIntensity(float)",
        ENJIN_AS_FN(PostProcess_SetSSAOIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetSSAOIntensity()",
        ENJIN_AS_FN(PostProcess_GetSSAOIntensity), ENJIN_AS_CALL_CDECL));

    // ---- Screen-Space Effects: Contact Shadows ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetContactShadowsEnabled(bool)",
        ENJIN_AS_FN(PostProcess_SetContactShadowsEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsContactShadowsEnabled()",
        ENJIN_AS_FN(PostProcess_IsContactShadowsEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetContactShadowsIntensity(float)",
        ENJIN_AS_FN(PostProcess_SetContactShadowsIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetContactShadowsIntensity()",
        ENJIN_AS_FN(PostProcess_GetContactShadowsIntensity), ENJIN_AS_CALL_CDECL));

    // ---- Screen-Space Effects: Caustics ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetCausticsEnabled(bool)",
        ENJIN_AS_FN(PostProcess_SetCausticsEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsCausticsEnabled()",
        ENJIN_AS_FN(PostProcess_IsCausticsEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetCausticsIntensity(float)",
        ENJIN_AS_FN(PostProcess_SetCausticsIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetCausticsIntensity()",
        ENJIN_AS_FN(PostProcess_GetCausticsIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetCausticsWaterY(float)",
        ENJIN_AS_FN(PostProcess_SetCausticsWaterY), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetCausticsWaterY()",
        ENJIN_AS_FN(PostProcess_GetCausticsWaterY), ENJIN_AS_CALL_CDECL));

    // ---- Screen-Space Effects: Fog Shafts ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFogShaftsEnabled(bool)",
        ENJIN_AS_FN(PostProcess_SetFogShaftsEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PostProcess_IsFogShaftsEnabled()",
        ENJIN_AS_FN(PostProcess_IsFogShaftsEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFogShaftsIntensity(float)",
        ENJIN_AS_FN(PostProcess_SetFogShaftsIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetFogShaftsIntensity()",
        ENJIN_AS_FN(PostProcess_GetFogShaftsIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PostProcess_SetFogShaftsMaxDistance(float)",
        ENJIN_AS_FN(PostProcess_SetFogShaftsMaxDistance), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PostProcess_GetFogShaftsMaxDistance()",
        ENJIN_AS_FN(PostProcess_GetFogShaftsMaxDistance), ENJIN_AS_CALL_CDECL));

    // ---- Post-Process Volumes ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PPVolume_SetActive(uint64, bool)",
        ENJIN_AS_FN(PPVolume_SetActive), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PPVolume_IsActive(uint64)",
        ENJIN_AS_FN(PPVolume_IsActive), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PPVolume_SetWeight(uint64, float)",
        ENJIN_AS_FN(PPVolume_SetWeight), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PPVolume_GetWeight(uint64)",
        ENJIN_AS_FN(PPVolume_GetWeight), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PPVolume_SetBlendRadius(uint64, float)",
        ENJIN_AS_FN(PPVolume_SetBlendRadius), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float PPVolume_GetBlendRadius(uint64)",
        ENJIN_AS_FN(PPVolume_GetBlendRadius), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PPVolume_SetPriority(uint64, int)",
        ENJIN_AS_FN(PPVolume_SetPriority), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int PPVolume_GetPriority(uint64)",
        ENJIN_AS_FN(PPVolume_GetPriority), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void PPVolume_SetGlobal(uint64, bool)",
        ENJIN_AS_FN(PPVolume_SetGlobal), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool PPVolume_IsGlobal(uint64)",
        ENJIN_AS_FN(PPVolume_IsGlobal), ENJIN_AS_CALL_CDECL));

    // One-shot particle effects at a world point ("spark"/"impact"/"pickup"/...)
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Particles_OneShot(const string &in preset, float x, float y, float z, int count)",
        ENJIN_AS_FN(Particles_OneShot), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
