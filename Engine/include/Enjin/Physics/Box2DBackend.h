#pragma once

#ifdef ENJIN_PHYSICS_BOX2D

#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Physics/PhysicsTypes2D.h"
#include <unordered_map>
#include <unordered_set>

// Box2D v3 uses opaque C handles — include the header directly since
// the handles are small value types (not pointers to forward-declare).
#include <box2d/box2d.h>

namespace Enjin {
namespace Physics {

// Box2D v3 backend implementing IPhysicsBackend2D.
// Wraps Box2D v3.0.0 C API and provides full ECS↔Box2D synchronization.
class ENJIN_API Box2DBackend : public IPhysicsBackend2D {
public:
    void SyncAndProcessEvents() override;
public:
    Box2DBackend();
    ~Box2DBackend() override;

    // IPhysicsBackend2D interface
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

    const char* GetName() const override { return "Box2D"; }

    // Rewind state capture/restoration
    bool GetBodyVelocity(ECS::Entity entity, Math::Vector3& outLinear) const override;
    void ForceSetBodyState(ECS::Entity entity, const Math::Vector3& position,
                            const Math::Vector3& velocity) override;

    // Resolve entity from a Box2D shape (public for use by C raycast/overlap callbacks)
    ECS::Entity ResolveEntity(b2ShapeId shapeId) const;

private:
    // ECS↔Box2D synchronization
    void SyncECSToBox2D();
    void SyncBox2DToECS();
    void SyncJointsToBox2D();
    void ApplyGravityZones();
    void ProcessEvents();

    // Body management
    void CreateBodyForEntity(ECS::Entity entity);
    void DestroyBodyForEntity(ECS::Entity entity);

    // Joint management
    void CreateJointForEntity(ECS::Entity entity);
    void DestroyJointForEntity(ECS::Entity entity);

    // Collision pair key encoding
    // WARNING: Truncates entity IDs to 32 bits. Assumes entity IDs fit in u32.
    // If entity IDs exceed 2^32, this will produce collisions. Changing the key
    // type requires broader refactoring of all collision pair tracking.
    static u64 MakeCollisionPairKey(ECS::Entity a, ECS::Entity b) {
        return (static_cast<u64>(std::min(a, b)) << 32) | static_cast<u64>(std::max(a, b));
    }

    // State
    ECS::World* m_World = nullptr;
    bool m_Initialized = false;
    Math::Vector2 m_Gravity = Math::Vector2(0.0f, -9.81f);
    u32 m_SubStepCount = 4;
    bool m_CCDEnabled = false;
    f32 m_LastDeltaTime = 1.0f / 60.0f;

    // Box2D world handle
    b2WorldId m_WorldId = b2_nullWorldId;

    // Entity↔Body mappings (keyed by body ID integer value)
    std::unordered_map<ECS::Entity, b2BodyId> m_EntityToBody;
    std::unordered_map<uint64_t, ECS::Entity> m_BodyUserDataToEntity;  // userData → entity

    // Joint tracking
    std::unordered_map<ECS::Entity, b2JointId> m_EntityToJoint;

    // Contact tracking for enter/exit
    std::unordered_set<u64> m_ActiveContacts;
    std::unordered_set<u64> m_ActiveSensorContacts;

    // Callbacks
    CollisionCallback m_OnCollisionEnter;
    CollisionCallback m_OnCollisionExit;
    CollisionCallback m_OnSensorEnter;
    CollisionCallback m_OnSensorExit;

    // Reusable per-frame temporaries (avoid per-frame heap allocation)
    std::vector<ECS::Entity> m_ToRemoveCache;
    std::vector<ECS::Entity> m_JointToRemoveCache;
    std::unordered_set<ECS::Entity> m_CurrentEntitiesCache;
    std::unordered_set<u64> m_NewContactsCache;
    std::unordered_set<u64> m_NewSensorContactsCache;
};

} // namespace Physics
} // namespace Enjin

#endif // ENJIN_PHYSICS_BOX2D
