#pragma once

#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Physics/Physics2D.h"

namespace Enjin {
namespace Physics {

// Adapter wrapping PhysicsWorld2D behind the IPhysicsBackend2D interface.
class ENJIN_API SimplePhysicsBackend2D : public IPhysicsBackend2D {
public:
    SimplePhysicsBackend2D();
    ~SimplePhysicsBackend2D() override = default;

    void Initialize(ECS::World* world) override;
    void Update(f32 deltaTime) override;
    void Shutdown() override;

    void SetGravity(const Math::Vector2& gravity) override;
    Math::Vector2 GetGravity() const override;

    void SetVelocityIterations(u32 iterations) override;
    void SetPositionIterations(u32 iterations) override;

    bool Raycast(const Math::Vector2& origin, const Math::Vector2& direction,
                 f32 maxDistance, RayHit2D& outHit, u32 layerMask = 0xFFFFFFFF) const override;
    std::vector<RayHit2D> RaycastAll(const Math::Vector2& origin, const Math::Vector2& direction,
                                      f32 maxDistance, u32 layerMask = 0xFFFFFFFF) const override;
    bool OverlapCircle(const Math::Vector2& center, f32 radius,
                       std::vector<ECS::Entity>& outEntities, u32 layerMask = 0xFFFFFFFF) const override;
    bool OverlapBox(const Math::Vector2& center, const Math::Vector2& halfExtents,
                    std::vector<ECS::Entity>& outEntities, u32 layerMask = 0xFFFFFFFF) const override;

    void SetOnCollisionEnter(CollisionCallback cb) override;
    void SetOnCollisionExit(CollisionCallback cb) override;
    void SetOnSensorEnter(CollisionCallback cb) override;
    void SetOnSensorExit(CollisionCallback cb) override;

    void SetCCDEnabled(bool enabled) override;

    const char* GetName() const override { return "SimplePhysics2D"; }

    // Direct access to the underlying PhysicsWorld2D (for migration / debugging)
    PhysicsWorld2D& GetInternal() { return m_Impl; }

private:
    PhysicsWorld2D m_Impl;
};

} // namespace Physics
} // namespace Enjin
