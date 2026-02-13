#ifdef ENJIN_PHYSICS_SIMPLE

#include "Enjin/Physics/SimplePhysicsBackend2D.h"

namespace Enjin {
namespace Physics {

SimplePhysicsBackend2D::SimplePhysicsBackend2D() = default;

void SimplePhysicsBackend2D::Initialize(ECS::World* world) {
    m_Impl.Initialize(world);
}

void SimplePhysicsBackend2D::Update(f32 deltaTime) {
    m_Impl.Update(deltaTime);
}

void SimplePhysicsBackend2D::Shutdown() {
    m_Impl.Shutdown();
}

void SimplePhysicsBackend2D::SetGravity(const Math::Vector2& gravity) {
    m_Impl.SetGravity(gravity);
}

Math::Vector2 SimplePhysicsBackend2D::GetGravity() const {
    return m_Impl.GetGravity();
}

void SimplePhysicsBackend2D::SetVelocityIterations(u32 iterations) {
    m_Impl.SetVelocityIterations(iterations);
}

void SimplePhysicsBackend2D::SetPositionIterations(u32 iterations) {
    m_Impl.SetPositionIterations(iterations);
}

bool SimplePhysicsBackend2D::Raycast(const Math::Vector2& origin, const Math::Vector2& direction,
                                      f32 maxDistance, RayHit2D& outHit, u32 layerMask) const {
    return m_Impl.Raycast(origin, direction, maxDistance, outHit, layerMask);
}

std::vector<RayHit2D> SimplePhysicsBackend2D::RaycastAll(const Math::Vector2& origin, const Math::Vector2& direction,
                                                           f32 maxDistance, u32 layerMask) const {
    return m_Impl.RaycastAll(origin, direction, maxDistance, layerMask);
}

bool SimplePhysicsBackend2D::OverlapCircle(const Math::Vector2& center, f32 radius,
                                            std::vector<ECS::Entity>& outEntities, u32 layerMask) const {
    return m_Impl.OverlapCircle(center, radius, outEntities, layerMask);
}

bool SimplePhysicsBackend2D::OverlapBox(const Math::Vector2& center, const Math::Vector2& halfExtents,
                                         std::vector<ECS::Entity>& outEntities, u32 layerMask) const {
    return m_Impl.OverlapBox(center, halfExtents, outEntities, layerMask);
}

void SimplePhysicsBackend2D::SetOnCollisionEnter(CollisionCallback cb) {
    m_Impl.SetOnCollisionEnter(std::move(cb));
}

void SimplePhysicsBackend2D::SetOnCollisionExit(CollisionCallback cb) {
    m_Impl.SetOnCollisionExit(std::move(cb));
}

void SimplePhysicsBackend2D::SetOnSensorEnter(CollisionCallback cb) {
    m_Impl.SetOnSensorEnter(std::move(cb));
}

void SimplePhysicsBackend2D::SetOnSensorExit(CollisionCallback cb) {
    m_Impl.SetOnSensorExit(std::move(cb));
}

void SimplePhysicsBackend2D::SetCCDEnabled(bool enabled) {
    m_Impl.SetCCDEnabled(enabled);
}

} // namespace Physics
} // namespace Enjin

#endif // ENJIN_PHYSICS_SIMPLE
