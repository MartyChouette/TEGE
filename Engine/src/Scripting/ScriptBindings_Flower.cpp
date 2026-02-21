#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

// Use shared world pointer from ScriptBindings.cpp (at global scope, uses 'using namespace Enjin')
extern ECS::World* s_BindingsWorld;
static ECS::World*& s_World = s_BindingsWorld;

namespace Enjin {
namespace Scripting {

void SetBindingsFlower(ECS::World* /*world*/) {
    // World pointer is now shared via s_BindingsWorld (set by SetBindingsWorld)
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Tether queries
// ============================================================================

static float Flower_GetTension(u64 entity) {
    if (!s_World) return 0.0f;
    auto* tether = s_World->GetComponent<ECS::TetherComponent>(static_cast<ECS::Entity>(entity));
    return tether ? tether->currentTension : 0.0f;
}

static bool Flower_IsBroken(u64 entity) {
    if (!s_World) return false;
    auto* tether = s_World->GetComponent<ECS::TetherComponent>(static_cast<ECS::Entity>(entity));
    return tether ? tether->isBroken : false;
}

static bool Flower_JustBroke(u64 entity) {
    if (!s_World) return false;
    auto* tether = s_World->GetComponent<ECS::TetherComponent>(static_cast<ECS::Entity>(entity));
    return tether ? tether->justBroke : false;
}

static void Flower_SetMaxDistance(u64 entity, float dist) {
    if (!s_World) return;
    auto* tether = s_World->GetComponent<ECS::TetherComponent>(static_cast<ECS::Entity>(entity));
    if (tether) tether->maxDistance = dist;
}

static float Flower_GetMaxDistance(u64 entity) {
    if (!s_World) return 0.0f;
    auto* tether = s_World->GetComponent<ECS::TetherComponent>(static_cast<ECS::Entity>(entity));
    return tether ? tether->maxDistance : 0.0f;
}

static void Flower_SetSpringK(u64 entity, float k) {
    if (!s_World) return;
    auto* tether = s_World->GetComponent<ECS::TetherComponent>(static_cast<ECS::Entity>(entity));
    if (tether) tether->autoSpringK = k;
}

static void Flower_SetDamping(u64 entity, float d) {
    if (!s_World) return;
    auto* tether = s_World->GetComponent<ECS::TetherComponent>(static_cast<ECS::Entity>(entity));
    if (tether) tether->autoDamping = d;
}

// ============================================================================
// Grabbable queries
// ============================================================================

static bool Flower_IsGrabbed(u64 entity) {
    if (!s_World) return false;
    auto* grab = s_World->GetComponent<ECS::GrabbableComponent>(static_cast<ECS::Entity>(entity));
    return grab ? grab->isGrabbed : false;
}

static void Flower_SetGrabRadius(u64 entity, float radius) {
    if (!s_World) return;
    auto* grab = s_World->GetComponent<ECS::GrabbableComponent>(static_cast<ECS::Entity>(entity));
    if (grab) grab->grabRadius = radius;
}

static float Flower_GetGrabRadius(u64 entity) {
    if (!s_World) return 0.0f;
    auto* grab = s_World->GetComponent<ECS::GrabbableComponent>(static_cast<ECS::Entity>(entity));
    return grab ? grab->grabRadius : 0.0f;
}

static void Flower_SetGrabSpring(u64 entity, float spring) {
    if (!s_World) return;
    auto* grab = s_World->GetComponent<ECS::GrabbableComponent>(static_cast<ECS::Entity>(entity));
    if (grab) grab->grabSpring = spring;
}

static float Flower_GetGrabSpring(u64 entity) {
    if (!s_World) return 0.0f;
    auto* grab = s_World->GetComponent<ECS::GrabbableComponent>(static_cast<ECS::Entity>(entity));
    return grab ? grab->grabSpring : 0.0f;
}

// ============================================================================
// Stem / scoring
// ============================================================================

static float Flower_GetScore(u64 entity) {
    if (!s_World) return 0.0f;
    auto* stem = s_World->GetComponent<ECS::FlowerStemComponent>(static_cast<ECS::Entity>(entity));
    return stem ? stem->score : 0.0f;
}

static int Flower_GetPartsRemoved(u64 entity) {
    if (!s_World) return 0;
    auto* stem = s_World->GetComponent<ECS::FlowerStemComponent>(static_cast<ECS::Entity>(entity));
    return stem ? stem->partsRemoved : 0;
}

static bool Flower_IsEvaluated(u64 entity) {
    if (!s_World) return false;
    auto* stem = s_World->GetComponent<ECS::FlowerStemComponent>(static_cast<ECS::Entity>(entity));
    return stem ? stem->evaluated : false;
}

static void Flower_SetSapColor(u64 entity, float r, float g, float b) {
    if (!s_World) return;
    auto* stem = s_World->GetComponent<ECS::FlowerStemComponent>(static_cast<ECS::Entity>(entity));
    if (stem) stem->sapColor = Math::Vector3(r, g, b);
}

static void Flower_SetLiquidIntensity(u64 entity, float intensity) {
    if (!s_World) return;
    auto* stem = s_World->GetComponent<ECS::FlowerStemComponent>(static_cast<ECS::Entity>(entity));
    if (stem) stem->liquidIntensity = intensity;
}

static void Flower_SetGroundLevel(u64 entity, float y) {
    if (!s_World) return;
    auto* stem = s_World->GetComponent<ECS::FlowerStemComponent>(static_cast<ECS::Entity>(entity));
    if (stem) stem->groundLevel = y;
}

// ============================================================================
// Jelly mesh
// ============================================================================

static void Flower_SetJellyStiffness(u64 entity, float stiffness) {
    if (!s_World) return;
    auto* jelly = s_World->GetComponent<ECS::JellyMeshComponent>(static_cast<ECS::Entity>(entity));
    if (jelly) jelly->springStiffness = stiffness;
}

static void Flower_SetJellyDamping(u64 entity, float damping) {
    if (!s_World) return;
    auto* jelly = s_World->GetComponent<ECS::JellyMeshComponent>(static_cast<ECS::Entity>(entity));
    if (jelly) jelly->damping = damping;
}

// ============================================================================
// Component queries (has/add)
// ============================================================================

static bool Flower_HasTether(u64 entity) {
    if (!s_World) return false;
    return s_World->HasComponent<ECS::TetherComponent>(static_cast<ECS::Entity>(entity));
}

static bool Flower_HasGrabbable(u64 entity) {
    if (!s_World) return false;
    return s_World->HasComponent<ECS::GrabbableComponent>(static_cast<ECS::Entity>(entity));
}

static bool Flower_HasStem(u64 entity) {
    if (!s_World) return false;
    return s_World->HasComponent<ECS::FlowerStemComponent>(static_cast<ECS::Entity>(entity));
}

static bool Flower_HasJelly(u64 entity) {
    if (!s_World) return false;
    return s_World->HasComponent<ECS::JellyMeshComponent>(static_cast<ECS::Entity>(entity));
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterFlowerBindings(asIScriptEngine* engine) {
    // Tether queries
    AS_CHECK(engine->RegisterGlobalFunction("float Flower_GetTension(uint64)",
        asFUNCTION(Flower_GetTension), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Flower_IsBroken(uint64)",
        asFUNCTION(Flower_IsBroken), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Flower_JustBroke(uint64)",
        asFUNCTION(Flower_JustBroke), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flower_SetMaxDistance(uint64, float)",
        asFUNCTION(Flower_SetMaxDistance), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flower_GetMaxDistance(uint64)",
        asFUNCTION(Flower_GetMaxDistance), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flower_SetSpringK(uint64, float)",
        asFUNCTION(Flower_SetSpringK), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flower_SetDamping(uint64, float)",
        asFUNCTION(Flower_SetDamping), asCALL_CDECL));

    // Grabbable queries
    AS_CHECK(engine->RegisterGlobalFunction("bool Flower_IsGrabbed(uint64)",
        asFUNCTION(Flower_IsGrabbed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flower_SetGrabRadius(uint64, float)",
        asFUNCTION(Flower_SetGrabRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flower_GetGrabRadius(uint64)",
        asFUNCTION(Flower_GetGrabRadius), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flower_SetGrabSpring(uint64, float)",
        asFUNCTION(Flower_SetGrabSpring), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flower_GetGrabSpring(uint64)",
        asFUNCTION(Flower_GetGrabSpring), asCALL_CDECL));

    // Stem / scoring
    AS_CHECK(engine->RegisterGlobalFunction("float Flower_GetScore(uint64)",
        asFUNCTION(Flower_GetScore), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Flower_GetPartsRemoved(uint64)",
        asFUNCTION(Flower_GetPartsRemoved), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Flower_IsEvaluated(uint64)",
        asFUNCTION(Flower_IsEvaluated), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flower_SetSapColor(uint64, float, float, float)",
        asFUNCTION(Flower_SetSapColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flower_SetLiquidIntensity(uint64, float)",
        asFUNCTION(Flower_SetLiquidIntensity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flower_SetGroundLevel(uint64, float)",
        asFUNCTION(Flower_SetGroundLevel), asCALL_CDECL));

    // Jelly mesh
    AS_CHECK(engine->RegisterGlobalFunction("void Flower_SetJellyStiffness(uint64, float)",
        asFUNCTION(Flower_SetJellyStiffness), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flower_SetJellyDamping(uint64, float)",
        asFUNCTION(Flower_SetJellyDamping), asCALL_CDECL));

    // Component queries
    AS_CHECK(engine->RegisterGlobalFunction("bool Flower_HasTether(uint64)",
        asFUNCTION(Flower_HasTether), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Flower_HasGrabbable(uint64)",
        asFUNCTION(Flower_HasGrabbable), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Flower_HasStem(uint64)",
        asFUNCTION(Flower_HasStem), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Flower_HasJelly(uint64)",
        asFUNCTION(Flower_HasJelly), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
