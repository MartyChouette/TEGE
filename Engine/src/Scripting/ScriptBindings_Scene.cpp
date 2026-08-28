#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/Scene/SceneManager.h"
#include <angelscript.h>
#include <string>
#include <vector>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

// The world pointer is set in ScriptBindings.cpp via SetBindingsWorld()
extern Enjin::ECS::World* s_BindingsWorld;

// Scene manager pointer for scene loading from scripts
static Enjin::Scene::SceneManager* s_BindingsSceneManager = nullptr;

// Startup-flow advance flag: Flow_Advance() from a scene script sets this so the
// player can advance a Scene step whose advance mode is "script" (e.g. an intro
// cutscene that ends itself). The player owns the bool and polls + clears it.
static bool* s_FlowAdvanceFlag = nullptr;

// Deferred entity destruction queue — entities are queued during script
// execution and destroyed after the script update completes, preventing
// invalidation of iterators and component pointers mid-frame.
static std::vector<ECS::Entity> s_DeferredDestroys;

namespace Enjin {
namespace Scripting {
void SetBindingsSceneManager(Scene::SceneManager* mgr) { s_BindingsSceneManager = mgr; }
void SetBindingsFlowAdvanceFlag(bool* flag) { s_FlowAdvanceFlag = flag; }

void FlushDeferredEntityDestroys() {
    for (ECS::Entity e : s_DeferredDestroys) {
        if (s_BindingsWorld && s_BindingsWorld->IsValid(e)) {
            s_BindingsWorld->DestroyEntity(e);
        }
    }
    s_DeferredDestroys.clear();
    // Reset per-frame entity creation cap for next frame (S3/S4 fix)
    ResetFrameEntityCreationCount();
}
} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Entity transform/name functions (called by TegeBehavior.as)
// ============================================================================

// SC-H5: Validate entity before component access to prevent stale/recycled data
static Vector3 Entity_GetPosition(u64 id) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return Vector3();
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    return t ? t->position : Vector3();
}

static void Entity_SetPosition(u64 id, const Vector3& pos) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return;
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    if (t) t->position = pos;
}

static Vector3 Entity_GetRotation(u64 id) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return Vector3();
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    if (!t) return Vector3();
    Vector3 euler = t->rotation.ToEuler();
    return Vector3(Degrees(euler.x), Degrees(euler.y), Degrees(euler.z));
}

static void Entity_SetRotation(u64 id, const Vector3& eulerDeg) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return;
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    if (t) {
        t->rotation = Quaternion::FromEuler(
            Vector3(Radians(eulerDeg.x), Radians(eulerDeg.y), Radians(eulerDeg.z)));
    }
}

// Direction bases straight from the transform quaternion. Scripts should use
// these for aiming instead of rebuilding rotations from Entity_GetRotation's
// euler angles - the euler roundtrip degenerates near yaw +/-90.
static Vector3 Entity_GetForward(u64 id) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return Vector3(0, 0, -1);
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    if (!t) return Vector3(0, 0, -1);
    return t->rotation.Rotate(Vector3(0, 0, -1));
}

static Vector3 Entity_GetRight(u64 id) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return Vector3(1, 0, 0);
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    if (!t) return Vector3(1, 0, 0);
    return t->rotation.Rotate(Vector3(1, 0, 0));
}

static Vector3 Entity_GetUp(u64 id) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return Vector3(0, 1, 0);
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    if (!t) return Vector3(0, 1, 0);
    return t->rotation.Rotate(Vector3(0, 1, 0));
}

static Vector3 Entity_GetScale(u64 id) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return Vector3(1.0f);
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    return t ? t->scale : Vector3(1.0f);
}

static void Entity_SetScale(u64 id, const Vector3& s) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return;
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    if (t) t->scale = s;
}

static std::string Entity_GetName(u64 id) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return "";
    auto* n = s_BindingsWorld->GetComponent<NameComponent>(static_cast<Entity>(id));
    return n ? n->name : "";
}

static bool Entity_IsVisible(u64 id) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return true;
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    return t ? t->visible : true;
}

static void Entity_SetVisible(u64 id, bool visible) {
    if (!s_BindingsWorld || !s_BindingsWorld->IsValid(static_cast<Entity>(id))) return;
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    if (t) t->visible = visible;
}

// ============================================================================
// Entity hierarchy (parent-child)
// ============================================================================

static void Entity_SetParent(u64 childId, u64 parentId) {
    if (!s_BindingsWorld) return;
    Entity child = static_cast<Entity>(childId);
    Entity parent = static_cast<Entity>(parentId);
    if (!s_BindingsWorld->IsValid(child)) return;
    if (parent != INVALID_ENTITY && !s_BindingsWorld->IsValid(parent)) return;
    ECS::SetParent(s_BindingsWorld, child, parent);
}

static void Entity_RemoveParent(u64 childId) {
    if (!s_BindingsWorld) return;
    Entity child = static_cast<Entity>(childId);
    if (!s_BindingsWorld->IsValid(child)) return;
    ECS::RemoveParent(s_BindingsWorld, child);
}

static u64 Entity_GetParent(u64 childId) {
    if (!s_BindingsWorld) return INVALID_ENTITY;
    Entity child = static_cast<Entity>(childId);
    if (!s_BindingsWorld->IsValid(child)) return INVALID_ENTITY;
    return static_cast<u64>(ECS::GetParent(s_BindingsWorld, child));
}

static int Entity_GetChildCount(u64 parentId) {
    if (!s_BindingsWorld) return 0;
    Entity parent = static_cast<Entity>(parentId);
    if (!s_BindingsWorld->IsValid(parent)) return 0;
    return static_cast<int>(ECS::GetChildren(s_BindingsWorld, parent).size());
}

static u64 Entity_GetChild(u64 parentId, int index) {
    if (!s_BindingsWorld) return INVALID_ENTITY;
    Entity parent = static_cast<Entity>(parentId);
    if (!s_BindingsWorld->IsValid(parent)) return INVALID_ENTITY;
    const auto& children = ECS::GetChildren(s_BindingsWorld, parent);
    if (index < 0 || index >= static_cast<int>(children.size())) return INVALID_ENTITY;
    return static_cast<u64>(children[index]);
}

// ============================================================================
// Scene functions
// ============================================================================

static u64 Scene_FindEntity(const std::string& name) {
    if (!s_BindingsWorld) {
        ENJIN_LOG_WARN(Script, "Scene_FindEntity: no active world");
        return INVALID_ENTITY;
    }

    return static_cast<u64>(s_BindingsWorld->FindEntityByName(name));
}

static u64 Scene_FindEntityByTag(const std::string& tag) {
    if (!s_BindingsWorld) {
        ENJIN_LOG_WARN(Script, "Scene_FindEntityByTag: no active world");
        return INVALID_ENTITY;
    }

    for (Entity e : s_BindingsWorld->GetEntitiesWithComponent<TagComponent>()) {
        auto* tc = s_BindingsWorld->GetComponent<TagComponent>(e);
        if (tc && tc->HasTag(tag)) {
            return static_cast<u64>(e);
        }
    }

    return static_cast<u64>(INVALID_ENTITY);
}

static void Scene_DestroyEntity(u64 id) {
    if (!s_BindingsWorld) {
        ENJIN_LOG_WARN(Script, "Scene_DestroyEntity: no active world");
        return;
    }

    Entity entity = static_cast<Entity>(id);
    if (!s_BindingsWorld->IsValid(entity)) {
        ENJIN_LOG_WARN(Script, "Scene_DestroyEntity: entity %llu is not valid", id);
        return;
    }

    // SC-H4: Cap deferred destroy queue to prevent script DoS
    if (s_DeferredDestroys.size() >= 1024) return;

    // Defer destruction until after script iteration completes to avoid
    // invalidating iterators and component pointers mid-frame.
    s_DeferredDestroys.push_back(entity);
}

// S3/S4 fix: Per-frame entity creation cap to prevent script DoS
static constexpr u32 kMaxEntityCreationsPerFrame = 256;
static u32 s_FrameEntityCreationCount = 0;

namespace Enjin { namespace Scripting {
void ResetFrameEntityCreationCount() { s_FrameEntityCreationCount = 0; }
} }

bool CheckEntityCreationCap(const char* funcName) {
    if (s_FrameEntityCreationCount >= kMaxEntityCreationsPerFrame) {
        ENJIN_LOG_WARN(Script, "%s: per-frame entity creation cap (%u) reached", funcName, kMaxEntityCreationsPerFrame);
        return false;
    }
    s_FrameEntityCreationCount++;
    return true;
}

static u64 Scene_Instantiate() {
    if (!s_BindingsWorld) {
        ENJIN_LOG_WARN(Script, "Scene_Instantiate: no active world");
        return INVALID_ENTITY;
    }
    if (!CheckEntityCreationCap("Scene_Instantiate")) return INVALID_ENTITY;

    Entity entity = s_BindingsWorld->CreateEntity();
    // New entities get a TransformComponent by default so scripts can
    // immediately set position/rotation/scale.
    s_BindingsWorld->AddComponent<TransformComponent>(entity);
    return static_cast<u64>(entity);
}

static u64 Scene_InstantiateNamed(const std::string& name) {
    if (!s_BindingsWorld) {
        ENJIN_LOG_WARN(Script, "Scene_InstantiateNamed: no active world");
        return INVALID_ENTITY;
    }
    if (!CheckEntityCreationCap("Scene_InstantiateNamed")) return INVALID_ENTITY;

    Entity entity = s_BindingsWorld->CreateEntity();
    s_BindingsWorld->AddComponent<TransformComponent>(entity);
    s_BindingsWorld->AddComponent<NameComponent>(entity, NameComponent(name));
    return static_cast<u64>(entity);
}

static u64 Scene_InstantiateAt(const Vector3& position) {
    if (!s_BindingsWorld) {
        ENJIN_LOG_WARN(Script, "Scene_InstantiateAt: no active world");
        return INVALID_ENTITY;
    }
    if (!CheckEntityCreationCap("Scene_InstantiateAt")) return INVALID_ENTITY;

    Entity entity = s_BindingsWorld->CreateEntity();
    TransformComponent tc;
    tc.position = position;
    s_BindingsWorld->AddComponent<TransformComponent>(entity, tc);
    return static_cast<u64>(entity);
}

static bool Scene_IsValid(u64 id) {
    if (!s_BindingsWorld) return false;
    return s_BindingsWorld->IsValid(static_cast<Entity>(id));
}

static u64 Scene_GetEntityCount() {
    if (!s_BindingsWorld) return 0;
    return static_cast<u64>(s_BindingsWorld->GetEntityCount());
}

static std::string Scene_GetEntityName(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* nc = s_BindingsWorld->GetComponent<NameComponent>(static_cast<Entity>(id));
    return nc ? nc->name : "";
}

static void Scene_SetEntityName(u64 id, const std::string& name) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(id);
    if (!s_BindingsWorld->IsValid(entity)) return;

    auto* nc = s_BindingsWorld->GetComponent<NameComponent>(entity);
    if (nc) {
        nc->name = name;
    } else {
        s_BindingsWorld->AddComponent<NameComponent>(entity, NameComponent(name));
    }
    s_BindingsWorld->InvalidateNameCache();
}

static void Scene_AddTag(u64 id, const std::string& tag) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(id);
    if (!s_BindingsWorld->IsValid(entity)) return;

    if (s_BindingsWorld->HasComponent<TagComponent>(entity)) {
        auto* tc = s_BindingsWorld->GetComponent<TagComponent>(entity);
        tc->AddTag(tag);
    } else {
        TagComponent tc;
        tc.AddTag(tag);
        s_BindingsWorld->AddComponent<TagComponent>(entity, tc);
    }
}

static void Scene_RemoveTag(u64 id, const std::string& tag) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(id);
    if (!s_BindingsWorld->IsValid(entity)) return;
    auto* tc = s_BindingsWorld->GetComponent<TagComponent>(entity);
    if (tc) tc->RemoveTag(tag);
}

static bool Scene_HasTag(u64 id, const std::string& tag) {
    if (!s_BindingsWorld) return false;
    auto* tc = s_BindingsWorld->GetComponent<TagComponent>(static_cast<Entity>(id));
    return tc ? tc->HasTag(tag) : false;
}

// ============================================================================
// Scene management functions
// ============================================================================

static void Scene_LoadScene(const std::string& sceneName) {
    if (!s_BindingsSceneManager) {
        ENJIN_LOG_WARN(Script, "Scene_LoadScene: no scene manager set");
        return;
    }
    // DEFERRED (2026-08-28): loading synchronously here cleared the world while
    // scripts were iterating it - a use-after-free waiting to fire. The request
    // is consumed at the runtime's safe point (top of frame, no scripts on the
    // stack). Unknown names still warn immediately for author feedback.
    if (!s_BindingsSceneManager->GetSceneByName(sceneName)) {
        ENJIN_LOG_WARN(Script, "Scene_LoadScene: scene '%s' is not in the project scene list", sceneName.c_str());
        return;
    }
    s_BindingsSceneManager->RequestSceneChange(sceneName);
}

static std::string Scene_GetCurrentScene() {
    if (!s_BindingsSceneManager) return "";
    return s_BindingsSceneManager->GetCurrentSceneName();
}

// Advance the startup flow to its next step. Use from a scene set to advance on
// "script" (an intro/cutscene that decides when it is done). No-op if no flow
// is running or the current step does not advance on script.
static void Flow_Advance() {
    if (s_FlowAdvanceFlag) *s_FlowAdvanceFlag = true;
}

static void Scene_Restart() {
    if (!s_BindingsSceneManager) {
        ENJIN_LOG_WARN(Script, "Scene_Restart: no scene manager set");
        return;
    }
    // DEFERRED (2026-08-28): no current-name lookup here - the runtime decides
    // what "current" means (the player restarts its loaded scene; editor play
    // restarts the play session). This is what makes R-to-restart work in BOTH.
    s_BindingsSceneManager->RequestRestart();
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterSceneBindings(asIScriptEngine* engine) {
    // Entity transform/name functions (used by TegeBehavior.as)
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Entity_GetPosition(uint64)",
        ENJIN_AS_FN(Entity_GetPosition), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Entity_SetPosition(uint64, const Vector3 &in)",
        ENJIN_AS_FN(Entity_SetPosition), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Entity_GetRotation(uint64)",
        ENJIN_AS_FN(Entity_GetRotation), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Entity_SetRotation(uint64, const Vector3 &in)",
        ENJIN_AS_FN(Entity_SetRotation), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Entity_GetForward(uint64)",
        ENJIN_AS_FN(Entity_GetForward), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Entity_GetRight(uint64)",
        ENJIN_AS_FN(Entity_GetRight), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Entity_GetUp(uint64)",
        ENJIN_AS_FN(Entity_GetUp), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Entity_GetScale(uint64)",
        ENJIN_AS_FN(Entity_GetScale), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Entity_SetScale(uint64, const Vector3 &in)",
        ENJIN_AS_FN(Entity_SetScale), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Entity_GetName(uint64)",
        ENJIN_AS_FN(Entity_GetName), ENJIN_AS_CALL_CDECL));

    // Entity visibility
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Entity_IsVisible(uint64)",
        ENJIN_AS_FN(Entity_IsVisible), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Entity_SetVisible(uint64, bool)",
        ENJIN_AS_FN(Entity_SetVisible), ENJIN_AS_CALL_CDECL));

    // Entity hierarchy (parent-child)
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Entity_SetParent(uint64, uint64)",
        ENJIN_AS_FN(Entity_SetParent), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Entity_RemoveParent(uint64)",
        ENJIN_AS_FN(Entity_RemoveParent), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Entity_GetParent(uint64)",
        ENJIN_AS_FN(Entity_GetParent), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Entity_GetChildCount(uint64)",
        ENJIN_AS_FN(Entity_GetChildCount), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Entity_GetChild(uint64, int)",
        ENJIN_AS_FN(Entity_GetChild), ENJIN_AS_CALL_CDECL));

    // Entity lookup
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_FindEntity(const string &in)",
        ENJIN_AS_FN(Scene_FindEntity), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_FindEntityByTag(const string &in)",
        ENJIN_AS_FN(Scene_FindEntityByTag), ENJIN_AS_CALL_CDECL));

    // Entity lifecycle
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_DestroyEntity(uint64)",
        ENJIN_AS_FN(Scene_DestroyEntity), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_Instantiate()",
        ENJIN_AS_FN(Scene_Instantiate), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_InstantiateNamed(const string &in)",
        ENJIN_AS_FN(Scene_InstantiateNamed), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_InstantiateAt(const Vector3 &in)",
        ENJIN_AS_FN(Scene_InstantiateAt), ENJIN_AS_CALL_CDECL));

    // Entity queries
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Scene_IsValid(uint64)",
        ENJIN_AS_FN(Scene_IsValid), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_GetEntityCount()",
        ENJIN_AS_FN(Scene_GetEntityCount), ENJIN_AS_CALL_CDECL));

    // Name management
    AS_CHECK(engine->RegisterGlobalFunction(
        "string Scene_GetEntityName(uint64)",
        ENJIN_AS_FN(Scene_GetEntityName), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_SetEntityName(uint64, const string &in)",
        ENJIN_AS_FN(Scene_SetEntityName), ENJIN_AS_CALL_CDECL));

    // Tag management
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_AddTag(uint64, const string &in)",
        ENJIN_AS_FN(Scene_AddTag), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_RemoveTag(uint64, const string &in)",
        ENJIN_AS_FN(Scene_RemoveTag), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Scene_HasTag(uint64, const string &in)",
        ENJIN_AS_FN(Scene_HasTag), ENJIN_AS_CALL_CDECL));

    // Scene management (SceneManager integration)
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_LoadScene(const string &in)",
        ENJIN_AS_FN(Scene_LoadScene), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Scene_GetCurrentScene()",
        ENJIN_AS_FN(Scene_GetCurrentScene), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Flow_Advance()",
        ENJIN_AS_FN(Flow_Advance), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_Restart()",
        ENJIN_AS_FN(Scene_Restart), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
