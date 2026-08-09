#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Gameplay/QuestSystem.h"
#include "Enjin/Gameplay/CinematicSystem.h"
#include "Enjin/Gameplay/ObjectPool.h"
#include "Enjin/Effects/Destructible.h"
#include "Enjin/ECS/Components/Transform.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

static Gameplay::QuestSystem* s_BindingsQuest = nullptr;
static Gameplay::CinematicSystem* s_BindingsCinematic = nullptr;
static Gameplay::ObjectPool* s_BindingsObjectPool = nullptr;
static Effects::DestructibleSystem* s_BindingsDestructible = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsQuestSystem(Gameplay::QuestSystem* quest) {
    s_BindingsQuest = quest;
}

void SetBindingsCinematicSystem(Gameplay::CinematicSystem* cinematic) {
    s_BindingsCinematic = cinematic;
}

void SetBindingsObjectPool(Gameplay::ObjectPool* pool) {
    s_BindingsObjectPool = pool;
}

void SetBindingsDestructible(Effects::DestructibleSystem* destructible) {
    s_BindingsDestructible = destructible;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Quest System
// ============================================================================

static void Quest_Start(const std::string& questId) {
    if (s_BindingsQuest && s_BindingsWorld)
        s_BindingsQuest->StartQuest(s_BindingsWorld, questId);
}

static void Quest_CompleteObjective(const std::string& questId, int objectiveIndex) {
    if (!s_BindingsQuest || !s_BindingsWorld) return;
    if (objectiveIndex < 0) return; // SC-H8: reject negative indices
    s_BindingsQuest->CompleteObjective(s_BindingsWorld, questId, objectiveIndex);
}

static void Quest_Fail(const std::string& questId) {
    if (s_BindingsQuest && s_BindingsWorld)
        s_BindingsQuest->FailQuest(s_BindingsWorld, questId);
}

static bool Quest_IsActive(const std::string& questId) {
    return (s_BindingsQuest && s_BindingsWorld) ?
        s_BindingsQuest->IsQuestActive(s_BindingsWorld, questId) : false;
}

static bool Quest_IsComplete(const std::string& questId) {
    return (s_BindingsQuest && s_BindingsWorld) ?
        s_BindingsQuest->IsQuestComplete(s_BindingsWorld, questId) : false;
}

// ============================================================================
// Cinematic System
// ============================================================================

static void Cinematic_Play(u64 entity) {
    if (s_BindingsCinematic && s_BindingsWorld)
        s_BindingsCinematic->Play(s_BindingsWorld, entity);
}

static void Cinematic_Stop(u64 entity) {
    if (s_BindingsCinematic && s_BindingsWorld)
        s_BindingsCinematic->Stop(s_BindingsWorld, entity);
}

static bool Cinematic_IsPlaying() {
    return (s_BindingsCinematic && s_BindingsWorld) ?
        s_BindingsCinematic->IsAnyCinematicPlaying(s_BindingsWorld) : false;
}

// ============================================================================
// Object Pool
// ============================================================================

static u64 Pool_Acquire(const std::string& poolId) {
    if (!s_BindingsObjectPool) return 0;
    return s_BindingsObjectPool->Acquire(poolId);
}

static void Pool_Release(const std::string& poolId, u64 entity) {
    if (s_BindingsObjectPool) s_BindingsObjectPool->Release(poolId, entity);
}

// ============================================================================
// Destructible System
// ============================================================================

static void Destructible_Destroy(u64 entity, float forceX, float forceY, float forceZ, float force) {
    if (!s_BindingsDestructible) return;
    Math::Vector3 impactDir(forceX, forceY, forceZ);
    Math::Vector3 impactPoint(0, 0, 0);
    // Get entity position as impact point if possible
    if (s_BindingsWorld) {
        auto* transform = s_BindingsWorld->GetComponent<ECS::TransformComponent>(entity);
        if (transform) impactPoint = transform->position;
    }
    s_BindingsDestructible->Destroy(entity, impactPoint, impactDir, force);
}

static void Destructible_ApplyDamage(u64 entity, float damage) {
    if (!s_BindingsDestructible) return;
    s_BindingsDestructible->ApplyDamage(entity, damage);
}

static void Destructible_ApplyDamageAt(u64 entity, float damage, float px, float py, float pz) {
    if (!s_BindingsDestructible) return;
    s_BindingsDestructible->ApplyDamage(entity, damage, Math::Vector3(px, py, pz));
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterGameplayBindings(asIScriptEngine* engine) {
    // -- Quest System --
    AS_CHECK(engine->RegisterGlobalFunction("void Quest_Start(const string &in)",
        ENJIN_AS_FN(Quest_Start), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Quest_CompleteObjective(const string &in, int)",
        ENJIN_AS_FN(Quest_CompleteObjective), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Quest_Fail(const string &in)",
        ENJIN_AS_FN(Quest_Fail), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Quest_IsActive(const string &in)",
        ENJIN_AS_FN(Quest_IsActive), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Quest_IsComplete(const string &in)",
        ENJIN_AS_FN(Quest_IsComplete), ENJIN_AS_CALL_CDECL));

    // -- Cinematic System --
    AS_CHECK(engine->RegisterGlobalFunction("void Cinematic_Play(uint64)",
        ENJIN_AS_FN(Cinematic_Play), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Cinematic_Stop(uint64)",
        ENJIN_AS_FN(Cinematic_Stop), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Cinematic_IsPlaying()",
        ENJIN_AS_FN(Cinematic_IsPlaying), ENJIN_AS_CALL_CDECL));

    // -- Object Pool --
    AS_CHECK(engine->RegisterGlobalFunction("uint64 Pool_Acquire(const string &in)",
        ENJIN_AS_FN(Pool_Acquire), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Pool_Release(const string &in, uint64)",
        ENJIN_AS_FN(Pool_Release), ENJIN_AS_CALL_CDECL));

    // -- Destructible System --
    AS_CHECK(engine->RegisterGlobalFunction("void Destructible_Destroy(uint64, float, float, float, float)",
        ENJIN_AS_FN(Destructible_Destroy), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Destructible_ApplyDamage(uint64, float)",
        ENJIN_AS_FN(Destructible_ApplyDamage), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Destructible_ApplyDamageAt(uint64, float, float, float, float)",
        ENJIN_AS_FN(Destructible_ApplyDamageAt), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
