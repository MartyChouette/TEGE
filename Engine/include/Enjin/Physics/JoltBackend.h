#pragma once

#ifdef ENJIN_PHYSICS_JOLT

#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/PhysicsTypes.h"  // AABB, Ray, RaycastHit, CollisionResult, CollisionEvent, ColliderInfo
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward-declare Jolt types to keep Jolt headers out of this header
namespace JPH {
    class PhysicsSystem;
    class TempAllocatorImpl;
    class JobSystemThreadPool;
    class BodyID;
    class Constraint;
}

namespace Enjin {
namespace Physics {

class JoltContactListener;

// Collision filter data stored per body for 32-bit bilateral filtering
struct CollisionFilterData {
    u32 categoryBits = 1;
    u32 collisionMask = 0xFFFFFFFF;
};

// Jolt Physics 3D backend implementing IPhysicsBackend.
// Wraps Jolt v5.2.0 and provides full ECS↔Jolt synchronization.
class ENJIN_API JoltBackend : public IPhysicsBackend {
public:
    JoltBackend();
    ~JoltBackend() override;

    // IPhysicsBackend interface
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

    ConstraintSolver* GetConstraintSolver() override { return nullptr; } // Jolt handles constraints internally

    const char* GetName() const override { return "Jolt Physics"; }

private:
    // Initialization
    void InitializeJolt();
    void ShutdownJolt();

    // ECS↔Jolt synchronization
    void SyncECSToJolt();
    void SyncJoltToECS();
    void SyncJointsToJolt();
    void ApplyGravityZones();
    void ProcessContactEvents();

    // Body management
    void CreateBodyForEntity(ECS::Entity entity);
    void DestroyBodyForEntity(ECS::Entity entity);
    ColliderInfo GetColliderInfo(ECS::Entity entity);

    // Joint management
    void CreateJointForEntity(ECS::Entity entity, u8 jointType);
    void DestroyJointForEntity(ECS::Entity entity);

    // Collision pair key (same encoding as SimplePhysics)
    static u64 MakeCollisionPairKey(ECS::Entity a, ECS::Entity b) {
        return (static_cast<u64>(std::min(a, b)) << 32) | static_cast<u64>(std::max(a, b));
    }

    // State
    ECS::World* m_World = nullptr;
    bool m_Initialized = false;
    Math::Vector3 m_Gravity = Math::Vector3(0.0f, -9.81f, 0.0f);

    // Jolt systems (opaque pointers, defined in .cpp)
    std::unique_ptr<JPH::PhysicsSystem> m_PhysicsSystem;
    std::unique_ptr<JPH::TempAllocatorImpl> m_TempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> m_JobSystem;
    std::unique_ptr<JoltContactListener> m_ContactListener;

    // Entity↔Body mappings
    std::unordered_map<ECS::Entity, uint32_t> m_EntityToBodyIndex;  // Entity → BodyID index
    std::unordered_map<uint32_t, ECS::Entity> m_BodyIndexToEntity;  // BodyID index → Entity

    // Per-body collision filter data (keyed by BodyID index)
    std::unordered_map<uint32_t, CollisionFilterData> m_BodyFilterData;

    // Joint tracking (entity owning the joint component → Jolt constraint pointer)
    std::unordered_map<ECS::Entity, JPH::Constraint*> m_EntityToConstraint;

    // Collision event tracking
    std::unordered_set<u64> m_PreviousCollisionPairs;
    std::unordered_set<u64> m_CurrentCollisionPairs;
    std::vector<CollisionEvent> m_PendingCollisionEvents;

    // Track which entities had bodies last frame for reconciliation
    std::unordered_set<ECS::Entity> m_PreviousBodyEntities;

    // Reusable per-frame temporaries (avoid per-frame heap allocation)
    std::unordered_set<ECS::Entity> m_CurrentEntitiesCache;
    std::vector<ECS::Entity> m_ToRemoveCache;
    std::unordered_set<ECS::Entity> m_CurrentJointEntitiesCache;
    std::vector<ECS::Entity> m_JointToRemoveCache;
};

} // namespace Physics
} // namespace Enjin

#endif // ENJIN_PHYSICS_JOLT
