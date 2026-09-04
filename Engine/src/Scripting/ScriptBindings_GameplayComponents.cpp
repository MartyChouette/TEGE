#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/TemperatureZone.h"
#include "Enjin/ECS/Components/ReflectionProbe.h"
#include "Enjin/ECS/Components/Lens.h"
#include "Enjin/ECS/Components/DynamicDifficulty.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
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


// --- DynamicDifficulty ------------------------------------------------------
// docs/SCRIPTING_API.md has documented this whole surface for a long time, but
// nothing ever registered it: the names existed only as a comment block in
// DynamicDifficultySystem.h. These are the real registrations. They read and
// write the component directly, so they work regardless of whether the system
// has recomputed yet; DynamicDifficultySystem recomputes the score once a
// second from whatever the record calls have accumulated.
static ECS::DynamicDifficultyComponent* DifficultyOf(u64 self) {
    if (!s_BindingsWorld) return nullptr;
    return s_BindingsWorld->GetComponent<ECS::DynamicDifficultyComponent>(self);
}
static float Difficulty_GetScore(u64 self) {
    auto* d = DifficultyOf(self);
    return d ? d->smoothedScore : 0.5f;
}
static float Difficulty_GetMultiplier(u64 self, const std::string& which) {
    auto* d = DifficultyOf(self);
    if (!d) return 1.0f;
    if (which == "enemyDamage")   return d->enemyDamageMultiplier;
    if (which == "enemyHealth")   return d->enemyHealthMultiplier;
    if (which == "aiAggression")  return d->aiAggressionMultiplier;
    if (which == "resourceDrops") return d->resourceDropMultiplier;
    if (which == "checkpoint")    return d->checkpointMultiplier;
    ENJIN_LOG_WARN(Script, "Difficulty_GetMultiplier: unknown output '%s' "
                   "(enemyDamage, enemyHealth, aiAggression, resourceDrops, checkpoint)",
                   which.c_str());
    return 1.0f;
}
static void Difficulty_RecordDeath(u64 self) {
    if (auto* d = DifficultyOf(self)) d->recentDeaths++;
}
static void Difficulty_RecordShot(u64 self) {
    if (auto* d = DifficultyOf(self)) d->shotsFired++;
}
static void Difficulty_RecordHit(u64 self) {
    if (auto* d = DifficultyOf(self)) d->shotsHit++;
}
static void Difficulty_RecordCheckpointHealth(u64 self, float healthPercent) {
    if (auto* d = DifficultyOf(self)) {
        d->lastCheckpointHealthPercent = healthPercent < 0.0f ? 0.0f
                                       : (healthPercent > 1.0f ? 1.0f : healthPercent);
    }
}
static void Difficulty_SetBaseDifficulty(u64 self, u32 level) {
    if (auto* d = DifficultyOf(self)) d->baseDifficulty = level;
}
static u32 Difficulty_GetBaseDifficulty(u64 self) {
    auto* d = DifficultyOf(self);
    return d ? d->baseDifficulty : 1u;
}
static void Difficulty_SetEnabled(u64 self, bool enabled) {
    if (auto* d = DifficultyOf(self)) d->enabled = enabled;
}
static void Difficulty_SetPlayerEntity(u64 self, u64 player) {
    if (auto* d = DifficultyOf(self)) d->playerEntity = static_cast<ECS::Entity>(player);
}
static void Difficulty_SetResourceRatio(u64 self, float ratio) {
    if (auto* d = DifficultyOf(self)) {
        d->resourceRatio = ratio < 0.0f ? 0.0f : (ratio > 1.0f ? 1.0f : ratio);
    }
}
static void Difficulty_Reset(u64 self) {
    if (auto* d = DifficultyOf(self)) {
        d->recentDeaths = 0;
        d->shotsFired = 0;
        d->shotsHit = 0;
        d->elapsedTime = 0.0f;
    }
}

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

// --- Possessable: entities the player can take control of -------------------
static bool Possessable_IsPossessed(u64 self) {
    if (!s_BindingsWorld) return false;
    auto* c = s_BindingsWorld->GetComponent<ECS::PossessableComponent>(self);
    return c ? c->isPossessed : false;
}
static void Possessable_SetPrompt(u64 self, const std::string& text) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::PossessableComponent>(self)) c->promptText = text;
}
static void Possessable_SetRange(u64 self, float range) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::PossessableComponent>(self)) c->possessRange = range;
}
static void Possessable_SetPlayerIndex(u64 self, int index) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::PossessableComponent>(self)) c->playerIndex = index;
}

// --- SavePoint --------------------------------------------------------------
static void SavePoint_SetSlot(u64 self, int slot) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::SavePointComponent>(self)) c->slotTarget = slot;
}
static void SavePoint_SetSaveOnEnter(u64 self, bool on) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::SavePointComponent>(self)) c->saveOnEnter = on;
}
static bool SavePoint_IsUsed(u64 self) {
    if (!s_BindingsWorld) return false;
    auto* c = s_BindingsWorld->GetComponent<ECS::SavePointComponent>(self);
    return c ? c->used : false;
}
static void SavePoint_SetRadius(u64 self, float r) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::SavePointComponent>(self)) c->radius = r;
}
static void SavePoint_SetMessage(u64 self, const std::string& msg) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::SavePointComponent>(self)) c->saveMessage = msg;
}

// --- Footstep ---------------------------------------------------------------
static void Footstep_SetVolume(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::FootstepComponent>(self)) c->volume = v;
}
static void Footstep_SetWalkInterval(u64 self, float s) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::FootstepComponent>(self)) c->walkStepInterval = s;
}
static void Footstep_SetRunInterval(u64 self, float s) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::FootstepComponent>(self)) c->runStepInterval = s;
}
static void Footstep_SetPitchVariance(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::FootstepComponent>(self)) c->pitchVariance = v;
}

// --- ReverbZone -------------------------------------------------------------
static void Reverb_SetActive(u64 self, bool a) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::ReverbZoneComponent>(self)) c->isActive = a;
}
static void Reverb_SetRoomSize(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::ReverbZoneComponent>(self)) c->roomSize = v;
}
static void Reverb_SetDamping(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::ReverbZoneComponent>(self)) c->damping = v;
}
static void Reverb_SetWetDryMix(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::ReverbZoneComponent>(self)) c->wetDryMix = v;
}
static void Reverb_SetDecayTime(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::ReverbZoneComponent>(self)) c->decayTime = v;
}

// --- Lens: per-camera lens distortion/vignette ------------------------------
static void Lens_SetEnabled(u64 self, bool e) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LensComponent>(self)) c->enabled = e;
}
static void Lens_SetDistortion(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LensComponent>(self)) c->distortion = v;
}
static void Lens_SetChromaticAberration(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LensComponent>(self)) c->chromaticAberration = v;
}
static void Lens_SetVignette(u64 self, float intensity, float softness) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LensComponent>(self)) {
        c->vignetteIntensity = intensity; c->vignetteSoftness = softness;
    }
}
static void Lens_SetAnamorphicSqueeze(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::LensComponent>(self)) c->anamorphicSqueeze = v;
}

// --- Physics joints (runtime tuning) ----------------------------------------
static void SpringJoint_SetRestLength(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::SpringJointComponent>(self)) c->restLength = v;
}
static void SpringJoint_SetStiffness(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::SpringJointComponent>(self)) c->springConstant = v;
}
static void SpringJoint_SetDamping(u64 self, float v) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::SpringJointComponent>(self)) c->dampingCoefficient = v;
}
static float SpringJoint_GetStress(u64 self) {
    if (!s_BindingsWorld) return 0.0f;
    auto* c = s_BindingsWorld->GetComponent<ECS::SpringJointComponent>(self);
    return c ? c->currentStress : 0.0f;
}
static void SliderJoint_SetMotor(u64 self, bool enable, float speed, float maxForce) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::SliderJointComponent>(self)) {
        c->useMotor = enable; c->motorSpeed = speed; c->motorMaxForce = maxForce;
    }
}
static void SliderJoint_SetLimits(u64 self, bool use, float lower, float upper) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::SliderJointComponent>(self)) {
        c->useLimits = use; c->lowerLimit = lower; c->upperLimit = upper;
    }
}
static float SliderJoint_GetDisplacement(u64 self) {
    if (!s_BindingsWorld) return 0.0f;
    auto* c = s_BindingsWorld->GetComponent<ECS::SliderJointComponent>(self);
    return c ? c->currentDisplacement : 0.0f;
}
static void FixedJoint_SetBreakable(u64 self, bool breakable, float force) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::FixedJointComponent>(self)) {
        c->breakable = breakable; c->breakForce = force;
    }
}
static void BallSocket_SetConeLimit(u64 self, bool use, float angleDeg) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::BallSocketJointComponent>(self)) {
        c->useConeLimit = use; c->coneAngleLimit = angleDeg;
    }
}
static void BallSocket_SetTwistLimit(u64 self, bool use, float lower, float upper) {
    if (!s_BindingsWorld) return;
    if (auto* c = s_BindingsWorld->GetComponent<ECS::BallSocketJointComponent>(self)) {
        c->useTwistLimit = use; c->twistLowerLimit = lower; c->twistUpperLimit = upper;
    }
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

    // Possessable
    AS_CHECK(engine->RegisterGlobalFunction("bool Possessable_IsPossessed(uint64)", ENJIN_AS_FN(Possessable_IsPossessed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Possessable_SetPrompt(uint64, const string &in)", ENJIN_AS_FN(Possessable_SetPrompt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Possessable_SetRange(uint64, float)", ENJIN_AS_FN(Possessable_SetRange), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Possessable_SetPlayerIndex(uint64, int)", ENJIN_AS_FN(Possessable_SetPlayerIndex), ENJIN_AS_CALL_CDECL));

    // SavePoint
    AS_CHECK(engine->RegisterGlobalFunction("void SavePoint_SetSlot(uint64, int)", ENJIN_AS_FN(SavePoint_SetSlot), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SavePoint_SetSaveOnEnter(uint64, bool)", ENJIN_AS_FN(SavePoint_SetSaveOnEnter), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool SavePoint_IsUsed(uint64)", ENJIN_AS_FN(SavePoint_IsUsed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SavePoint_SetRadius(uint64, float)", ENJIN_AS_FN(SavePoint_SetRadius), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SavePoint_SetMessage(uint64, const string &in)", ENJIN_AS_FN(SavePoint_SetMessage), ENJIN_AS_CALL_CDECL));

    // Footstep
    AS_CHECK(engine->RegisterGlobalFunction("void Footstep_SetVolume(uint64, float)", ENJIN_AS_FN(Footstep_SetVolume), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Footstep_SetWalkInterval(uint64, float)", ENJIN_AS_FN(Footstep_SetWalkInterval), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Footstep_SetRunInterval(uint64, float)", ENJIN_AS_FN(Footstep_SetRunInterval), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Footstep_SetPitchVariance(uint64, float)", ENJIN_AS_FN(Footstep_SetPitchVariance), ENJIN_AS_CALL_CDECL));

    // ReverbZone
    AS_CHECK(engine->RegisterGlobalFunction("void Reverb_SetActive(uint64, bool)", ENJIN_AS_FN(Reverb_SetActive), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Reverb_SetRoomSize(uint64, float)", ENJIN_AS_FN(Reverb_SetRoomSize), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Reverb_SetDamping(uint64, float)", ENJIN_AS_FN(Reverb_SetDamping), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Reverb_SetWetDryMix(uint64, float)", ENJIN_AS_FN(Reverb_SetWetDryMix), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Reverb_SetDecayTime(uint64, float)", ENJIN_AS_FN(Reverb_SetDecayTime), ENJIN_AS_CALL_CDECL));

    // Lens
    AS_CHECK(engine->RegisterGlobalFunction("void Lens_SetEnabled(uint64, bool)", ENJIN_AS_FN(Lens_SetEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Lens_SetDistortion(uint64, float)", ENJIN_AS_FN(Lens_SetDistortion), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Lens_SetChromaticAberration(uint64, float)", ENJIN_AS_FN(Lens_SetChromaticAberration), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Lens_SetVignette(uint64, float, float)", ENJIN_AS_FN(Lens_SetVignette), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Lens_SetAnamorphicSqueeze(uint64, float)", ENJIN_AS_FN(Lens_SetAnamorphicSqueeze), ENJIN_AS_CALL_CDECL));


    // DynamicDifficulty (documented in SCRIPTING_API.md, never registered until now)
    AS_CHECK(engine->RegisterGlobalFunction("float Difficulty_GetScore(uint64)", ENJIN_AS_FN(Difficulty_GetScore), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Difficulty_GetMultiplier(uint64, const string &in)", ENJIN_AS_FN(Difficulty_GetMultiplier), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Difficulty_RecordDeath(uint64)", ENJIN_AS_FN(Difficulty_RecordDeath), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Difficulty_RecordShot(uint64)", ENJIN_AS_FN(Difficulty_RecordShot), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Difficulty_RecordHit(uint64)", ENJIN_AS_FN(Difficulty_RecordHit), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Difficulty_RecordCheckpointHealth(uint64, float)", ENJIN_AS_FN(Difficulty_RecordCheckpointHealth), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Difficulty_SetBaseDifficulty(uint64, uint)", ENJIN_AS_FN(Difficulty_SetBaseDifficulty), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint Difficulty_GetBaseDifficulty(uint64)", ENJIN_AS_FN(Difficulty_GetBaseDifficulty), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Difficulty_SetEnabled(uint64, bool)", ENJIN_AS_FN(Difficulty_SetEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Difficulty_SetPlayerEntity(uint64, uint64)", ENJIN_AS_FN(Difficulty_SetPlayerEntity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Difficulty_SetResourceRatio(uint64, float)", ENJIN_AS_FN(Difficulty_SetResourceRatio), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Difficulty_Reset(uint64)", ENJIN_AS_FN(Difficulty_Reset), ENJIN_AS_CALL_CDECL));
    // Physics joints
    AS_CHECK(engine->RegisterGlobalFunction("void SpringJoint_SetRestLength(uint64, float)", ENJIN_AS_FN(SpringJoint_SetRestLength), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SpringJoint_SetStiffness(uint64, float)", ENJIN_AS_FN(SpringJoint_SetStiffness), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SpringJoint_SetDamping(uint64, float)", ENJIN_AS_FN(SpringJoint_SetDamping), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float SpringJoint_GetStress(uint64)", ENJIN_AS_FN(SpringJoint_GetStress), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SliderJoint_SetMotor(uint64, bool, float, float)", ENJIN_AS_FN(SliderJoint_SetMotor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SliderJoint_SetLimits(uint64, bool, float, float)", ENJIN_AS_FN(SliderJoint_SetLimits), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float SliderJoint_GetDisplacement(uint64)", ENJIN_AS_FN(SliderJoint_GetDisplacement), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void FixedJoint_SetBreakable(uint64, bool, float)", ENJIN_AS_FN(FixedJoint_SetBreakable), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BallSocket_SetConeLimit(uint64, bool, float)", ENJIN_AS_FN(BallSocket_SetConeLimit), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BallSocket_SetTwistLimit(uint64, bool, float, float)", ENJIN_AS_FN(BallSocket_SetTwistLimit), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
