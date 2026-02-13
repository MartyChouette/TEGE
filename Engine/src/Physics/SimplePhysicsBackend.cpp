#ifdef ENJIN_PHYSICS_SIMPLE

#include "Enjin/Physics/SimplePhysicsBackend.h"

namespace Enjin {
namespace Physics {

SimplePhysicsBackend::SimplePhysicsBackend() = default;

void SimplePhysicsBackend::SetWorld(ECS::World* world) {
    m_Impl.SetWorld(world);
}

void SimplePhysicsBackend::Update(f32 deltaTime) {
    m_Impl.Update(deltaTime);
}

void SimplePhysicsBackend::SetGravity(const Math::Vector3& gravity) {
    m_Impl.SetGravity(gravity);
}

Math::Vector3 SimplePhysicsBackend::GetGravity() const {
    return m_Impl.GetGravity();
}

bool SimplePhysicsBackend::CheckAABBCollision(const AABB& a, const AABB& b, CollisionResult& result) {
    return m_Impl.CheckAABBCollision(a, b, result);
}

bool SimplePhysicsBackend::CheckSphereCollision(const Math::Vector3& centerA, f32 radiusA,
                                                 const Math::Vector3& centerB, f32 radiusB,
                                                 CollisionResult& result) {
    return m_Impl.CheckSphereCollision(centerA, radiusA, centerB, radiusB, result);
}

RaycastHit SimplePhysicsBackend::Raycast(const Ray& ray, f32 maxDistance, u32 layerMask) {
    return m_Impl.Raycast(ray, maxDistance, layerMask);
}

std::vector<RaycastHit> SimplePhysicsBackend::RaycastAll(const Ray& ray, f32 maxDistance, u32 layerMask) {
    return m_Impl.RaycastAll(ray, maxDistance, layerMask);
}

bool SimplePhysicsBackend::CheckGround(const Math::Vector3& position, f32 checkDistance, RaycastHit& hit, u32 layerMask) {
    return m_Impl.CheckGround(position, checkDistance, hit, layerMask);
}

Math::Vector3 SimplePhysicsBackend::MoveAndSlide(const Math::Vector3& position, const Math::Vector3& velocity,
                                                   const AABB& collider, f32 deltaTime, u32 layerMask) {
    return m_Impl.MoveAndSlide(position, velocity, collider, deltaTime, layerMask);
}

std::vector<ECS::Entity> SimplePhysicsBackend::GetCollidersInRadius(const Math::Vector3& center, f32 radius, u32 layerMask) {
    return m_Impl.GetCollidersInRadius(center, radius, layerMask);
}

std::vector<ECS::Entity> SimplePhysicsBackend::OverlapBox(const Math::Vector3& center, const Math::Vector3& halfExtents, u32 layerMask) {
    return m_Impl.OverlapBox(center, halfExtents, layerMask);
}

const std::vector<CollisionEvent>& SimplePhysicsBackend::GetPendingCollisionEvents() const {
    return m_Impl.GetPendingCollisionEvents();
}

void SimplePhysicsBackend::ClearPendingCollisionEvents() {
    m_Impl.ClearPendingCollisionEvents();
}

ConstraintSolver* SimplePhysicsBackend::GetConstraintSolver() {
    return m_Impl.GetConstraintSolver();
}

} // namespace Physics
} // namespace Enjin

#endif // ENJIN_PHYSICS_SIMPLE
