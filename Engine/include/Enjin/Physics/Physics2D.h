#pragma once

#ifdef ENJIN_PHYSICS_SIMPLE

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace Enjin {
namespace Physics {

// Collision manifold (internal)
struct Manifold2D {
    ECS::Entity entityA;
    ECS::Entity entityB;
    Math::Vector2 normal;
    f32 penetration = 0.0f;
    Math::Vector2 contactPoints[2];
    u32 contactCount = 0;
};

// Main 2D physics world
class ENJIN_API PhysicsWorld2D {
public:
    PhysicsWorld2D();
    ~PhysicsWorld2D();

    void Initialize(ECS::World* world);
    void Update(f32 deltaTime);
    void Shutdown();

    // Gravity
    void SetGravity(const Math::Vector2& gravity);
    Math::Vector2 GetGravity() const { return m_Gravity; }

    // Iteration settings
    void SetVelocityIterations(u32 iterations) { m_VelocityIterations = iterations; }
    void SetPositionIterations(u32 iterations) { m_PositionIterations = iterations; }

    // Queries
    bool Raycast(const Math::Vector2& origin, const Math::Vector2& direction,
                 f32 maxDistance, RayHit2D& outHit, u32 layerMask = 0xFFFFFFFF) const;
    std::vector<RayHit2D> RaycastAll(const Math::Vector2& origin, const Math::Vector2& direction,
                                      f32 maxDistance, u32 layerMask = 0xFFFFFFFF) const;
    bool OverlapCircle(const Math::Vector2& center, f32 radius,
                       std::vector<ECS::Entity>& outEntities, u32 layerMask = 0xFFFFFFFF) const;
    bool OverlapBox(const Math::Vector2& center, const Math::Vector2& halfExtents,
                    std::vector<ECS::Entity>& outEntities, u32 layerMask = 0xFFFFFFFF) const;

    // Callbacks
    using CollisionCallback = std::function<void(const Contact2D&)>;
    void SetOnCollisionEnter(CollisionCallback cb) { m_OnCollisionEnter = cb; }
    void SetOnCollisionExit(CollisionCallback cb) { m_OnCollisionExit = cb; }
    void SetOnSensorEnter(CollisionCallback cb) { m_OnSensorEnter = cb; }
    void SetOnSensorExit(CollisionCallback cb) { m_OnSensorExit = cb; }

    // CCD
    void SetCCDEnabled(bool enabled) { m_CCDEnabled = enabled; }

private:
    // Physics pipeline
    void BroadPhase(std::vector<std::pair<ECS::Entity, ECS::Entity>>& pairs);
    void NarrowPhase(const std::vector<std::pair<ECS::Entity, ECS::Entity>>& pairs,
                     std::vector<Manifold2D>& manifolds);
    void ResolveCollisions(std::vector<Manifold2D>& manifolds, f32 dt);
    void IntegrateVelocities(f32 dt);
    void IntegratePositions(f32 dt);
    void SolveJoints(f32 dt);
    void PerformCCD(f32 dt);

    // Shape vs shape collision detection
    bool TestCircleCircle(const Math::Vector2& posA, const CircleShape2D& a,
                          const Math::Vector2& posB, const CircleShape2D& b,
                          Manifold2D& manifold) const;
    bool TestCircleBox(const Math::Vector2& posA, const CircleShape2D& circle,
                       const Math::Vector2& posB, const BoxShape2D& box, f32 boxAngle,
                       Manifold2D& manifold) const;
    bool TestBoxBox(const Math::Vector2& posA, const BoxShape2D& a, f32 angleA,
                    const Math::Vector2& posB, const BoxShape2D& b, f32 angleB,
                    Manifold2D& manifold) const;
    bool TestCirclePolygon(const Math::Vector2& posA, const CircleShape2D& circle,
                           const Math::Vector2& posB, const PolygonShape2D& poly, f32 polyAngle,
                           Manifold2D& manifold) const;

    ECS::World* m_World = nullptr;
    Math::Vector2 m_Gravity = Math::Vector2(0.0f, -9.81f);
    u32 m_VelocityIterations = 8;
    u32 m_PositionIterations = 3;
    bool m_CCDEnabled = false;

    // Reusable per-frame buffers (avoid heap allocation every frame)
    std::vector<std::pair<ECS::Entity, ECS::Entity>> m_CachedPairs;
    std::vector<Manifold2D> m_CachedManifolds;

    // Contact tracking for enter/exit callbacks
    std::unordered_set<u64> m_ActiveContacts;  // Pack entity pair into u64
    std::unordered_set<u64> m_NewContactsCache;  // Reused per-frame to avoid alloc

    CollisionCallback m_OnCollisionEnter;
    CollisionCallback m_OnCollisionExit;
    CollisionCallback m_OnSensorEnter;
    CollisionCallback m_OnSensorExit;
};

} // namespace Physics
} // namespace Enjin

#endif // ENJIN_PHYSICS_SIMPLE
