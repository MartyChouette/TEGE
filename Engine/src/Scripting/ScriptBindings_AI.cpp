#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/AI/Navmesh.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

// Navmesh and Pathfinder pointers (set via SetBindingsNavmesh)
static AI::Navmesh* s_BindingsNavmesh = nullptr;
static AI::Pathfinder* s_BindingsPathfinder = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsNavmesh(AI::Navmesh* navmesh, AI::Pathfinder* pathfinder) {
    s_BindingsNavmesh = navmesh;
    s_BindingsPathfinder = pathfinder;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// AI Controller bindings (AIControllerComponent on ECS)
// ============================================================================

static void AI_SetState(u64 entityId, int state) {
    if (!s_BindingsWorld) return;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    if (!ai) return;
    if (state < 0 || state > 5) return;
    ai->currentState = static_cast<AIControllerComponent::AIState>(state);
}

static int AI_GetState(u64 entityId) {
    if (!s_BindingsWorld) return 0;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    return ai ? static_cast<int>(ai->currentState) : 0;
}

static void AI_SetTarget(u64 entityId, u64 targetId) {
    if (!s_BindingsWorld) return;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    if (ai) ai->targetEntity = static_cast<Entity>(targetId);
}

static u64 AI_GetTarget(u64 entityId) {
    if (!s_BindingsWorld) return 0;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    return ai ? static_cast<u64>(ai->targetEntity) : 0;
}

static void AI_SetTargetPosition(u64 entityId, float x, float y, float z) {
    if (!s_BindingsWorld) return;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    if (ai) ai->targetPosition = Math::Vector3(x, y, z);
}

static void AI_SetMoveSpeed(u64 entityId, float speed) {
    if (!s_BindingsWorld) return;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    if (ai) ai->moveSpeed = speed;
}

static float AI_GetMoveSpeed(u64 entityId) {
    if (!s_BindingsWorld) return 0.0f;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    return ai ? ai->moveSpeed : 0.0f;
}

static void AI_SetDetectionRange(u64 entityId, float range) {
    if (!s_BindingsWorld) return;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    if (ai) ai->detectionRange = range;
}

static float AI_GetDetectionRange(u64 entityId) {
    if (!s_BindingsWorld) return 0.0f;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    return ai ? ai->detectionRange : 0.0f;
}

static void AI_SetAttackRange(u64 entityId, float range) {
    if (!s_BindingsWorld) return;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    if (ai) ai->attackRange = range;
}

static float AI_GetAttackRange(u64 entityId) {
    if (!s_BindingsWorld) return 0.0f;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    return ai ? ai->attackRange : 0.0f;
}

static void AI_SetChaseSpeed(u64 entityId, float speed) {
    if (!s_BindingsWorld) return;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    if (ai) ai->chaseSpeed = speed;
}

static void AI_SetFleeSpeed(u64 entityId, float speed) {
    if (!s_BindingsWorld) return;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    if (ai) ai->fleeSpeed = speed;
}

static void AI_SetFieldOfView(u64 entityId, float fov) {
    if (!s_BindingsWorld) return;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    if (ai) ai->fieldOfView = fov;
}

static void AI_SetUseNavmesh(u64 entityId, bool use) {
    if (!s_BindingsWorld) return;
    auto* ai = s_BindingsWorld->GetComponent<AIControllerComponent>(static_cast<Entity>(entityId));
    if (ai) ai->useNavmesh = use;
}

// ============================================================================
// Behavior Tree bindings (BehaviorTreeComponent on ECS)
// ============================================================================

static bool BT_IsEnabled(u64 entityId) {
    if (!s_BindingsWorld) return false;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    return bt ? bt->enabled : false;
}

static void BT_Enable(u64 entityId) {
    if (!s_BindingsWorld) return;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (bt) bt->enabled = true;
}

static void BT_Disable(u64 entityId) {
    if (!s_BindingsWorld) return;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (bt) bt->enabled = false;
}

static void BT_Reset(u64 entityId) {
    if (!s_BindingsWorld) return;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (bt) bt->ResetRuntimeState();
}

static void BT_SetBlackboardFloat(u64 entityId, const std::string& key, float value) {
    if (!s_BindingsWorld) return;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (bt) bt->blackboard.Set(key, AI::BlackboardValue(value));
}

static float BT_GetBlackboardFloat(u64 entityId, const std::string& key) {
    if (!s_BindingsWorld) return 0.0f;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (!bt) return 0.0f;
    auto* val = bt->blackboard.Get(key);
    if (val && std::holds_alternative<f32>(*val)) return std::get<f32>(*val);
    return 0.0f;
}

static void BT_SetBlackboardInt(u64 entityId, const std::string& key, int value) {
    if (!s_BindingsWorld) return;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (bt) bt->blackboard.Set(key, AI::BlackboardValue(static_cast<i32>(value)));
}

static int BT_GetBlackboardInt(u64 entityId, const std::string& key) {
    if (!s_BindingsWorld) return 0;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (!bt) return 0;
    auto* val = bt->blackboard.Get(key);
    if (val && std::holds_alternative<i32>(*val)) return std::get<i32>(*val);
    return 0;
}

static void BT_SetBlackboardBool(u64 entityId, const std::string& key, bool value) {
    if (!s_BindingsWorld) return;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (bt) bt->blackboard.Set(key, AI::BlackboardValue(value));
}

static bool BT_GetBlackboardBool(u64 entityId, const std::string& key) {
    if (!s_BindingsWorld) return false;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (!bt) return false;
    auto* val = bt->blackboard.Get(key);
    if (val && std::holds_alternative<bool>(*val)) return std::get<bool>(*val);
    return false;
}

static void BT_SetBlackboardString(u64 entityId, const std::string& key, const std::string& value) {
    if (!s_BindingsWorld) return;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (bt) bt->blackboard.Set(key, AI::BlackboardValue(value));
}

static std::string BT_GetBlackboardString(u64 entityId, const std::string& key) {
    if (!s_BindingsWorld) return "";
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (!bt) return "";
    auto* val = bt->blackboard.Get(key);
    if (val && std::holds_alternative<std::string>(*val)) return std::get<std::string>(*val);
    return "";
}

static void BT_ClearBlackboard(u64 entityId) {
    if (!s_BindingsWorld) return;
    auto* bt = s_BindingsWorld->GetComponent<BehaviorTreeComponent>(static_cast<Entity>(entityId));
    if (bt) bt->blackboard.Clear();
}

// ============================================================================
// Navmesh bindings
// ============================================================================

static bool Navmesh_IsPointOnNavmesh(float x, float y, float z) {
    if (!s_BindingsNavmesh) return false;
    return s_BindingsNavmesh->IsPointOnNavmesh(Math::Vector3(x, y, z));
}

static bool Navmesh_HasNavmesh() {
    return s_BindingsNavmesh != nullptr && s_BindingsPathfinder != nullptr;
}

// ============================================================================
// Pathfinding
// ============================================================================

static AI::PathResult s_LastPathResult;

static int Navmesh_FindPath(f32 sx, f32 sy, f32 sz, f32 ex, f32 ey, f32 ez) {
    s_LastPathResult = AI::PathResult{};
    if (!s_BindingsPathfinder) return 0;
    s_LastPathResult = s_BindingsPathfinder->FindPath(
        Math::Vector3(sx, sy, sz), Math::Vector3(ex, ey, ez));
    return s_LastPathResult.success ? static_cast<int>(s_LastPathResult.waypoints.size()) : 0;
}

static bool Navmesh_PathExists(f32 sx, f32 sy, f32 sz, f32 ex, f32 ey, f32 ez) {
    if (!s_BindingsPathfinder) return false;
    return s_BindingsPathfinder->PathExists(
        Math::Vector3(sx, sy, sz), Math::Vector3(ex, ey, ez));
}

static Math::Vector3 Navmesh_GetPathWaypoint(int index) {
    if (index < 0 || index >= static_cast<int>(s_LastPathResult.waypoints.size()))
        return Math::Vector3(0, 0, 0);
    return s_LastPathResult.waypoints[index];
}

static f32 Navmesh_GetPathCost() {
    return s_LastPathResult.totalCost;
}

// ---------------------------------------------------------------------------
// Public forwarders for VisualScript navmesh nodes. Reuse the same navmesh /
// pathfinder pointers + last-path buffer the AngelScript API uses (set via
// SetBindingsNavmesh at every call site), so node queries behave identically.
// ---------------------------------------------------------------------------
namespace Enjin { namespace Scripting {

bool VSNavmeshHasNavmesh() { return s_BindingsNavmesh != nullptr && s_BindingsPathfinder != nullptr; }

bool VSNavmeshIsPointOn(f32 x, f32 y, f32 z) {
    return s_BindingsNavmesh ? s_BindingsNavmesh->IsPointOnNavmesh(Math::Vector3(x, y, z)) : false;
}

i32 VSNavmeshFindPath(f32 sx, f32 sy, f32 sz, f32 ex, f32 ey, f32 ez) {
    s_LastPathResult = AI::PathResult{};
    if (!s_BindingsPathfinder) return 0;
    s_LastPathResult = s_BindingsPathfinder->FindPath(Math::Vector3(sx, sy, sz), Math::Vector3(ex, ey, ez));
    return s_LastPathResult.success ? static_cast<i32>(s_LastPathResult.waypoints.size()) : 0;
}

bool VSNavmeshPathExists(f32 sx, f32 sy, f32 sz, f32 ex, f32 ey, f32 ez) {
    return s_BindingsPathfinder
        ? s_BindingsPathfinder->PathExists(Math::Vector3(sx, sy, sz), Math::Vector3(ex, ey, ez))
        : false;
}

void VSNavmeshGetWaypoint(i32 index, f32& x, f32& y, f32& z) {
    x = 0.0f; y = 0.0f; z = 0.0f;
    if (index < 0 || index >= static_cast<i32>(s_LastPathResult.waypoints.size())) return;
    const auto& w = s_LastPathResult.waypoints[static_cast<usize>(index)];
    x = w.x; y = w.y; z = w.z;
}

} } // namespace Enjin::Scripting

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterAIBindings(asIScriptEngine* engine) {
    // AI state enum constants
    AS_CHECK(engine->RegisterEnum("AIState"));
    AS_CHECK(engine->RegisterEnumValue("AIState", "AI_IDLE", 0));
    AS_CHECK(engine->RegisterEnumValue("AIState", "AI_PATROL", 1));
    AS_CHECK(engine->RegisterEnumValue("AIState", "AI_CHASE", 2));
    AS_CHECK(engine->RegisterEnumValue("AIState", "AI_ATTACK", 3));
    AS_CHECK(engine->RegisterEnumValue("AIState", "AI_FLEE", 4));
    AS_CHECK(engine->RegisterEnumValue("AIState", "AI_DEAD", 5));

    // AI Controller
    AS_CHECK(engine->RegisterGlobalFunction("void AI_SetState(uint64, int)",
        ENJIN_AS_FN(AI_SetState), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int AI_GetState(uint64)",
        ENJIN_AS_FN(AI_GetState), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AI_SetTarget(uint64, uint64)",
        ENJIN_AS_FN(AI_SetTarget), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint64 AI_GetTarget(uint64)",
        ENJIN_AS_FN(AI_GetTarget), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AI_SetTargetPosition(uint64, float, float, float)",
        ENJIN_AS_FN(AI_SetTargetPosition), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AI_SetMoveSpeed(uint64, float)",
        ENJIN_AS_FN(AI_SetMoveSpeed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float AI_GetMoveSpeed(uint64)",
        ENJIN_AS_FN(AI_GetMoveSpeed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AI_SetDetectionRange(uint64, float)",
        ENJIN_AS_FN(AI_SetDetectionRange), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float AI_GetDetectionRange(uint64)",
        ENJIN_AS_FN(AI_GetDetectionRange), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AI_SetAttackRange(uint64, float)",
        ENJIN_AS_FN(AI_SetAttackRange), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float AI_GetAttackRange(uint64)",
        ENJIN_AS_FN(AI_GetAttackRange), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AI_SetChaseSpeed(uint64, float)",
        ENJIN_AS_FN(AI_SetChaseSpeed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AI_SetFleeSpeed(uint64, float)",
        ENJIN_AS_FN(AI_SetFleeSpeed), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AI_SetFieldOfView(uint64, float)",
        ENJIN_AS_FN(AI_SetFieldOfView), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AI_SetUseNavmesh(uint64, bool)",
        ENJIN_AS_FN(AI_SetUseNavmesh), ENJIN_AS_CALL_CDECL));

    // Behavior Tree
    AS_CHECK(engine->RegisterGlobalFunction("bool BT_IsEnabled(uint64)",
        ENJIN_AS_FN(BT_IsEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BT_Enable(uint64)",
        ENJIN_AS_FN(BT_Enable), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BT_Disable(uint64)",
        ENJIN_AS_FN(BT_Disable), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BT_Reset(uint64)",
        ENJIN_AS_FN(BT_Reset), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BT_SetBlackboardFloat(uint64, const string&in, float)",
        ENJIN_AS_FN(BT_SetBlackboardFloat), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float BT_GetBlackboardFloat(uint64, const string&in)",
        ENJIN_AS_FN(BT_GetBlackboardFloat), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BT_SetBlackboardInt(uint64, const string&in, int)",
        ENJIN_AS_FN(BT_SetBlackboardInt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int BT_GetBlackboardInt(uint64, const string&in)",
        ENJIN_AS_FN(BT_GetBlackboardInt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BT_SetBlackboardBool(uint64, const string&in, bool)",
        ENJIN_AS_FN(BT_SetBlackboardBool), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool BT_GetBlackboardBool(uint64, const string&in)",
        ENJIN_AS_FN(BT_GetBlackboardBool), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BT_SetBlackboardString(uint64, const string&in, const string&in)",
        ENJIN_AS_FN(BT_SetBlackboardString), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string BT_GetBlackboardString(uint64, const string&in)",
        ENJIN_AS_FN(BT_GetBlackboardString), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void BT_ClearBlackboard(uint64)",
        ENJIN_AS_FN(BT_ClearBlackboard), ENJIN_AS_CALL_CDECL));

    // Navmesh
    AS_CHECK(engine->RegisterGlobalFunction("bool Navmesh_IsPointOnNavmesh(float, float, float)",
        ENJIN_AS_FN(Navmesh_IsPointOnNavmesh), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Navmesh_HasNavmesh()",
        ENJIN_AS_FN(Navmesh_HasNavmesh), ENJIN_AS_CALL_CDECL));

    // Pathfinding
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Navmesh_FindPath(float, float, float, float, float, float)",
        ENJIN_AS_FN(Navmesh_FindPath), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Navmesh_PathExists(float, float, float, float, float, float)",
        ENJIN_AS_FN(Navmesh_PathExists), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Navmesh_GetPathWaypoint(int)",
        ENJIN_AS_FN(Navmesh_GetPathWaypoint), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float Navmesh_GetPathCost()",
        ENJIN_AS_FN(Navmesh_GetPathCost), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
