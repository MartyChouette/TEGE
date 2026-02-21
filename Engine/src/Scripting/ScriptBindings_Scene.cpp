#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
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

// Deferred entity destruction queue — entities are queued during script
// execution and destroyed after the script update completes, preventing
// invalidation of iterators and component pointers mid-frame.
static std::vector<ECS::Entity> s_DeferredDestroys;

namespace Enjin {
namespace Scripting {
void SetBindingsSceneManager(Scene::SceneManager* mgr) { s_BindingsSceneManager = mgr; }

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
    if (!s_BindingsSceneManager->LoadScene(sceneName)) {
        ENJIN_LOG_WARN(Script, "Scene_LoadScene: failed to load scene '%s'", sceneName.c_str());
    }
}

static std::string Scene_GetCurrentScene() {
    if (!s_BindingsSceneManager) return "";
    return s_BindingsSceneManager->GetCurrentSceneName();
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
        asFUNCTION(Entity_GetPosition), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Entity_SetPosition(uint64, const Vector3 &in)",
        asFUNCTION(Entity_SetPosition), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Entity_GetRotation(uint64)",
        asFUNCTION(Entity_GetRotation), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Entity_SetRotation(uint64, const Vector3 &in)",
        asFUNCTION(Entity_SetRotation), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Entity_GetScale(uint64)",
        asFUNCTION(Entity_GetScale), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Entity_SetScale(uint64, const Vector3 &in)",
        asFUNCTION(Entity_SetScale), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Entity_GetName(uint64)",
        asFUNCTION(Entity_GetName), asCALL_CDECL));

    // Entity visibility
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Entity_IsVisible(uint64)",
        asFUNCTION(Entity_IsVisible), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Entity_SetVisible(uint64, bool)",
        asFUNCTION(Entity_SetVisible), asCALL_CDECL));

    // Entity lookup
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_FindEntity(const string &in)",
        asFUNCTION(Scene_FindEntity), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_FindEntityByTag(const string &in)",
        asFUNCTION(Scene_FindEntityByTag), asCALL_CDECL));

    // Entity lifecycle
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_DestroyEntity(uint64)",
        asFUNCTION(Scene_DestroyEntity), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_Instantiate()",
        asFUNCTION(Scene_Instantiate), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_InstantiateNamed(const string &in)",
        asFUNCTION(Scene_InstantiateNamed), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_InstantiateAt(const Vector3 &in)",
        asFUNCTION(Scene_InstantiateAt), asCALL_CDECL));

    // Entity queries
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Scene_IsValid(uint64)",
        asFUNCTION(Scene_IsValid), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Scene_GetEntityCount()",
        asFUNCTION(Scene_GetEntityCount), asCALL_CDECL));

    // Name management
    AS_CHECK(engine->RegisterGlobalFunction(
        "string Scene_GetEntityName(uint64)",
        asFUNCTION(Scene_GetEntityName), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_SetEntityName(uint64, const string &in)",
        asFUNCTION(Scene_SetEntityName), asCALL_CDECL));

    // Tag management
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_AddTag(uint64, const string &in)",
        asFUNCTION(Scene_AddTag), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_RemoveTag(uint64, const string &in)",
        asFUNCTION(Scene_RemoveTag), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Scene_HasTag(uint64, const string &in)",
        asFUNCTION(Scene_HasTag), asCALL_CDECL));

    // Scene management (SceneManager integration)
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Scene_LoadScene(const string &in)",
        asFUNCTION(Scene_LoadScene), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Scene_GetCurrentScene()",
        asFUNCTION(Scene_GetCurrentScene), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
