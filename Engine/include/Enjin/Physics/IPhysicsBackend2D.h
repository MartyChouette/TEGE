#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Physics/PhysicsTypes2D.h"  // Contact2D, RayHit2D, Body2DComponent, Joint2DComponent
#include <vector>
#include <functional>
#include <string>

namespace Enjin {
namespace ECS { class World; }
namespace Physics {

// Abstract 2D physics backend interface.
// Matches the PhysicsWorld2D public API so backends can be swapped transparently.
class ENJIN_API IPhysicsBackend2D {
public:
    virtual ~IPhysicsBackend2D() = default;

    virtual void Initialize(ECS::World* world) = 0;
    virtual void Update(f32 deltaTime) = 0;
    virtual void Shutdown() = 0;

    // Re-sync ECS positions to physics and fire any new collision/sensor events.
    // Call after controller systems move entities to catch overlaps in the same frame.
    virtual void SyncAndProcessEvents() {}

    // Gravity
    virtual void SetGravity(const Math::Vector2& gravity) = 0;
    virtual Math::Vector2 GetGravity() const = 0;

    // Iteration settings
    virtual void SetVelocityIterations(u32 iterations) = 0;
    virtual void SetPositionIterations(u32 iterations) = 0;

    // Queries
    virtual bool Raycast(const Math::Vector2& origin, const Math::Vector2& direction,
                         f32 maxDistance, RayHit2D& outHit, u32 layerMask = 0xFFFFFFFF) const = 0;
    virtual std::vector<RayHit2D> RaycastAll(const Math::Vector2& origin, const Math::Vector2& direction,
                                              f32 maxDistance, u32 layerMask = 0xFFFFFFFF) const = 0;
    virtual bool OverlapCircle(const Math::Vector2& center, f32 radius,
                               std::vector<ECS::Entity>& outEntities, u32 layerMask = 0xFFFFFFFF) const = 0;
    virtual bool OverlapBox(const Math::Vector2& center, const Math::Vector2& halfExtents,
                            std::vector<ECS::Entity>& outEntities, u32 layerMask = 0xFFFFFFFF) const = 0;

    // Callbacks
    using CollisionCallback = std::function<void(const Contact2D&)>;
    virtual void SetOnCollisionEnter(CollisionCallback cb) = 0;
    virtual void SetOnCollisionExit(CollisionCallback cb) = 0;
    virtual void SetOnSensorEnter(CollisionCallback cb) = 0;
    virtual void SetOnSensorExit(CollisionCallback cb) = 0;

    // CCD
    virtual void SetCCDEnabled(bool enabled) = 0;

    // Force-set body state (for rewind system — restores position/velocity)
    virtual void ForceSetBodyState(ECS::Entity entity, const Math::Vector3& position,
                                    const Math::Vector3& velocity) {}

    // Backend identification
    virtual const char* GetName() const = 0;
};

} // namespace Physics
} // namespace Enjin
