#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/MorphTarget.h"
#include <angelscript.h>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

namespace Enjin {
namespace Scripting {

// ============================================================================
// Audio-Reactive Bindings
// ============================================================================

// RTPC
static void RTPC_SetParameter(u64 id, const std::string& name, float value) {
    if (!s_BindingsWorld) return;
    auto* rtpc = s_BindingsWorld->GetComponent<ECS::RTPCComponent>(static_cast<ECS::Entity>(id));
    if (rtpc) rtpc->SetParameter(name, value);
}

static float RTPC_GetParameter(u64 id, const std::string& name) {
    if (!s_BindingsWorld) return 0.0f;
    auto* rtpc = s_BindingsWorld->GetComponent<ECS::RTPCComponent>(static_cast<ECS::Entity>(id));
    return rtpc ? rtpc->GetParameter(name) : 0.0f;
}

// Beat Clock
static float BeatClock_GetBPM(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* bc = s_BindingsWorld->GetComponent<ECS::BeatClockComponent>(static_cast<ECS::Entity>(id));
    return bc ? bc->bpm : 0.0f;
}

static void BeatClock_SetBPM(u64 id, float bpm) {
    if (!s_BindingsWorld) return;
    auto* bc = s_BindingsWorld->GetComponent<ECS::BeatClockComponent>(static_cast<ECS::Entity>(id));
    if (bc) bc->bpm = bpm;
}

static u32 BeatClock_GetCurrentBeat(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* bc = s_BindingsWorld->GetComponent<ECS::BeatClockComponent>(static_cast<ECS::Entity>(id));
    return bc ? bc->currentBeat : 0;
}

static u32 BeatClock_GetCurrentBar(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* bc = s_BindingsWorld->GetComponent<ECS::BeatClockComponent>(static_cast<ECS::Entity>(id));
    return bc ? bc->currentBar : 0;
}

static bool BeatClock_IsBeatThisFrame(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* bc = s_BindingsWorld->GetComponent<ECS::BeatClockComponent>(static_cast<ECS::Entity>(id));
    return bc ? bc->beatThisFrame : false;
}

static bool BeatClock_IsDownbeatThisFrame(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* bc = s_BindingsWorld->GetComponent<ECS::BeatClockComponent>(static_cast<ECS::Entity>(id));
    return bc ? bc->downbeatThisFrame : false;
}

// Conductor
static void Conductor_SetState(u64 id, int state) {
    if (!s_BindingsWorld) return;
    auto* c = s_BindingsWorld->GetComponent<ECS::ConductorComponent>(static_cast<ECS::Entity>(id));
    if (c) c->currentState = static_cast<ECS::ConductorComponent::GameplayState>(state);
}

static int Conductor_GetState(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* c = s_BindingsWorld->GetComponent<ECS::ConductorComponent>(static_cast<ECS::Entity>(id));
    return c ? static_cast<int>(c->currentState) : 0;
}

// Audio Reactive
static void AudioReactive_SetEnabled(u64 id, bool enabled) {
    if (!s_BindingsWorld) return;
    auto* ar = s_BindingsWorld->GetComponent<ECS::AudioReactiveComponent>(static_cast<ECS::Entity>(id));
    if (ar) ar->enabled = enabled;
}

static float AudioReactive_GetCurrentValue(u64 id) {
    if (!s_BindingsWorld) return 0.0f;
    auto* ar = s_BindingsWorld->GetComponent<ECS::AudioReactiveComponent>(static_cast<ECS::Entity>(id));
    return ar ? ar->currentValue : 0.0f;
}

// Sidechain
static void Sidechain_SetEnabled(u64 id, bool enabled) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<ECS::SidechainComponent>(static_cast<ECS::Entity>(id));
    if (sc) sc->enabled = enabled;
}

static bool Sidechain_IsDucking(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* sc = s_BindingsWorld->GetComponent<ECS::SidechainComponent>(static_cast<ECS::Entity>(id));
    return sc ? sc->ducking : false;
}

// Morph targets
static void Morph_SetWeight(u64 id, const std::string& name, float weight) {
    if (!s_BindingsWorld) return;
    auto* m = s_BindingsWorld->GetComponent<ECS::MorphTargetComponent>(static_cast<ECS::Entity>(id));
    if (m) m->SetWeight(name, weight);
}

static float Morph_GetWeight(u64 id, const std::string& name) {
    if (!s_BindingsWorld) return 0.0f;
    auto* m = s_BindingsWorld->GetComponent<ECS::MorphTargetComponent>(static_cast<ECS::Entity>(id));
    return m ? m->GetWeight(name) : 0.0f;
}

static int Morph_GetTargetCount(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* m = s_BindingsWorld->GetComponent<ECS::MorphTargetComponent>(static_cast<ECS::Entity>(id));
    return m ? static_cast<int>(m->targets.size()) : 0;
}

// ============================================================================
// Registration
// ============================================================================

void RegisterAudioReactiveBindings(asIScriptEngine* engine) {
    // RTPC
    AS_CHECK(engine->RegisterGlobalFunction("void RTPC_SetParameter(uint64, const string &in, float)", asFUNCTION(RTPC_SetParameter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float RTPC_GetParameter(uint64, const string &in)", asFUNCTION(RTPC_GetParameter), asCALL_CDECL));

    // Beat Clock
    AS_CHECK(engine->RegisterGlobalFunction("float BeatClock_GetBPM(uint64)", asFUNCTION(BeatClock_GetBPM), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BeatClock_SetBPM(uint64, float)", asFUNCTION(BeatClock_SetBPM), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint BeatClock_GetCurrentBeat(uint64)", asFUNCTION(BeatClock_GetCurrentBeat), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint BeatClock_GetCurrentBar(uint64)", asFUNCTION(BeatClock_GetCurrentBar), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool BeatClock_IsBeatThisFrame(uint64)", asFUNCTION(BeatClock_IsBeatThisFrame), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool BeatClock_IsDownbeatThisFrame(uint64)", asFUNCTION(BeatClock_IsDownbeatThisFrame), asCALL_CDECL));

    // Conductor
    AS_CHECK(engine->RegisterGlobalFunction("void Conductor_SetState(uint64, int)", asFUNCTION(Conductor_SetState), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Conductor_GetState(uint64)", asFUNCTION(Conductor_GetState), asCALL_CDECL));

    // Audio Reactive
    AS_CHECK(engine->RegisterGlobalFunction("void AudioReactive_SetEnabled(uint64, bool)", asFUNCTION(AudioReactive_SetEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float AudioReactive_GetCurrentValue(uint64)", asFUNCTION(AudioReactive_GetCurrentValue), asCALL_CDECL));

    // Sidechain
    AS_CHECK(engine->RegisterGlobalFunction("void Sidechain_SetEnabled(uint64, bool)", asFUNCTION(Sidechain_SetEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Sidechain_IsDucking(uint64)", asFUNCTION(Sidechain_IsDucking), asCALL_CDECL));

    // Morph Targets
    AS_CHECK(engine->RegisterGlobalFunction("void Morph_SetWeight(uint64, const string &in, float)", asFUNCTION(Morph_SetWeight), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Morph_GetWeight(uint64, const string &in)", asFUNCTION(Morph_GetWeight), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Morph_GetTargetCount(uint64)", asFUNCTION(Morph_GetTargetCount), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
