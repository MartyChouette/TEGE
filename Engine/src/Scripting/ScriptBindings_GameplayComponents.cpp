#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/ReflectionProbe.h"
#include <angelscript.h>
#include <string>
#include <cctype>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// Script access for gameplay/visual components that had no bindings — closing
// the script-vs-C++ parity gap (docs/SCRIPTING_PARITY.md). Each function looks
// up the component on the entity and returns/sets a field, matching the existing
// component-binding style. Missing component = safe no-op / default return.
// ============================================================================

// --- LookAtTarget: make an entity rotate to face a target -------------------
static void LookAt_SetTarget(u64 self, u64 target) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LookAtTargetComponent>(self)) {
        c->target = static_cast<ECS::Entity>(target);
        c->useWorldTarget = false;
    }
}
static void LookAt_SetTargetPosition(u64 self, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LookAtTargetComponent>(self)) {
        c->worldTarget = Math::Vector3(x, y, z);
        c->useWorldTarget = true;
    }
}
static void LookAt_ClearTarget(u64 self) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LookAtTargetComponent>(self)) c->target = 0;
}
static void LookAt_SetSpeed(u64 self, float degPerSec) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LookAtTargetComponent>(self)) c->rotationSpeed = degPerSec;
}
static float LookAt_GetSpeed(u64 self) {
    if (!s_BindingsWorld) return 0.0f;
    auto* c = s_BindingsWorld->GetComponent<ECS::LookAtTargetComponent>(self);
    return c ? c->rotationSpeed : 0.0f;
}
static void LookAt_SetInstant(u64 self, bool instant) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LookAtTargetComponent>(self)) c->instant = instant;
}
static void LookAt_SetConstraints(u64 self, bool x, bool y, bool z) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LookAtTargetComponent>(self)) {
        c->constrainX = x; c->constrainY = y; c->constrainZ = z;
    }
}

// --- DamageResistance: per-damage-type multipliers --------------------------
static f32* ResistField(ECS::DamageResistanceComponent* c, const std::string& type) {
    std::string s; s.reserve(type.size());
    for (char ch : type) s += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (s == "physical") return &c->physicalMult;
    if (s == "fire")     return &c->fireMult;
    if (s == "ice")      return &c->iceMult;
    if (s == "electric") return &c->electricMult;
    if (s == "poison")   return &c->poisonMult;
    if (s == "magic")    return &c->magicMult;
    return nullptr;
}
static void DamageResist_Set(u64 self, const std::string& type, float mult) {
    if (!s_BindingsWorld) return;
    auto* c = s_BindingsWorld->GetComponent<ECS::DamageResistanceComponent>(self);
    if (!c) return;
    if (f32* f = ResistField(c, type)) *f = mult;
}
static float DamageResist_Get(u64 self, const std::string& type) {
    if (!s_BindingsWorld) return 1.0f;
    auto* c = s_BindingsWorld->GetComponent<ECS::DamageResistanceComponent>(self);
    if (!c) return 1.0f;
    f32* f = ResistField(c, type);
    return f ? *f : 1.0f;
}

// --- Ragdoll: activate physics-driven bodies --------------------------------
static void Ragdoll_SetActive(u64 self, bool active) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::RagdollComponent>(self)) c->enabled = active;
}
static bool Ragdoll_IsActive(u64 self) {
    if (!s_BindingsWorld) return false;
    auto* c = s_BindingsWorld->GetComponent<ECS::RagdollComponent>(self);
    return c ? c->enabled : false;
}
static void Ragdoll_SetBlendWeight(u64 self, float w) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::RagdollComponent>(self)) c->blendWeight = w;
}
static void Ragdoll_SetGravityScale(u64 self, float g) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::RagdollComponent>(self)) c->gravityScale = g;
}

// --- Pushable: block-pushing objects ----------------------------------------
static void Pushable_SetAxes(u64 self, bool x, bool y, bool z) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::PushableComponent>(self)) {
        c->pushableX = x; c->pushableY = y; c->pushableZ = z;
    }
}
static void Pushable_SetPushSpeed(u64 self, float speed) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::PushableComponent>(self)) c->pushSpeed = speed;
}
static bool Pushable_IsBeingPushed(u64 self) {
    if (!s_BindingsWorld) return false;
    auto* c = s_BindingsWorld->GetComponent<ECS::PushableComponent>(self);
    return c ? c->isBeingPushed : false;
}

// --- TemperatureZone: hot/cold regions --------------------------------------
static void TempZone_SetTemperature(u64 self, float t) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::TemperatureZoneComponent>(self)) c->temperature = t;
}
static float TempZone_GetTemperature(u64 self) {
    if (!s_BindingsWorld) return 0.0f;
    auto* c = s_BindingsWorld->GetComponent<ECS::TemperatureZoneComponent>(self);
    return c ? c->temperature : 0.0f;
}
static void TempZone_SetPriority(u64 self, int p) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::TemperatureZoneComponent>(self)) c->priority = p;
}

// --- ReflectionProbe: environment reflections -------------------------------
static void ReflectionProbe_SetIntensity(u64 self, float i) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::ReflectionProbeComponent>(self)) c->intensity = i;
}
static float ReflectionProbe_GetIntensity(u64 self) {
    if (!s_BindingsWorld) return 0.0f;
    auto* c = s_BindingsWorld->GetComponent<ECS::ReflectionProbeComponent>(self);
    return c ? c->intensity : 0.0f;
}
static void ReflectionProbe_SetActive(u64 self, bool a) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::ReflectionProbeComponent>(self)) c->isActive = a;
}
static bool ReflectionProbe_IsActive(u64 self) {
    if (!s_BindingsWorld) return false;
    auto* c = s_BindingsWorld->GetComponent<ECS::ReflectionProbeComponent>(self);
    return c ? c->isActive : false;
}

// --- Billboard: camera-facing quads -----------------------------------------
static void Billboard_SetFaceCamera(u64 self, bool face) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::BillboardComponent>(self)) c->faceCamera = face;
}
static void Billboard_SetLockY(u64 self, bool lock) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::BillboardComponent>(self)) c->lockY = lock;
}
static void Billboard_SetRotationOffset(u64 self, float deg) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::BillboardComponent>(self)) c->rotationOffset = deg;
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterGameplayComponentBindings(asIScriptEngine* engine) {
    // LookAtTarget
    AS_CHECK(engine->RegisterGlobalFunction("void LookAt_SetTarget(uint64, uint64)", ENJIN_AS_FN(LookAt_SetTarget), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void LookAt_SetTargetPosition(uint64, float, float, float)", ENJIN_AS_FN(LookAt_SetTargetPosition), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void LookAt_ClearTarget(uint64)", ENJIN_AS_FN(LookAt_ClearTarget), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void LookAt_SetSpeed(uint64, float)", ENJIN_AS_FN(LookAt_SetSpeed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float LookAt_GetSpeed(uint64)", ENJIN_AS_FN(LookAt_GetSpeed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void LookAt_SetInstant(uint64, bool)", ENJIN_AS_FN(LookAt_SetInstant), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void LookAt_SetConstraints(uint64, bool, bool, bool)", ENJIN_AS_FN(LookAt_SetConstraints), ENJIN_AS_CALL_CDECL));

    // DamageResistance (type: "physical"/"fire"/"ice"/"electric"/"poison"/"magic")
    AS_CHECK(engine->RegisterGlobalFunction("void DamageResist_Set(uint64, const string &in, float)", ENJIN_AS_FN(DamageResist_Set), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float DamageResist_Get(uint64, const string &in)", ENJIN_AS_FN(DamageResist_Get), ENJIN_AS_CALL_CDECL));

    // Ragdoll
    AS_CHECK(engine->RegisterGlobalFunction("void Ragdoll_SetActive(uint64, bool)", ENJIN_AS_FN(Ragdoll_SetActive), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Ragdoll_IsActive(uint64)", ENJIN_AS_FN(Ragdoll_IsActive), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Ragdoll_SetBlendWeight(uint64, float)", ENJIN_AS_FN(Ragdoll_SetBlendWeight), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Ragdoll_SetGravityScale(uint64, float)", ENJIN_AS_FN(Ragdoll_SetGravityScale), ENJIN_AS_CALL_CDECL));

    // Pushable
    AS_CHECK(engine->RegisterGlobalFunction("void Pushable_SetAxes(uint64, bool, bool, bool)", ENJIN_AS_FN(Pushable_SetAxes), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Pushable_SetPushSpeed(uint64, float)", ENJIN_AS_FN(Pushable_SetPushSpeed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Pushable_IsBeingPushed(uint64)", ENJIN_AS_FN(Pushable_IsBeingPushed), ENJIN_AS_CALL_CDECL));

    // TemperatureZone
    AS_CHECK(engine->RegisterGlobalFunction("void TempZone_SetTemperature(uint64, float)", ENJIN_AS_FN(TempZone_SetTemperature), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float TempZone_GetTemperature(uint64)", ENJIN_AS_FN(TempZone_GetTemperature), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void TempZone_SetPriority(uint64, int)", ENJIN_AS_FN(TempZone_SetPriority), ENJIN_AS_CALL_CDECL));

    // ReflectionProbe
    AS_CHECK(engine->RegisterGlobalFunction("void ReflectionProbe_SetIntensity(uint64, float)", ENJIN_AS_FN(ReflectionProbe_SetIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float ReflectionProbe_GetIntensity(uint64)", ENJIN_AS_FN(ReflectionProbe_GetIntensity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void ReflectionProbe_SetActive(uint64, bool)", ENJIN_AS_FN(ReflectionProbe_SetActive), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool ReflectionProbe_IsActive(uint64)", ENJIN_AS_FN(ReflectionProbe_IsActive), ENJIN_AS_CALL_CDECL));

    // Billboard
    AS_CHECK(engine->RegisterGlobalFunction("void Billboard_SetFaceCamera(uint64, bool)", ENJIN_AS_FN(Billboard_SetFaceCamera), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Billboard_SetLockY(uint64, bool)", ENJIN_AS_FN(Billboard_SetLockY), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Billboard_SetRotationOffset(uint64, float)", ENJIN_AS_FN(Billboard_SetRotationOffset), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
