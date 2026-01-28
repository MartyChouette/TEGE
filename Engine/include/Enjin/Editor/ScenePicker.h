#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Math/Vector.h"
#include <cfloat>
#include <algorithm>

namespace Enjin {
namespace Editor {

// Ray structure for picking
struct Ray {
    Math::Vector3 origin;
    Math::Vector3 direction;
};

// Axis-Aligned Bounding Box
struct AABB {
    Math::Vector3 min;
    Math::Vector3 max;

    AABB() : min(Math::Vector3(FLT_MAX)), max(Math::Vector3(-FLT_MAX)) {}

    void Expand(const Math::Vector3& point) {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    }

    Math::Vector3 Center() const {
        return (min + max) * 0.5f;
    }

    Math::Vector3 Size() const {
        return max - min;
    }
};

// Scene picker for entity selection via mouse click
class ENJIN_API ScenePicker {
public:
    // Create a ray from screen coordinates
    static Ray ScreenToRay(const Renderer::Camera* camera, f32 screenX, f32 screenY,
                           f32 viewportWidth, f32 viewportHeight);

    // Test ray-AABB intersection, returns distance to hit or -1 if no hit
    static f32 RayAABBIntersect(const Ray& ray, const AABB& aabb);

    // Pick an entity at screen coordinates
    static ECS::Entity PickEntity(ECS::World* world, const Renderer::Camera* camera,
                                   f32 screenX, f32 screenY,
                                   f32 viewportWidth, f32 viewportHeight);

    // Calculate AABB for an entity's mesh (in world space)
    static AABB CalculateEntityAABB(ECS::World* world, ECS::Entity entity);
};

} // namespace Editor
} // namespace Enjin
