#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/Effects/Water.h"
#include "Enjin/ECS/Components/Water3D.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// Interactive water interaction API (splash / wake / sustained pressure / query)
//
// These drive the height-field water on an entity that carries an
// InteractiveWaterComponent (+ TransformComponent). The interaction methods are
// stateless with respect to the system, so a local InteractiveWaterSystem is used
// to reach them without threading a system pointer through the bindings.
// ============================================================================

static Effects::InteractiveWaterComponent* WaterOf(u64 entity, ECS::TransformComponent** outT) {
    if (!s_BindingsWorld) return nullptr;
    auto* w = s_BindingsWorld->GetComponent<Effects::InteractiveWaterComponent>(entity);
    auto* t = s_BindingsWorld->GetComponent<ECS::TransformComponent>(entity);
    if (!w || !t) return nullptr;
    *outT = t;
    return w;
}

static void Water_Splash(u64 entity, float x, float z, float strength) {
    ECS::TransformComponent* t = nullptr;
    auto* w = WaterOf(entity, &t);
    if (!w) return;
    Effects::InteractiveWaterSystem sys;
    sys.CreateSplash(*w, *t, x, z, strength);
}

static void Water_Wake(u64 entity, float x, float z, float velX, float velZ, float wakeWidth) {
    ECS::TransformComponent* t = nullptr;
    auto* w = WaterOf(entity, &t);
    if (!w) return;
    Effects::InteractiveWaterSystem sys;
    sys.CreateWake(*w, *t, x, z, velX, velZ, wakeWidth);
}

static void Water_SustainedPressure(u64 entity, float x, float z, float radius, float force) {
    ECS::TransformComponent* t = nullptr;
    auto* w = WaterOf(entity, &t);
    if (!w) return;
    Effects::InteractiveWaterSystem sys;
    sys.ApplySustainedPressure(*w, *t, x, z, radius, force);
}

static float Water_GetHeight(u64 entity, float x, float z) {
    ECS::TransformComponent* t = nullptr;
    auto* w = WaterOf(entity, &t);
    if (!w) return 0.0f;
    Effects::InteractiveWaterSystem sys;
    return sys.GetWaterHeight(*w, *t, x, z);
}


// ============================================================================
// Water3D: the animated Gerstner surface
//
// PlayMode copies the COMPONENT's settings into the shared Water3D instance
// every frame before rebuilding the mesh, so anything written to that instance
// is gone on the next tick. (The VisualScript water setters write to the
// instance and are silently undone by exactly this.) These setters therefore
// write the component, which is the thing that actually persists.
//
// Water3D_GetHeight is the important one: it asks the SAME object that displaces
// the mesh, using the same wave clock, so a boat sampling it sits on the surface
// that is really being drawn instead of on a copy of the formula that drifts.
// ============================================================================

extern Enjin::Effects::Water3D* s_VisualScriptWater;

static ECS::Water3DComponent* W3D(u64 entity) {
    if (!s_BindingsWorld) return nullptr;
    return s_BindingsWorld->GetComponent<ECS::Water3DComponent>(static_cast<ECS::Entity>(entity));
}

static bool Water3D_Has(u64 entity) { return W3D(entity) != nullptr; }

// Surface height at a world XZ. Gerstner also shifts vertices HORIZONTALLY, and
// inverting that needs iteration, so this is the vertical term only - the same
// approximation the engine uses for its own buoyancy.
static float Water3D_GetHeight(float x, float z) {
    return s_VisualScriptWater ? s_VisualScriptWater->GetWaveHeight(x, z) : 0.0f;
}

static void Water3D_SetWaveHeight(u64 e, float v) { if (auto* w = W3D(e)) w->settings.waveHeight = v; }
static float Water3D_GetWaveHeight(u64 e) { auto* w = W3D(e); return w ? w->settings.waveHeight : 0.0f; }
static void Water3D_SetWaveSpeed(u64 e, float v) { if (auto* w = W3D(e)) w->settings.waveSpeed = v; }
static void Water3D_SetWaveFrequency(u64 e, float v) { if (auto* w = W3D(e)) w->settings.waveFrequency = v; }
static void Water3D_SetWaveDirection(u64 e, float x, float z) {
    if (auto* w = W3D(e)) w->settings.waveDirection = Math::Vector2(x, z);
}

// Trochoidal crests. Steepness 0 is a plain sine swell, 1 is as sharp as the
// surface can go before it folds through itself.
static void Water3D_SetGerstner(u64 e, bool on, float steepness) {
    if (auto* w = W3D(e)) {
        w->settings.gerstnerWaves = on;
        w->settings.waveSteepness = Math::Clamp(steepness, 0.0f, 1.0f);
    }
}

// WaterStyle: 0 Flat, 1 Animated, 2 VertexWave, 3 Reflective, 4 Refractive.
// Gerstner only displaces on 2, 3 and 4.
static void Water3D_SetStyle(u64 e, int style) {
    if (auto* w = W3D(e)) {
        if (style >= 0 && style <= 4) w->settings.style = static_cast<Effects::WaterStyle>(style);
    }
}

static void Water3D_SetShallowColor(u64 e, const Math::Vector3& c) { if (auto* w = W3D(e)) w->settings.shallowColor = c; }
static void Water3D_SetDeepColor(u64 e, const Math::Vector3& c) { if (auto* w = W3D(e)) w->settings.deepColor = c; }
static void Water3D_SetOpacity(u64 e, float v) { if (auto* w = W3D(e)) w->settings.opacity = Math::Clamp(v, 0.0f, 1.0f); }
static void Water3D_SetFoam(u64 e, bool on, float threshold, float scale) {
    if (auto* w = W3D(e)) {
        w->settings.enableFoam = on;
        w->settings.foamThreshold = threshold;
        w->settings.foamScale = scale;
    }
}
static void Water3D_SetReflection(u64 e, float strength, float fresnelPower) {
    if (auto* w = W3D(e)) {
        w->settings.reflectionStrength = strength;
        w->settings.fresnelPower = fresnelPower;
    }
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterWaterBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction("void Water_Splash(uint64, float, float, float)",
        ENJIN_AS_FN(Water_Splash), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water_Wake(uint64, float, float, float, float, float)",
        ENJIN_AS_FN(Water_Wake), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water_SustainedPressure(uint64, float, float, float, float)",
        ENJIN_AS_FN(Water_SustainedPressure), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Water_GetHeight(uint64, float, float)",
        ENJIN_AS_FN(Water_GetHeight), ENJIN_AS_CALL_CDECL));

    // Water3D animated surface
    AS_CHECK(engine->RegisterGlobalFunction("bool Water3D_Has(uint64)",
        ENJIN_AS_FN(Water3D_Has), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Water3D_GetHeight(float, float)",
        ENJIN_AS_FN(Water3D_GetHeight), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetWaveHeight(uint64, float)",
        ENJIN_AS_FN(Water3D_SetWaveHeight), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Water3D_GetWaveHeight(uint64)",
        ENJIN_AS_FN(Water3D_GetWaveHeight), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetWaveSpeed(uint64, float)",
        ENJIN_AS_FN(Water3D_SetWaveSpeed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetWaveFrequency(uint64, float)",
        ENJIN_AS_FN(Water3D_SetWaveFrequency), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetWaveDirection(uint64, float, float)",
        ENJIN_AS_FN(Water3D_SetWaveDirection), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetGerstner(uint64, bool, float)",
        ENJIN_AS_FN(Water3D_SetGerstner), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetStyle(uint64, int)",
        ENJIN_AS_FN(Water3D_SetStyle), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetShallowColor(uint64, const Vector3 &in)",
        ENJIN_AS_FN(Water3D_SetShallowColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetDeepColor(uint64, const Vector3 &in)",
        ENJIN_AS_FN(Water3D_SetDeepColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetOpacity(uint64, float)",
        ENJIN_AS_FN(Water3D_SetOpacity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetFoam(uint64, bool, float, float)",
        ENJIN_AS_FN(Water3D_SetFoam), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Water3D_SetReflection(uint64, float, float)",
        ENJIN_AS_FN(Water3D_SetReflection), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
