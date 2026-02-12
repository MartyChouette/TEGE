#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

extern ECS::World* s_BindingsWorld;

static Physics::IPhysicsBackend2D* s_BindingsPhysics2D = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsPhysics2D(Physics::IPhysicsBackend2D* physics2d) {
    s_BindingsPhysics2D = physics2d;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Raycast
// ============================================================================

static bool Physics2D_Raycast(const Vector2& origin, const Vector2& direction, f32 maxDistance) {
    if (!s_BindingsPhysics2D) return false;
    Physics::RayHit2D hit;
    return s_BindingsPhysics2D->Raycast(origin, direction, maxDistance, hit);
}

static bool Physics2D_RaycastMask(const Vector2& origin, const Vector2& direction, f32 maxDistance, u32 layerMask) {
    if (!s_BindingsPhysics2D) return false;
    Physics::RayHit2D hit;
    return s_BindingsPhysics2D->Raycast(origin, direction, maxDistance, hit, layerMask);
}

static u64 Physics2D_RaycastHit(const Vector2& origin, const Vector2& direction, f32 maxDistance) {
    if (!s_BindingsPhysics2D) return static_cast<u64>(INVALID_ENTITY);
    Physics::RayHit2D hit;
    if (s_BindingsPhysics2D->Raycast(origin, direction, maxDistance, hit))
        return static_cast<u64>(hit.entity);
    return static_cast<u64>(INVALID_ENTITY);
}

static u64 Physics2D_RaycastHitMask(const Vector2& origin, const Vector2& direction, f32 maxDistance, u32 layerMask) {
    if (!s_BindingsPhysics2D) return static_cast<u64>(INVALID_ENTITY);
    Physics::RayHit2D hit;
    if (s_BindingsPhysics2D->Raycast(origin, direction, maxDistance, hit, layerMask))
        return static_cast<u64>(hit.entity);
    return static_cast<u64>(INVALID_ENTITY);
}

// ============================================================================
// Overlap queries
// ============================================================================

static bool Physics2D_OverlapCircle(const Vector2& center, f32 radius) {
    if (!s_BindingsPhysics2D) return false;
    std::vector<Entity> entities;
    return s_BindingsPhysics2D->OverlapCircle(center, radius, entities);
}

static bool Physics2D_OverlapCircleMask(const Vector2& center, f32 radius, u32 layerMask) {
    if (!s_BindingsPhysics2D) return false;
    std::vector<Entity> entities;
    return s_BindingsPhysics2D->OverlapCircle(center, radius, entities, layerMask);
}

static bool Physics2D_OverlapBox(const Vector2& center, const Vector2& halfExtents) {
    if (!s_BindingsPhysics2D) return false;
    std::vector<Entity> entities;
    return s_BindingsPhysics2D->OverlapBox(center, halfExtents, entities);
}

static bool Physics2D_OverlapBoxMask(const Vector2& center, const Vector2& halfExtents, u32 layerMask) {
    if (!s_BindingsPhysics2D) return false;
    std::vector<Entity> entities;
    return s_BindingsPhysics2D->OverlapBox(center, halfExtents, entities, layerMask);
}

// ============================================================================
// Body manipulation (operates on RigidbodyComponent velocity, same as 3D)
// ============================================================================

static void Physics2D_AddForce(u64 entityId, const Vector2& force) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(entityId);
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(entity);
    if (!rb || rb->bodyType != RigidbodyComponent::BodyType::Dynamic) return;

    f32 invMass = (rb->mass > 0.0f) ? 1.0f / rb->mass : 0.0f;
    rb->velocity.x += force.x * invMass;
    rb->velocity.y += force.y * invMass;
}

static void Physics2D_AddImpulse(u64 entityId, const Vector2& impulse) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(entityId);
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(entity);
    if (!rb || rb->bodyType != RigidbodyComponent::BodyType::Dynamic) return;

    f32 invMass = (rb->mass > 0.0f) ? 1.0f / rb->mass : 0.0f;
    rb->velocity.x += impulse.x * invMass;
    rb->velocity.y += impulse.y * invMass;
}

static void Physics2D_SetVelocity(u64 entityId, const Vector2& velocity) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(entityId);
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(entity);
    if (rb) {
        rb->velocity.x = velocity.x;
        rb->velocity.y = velocity.y;
    }
}

static Vector2 Physics2D_GetVelocity(u64 entityId) {
    if (!s_BindingsWorld) return Vector2();
    Entity entity = static_cast<Entity>(entityId);
    auto* rb = s_BindingsWorld->GetComponent<RigidbodyComponent>(entity);
    return rb ? Vector2(rb->velocity.x, rb->velocity.y) : Vector2();
}

// ============================================================================
// Gravity
// ============================================================================

static void Physics2D_SetGravity(const Vector2& gravity) {
    if (s_BindingsPhysics2D) s_BindingsPhysics2D->SetGravity(gravity);
}

static Vector2 Physics2D_GetGravity() {
    if (!s_BindingsPhysics2D) return Vector2(0.0f, -9.81f);
    return s_BindingsPhysics2D->GetGravity();
}

static void Physics2D_SetGravityScale(u64 entityId, f32 scale) {
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

void RegisterPhysics2DBindings(asIScriptEngine* engine) {
    // ---- Raycast ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics2D_Raycast(const Vector2 &in, const Vector2 &in, float)",
        asFUNCTION(Physics2D_Raycast), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics2D_RaycastMask(const Vector2 &in, const Vector2 &in, float, uint)",
        asFUNCTION(Physics2D_RaycastMask), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Physics2D_RaycastHit(const Vector2 &in, const Vector2 &in, float)",
        asFUNCTION(Physics2D_RaycastHit), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Physics2D_RaycastHitMask(const Vector2 &in, const Vector2 &in, float, uint)",
        asFUNCTION(Physics2D_RaycastHitMask), asCALL_CDECL));

    // ---- Overlap queries ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics2D_OverlapCircle(const Vector2 &in, float)",
        asFUNCTION(Physics2D_OverlapCircle), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics2D_OverlapCircleMask(const Vector2 &in, float, uint)",
        asFUNCTION(Physics2D_OverlapCircleMask), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics2D_OverlapBox(const Vector2 &in, const Vector2 &in)",
        asFUNCTION(Physics2D_OverlapBox), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Physics2D_OverlapBoxMask(const Vector2 &in, const Vector2 &in, uint)",
        asFUNCTION(Physics2D_OverlapBoxMask), asCALL_CDECL));

    // ---- Body manipulation ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics2D_AddForce(uint64, const Vector2 &in)",
        asFUNCTION(Physics2D_AddForce), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics2D_AddImpulse(uint64, const Vector2 &in)",
        asFUNCTION(Physics2D_AddImpulse), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics2D_SetVelocity(uint64, const Vector2 &in)",
        asFUNCTION(Physics2D_SetVelocity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector2 Physics2D_GetVelocity(uint64)",
        asFUNCTION(Physics2D_GetVelocity), asCALL_CDECL));

    // ---- Gravity ----
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics2D_SetGravity(const Vector2 &in)",
        asFUNCTION(Physics2D_SetGravity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "Vector2 Physics2D_GetGravity()",
        asFUNCTION(Physics2D_GetGravity), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Physics2D_SetGravityScale(uint64, float)",
        asFUNCTION(Physics2D_SetGravityScale), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
