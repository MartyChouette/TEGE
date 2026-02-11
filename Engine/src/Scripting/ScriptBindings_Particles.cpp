#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// Particle Emitter component access
// ============================================================================

static void Particle_Play(u64 entity) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (emitter) emitter->isPlaying = true;
}

static void Particle_Stop(u64 entity) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (emitter) emitter->isPlaying = false;
}

static bool Particle_IsPlaying(u64 entity) {
    if (!s_BindingsWorld) return false;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    return emitter ? emitter->isPlaying : false;
}

static void Particle_SetEmissionRate(u64 entity, float rate) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (emitter) emitter->emissionRate = Math::Max(0.0f, rate);
}

static float Particle_GetEmissionRate(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    return emitter ? emitter->emissionRate : 0.0f;
}

static void Particle_Burst(u64 entity, int count) {
    if (!s_BindingsWorld || count <= 0) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (emitter) emitter->burstCount = count;
}

static void Particle_SetLifetime(u64 entity, float lifetime) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (emitter) emitter->lifetime = Math::Max(0.01f, lifetime);
}

static void Particle_SetSpeed(u64 entity, float speed) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (emitter) emitter->startSpeed = speed;
}

static void Particle_SetSize(u64 entity, float startSize, float endSize) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (!emitter) return;
    emitter->startSize = startSize;
    emitter->endSize = endSize;
}

static void Particle_SetColor(u64 entity, float sr, float sg, float sb, float er, float eg, float eb) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (!emitter) return;
    emitter->startColor = Math::Vector3(sr, sg, sb);
    emitter->endColor = Math::Vector3(er, eg, eb);
}

static void Particle_SetAlpha(u64 entity, float startAlpha, float endAlpha) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (!emitter) return;
    emitter->startAlpha = Math::Clamp(startAlpha, 0.0f, 1.0f);
    emitter->endAlpha = Math::Clamp(endAlpha, 0.0f, 1.0f);
}

static void Particle_SetLoop(u64 entity, bool loop) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (emitter) emitter->loop = loop;
}

static void Particle_SetGravity(u64 entity, float gx, float gy, float gz) {
    if (!s_BindingsWorld) return;
    auto* emitter = s_BindingsWorld->GetComponent<ECS::ParticleEmitterComponent>(entity);
    if (emitter) emitter->gravity = Math::Vector3(gx, gy, gz);
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterParticleBindings(asIScriptEngine* engine) {
    // Playback
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_Play(uint64)",
        asFUNCTION(Particle_Play), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_Stop(uint64)",
        asFUNCTION(Particle_Stop), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Particle_IsPlaying(uint64)",
        asFUNCTION(Particle_IsPlaying), asCALL_CDECL));

    // Emission
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_SetEmissionRate(uint64, float)",
        asFUNCTION(Particle_SetEmissionRate), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Particle_GetEmissionRate(uint64)",
        asFUNCTION(Particle_GetEmissionRate), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_Burst(uint64, int)",
        asFUNCTION(Particle_Burst), asCALL_CDECL));

    // Properties
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_SetLifetime(uint64, float)",
        asFUNCTION(Particle_SetLifetime), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_SetSpeed(uint64, float)",
        asFUNCTION(Particle_SetSpeed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_SetSize(uint64, float, float)",
        asFUNCTION(Particle_SetSize), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_SetColor(uint64, float, float, float, float, float, float)",
        asFUNCTION(Particle_SetColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_SetAlpha(uint64, float, float)",
        asFUNCTION(Particle_SetAlpha), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_SetLoop(uint64, bool)",
        asFUNCTION(Particle_SetLoop), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Particle_SetGravity(uint64, float, float, float)",
        asFUNCTION(Particle_SetGravity), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
