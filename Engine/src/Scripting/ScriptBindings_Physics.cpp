#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Physics/SimplePhysics.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

extern ECS::World* s_BindingsWorld;

static Physics::SimplePhysics* s_BindingsPhysics = nullptr;

namespace Enjin {
namespace Scripting {
void SetBindingsPhysics(Physics::SimplePhysics* physics) { s_BindingsPhysics = physics; }
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
// Physics functions — wired to SimplePhysics
// ============================================================================

static bool Physics_Raycast(const Vector3& origin, const Vector3& direction, f32 maxDistance) {
    if (!s_BindingsPhysics) return false;

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
        "Vector3 Physics_GetVelocity(uint64)",
        asFUNCTION(Physics_GetVelocity), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics_SetGravityScale(uint64, float)",
        asFUNCTION(Physics_SetGravityScale), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
