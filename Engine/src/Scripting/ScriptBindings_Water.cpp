#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Effects/InteractiveWater.h"
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
}

} // namespace Scripting
} // namespace Enjin
