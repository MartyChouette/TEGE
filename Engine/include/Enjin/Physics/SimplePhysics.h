#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <vector>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <cmath>

namespace Enjin {
namespace Physics {

// Simple AABB (Axis-Aligned Bounding Box)
struct AABB {
    Math::Vector3 min;
    Math::Vector3 max;

    AABB() : min(0, 0, 0), max(0, 0, 0) {}
    AABB(const Math::Vector3& min_, const Math::Vector3& max_) : min(min_), max(max_) {}

    // Create from center and half-extents
    static AABB FromCenterSize(const Math::Vector3& center, const Math::Vector3& size) {
        Math::Vector3 half = size * 0.5f;
        return AABB(center - half, center + half);
    }

    Math::Vector3 GetCenter() const { return (min + max) * 0.5f; }
    Math::Vector3 GetSize() const { return max - min; }
    Math::Vector3 GetHalfSize() const { return (max - min) * 0.5f; }

    bool Contains(const Math::Vector3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }

    bool Intersects(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }
};

// Collision result
struct CollisionResult {
    bool hit = false;
    Math::Vector3 normal;       // Surface normal at collision
    Math::Vector3 point;        // Contact point
    f32 penetration = 0.0f;     // How deep the collision is
    ECS::Entity otherEntity = 0;
};

// Ray for raycasting
struct Ray {
    Math::Vector3 origin;
    Math::Vector3 direction;  // Should be normalized

    Math::Vector3 GetPoint(f32 t) const { return origin + direction * t; }
};

// Raycast result
struct RaycastHit {
    bool hit = false;
    f32 distance = 0.0f;
    Math::Vector3 point;
    Math::Vector3 normal;
    ECS::Entity entity = 0;
};

// Collision event for enter/exit detection
struct CollisionEvent {
    ECS::Entity entityA = 0;
    ECS::Entity entityB = 0;
    Math::Vector3 contactPoint;
    Math::Vector3 normal;
    enum class Type : u8 { Enter, Exit } type = Type::Enter;
    bool isTrigger = false;
};

// Collider info extracted from whichever collider type exists on an entity
struct ColliderInfo {
    u32 categoryBits = 1;
    u32 collisionMask = 0xFFFFFFFF;
    bool isTrigger = false;
    bool hasCollider = false;
};

// Spatial hash grid for broad-phase collision culling
struct SpatialHashGrid {
    f32 cellSize = 4.0f;
    std::unordered_map<u64, std::vector<ECS::Entity>> cells;
    std::vector<ECS::Entity> oversizedEntities; // Large entities that span too many cells

    void Clear() { cells.clear(); oversizedEntities.clear(); }

    void Insert(ECS::Entity e, const AABB& bounds) {
        i32 minX = static_cast<i32>(std::floor(bounds.min.x / cellSize));
        i32 minY = static_cast<i32>(std::floor(bounds.min.y / cellSize));
        i32 minZ = static_cast<i32>(std::floor(bounds.min.z / cellSize));
        i32 maxX = static_cast<i32>(std::floor(bounds.max.x / cellSize));
        i32 maxY = static_cast<i32>(std::floor(bounds.max.y / cellSize));
        i32 maxZ = static_cast<i32>(std::floor(bounds.max.z / cellSize));

        // Cap: if entity spans more than 8 cells in any dimension, treat as oversized
        // (e.g., a 50x50 ground plane would need 13x13=169+ cells — way too expensive)
        i32 spanX = maxX - minX + 1;
        i32 spanY = maxY - minY + 1;
        i32 spanZ = maxZ - minZ + 1;
        if (spanX > 8 || spanY > 8 || spanZ > 8) {
            oversizedEntities.push_back(e);
            return;
        }

        for (i32 x = minX; x <= maxX; ++x) {
            for (i32 y = minY; y <= maxY; ++y) {
                for (i32 z = minZ; z <= maxZ; ++z) {
                    cells[HashCell(x, y, z)].push_back(e);
                }
            }
        }
    }

    void GetPotentialPairs(std::vector<std::pair<ECS::Entity, ECS::Entity>>& pairs) {
        std::unordered_set<u64> seen;
        // Standard cell-based pairs
        for (auto& [cellKey, entities] : cells) {
            for (usize i = 0; i < entities.size(); ++i) {
                for (usize j = i + 1; j < entities.size(); ++j) {
                    ECS::Entity a = entities[i];
                    ECS::Entity b = entities[j];
                    u64 pairKey = (static_cast<u64>(std::min(a, b)) << 32) |
                                  static_cast<u64>(std::max(a, b));
                    if (seen.insert(pairKey).second) {
                        pairs.push_back({std::min(a, b), std::max(a, b)});
                    }
                }
            }
        }
        // Oversized entities must be tested against ALL other entities (brute force)
        // Collect all unique entities from the grid
        std::unordered_set<ECS::Entity> gridEntities;
        for (auto& [cellKey, entities] : cells) {
            for (ECS::Entity e : entities) {
                gridEntities.insert(e);
            }
        }
        for (ECS::Entity big : oversizedEntities) {
            // Pair with every grid entity
            for (ECS::Entity other : gridEntities) {
                u64 pairKey = (static_cast<u64>(std::min(big, other)) << 32) |
                              static_cast<u64>(std::max(big, other));
                if (seen.insert(pairKey).second) {
                    pairs.push_back({std::min(big, other), std::max(big, other)});
                }
            }
            // Pair oversized entities with each other
            for (ECS::Entity other : oversizedEntities) {
                if (other == big) continue;
                u64 pairKey = (static_cast<u64>(std::min(big, other)) << 32) |
                              static_cast<u64>(std::max(big, other));
                if (seen.insert(pairKey).second) {
                    pairs.push_back({std::min(big, other), std::max(big, other)});
                }
            }
        }
    }

private:
    static u64 HashCell(i32 x, i32 y, i32 z) {
        // FNV-1a style hash combining three ints
        u64 h = 14695981039346656037ULL;
        h ^= static_cast<u64>(static_cast<u32>(x)); h *= 1099511628211ULL;
        h ^= static_cast<u64>(static_cast<u32>(y)); h *= 1099511628211ULL;
        h ^= static_cast<u64>(static_cast<u32>(z)); h *= 1099511628211ULL;
        return h;
    }
};

class ConstraintSolver;

// Simple physics world - handles basic collision detection
class ENJIN_API SimplePhysics {
public:
    SimplePhysics();
    ~SimplePhysics();

    void SetWorld(ECS::World* world);

    // Get the constraint solver for joint configuration
    ConstraintSolver* GetConstraintSolver() { return m_ConstraintSolver.get(); }

    // Update physics simulation
    void Update(f32 deltaTime);

    // Collision queries
    bool CheckAABBCollision(const AABB& a, const AABB& b, CollisionResult& result);
    bool CheckSphereCollision(const Math::Vector3& centerA, f32 radiusA,
                               const Math::Vector3& centerB, f32 radiusB,
                               CollisionResult& result);

    // Raycasting
    RaycastHit Raycast(const Ray& ray, f32 maxDistance = 1000.0f, u32 layerMask = 0xFFFFFFFF);
    std::vector<RaycastHit> RaycastAll(const Ray& ray, f32 maxDistance = 1000.0f, u32 layerMask = 0xFFFFFFFF);

    // Ground check (raycast downward)
    bool CheckGround(const Math::Vector3& position, f32 checkDistance, RaycastHit& hit, u32 layerMask = 0xFFFFFFFF);

    // Move and slide (for character controllers)
    Math::Vector3 MoveAndSlide(const Math::Vector3& position, const Math::Vector3& velocity,
                                const AABB& collider, f32 deltaTime, u32 layerMask = 0xFFFFFFFF);

    // Get all entities with colliders
    std::vector<ECS::Entity> GetCollidersInRadius(const Math::Vector3& center, f32 radius, u32 layerMask = 0xFFFFFFFF);

    // Get all entities whose AABB overlaps with a box
    std::vector<ECS::Entity> OverlapBox(const Math::Vector3& center, const Math::Vector3& halfExtents, u32 layerMask = 0xFFFFFFFF);

    // Gravity
    void SetGravity(const Math::Vector3& gravity) { m_Gravity = gravity; }
    Math::Vector3 GetGravity() const { return m_Gravity; }

    // Collision events (for visual scripting / gameplay callbacks)
    const std::vector<CollisionEvent>& GetPendingCollisionEvents() const { return m_PendingCollisionEvents; }
    void ClearPendingCollisionEvents() { m_PendingCollisionEvents.clear(); }

private:
    // Rebuild the per-frame collider entity cache (call once at start of Update)
    void RebuildColliderCache();

    // Detect collision enter/exit events
    void DetectCollisionEvents();
    // Get world-space AABB for an entity
    AABB GetEntityAABB(ECS::Entity entity);

    // Get collider info (categoryBits, collisionMask, isTrigger) from whichever collider exists
    ColliderInfo GetColliderInfo(ECS::Entity entity);

    // Get categoryBits from whichever collider exists on an entity
    u32 GetEntityCategoryBits(ECS::Entity entity);

    ECS::World* m_World = nullptr;
    Math::Vector3 m_Gravity = Math::Vector3(0.0f, -9.81f, 0.0f);
    std::unique_ptr<ConstraintSolver> m_ConstraintSolver;

    // Per-frame cached list of all entities with any collider type
    std::vector<ECS::Entity> m_CachedColliderEntities;

    // Per-frame cached AABBs and ColliderInfos — parallel to m_CachedColliderEntities
    std::vector<AABB> m_CachedAABBs;
    std::vector<ColliderInfo> m_CachedColliderInfos;

    // Spatial hash grid for broad-phase collision detection
    SpatialHashGrid m_SpatialHash;

    // Reusable per-frame buffer for candidate pairs (avoid per-frame allocation)
    std::vector<std::pair<ECS::Entity, ECS::Entity>> m_CandidatePairs;

    // Collision pair tracking for enter/exit detection
    // Each pair is encoded as (min(a,b) << 32) | max(a,b) for consistent ordering
    std::unordered_set<u64> m_PreviousCollisionPairs;
    std::unordered_set<u64> m_CurrentCollisionPairs;
    std::vector<CollisionEvent> m_PendingCollisionEvents;

    static u64 MakeCollisionPairKey(ECS::Entity a, ECS::Entity b) {
        return (static_cast<u64>(std::min(a, b)) << 32) | static_cast<u64>(std::max(a, b));
    }
};

} // namespace Physics
} // namespace Enjin
