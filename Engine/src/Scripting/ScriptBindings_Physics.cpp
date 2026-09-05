#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/ECS/CameraMath.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#ifndef ENJIN_PLATFORM_WEB
#include "Enjin/Editor/ScenePicker.h"
#endif
#include "Enjin/Renderer/Camera.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

static Physics::IPhysicsBackend* s_BindingsPhysics = nullptr;

// Render view for screen-space queries (mouse picking). The runtime pushes the active
// render camera + viewport size each frame so scripts can raycast under the cursor.
static const Renderer::Camera* s_BindingsRenderCamera = nullptr;
static f32 s_BindingsViewW = 1280.0f;
static f32 s_BindingsViewH = 720.0f;

namespace Enjin {
namespace Scripting {
void SetBindingsPhysics(Physics::IPhysicsBackend* physics) { s_BindingsPhysics = physics; }
void SetBindingsRenderView(const Renderer::Camera* camera, f32 viewportWidth, f32 viewportHeight) {
    s_BindingsRenderCamera = camera;
    if (viewportWidth > 0.0f) s_BindingsViewW = viewportWidth;
    if (viewportHeight > 0.0f) s_BindingsViewH = viewportHeight;
}
} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Physics data structures
// ============================================================================

struct RaycastHit {
    Vector3 point;
    Vector3 normal;
    f32     distance;
    u64     entity;

    RaycastHit()
        : point(), normal(0, 1, 0), distance(0.0f), entity(0) {}
};

static void RaycastHit_DefaultConstruct(RaycastHit* self) {
    new(self) RaycastHit();
}

static void RaycastHit_CopyConstruct(const RaycastHit& other, RaycastHit* self) {
    new(self) RaycastHit(other);
}

// ============================================================================
// Physics functions — wired to IPhysicsBackend
// ============================================================================

static bool Physics_Raycast(const Vector3& origin, const Vector3& direction, f32 maxDistance) {
    if (!s_BindingsPhysics) return false;
    if (direction.LengthSquared() < 1e-12f) return false; // zero-length direction

    Physics::Ray ray;
    ray.origin = origin;
    ray.direction = direction.Normalized();

    Physics::RaycastHit hit = s_BindingsPhysics->Raycast(ray, maxDistance);
    return hit.hit;
}

static bool Physics_RaycastHit(const Vector3& origin, const Vector3& direction,
                               f32 maxDistance, RaycastHit& outHit) {
    if (!s_BindingsPhysics) {
        outHit = RaycastHit();
        return false;
    }
    if (direction.LengthSquared() < 1e-12f) {
        outHit = RaycastHit();
        return false; // zero-length direction
    }

    Physics::Ray ray;
    ray.origin = origin;
    ray.direction = direction.Normalized();

    Physics::RaycastHit hit = s_BindingsPhysics->Raycast(ray, maxDistance);
    if (hit.hit) {
        outHit.point = hit.point;
        outHit.normal = hit.normal;
        outHit.distance = hit.distance;
        outHit.entity = static_cast<u64>(hit.entity);
        return true;
    }

    outHit = RaycastHit();
    return false;
}

// Screen-space pick: turn a mouse/screen position into a world ray through the active
// camera and return the first entity hit (0 = nothing). Pair with Input_GetMousePosition
// for "is the cursor over this object" gameplay (hover-to-open, click-to-use).
static u64 Physics_RaycastScreen(f32 screenX, f32 screenY) {
    // Through ECS::ScreenToRayVP, which lives in the Engine module. This used
    // to call Editor::ScenePicker, and the web build excludes the Editor
    // module -- so on web this function returned "nothing hit" unconditionally
    // and every click-to-pick game was dead in the browser.
    if (!s_BindingsPhysics || !s_BindingsRenderCamera) return 0;
    Math::Vector3 origin, dir;
    if (!ECS::ScreenToRayVP(s_BindingsRenderCamera->GetViewProjectionMatrix(),
                            Vector2(screenX, screenY),
                            s_BindingsViewW, s_BindingsViewH, origin, dir)) {
        return 0;
    }
    Physics::Ray pr;
    pr.origin = origin;
    pr.direction = dir;
    Physics::RaycastHit hit = s_BindingsPhysics->Raycast(pr, 1000.0f);
    return hit.hit ? static_cast<u64>(hit.entity) : 0;
}

// Engine-side entry for the same pick (mouse-hover script callbacks). Lives here
// because the camera/viewport/physics statics are file-local to this TU.
u64 Enjin::Scripting::BindingsPickEntityAtScreen(f32 screenX, f32 screenY) {
    return Physics_RaycastScreen(screenX, screenY);
}

static Vector2 Input_GetScreenSize() {
    return Vector2(s_BindingsViewW, s_BindingsViewH);
}

// ---------------------------------------------------------------------------
// Screen <-> world.
//
// The engine has always held the projection matrix and never exposed this, so
// a 2D game had to reimplement orthographic unprojection in script from the
// screen size plus a hand-copied camera orthoSize. That duplicates the
// camera's size into a second file: change it in the scene and every click
// lands on the wrong place, with nothing on screen to say why.
// ---------------------------------------------------------------------------

// The world point under a screen pixel, on the z = 0 plane. This is the 2D
// case -- a board, a card field, a tilemap.
static Vector3 Camera_ScreenToWorld(u64 cameraEntity, const Vector2& screen) {
    Math::Vector3 out(0.0f, 0.0f, 0.0f);
    if (!s_BindingsWorld) return out;
    ECS::ScreenToWorldOnPlane(s_BindingsWorld, static_cast<ECS::Entity>(cameraEntity),
                              screen, s_BindingsViewW, s_BindingsViewH, 0.0f, out);
    return out;
}

// The same against any plane of constant Z, for a board that does not sit at
// the origin.
static Vector3 Camera_ScreenToWorldOnPlane(u64 cameraEntity, const Vector2& screen, f32 planeZ) {
    Math::Vector3 out(0.0f, 0.0f, 0.0f);
    if (!s_BindingsWorld) return out;
    ECS::ScreenToWorldOnPlane(s_BindingsWorld, static_cast<ECS::Entity>(cameraEntity),
                              screen, s_BindingsViewW, s_BindingsViewH, planeZ, out);
    return out;
}

// Where a world point lands on screen, in pixels from the top-left. Returns
// (-1, -1) when the point is behind the camera, which a caller must be able to
// tell from a real position -- pinning a label to something behind you would
// otherwise look like a valid answer.
static Vector2 Camera_WorldToScreen(u64 cameraEntity, const Vector3& worldPoint) {
    Math::Vector2 out(-1.0f, -1.0f);
    if (!s_BindingsWorld) return out;
    if (!ECS::WorldToScreen(s_BindingsWorld, static_cast<ECS::Entity>(cameraEntity),
                            worldPoint, s_BindingsViewW, s_BindingsViewH, out)) {
        return Math::Vector2(-1.0f, -1.0f);
    }
    return out;
}

static bool Physics_CheckSphere(const Vector3& center, f32 radius) {
    if (!s_BindingsPhysics) return false;

    auto entities = s_BindingsPhysics->GetCollidersInRadius(center, radius);
    return !entities.empty();
}

static bool Physics_CheckBox(const Vector3& center, const Vector3& halfExtents) {
    if (!s_BindingsPhysics || !s_BindingsWorld) return false;

    Physics::AABB testBox = Physics::AABB::FromCenterSize(center, halfExtents * 2.0f);

    // Check against all entities with box colliders
    auto entities = s_BindingsWorld->GetEntitiesWithComponent<BoxColliderComponent>();
    for (Entity e : entities) {
        auto* tc = s_BindingsWorld->GetComponent<TransformComponent>(e);
        auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(e);
        if (!tc || !bc) continue;

        Vector3 worldCenter = tc->position + bc->center;
        Physics::AABB entityBox = Physics::AABB::FromCenterSize(worldCenter, bc->size);
        if (testBox.Intersects(entityBox)) {
            return true;
        }
    }
    return false;
}

// Masked overloads — filter by collision layer mask
static bool Physics_Raycast_Masked(const Vector3& origin, const Vector3& direction, f32 maxDistance, u32 layerMask) {
    if (!s_BindingsPhysics) return false;
    if (direction.LengthSquared() < 1e-12f) return false; // zero-length direction

    Physics::Ray ray;
    ray.origin = origin;
    ray.direction = direction.Normalized();

    Physics::RaycastHit hit = s_BindingsPhysics->Raycast(ray, maxDistance, layerMask);
    return hit.hit;
}

static bool Physics_RaycastHit_Masked(const Vector3& origin, const Vector3& direction,
                                      f32 maxDistance, u32 layerMask, RaycastHit& outHit) {
    if (!s_BindingsPhysics) {
        outHit = RaycastHit();
        return false;
    }
    if (direction.LengthSquared() < 1e-12f) {
        outHit = RaycastHit();
        return false; // zero-length direction
    }

    Physics::Ray ray;
    ray.origin = origin;
    ray.direction = direction.Normalized();

    Physics::RaycastHit hit = s_BindingsPhysics->Raycast(ray, maxDistance, layerMask);
    if (hit.hit) {
        outHit.point = hit.point;
        outHit.normal = hit.normal;
        outHit.distance = hit.distance;
        outHit.entity = static_cast<u64>(hit.entity);
        return true;
    }

    outHit = RaycastHit();
    return false;
}

static bool Physics_CheckSphere_Masked(const Vector3& center, f32 radius, u32 layerMask) {
    if (!s_BindingsPhysics) return false;

    auto entities = s_BindingsPhysics->GetCollidersInRadius(center, radius, layerMask);
    return !entities.empty();
}

static bool Physics_CheckBox_Masked(const Vector3& center, const Vector3& halfExtents, u32 layerMask) {
    if (!s_BindingsPhysics || !s_BindingsWorld) return false;

    Physics::AABB testBox = Physics::AABB::FromCenterSize(center, halfExtents * 2.0f);

    auto entities = s_BindingsWorld->GetEntitiesWithComponent<BoxColliderComponent>();
    for (Entity e : entities) {
        auto* tc = s_BindingsWorld->GetComponent<TransformComponent>(e);
        auto* bc = s_BindingsWorld->GetComponent<BoxColliderComponent>(e);
        if (!tc || !bc) continue;

        if (!(bc->categoryBits & layerMask)) continue;

        Vector3 worldCenter = tc->position + bc->center;
        Physics::AABB entityBox = Physics::AABB::FromCenterSize(worldCenter, bc->size);
        if (testBox.Intersects(entityBox)) {
            return true;
        }
    }
    return false;
}

static void Physics_AddForce(u64 entityId, const Vector3& force) {
    if (!s_BindingsWorld) return;

    Entity entity = static_cast<Entity>(entityId);
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(entity);
    if (!rb || rb->bodyType != RigidbodyComponent::BodyType::Dynamic) return;

    // F = ma, so dv = F/m * dt. We accumulate as velocity change (applied over frame).
    f32 invMass = (rb->mass > 0.0f) ? 1.0f / rb->mass : 0.0f;
    rb->velocity = rb->velocity + force * invMass;
}

static void Physics_AddImpulse(u64 entityId, const Vector3& impulse) {
    if (!s_BindingsWorld) return;

    Entity entity = static_cast<Entity>(entityId);
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(entity);
    if (!rb || rb->bodyType != RigidbodyComponent::BodyType::Dynamic) return;

    // Impulse = instant velocity change: dv = J/m
    f32 invMass = (rb->mass > 0.0f) ? 1.0f / rb->mass : 0.0f;
    rb->velocity = rb->velocity + impulse * invMass;
}

static void Physics_SetVelocity(u64 entityId, const Vector3& velocity) {
    if (!s_BindingsWorld) return;

    Entity entity = static_cast<Entity>(entityId);
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(entity);
    if (rb) rb->velocity = velocity;
}

static void Physics_Teleport(u64 entityId, const Vector3& position) {
    if (!s_BindingsWorld) return;

    Entity entity = static_cast<Entity>(entityId);
    auto* t = s_BindingsWorld->GetComponent<TransformComponent>(entity);
    if (!t) return;
    t->position = position;

    // Zero component-side velocity so the backend sync doesn't re-apply it
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(entity);
    if (rb) {
        rb->velocity = Vector3(0, 0, 0);
        rb->angularVelocity = Vector3(0, 0, 0);
    }

    // Teleport the physics body through the rewind system's force-set path.
    // Setting only the TransformComponent is not enough for dynamic bodies —
    // the body's pose overwrites the transform on the next physics step.
    if (s_BindingsPhysics) {
        s_BindingsPhysics->ForceSetBodyState(entity, position, t->rotation,
                                             Vector3(0, 0, 0), Vector3(0, 0, 0));
    }
}

static Vector3 Physics_GetVelocity(u64 entityId) {
    if (!s_BindingsWorld) return Vector3();

    Entity entity = static_cast<Entity>(entityId);
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(entity);
    return rb ? rb->velocity : Vector3();
}

static void Physics_SetGravityScale(u64 entityId, f32 scale) {
    if (!s_BindingsWorld) return;

    Entity entity = static_cast<Entity>(entityId);
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(entity);
    if (rb) rb->gravityScale = scale;
}

// ============================================================================
// Physics joints
// ============================================================================

static u64 Physics_CreateDistanceJoint(u64 entityAId, u64 entityBId, f32 restDistance) {
    if (!s_BindingsWorld) return INVALID_ENTITY;
    Entity entityA = static_cast<Entity>(entityAId);
    Entity entityB = static_cast<Entity>(entityBId);
    if (!s_BindingsWorld->IsValid(entityA) || !s_BindingsWorld->IsValid(entityB)) return INVALID_ENTITY;

    Entity joint = s_BindingsWorld->CreateEntity();
    s_BindingsWorld->AddComponent<NameComponent>(joint, NameComponent{"DistanceJoint"});
    s_BindingsWorld->AddComponent<DistanceJointComponent>(joint);
    auto* jc = s_BindingsWorld->GetComponent<DistanceJointComponent>(joint);
    jc->entityA = entityA;
    jc->entityB = entityB;
    jc->restDistance = restDistance;
    return static_cast<u64>(joint);
}

static u64 Physics_CreateHingeJoint(u64 entityAId, u64 entityBId, f32 axisX, f32 axisY, f32 axisZ) {
    if (!s_BindingsWorld) return INVALID_ENTITY;
    Entity entityA = static_cast<Entity>(entityAId);
    Entity entityB = static_cast<Entity>(entityBId);
    if (!s_BindingsWorld->IsValid(entityA) || !s_BindingsWorld->IsValid(entityB)) return INVALID_ENTITY;

    Entity joint = s_BindingsWorld->CreateEntity();
    s_BindingsWorld->AddComponent<NameComponent>(joint, NameComponent{"HingeJoint"});
    s_BindingsWorld->AddComponent<HingeJointComponent>(joint);
    auto* jc = s_BindingsWorld->GetComponent<HingeJointComponent>(joint);
    jc->entityA = entityA;
    jc->entityB = entityB;
    jc->axis = Vector3(axisX, axisY, axisZ);
    return static_cast<u64>(joint);
}

static void Physics_DestroyJoint(u64 jointId) {
    if (!s_BindingsWorld) return;
    Entity joint = static_cast<Entity>(jointId);
    if (!s_BindingsWorld->IsValid(joint)) return;
    s_BindingsWorld->DestroyEntity(joint);
}

static void DistanceJoint_SetRestDistance(u64 jointId, f32 distance) {
    if (!s_BindingsWorld) return;
    auto* jc = s_BindingsWorld->GetComponent<DistanceJointComponent>(static_cast<Entity>(jointId));
    if (jc) jc->restDistance = distance;
}

static f32 DistanceJoint_GetCurrentStress(u64 jointId) {
    if (!s_BindingsWorld) return 0.0f;
    auto* jc = s_BindingsWorld->GetComponent<DistanceJointComponent>(static_cast<Entity>(jointId));
    return jc ? jc->currentStress : 0.0f;
}

static void HingeJoint_SetLimits(u64 jointId, f32 lower, f32 upper) {
    if (!s_BindingsWorld) return;
    auto* jc = s_BindingsWorld->GetComponent<HingeJointComponent>(static_cast<Entity>(jointId));
    if (jc) {
        jc->useLimits = true;
        jc->lowerLimit = lower;
        jc->upperLimit = upper;
    }
}

static void HingeJoint_SetMotor(u64 jointId, f32 speed, f32 maxForce) {
    if (!s_BindingsWorld) return;
    auto* jc = s_BindingsWorld->GetComponent<HingeJointComponent>(static_cast<Entity>(jointId));
    if (jc) {
        jc->useMotor = true;
        jc->motorSpeed = speed;
        jc->motorMaxForce = maxForce;
    }
}

static f32 HingeJoint_GetCurrentAngle(u64 jointId) {
    if (!s_BindingsWorld) return 0.0f;
    auto* jc = s_BindingsWorld->GetComponent<HingeJointComponent>(static_cast<Entity>(jointId));
    return jc ? jc->currentAngle : 0.0f;
}

// ============================================================================
// Overlap queries returning entity lists (buffered)
// ============================================================================

static std::vector<Entity> s_OverlapResults;

static int Physics_OverlapSphereEntities(const Vector3& center, f32 radius) {
    s_OverlapResults.clear();
    if (!s_BindingsPhysics) return 0;
    s_OverlapResults = s_BindingsPhysics->GetCollidersInRadius(center, radius);
    return static_cast<int>(s_OverlapResults.size());
}

static int Physics_OverlapSphereEntitiesMask(const Vector3& center, f32 radius, u32 layerMask) {
    s_OverlapResults.clear();
    if (!s_BindingsPhysics) return 0;
    s_OverlapResults = s_BindingsPhysics->GetCollidersInRadius(center, radius, layerMask);
    return static_cast<int>(s_OverlapResults.size());
}

static int Physics_OverlapBoxEntities(const Vector3& center, const Vector3& halfExtents) {
    s_OverlapResults.clear();
    if (!s_BindingsPhysics) return 0;
    s_OverlapResults = s_BindingsPhysics->OverlapBox(center, halfExtents);
    return static_cast<int>(s_OverlapResults.size());
}

static int Physics_OverlapBoxEntitiesMask(const Vector3& center, const Vector3& halfExtents, u32 layerMask) {
    s_OverlapResults.clear();
    if (!s_BindingsPhysics) return 0;
    s_OverlapResults = s_BindingsPhysics->OverlapBox(center, halfExtents, layerMask);
    return static_cast<int>(s_OverlapResults.size());
}

static u64 Physics_GetOverlapResult(int index) {
    if (index < 0 || index >= static_cast<int>(s_OverlapResults.size())) return INVALID_ENTITY;
    return static_cast<u64>(s_OverlapResults[index]);
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterPhysicsBindings(asIScriptEngine* engine) {
    // ---- RaycastHit value type ----
    AS_CHECK(engine->RegisterObjectType("RaycastHit", sizeof(RaycastHit),
        asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<RaycastHit>()));

    AS_CHECK(engine->RegisterObjectBehaviour("RaycastHit", asBEHAVE_CONSTRUCT,
        "void f()", ENJIN_AS_OBJ_LAST(RaycastHit_DefaultConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("RaycastHit", asBEHAVE_CONSTRUCT,
        "void f(const RaycastHit &in)", ENJIN_AS_OBJ_LAST(RaycastHit_CopyConstruct), ENJIN_AS_CALL_CDECL_OBJLAST));

    AS_CHECK(engine->RegisterObjectProperty("RaycastHit", "Vector3 point",
        asOFFSET(RaycastHit, point)));
    AS_CHECK(engine->RegisterObjectProperty("RaycastHit", "Vector3 normal",
        asOFFSET(RaycastHit, normal)));
    AS_CHECK(engine->RegisterObjectProperty("RaycastHit", "float distance",
        asOFFSET(RaycastHit, distance)));
    AS_CHECK(engine->RegisterObjectProperty("RaycastHit", "uint64 entity",
        asOFFSET(RaycastHit, entity)));

    // ---- Physics query functions ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_Raycast(const Vector3 &in, const Vector3 &in, float)",
        ENJIN_AS_FN(Physics_Raycast), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_RaycastHit(const Vector3 &in, const Vector3 &in, float, RaycastHit &out)",
        ENJIN_AS_FN(Physics_RaycastHit), ENJIN_AS_CALL_CDECL));

    // Screen-space pick + screen size (world-object hover/click from script)
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Physics_RaycastScreen(float, float)",
        ENJIN_AS_FN(Physics_RaycastScreen), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector2 Input_GetScreenSize()",
        ENJIN_AS_FN(Input_GetScreenSize), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Camera_ScreenToWorld(uint64 camera, const Vector2 &in screen)",
        ENJIN_AS_FN(Camera_ScreenToWorld), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Camera_ScreenToWorldOnPlane(uint64 camera, const Vector2 &in screen, float planeZ)",
        ENJIN_AS_FN(Camera_ScreenToWorldOnPlane), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector2 Camera_WorldToScreen(uint64 camera, const Vector3 &in worldPoint)",
        ENJIN_AS_FN(Camera_WorldToScreen), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_CheckSphere(const Vector3 &in, float)",
        ENJIN_AS_FN(Physics_CheckSphere), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_CheckBox(const Vector3 &in, const Vector3 &in)",
        ENJIN_AS_FN(Physics_CheckBox), ENJIN_AS_CALL_CDECL));

    // Masked overloads (with layerMask parameter)
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_Raycast(const Vector3 &in, const Vector3 &in, float, uint)",
        ENJIN_AS_FN(Physics_Raycast_Masked), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_RaycastHit(const Vector3 &in, const Vector3 &in, float, uint, RaycastHit &out)",
        ENJIN_AS_FN(Physics_RaycastHit_Masked), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_CheckSphere(const Vector3 &in, float, uint)",
        ENJIN_AS_FN(Physics_CheckSphere_Masked), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_CheckBox(const Vector3 &in, const Vector3 &in, uint)",
        ENJIN_AS_FN(Physics_CheckBox_Masked), ENJIN_AS_CALL_CDECL));

    // ---- Physics body manipulation ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_AddForce(uint64, const Vector3 &in)",
        ENJIN_AS_FN(Physics_AddForce), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_AddImpulse(uint64, const Vector3 &in)",
        ENJIN_AS_FN(Physics_AddImpulse), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_SetVelocity(uint64, const Vector3 &in)",
        ENJIN_AS_FN(Physics_SetVelocity), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_Teleport(uint64, const Vector3 &in)",
        ENJIN_AS_FN(Physics_Teleport), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Physics_GetVelocity(uint64)",
        ENJIN_AS_FN(Physics_GetVelocity), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_SetGravityScale(uint64, float)",
        ENJIN_AS_FN(Physics_SetGravityScale), ENJIN_AS_CALL_CDECL));

    // ---- Physics joints ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Physics_CreateDistanceJoint(uint64, uint64, float)",
        ENJIN_AS_FN(Physics_CreateDistanceJoint), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Physics_CreateHingeJoint(uint64, uint64, float, float, float)",
        ENJIN_AS_FN(Physics_CreateHingeJoint), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_DestroyJoint(uint64)",
        ENJIN_AS_FN(Physics_DestroyJoint), ENJIN_AS_CALL_CDECL));

    // Distance joint accessors
    AS_CHECK(engine->RegisterGlobalFunction(
        "void DistanceJoint_SetRestDistance(uint64, float)",
        ENJIN_AS_FN(DistanceJoint_SetRestDistance), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float DistanceJoint_GetCurrentStress(uint64)",
        ENJIN_AS_FN(DistanceJoint_GetCurrentStress), ENJIN_AS_CALL_CDECL));

    // Hinge joint accessors
    AS_CHECK(engine->RegisterGlobalFunction(
        "void HingeJoint_SetLimits(uint64, float, float)",
        ENJIN_AS_FN(HingeJoint_SetLimits), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void HingeJoint_SetMotor(uint64, float, float)",
        ENJIN_AS_FN(HingeJoint_SetMotor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float HingeJoint_GetCurrentAngle(uint64)",
        ENJIN_AS_FN(HingeJoint_GetCurrentAngle), ENJIN_AS_CALL_CDECL));

    // ---- Overlap queries returning entities ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Physics_OverlapSphereEntities(const Vector3 &in, float)",
        ENJIN_AS_FN(Physics_OverlapSphereEntities), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Physics_OverlapSphereEntitiesMask(const Vector3 &in, float, uint)",
        ENJIN_AS_FN(Physics_OverlapSphereEntitiesMask), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Physics_OverlapBoxEntities(const Vector3 &in, const Vector3 &in)",
        ENJIN_AS_FN(Physics_OverlapBoxEntities), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Physics_OverlapBoxEntitiesMask(const Vector3 &in, const Vector3 &in, uint)",
        ENJIN_AS_FN(Physics_OverlapBoxEntitiesMask), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Physics_GetOverlapResult(int)",
        ENJIN_AS_FN(Physics_GetOverlapResult), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
