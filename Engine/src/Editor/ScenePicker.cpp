#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/Math/Math.h"
#include <cfloat>

namespace Enjin {
namespace Editor {

Ray ScenePicker::ScreenToRay(const Renderer::Camera* camera, f32 screenX, f32 screenY,
                              f32 viewportWidth, f32 viewportHeight) {
    Ray ray;

    if (!camera) {
        ray.origin = Math::Vector3(0.0f);
        ray.direction = Math::Vector3(0.0f, 0.0f, -1.0f);
        return ray;
    }

    // Convert screen coords to NDC (-1 to 1)
    f32 ndcX = (2.0f * screenX) / viewportWidth - 1.0f;
    f32 ndcY = 1.0f - (2.0f * screenY) / viewportHeight; // Flip Y

    // Get inverse view-projection matrix
    Math::Matrix4 viewProj = camera->GetViewProjectionMatrix();
    Math::Matrix4 invViewProj = viewProj.Inverse();

    // Unproject near and far points
    Math::Vector4 nearPoint4 = invViewProj * Math::Vector4(ndcX, ndcY, 0.0f, 1.0f);
    Math::Vector4 farPoint4 = invViewProj * Math::Vector4(ndcX, ndcY, 1.0f, 1.0f);

    // Perspective divide
    Math::Vector3 nearPoint = Math::Vector3(nearPoint4.x, nearPoint4.y, nearPoint4.z) * (1.0f / nearPoint4.w);
    Math::Vector3 farPoint = Math::Vector3(farPoint4.x, farPoint4.y, farPoint4.z) * (1.0f / farPoint4.w);

    ray.origin = nearPoint;
    ray.direction = (farPoint - nearPoint).Normalized();

    return ray;
}

f32 ScenePicker::RayAABBIntersect(const Ray& ray, const AABB& aabb) {
    f32 tmin = -FLT_MAX;
    f32 tmax = FLT_MAX;

    // X slab
    if (std::abs(ray.direction.x) > 1e-6f) {
        f32 t1 = (aabb.min.x - ray.origin.x) / ray.direction.x;
        f32 t2 = (aabb.max.x - ray.origin.x) / ray.direction.x;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
    } else if (ray.origin.x < aabb.min.x || ray.origin.x > aabb.max.x) {
        return -1.0f;
    }

    // Y slab
    if (std::abs(ray.direction.y) > 1e-6f) {
        f32 t1 = (aabb.min.y - ray.origin.y) / ray.direction.y;
        f32 t2 = (aabb.max.y - ray.origin.y) / ray.direction.y;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
    } else if (ray.origin.y < aabb.min.y || ray.origin.y > aabb.max.y) {
        return -1.0f;
    }

    // Z slab
    if (std::abs(ray.direction.z) > 1e-6f) {
        f32 t1 = (aabb.min.z - ray.origin.z) / ray.direction.z;
        f32 t2 = (aabb.max.z - ray.origin.z) / ray.direction.z;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
    } else if (ray.origin.z < aabb.min.z || ray.origin.z > aabb.max.z) {
        return -1.0f;
    }

    if (tmax < 0.0f || tmin > tmax) {
        return -1.0f;
    }

    return tmin >= 0.0f ? tmin : tmax;
}

AABB ScenePicker::CalculateEntityAABB(ECS::World* world, ECS::Entity entity) {
    AABB aabb;

    if (!world) return aabb;

    auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
    auto* transform = world->GetComponent<ECS::TransformComponent>(entity);

    if (!mesh || mesh->vertices.empty()) {
        // No mesh - create a small default AABB at entity position
        Math::Vector3 pos = transform ? transform->position : Math::Vector3(0.0f);
        aabb.min = pos - Math::Vector3(0.5f);
        aabb.max = pos + Math::Vector3(0.5f);
        return aabb;
    }

    // Calculate world transform matrix
    Math::Matrix4 worldMatrix = Math::Matrix4::Identity();
    if (transform) {
        worldMatrix = Math::Matrix4::Translation(transform->position) *
                      transform->rotation.ToMatrix() *
                      Math::Matrix4::Scale(transform->scale);
    }

    // Transform all vertices to world space and expand AABB
    for (const auto& vertex : mesh->vertices) {
        Math::Vector4 worldPos = worldMatrix * Math::Vector4(vertex.position.x, vertex.position.y,
                                                              vertex.position.z, 1.0f);
        aabb.Expand(Math::Vector3(worldPos.x, worldPos.y, worldPos.z));
    }

    return aabb;
}

ECS::Entity ScenePicker::PickEntity(ECS::World* world, const Renderer::Camera* camera,
                                     f32 screenX, f32 screenY,
                                     f32 viewportWidth, f32 viewportHeight) {
    if (!world || !camera) {
        return ECS::INVALID_ENTITY;
    }

    Ray ray = ScreenToRay(camera, screenX, screenY, viewportWidth, viewportHeight);

    ECS::Entity closestEntity = ECS::INVALID_ENTITY;
    f32 closestDistance = FLT_MAX;

    const auto& entities = world->GetAllEntities();
    for (ECS::Entity entity : entities) {
        // Only pick entities with meshes
        if (!world->HasComponent<ECS::MeshComponent>(entity)) {
            continue;
        }

        AABB aabb = CalculateEntityAABB(world, entity);
        f32 distance = RayAABBIntersect(ray, aabb);

        if (distance >= 0.0f && distance < closestDistance) {
            closestDistance = distance;
            closestEntity = entity;
        }
    }

    return closestEntity;
}

} // namespace Editor
} // namespace Enjin
