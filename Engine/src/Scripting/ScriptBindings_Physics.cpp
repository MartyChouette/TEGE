#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

static Physics::IPhysicsBackend* s_BindingsPhysics = nullptr;

namespace Enjin {
namespace Scripting {
void SetBindingsPhysics(Physics::IPhysicsBackend* physics) { s_BindingsPhysics = physics; }
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
        "void f()", asFUNCTION(RaycastHit_DefaultConstruct), asCALL_CDECL_OBJLAST));
    AS_CHECK(engine->RegisterObjectBehaviour("RaycastHit", asBEHAVE_CONSTRUCT,
        "void f(const RaycastHit &in)", asFUNCTION(RaycastHit_CopyConstruct), asCALL_CDECL_OBJLAST));

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
        asFUNCTION(Physics_Raycast), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_RaycastHit(const Vector3 &in, const Vector3 &in, float, RaycastHit &out)",
        asFUNCTION(Physics_RaycastHit), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_CheckSphere(const Vector3 &in, float)",
        asFUNCTION(Physics_CheckSphere), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_CheckBox(const Vector3 &in, const Vector3 &in)",
        asFUNCTION(Physics_CheckBox), asCALL_CDECL));

    // Masked overloads (with layerMask parameter)
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_Raycast(const Vector3 &in, const Vector3 &in, float, uint)",
        asFUNCTION(Physics_Raycast_Masked), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_RaycastHit(const Vector3 &in, const Vector3 &in, float, uint, RaycastHit &out)",
        asFUNCTION(Physics_RaycastHit_Masked), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_CheckSphere(const Vector3 &in, float, uint)",
        asFUNCTION(Physics_CheckSphere_Masked), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics_CheckBox(const Vector3 &in, const Vector3 &in, uint)",
        asFUNCTION(Physics_CheckBox_Masked), asCALL_CDECL));

    // ---- Physics body manipulation ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_AddForce(uint64, const Vector3 &in)",
        asFUNCTION(Physics_AddForce), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_AddImpulse(uint64, const Vector3 &in)",
        asFUNCTION(Physics_AddImpulse), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_SetVelocity(uint64, const Vector3 &in)",
        asFUNCTION(Physics_SetVelocity), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_Teleport(uint64, const Vector3 &in)",
        asFUNCTION(Physics_Teleport), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector3 Physics_GetVelocity(uint64)",
        asFUNCTION(Physics_GetVelocity), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_SetGravityScale(uint64, float)",
        asFUNCTION(Physics_SetGravityScale), asCALL_CDECL));

    // ---- Physics joints ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Physics_CreateDistanceJoint(uint64, uint64, float)",
        asFUNCTION(Physics_CreateDistanceJoint), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Physics_CreateHingeJoint(uint64, uint64, float, float, float)",
        asFUNCTION(Physics_CreateHingeJoint), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_DestroyJoint(uint64)",
        asFUNCTION(Physics_DestroyJoint), asCALL_CDECL));

    // Distance joint accessors
    AS_CHECK(engine->RegisterGlobalFunction(
        "void DistanceJoint_SetRestDistance(uint64, float)",
        asFUNCTION(DistanceJoint_SetRestDistance), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float DistanceJoint_GetCurrentStress(uint64)",
        asFUNCTION(DistanceJoint_GetCurrentStress), asCALL_CDECL));

    // Hinge joint accessors
    AS_CHECK(engine->RegisterGlobalFunction(
        "void HingeJoint_SetLimits(uint64, float, float)",
        asFUNCTION(HingeJoint_SetLimits), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void HingeJoint_SetMotor(uint64, float, float)",
        asFUNCTION(HingeJoint_SetMotor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float HingeJoint_GetCurrentAngle(uint64)",
        asFUNCTION(HingeJoint_GetCurrentAngle), asCALL_CDECL));

    // ---- Overlap queries returning entities ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Physics_OverlapSphereEntities(const Vector3 &in, float)",
        asFUNCTION(Physics_OverlapSphereEntities), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Physics_OverlapSphereEntitiesMask(const Vector3 &in, float, uint)",
        asFUNCTION(Physics_OverlapSphereEntitiesMask), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Physics_OverlapBoxEntities(const Vector3 &in, const Vector3 &in)",
        asFUNCTION(Physics_OverlapBoxEntities), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Physics_OverlapBoxEntitiesMask(const Vector3 &in, const Vector3 &in, uint)",
        asFUNCTION(Physics_OverlapBoxEntitiesMask), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Physics_GetOverlapResult(int)",
        asFUNCTION(Physics_GetOverlapResult), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
