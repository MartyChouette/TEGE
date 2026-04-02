#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Physics/PhysicsTypes.h"  // AABB, Ray, RaycastHit, CollisionResult, CollisionEvent, ColliderInfo
#include <vector>
#include <string>

namespace Enjin {
namespace ECS { class World; }
namespace Physics {

class ConstraintSolver;

// Abstract 3D physics backend interface.
// Matches the SimplePhysics public API so backends can be swapped transparently.
class ENJIN_API IPhysicsBackend {
public:
    virtual ~IPhysicsBackend() = default;

    virtual void SetWorld(ECS::World* world) = 0;

    // Simulation
    virtual void Update(f32 deltaTime) = 0;

    // Gravity
    virtual void SetGravity(const Math::Vector3& gravity) = 0;
    virtual Math::Vector3 GetGravity() const = 0;

    // Collision queries
    virtual bool CheckAABBCollision(const AABB& a, const AABB& b, CollisionResult& result) = 0;
    virtual bool CheckSphereCollision(const Math::Vector3& centerA, f32 radiusA,
                                       const Math::Vector3& centerB, f32 radiusB,
                                       CollisionResult& result) = 0;

    // Raycasting
    virtual RaycastHit Raycast(const Ray& ray, f32 maxDistance = 1000.0f, u32 layerMask = 0xFFFFFFFF) = 0;
    virtual std::vector<RaycastHit> RaycastAll(const Ray& ray, f32 maxDistance = 1000.0f, u32 layerMask = 0xFFFFFFFF) = 0;

    // Ground check
    virtual bool CheckGround(const Math::Vector3& position, f32 checkDistance, RaycastHit& hit, u32 layerMask = 0xFFFFFFFF, ECS::Entity ignoreEntity = 0) = 0;

    // Character controller movement (legacy AABB slide)
    virtual Math::Vector3 MoveAndSlide(const Math::Vector3& position, const Math::Vector3& velocity,
                                        const AABB& collider, f32 deltaTime, u32 layerMask = 0xFFFFFFFF) = 0;

    // Physics-based character controller (capsule collision, wall sliding, stair stepping)
    enum class CharacterGroundState : u8 { OnGround, OnSteepGround, InAir };
    struct CharacterState {
        Math::Vector3 position;
        Math::Vector3 groundNormal = Math::Vector3(0, 1, 0);
        Math::Vector3 groundVelocity;  // Moving platform velocity
        CharacterGroundState groundState = CharacterGroundState::InAir;
    };
    virtual void CreateCharacterController(ECS::Entity entity, f32 capsuleRadius, f32 capsuleHalfHeight,
                                            const Math::Vector3& position) {}
    virtual void DestroyCharacterController(ECS::Entity entity) {}
    virtual void DestroyAllCharacterControllers() {}
    virtual CharacterState UpdateCharacterController(ECS::Entity entity, const Math::Vector3& velocity,
                                                      f32 deltaTime) { return {}; }
    virtual bool HasCharacterController(ECS::Entity entity) const { return false; }

    // Spatial queries
    virtual std::vector<ECS::Entity> GetCollidersInRadius(const Math::Vector3& center, f32 radius, u32 layerMask = 0xFFFFFFFF) = 0;
    virtual std::vector<ECS::Entity> OverlapBox(const Math::Vector3& center, const Math::Vector3& halfExtents, u32 layerMask = 0xFFFFFFFF) = 0;

    // Collision events
    virtual const std::vector<CollisionEvent>& GetPendingCollisionEvents() const = 0;
    virtual void ClearPendingCollisionEvents() = 0;

    // Constraint solver
    virtual ConstraintSolver* GetConstraintSolver() = 0;

    // Force-set body state (for rewind system — restores position/rotation/velocity)
    virtual void ForceSetBodyState(ECS::Entity entity, const Math::Vector3& position,
                                    const Math::Quaternion& rotation,
                                    const Math::Vector3& linearVel,
                                    const Math::Vector3& angularVel) {}

    // Backend identification
    virtual const char* GetName() const = 0;
};

} // namespace Physics
} // namespace Enjin
