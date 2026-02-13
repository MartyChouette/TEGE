#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"

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

} // namespace Physics
} // namespace Enjin
