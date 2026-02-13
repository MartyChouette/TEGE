#pragma once

#ifdef ENJIN_PHYSICS_SIMPLE

#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/SimplePhysics.h"
#include <memory>

namespace Enjin {
namespace Physics {

// Adapter wrapping SimplePhysics behind the IPhysicsBackend interface.
class ENJIN_API SimplePhysicsBackend : public IPhysicsBackend {
public:
    SimplePhysicsBackend();
    ~SimplePhysicsBackend() override = default;

    void SetWorld(ECS::World* world) override;
    void Update(f32 deltaTime) override;

    void SetGravity(const Math::Vector3& gravity) override;
    Math::Vector3 GetGravity() const override;

    bool CheckAABBCollision(const AABB& a, const AABB& b, CollisionResult& result) override;
    bool CheckSphereCollision(const Math::Vector3& centerA, f32 radiusA,
                               const Math::Vector3& centerB, f32 radiusB,
                               CollisionResult& result) override;

    RaycastHit Raycast(const Ray& ray, f32 maxDistance = 1000.0f, u32 layerMask = 0xFFFFFFFF) override;
    std::vector<RaycastHit> RaycastAll(const Ray& ray, f32 maxDistance = 1000.0f, u32 layerMask = 0xFFFFFFFF) override;

    bool CheckGround(const Math::Vector3& position, f32 checkDistance, RaycastHit& hit, u32 layerMask = 0xFFFFFFFF) override;

    Math::Vector3 MoveAndSlide(const Math::Vector3& position, const Math::Vector3& velocity,
                                const AABB& collider, f32 deltaTime, u32 layerMask = 0xFFFFFFFF) override;

    std::vector<ECS::Entity> GetCollidersInRadius(const Math::Vector3& center, f32 radius, u32 layerMask = 0xFFFFFFFF) override;
    std::vector<ECS::Entity> OverlapBox(const Math::Vector3& center, const Math::Vector3& halfExtents, u32 layerMask = 0xFFFFFFFF) override;

    const std::vector<CollisionEvent>& GetPendingCollisionEvents() const override;
    void ClearPendingCollisionEvents() override;

    ConstraintSolver* GetConstraintSolver() override;

    const char* GetName() const override { return "SimplePhysics"; }

    // Direct access to the underlying SimplePhysics (for migration / debugging)
    SimplePhysics& GetInternal() { return m_Impl; }

private:
    SimplePhysics m_Impl;
};

} // namespace Physics
} // namespace Enjin

#endif // ENJIN_PHYSICS_SIMPLE
