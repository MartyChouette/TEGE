#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <vector>

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

// Simple physics world - handles basic collision detection
class ENJIN_API SimplePhysics {
public:
    SimplePhysics() = default;
    ~SimplePhysics() = default;

    void SetWorld(ECS::World* world) { m_World = world; }

    // Update physics simulation
    void Update(f32 deltaTime);

    // Collision queries
    bool CheckAABBCollision(const AABB& a, const AABB& b, CollisionResult& result);
    bool CheckSphereCollision(const Math::Vector3& centerA, f32 radiusA,
                               const Math::Vector3& centerB, f32 radiusB,
                               CollisionResult& result);

    // Raycasting
    RaycastHit Raycast(const Ray& ray, f32 maxDistance = 1000.0f);
    std::vector<RaycastHit> RaycastAll(const Ray& ray, f32 maxDistance = 1000.0f);

    // Ground check (raycast downward)
    bool CheckGround(const Math::Vector3& position, f32 checkDistance, RaycastHit& hit);

    // Move and slide (for character controllers)
    Math::Vector3 MoveAndSlide(const Math::Vector3& position, const Math::Vector3& velocity,
                                const AABB& collider, f32 deltaTime);

    // Get all entities with colliders
    std::vector<ECS::Entity> GetCollidersInRadius(const Math::Vector3& center, f32 radius);

    // Gravity
    void SetGravity(const Math::Vector3& gravity) { m_Gravity = gravity; }
    Math::Vector3 GetGravity() const { return m_Gravity; }

private:
    // Get world-space AABB for an entity
    AABB GetEntityAABB(ECS::Entity entity);

    ECS::World* m_World = nullptr;
    Math::Vector3 m_Gravity = Math::Vector3(0.0f, -9.81f, 0.0f);
};

} // namespace Physics
} // namespace Enjin
