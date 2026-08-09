#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Effects/ElementalSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Elemental.h"
#include "Enjin/Math/Vector.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

static Effects::ElementalSystem* s_BindingsElemental = nullptr;
extern ECS::World* s_BindingsWorld;

namespace Enjin {
namespace Scripting {

void SetBindingsElemental(Effects::ElementalSystem* system) {
    s_BindingsElemental = system;
}

} // namespace Scripting
} // namespace Enjin

// =============================================================================
// System-level functions (spawn, query, stats)
// =============================================================================

static u32 Elemental_SpawnFire(float x, float y, float z, float intensity, float lifetime) {
    if (!s_BindingsElemental) return 0;
    return s_BindingsElemental->SpawnFire(Math::Vector3(x, y, z), intensity, lifetime);
}

static u32 Elemental_SpawnWater(float x, float y, float z, float vx, float vy, float vz, float intensity) {
    if (!s_BindingsElemental) return 0;
    return s_BindingsElemental->SpawnWater(Math::Vector3(x, y, z), Math::Vector3(vx, vy, vz), intensity);
}

static u32 Elemental_SpawnEarth(float x, float y, float z, float vx, float vy, float vz, float size) {
    if (!s_BindingsElemental) return 0;
    return s_BindingsElemental->SpawnEarth(Math::Vector3(x, y, z), Math::Vector3(vx, vy, vz), size);
}

static u32 Elemental_SpawnSnow(float x, float y, float z, float size) {
    if (!s_BindingsElemental) return 0;
    return s_BindingsElemental->SpawnSnow(Math::Vector3(x, y, z), size);
}

static u32 Elemental_SpawnSteam(float x, float y, float z, float intensity) {
    if (!s_BindingsElemental) return 0;
    return s_BindingsElemental->SpawnSteam(Math::Vector3(x, y, z), intensity);
}

static void Elemental_SpawnRainBurst(float cx, float cy, float cz, float radius, int count) {
    if (!s_BindingsElemental || count <= 0) return;
    s_BindingsElemental->SpawnRainBurst(Math::Vector3(cx, cy, cz), radius, static_cast<u32>(count));
}

static void Elemental_SpawnDebrisBurst(float cx, float cy, float cz, float fx, float fy, float fz, int count) {
    if (!s_BindingsElemental || count <= 0) return;
    s_BindingsElemental->SpawnDebrisBurst(Math::Vector3(cx, cy, cz), Math::Vector3(fx, fy, fz), static_cast<u32>(count));
}

static float Elemental_GetFireIntensityAt(float x, float y, float z, float radius) {
    if (!s_BindingsElemental) return 0.0f;
    return s_BindingsElemental->GetFireIntensityAt(Math::Vector3(x, y, z), radius);
}

static float Elemental_GetMoistureAt(float x, float y, float z, float radius) {
    if (!s_BindingsElemental) return 0.0f;
    return s_BindingsElemental->GetMoistureAt(Math::Vector3(x, y, z), radius);
}

static int Elemental_GetActiveCount() {
    return s_BindingsElemental ? static_cast<int>(s_BindingsElemental->GetActiveCount()) : 0;
}

static int Elemental_GetFireCount() {
    return s_BindingsElemental ? static_cast<int>(s_BindingsElemental->GetFireCount()) : 0;
}

static int Elemental_GetWaterCount() {
    return s_BindingsElemental ? static_cast<int>(s_BindingsElemental->GetWaterCount()) : 0;
}

// =============================================================================
// Component-level functions (emitter, surface, volume)
// =============================================================================

static bool Elemental_HasEmitter(u64 entity) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<ECS::ElementalEmitterComponent>(entity);
}

static void Elemental_SetEmitterActive(u64 entity, bool active) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ElementalEmitterComponent>(entity);
    if (emitter) emitter->active = active;
}

static bool Elemental_IsEmitterActive(u64 entity) {
    if (!s_BindingsWorld) return false;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ElementalEmitterComponent>(entity);
    return emitter ? emitter->active : false;
}

static void Elemental_SetEmitterElement(u64 entity, float fire, float water, float earth, float air) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ElementalEmitterComponent>(entity);
    if (emitter) emitter->element = Math::Vector4(fire, water, earth, air);
}

static void Elemental_SetEmitterRate(u64 entity, float rate) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ElementalEmitterComponent>(entity);
    if (emitter) emitter->emissionRate = Math::Max(0.0f, rate);
}

static float Elemental_GetEmitterRate(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ElementalEmitterComponent>(entity);
    return emitter ? emitter->emissionRate : 0.0f;
}

static void Elemental_SetEmitterIntensity(u64 entity, float intensity) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ElementalEmitterComponent>(entity);
    if (emitter) emitter->intensity = Math::Clamp(intensity, 0.0f, 10.0f);
}

static bool Elemental_HasSurface(u64 entity) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<ECS::ElementalSurfaceComponent>(entity);
}

static float Elemental_GetSurfaceChar(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* s = s_BindingsWorld->GetComponent<ECS::ElementalSurfaceComponent>(entity);
    return s ? s->charAmount : 0.0f;
}

static float Elemental_GetSurfaceWetness(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* s = s_BindingsWorld->GetComponent<ECS::ElementalSurfaceComponent>(entity);
    return s ? s->wetness : 0.0f;
}

static float Elemental_GetSurfaceSnow(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* s = s_BindingsWorld->GetComponent<ECS::ElementalSurfaceComponent>(entity);
    return s ? s->snowCoverage : 0.0f;
}

static float Elemental_GetSurfaceFrost(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* s = s_BindingsWorld->GetComponent<ECS::ElementalSurfaceComponent>(entity);
    return s ? s->frostAmount : 0.0f;
}

static void Elemental_SetFlammability(u64 entity, float flammability) {
    if (!s_BindingsWorld) return;
    auto* s = s_BindingsWorld->GetComponent<ECS::ElementalSurfaceComponent>(entity);
    if (s) s->flammability = Math::Clamp(flammability, 0.0f, 1.0f);
}

static float Elemental_GetFlammability(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* s = s_BindingsWorld->GetComponent<ECS::ElementalSurfaceComponent>(entity);
    return s ? s->flammability : 0.0f;
}

static bool Elemental_HasVolume(u64 entity) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->HasComponent<ECS::ElementalVolumeComponent>(entity);
}

static void Elemental_SetVolumeKill(u64 entity, bool kill) {
    if (!s_BindingsWorld) return;
    auto* v = s_BindingsWorld->GetComponent<ECS::ElementalVolumeComponent>(entity);
    if (v) v->killOnContact = kill;
}

static void Elemental_SetVolumeTempBias(u64 entity, float bias) {
    if (!s_BindingsWorld) return;
    auto* v = s_BindingsWorld->GetComponent<ECS::ElementalVolumeComponent>(entity);
    if (v) v->temperatureBias = bias;
}

// =============================================================================
// Registration
// =============================================================================

namespace Enjin {
namespace Scripting {

void RegisterElementalBindings(asIScriptEngine* engine) {
    // --- Spawn functions ---
    AS_CHECK(engine->RegisterGlobalFunction("uint Elemental_SpawnFire(float, float, float, float, float)",
        ENJIN_AS_FN(Elemental_SpawnFire), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint Elemental_SpawnWater(float, float, float, float, float, float, float)",
        ENJIN_AS_FN(Elemental_SpawnWater), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint Elemental_SpawnEarth(float, float, float, float, float, float, float)",
        ENJIN_AS_FN(Elemental_SpawnEarth), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint Elemental_SpawnSnow(float, float, float, float)",
        ENJIN_AS_FN(Elemental_SpawnSnow), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint Elemental_SpawnSteam(float, float, float, float)",
        ENJIN_AS_FN(Elemental_SpawnSteam), ENJIN_AS_CALL_CDECL));

    // --- Bulk spawners ---
    AS_CHECK(engine->RegisterGlobalFunction("void Elemental_SpawnRainBurst(float, float, float, float, int)",
        ENJIN_AS_FN(Elemental_SpawnRainBurst), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Elemental_SpawnDebrisBurst(float, float, float, float, float, float, int)",
        ENJIN_AS_FN(Elemental_SpawnDebrisBurst), ENJIN_AS_CALL_CDECL));

    // --- Query functions ---
    AS_CHECK(engine->RegisterGlobalFunction("float Elemental_GetFireIntensityAt(float, float, float, float)",
        ENJIN_AS_FN(Elemental_GetFireIntensityAt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Elemental_GetMoistureAt(float, float, float, float)",
        ENJIN_AS_FN(Elemental_GetMoistureAt), ENJIN_AS_CALL_CDECL));

    // --- Stats ---
    AS_CHECK(engine->RegisterGlobalFunction("int Elemental_GetActiveCount()",
        ENJIN_AS_FN(Elemental_GetActiveCount), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Elemental_GetFireCount()",
        ENJIN_AS_FN(Elemental_GetFireCount), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Elemental_GetWaterCount()",
        ENJIN_AS_FN(Elemental_GetWaterCount), ENJIN_AS_CALL_CDECL));

    // --- Emitter component ---
    AS_CHECK(engine->RegisterGlobalFunction("bool Elemental_HasEmitter(uint64)",
        ENJIN_AS_FN(Elemental_HasEmitter), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Elemental_SetEmitterActive(uint64, bool)",
        ENJIN_AS_FN(Elemental_SetEmitterActive), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Elemental_IsEmitterActive(uint64)",
        ENJIN_AS_FN(Elemental_IsEmitterActive), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Elemental_SetEmitterElement(uint64, float, float, float, float)",
        ENJIN_AS_FN(Elemental_SetEmitterElement), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Elemental_SetEmitterRate(uint64, float)",
        ENJIN_AS_FN(Elemental_SetEmitterRate), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Elemental_GetEmitterRate(uint64)",
        ENJIN_AS_FN(Elemental_GetEmitterRate), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Elemental_SetEmitterIntensity(uint64, float)",
        ENJIN_AS_FN(Elemental_SetEmitterIntensity), ENJIN_AS_CALL_CDECL));

    // --- Surface component ---
    AS_CHECK(engine->RegisterGlobalFunction("bool Elemental_HasSurface(uint64)",
        ENJIN_AS_FN(Elemental_HasSurface), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Elemental_GetSurfaceChar(uint64)",
        ENJIN_AS_FN(Elemental_GetSurfaceChar), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Elemental_GetSurfaceWetness(uint64)",
        ENJIN_AS_FN(Elemental_GetSurfaceWetness), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Elemental_GetSurfaceSnow(uint64)",
        ENJIN_AS_FN(Elemental_GetSurfaceSnow), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Elemental_GetSurfaceFrost(uint64)",
        ENJIN_AS_FN(Elemental_GetSurfaceFrost), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Elemental_SetFlammability(uint64, float)",
        ENJIN_AS_FN(Elemental_SetFlammability), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Elemental_GetFlammability(uint64)",
        ENJIN_AS_FN(Elemental_GetFlammability), ENJIN_AS_CALL_CDECL));

    // --- Volume component ---
    AS_CHECK(engine->RegisterGlobalFunction("bool Elemental_HasVolume(uint64)",
        ENJIN_AS_FN(Elemental_HasVolume), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Elemental_SetVolumeKill(uint64, bool)",
        ENJIN_AS_FN(Elemental_SetVolumeKill), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Elemental_SetVolumeTempBias(uint64, float)",
        ENJIN_AS_FN(Elemental_SetVolumeTempBias), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
