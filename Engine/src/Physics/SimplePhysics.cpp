#include "Enjin/Physics/SimplePhysics.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/Components/Mesh.h"
#include <algorithm>

namespace Enjin {
namespace Physics {

void SimplePhysics::Update(f32 deltaTime) {
    if (!m_World) return;

    // Update rigidbodies
    for (ECS::Entity entity : m_World->GetAllEntities()) {
        if (!m_World->HasComponent<ECS::RigidbodyComponent>(entity) ||
            !m_World->HasComponent<ECS::TransformComponent>(entity)) {
            continue;
        }

        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);

        if (rb->bodyType == ECS::RigidbodyComponent::BodyType::Static || rb->isSleeping) {
            continue;
        }

        // Apply gravity
        if (rb->useGravity && !rb->isGrounded) {
            rb->velocity = rb->velocity + m_Gravity * rb->gravityScale * deltaTime;
        }

        // Apply drag
        if (rb->drag > 0.0f) {
            rb->velocity = rb->velocity * (1.0f - rb->drag * deltaTime);
        }

        // Apply velocity constraints
        if (rb->freezePositionX) rb->velocity.x = 0.0f;
        if (rb->freezePositionY) rb->velocity.y = 0.0f;
        if (rb->freezePositionZ) rb->velocity.z = 0.0f;

        // Update position
        transform->position = transform->position + rb->velocity * deltaTime;

        // Simple ground check (assume ground at Y=0)
        if (transform->position.y <= 0.0f && rb->velocity.y <= 0.0f) {
            transform->position.y = 0.0f;
            rb->velocity.y = 0.0f;
            rb->isGrounded = true;
        } else {
            rb->isGrounded = false;
        }
    }
}

bool SimplePhysics::CheckAABBCollision(const AABB& a, const AABB& b, CollisionResult& result) {
    result.hit = false;

    // Check if boxes overlap
    if (!a.Intersects(b)) {
        return false;
    }

    result.hit = true;

    // Calculate overlap on each axis
    f32 overlapX = Math::Min(a.max.x, b.max.x) - Math::Max(a.min.x, b.min.x);
    f32 overlapY = Math::Min(a.max.y, b.max.y) - Math::Max(a.min.y, b.min.y);
    f32 overlapZ = Math::Min(a.max.z, b.max.z) - Math::Max(a.min.z, b.min.z);

    // Find minimum penetration axis
    if (overlapX <= overlapY && overlapX <= overlapZ) {
        result.penetration = overlapX;
        result.normal = (a.GetCenter().x < b.GetCenter().x) ?
                        Math::Vector3(-1, 0, 0) : Math::Vector3(1, 0, 0);
    } else if (overlapY <= overlapX && overlapY <= overlapZ) {
        result.penetration = overlapY;
        result.normal = (a.GetCenter().y < b.GetCenter().y) ?
                        Math::Vector3(0, -1, 0) : Math::Vector3(0, 1, 0);
    } else {
        result.penetration = overlapZ;
        result.normal = (a.GetCenter().z < b.GetCenter().z) ?
                        Math::Vector3(0, 0, -1) : Math::Vector3(0, 0, 1);
    }

    // Contact point is center of overlap region
    result.point = Math::Vector3(
        (Math::Max(a.min.x, b.min.x) + Math::Min(a.max.x, b.max.x)) * 0.5f,
        (Math::Max(a.min.y, b.min.y) + Math::Min(a.max.y, b.max.y)) * 0.5f,
        (Math::Max(a.min.z, b.min.z) + Math::Min(a.max.z, b.max.z)) * 0.5f
    );

    return true;
}

bool SimplePhysics::CheckSphereCollision(const Math::Vector3& centerA, f32 radiusA,
                                          const Math::Vector3& centerB, f32 radiusB,
                                          CollisionResult& result) {
    result.hit = false;

    Math::Vector3 diff = centerB - centerA;
    f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    f32 radiusSum = radiusA + radiusB;

    if (distSq >= radiusSum * radiusSum) {
        return false;
    }

    result.hit = true;
    f32 dist = Math::Sqrt(distSq);

    if (dist > 0.0001f) {
        result.normal = diff * (1.0f / dist);
    } else {
        result.normal = Math::Vector3(0, 1, 0);  // Default up if centers coincide
    }

    result.penetration = radiusSum - dist;
    result.point = centerA + result.normal * radiusA;

    return true;
}

RaycastHit SimplePhysics::Raycast(const Ray& ray, f32 maxDistance) {
    RaycastHit closestHit;
    closestHit.distance = maxDistance;

    if (!m_World) return closestHit;

    for (ECS::Entity entity : m_World->GetAllEntities()) {
        AABB aabb = GetEntityAABB(entity);
        if (aabb.GetSize().x <= 0) continue;  // No collider

        // Ray-AABB intersection (slab method)
        f32 tmin = 0.0f;
        f32 tmax = maxDistance;

        for (int i = 0; i < 3; ++i) {
            f32 origin = (i == 0) ? ray.origin.x : (i == 1) ? ray.origin.y : ray.origin.z;
            f32 dir = (i == 0) ? ray.direction.x : (i == 1) ? ray.direction.y : ray.direction.z;
            f32 bmin = (i == 0) ? aabb.min.x : (i == 1) ? aabb.min.y : aabb.min.z;
            f32 bmax = (i == 0) ? aabb.max.x : (i == 1) ? aabb.max.y : aabb.max.z;

            if (Math::Abs(dir) < 0.0001f) {
                if (origin < bmin || origin > bmax) {
                    tmin = maxDistance + 1.0f;  // No hit
                    break;
                }
            } else {
                f32 t1 = (bmin - origin) / dir;
                f32 t2 = (bmax - origin) / dir;

                if (t1 > t2) {
                    f32 temp = t1;
                    t1 = t2;
                    t2 = temp;
                }

                tmin = Math::Max(tmin, t1);
                tmax = Math::Min(tmax, t2);

                if (tmin > tmax) {
                    break;  // No hit
                }
            }
        }

        if (tmin <= tmax && tmin < closestHit.distance && tmin >= 0.0f) {
            closestHit.hit = true;
            closestHit.distance = tmin;
            closestHit.point = ray.GetPoint(tmin);
            closestHit.entity = entity;

            // Calculate normal (which face was hit)
            Math::Vector3 hitPoint = closestHit.point;
            f32 eps = 0.001f;

            if (Math::Abs(hitPoint.x - aabb.min.x) < eps) closestHit.normal = Math::Vector3(-1, 0, 0);
            else if (Math::Abs(hitPoint.x - aabb.max.x) < eps) closestHit.normal = Math::Vector3(1, 0, 0);
            else if (Math::Abs(hitPoint.y - aabb.min.y) < eps) closestHit.normal = Math::Vector3(0, -1, 0);
            else if (Math::Abs(hitPoint.y - aabb.max.y) < eps) closestHit.normal = Math::Vector3(0, 1, 0);
            else if (Math::Abs(hitPoint.z - aabb.min.z) < eps) closestHit.normal = Math::Vector3(0, 0, -1);
            else closestHit.normal = Math::Vector3(0, 0, 1);
        }
    }

    return closestHit;
}

std::vector<RaycastHit> SimplePhysics::RaycastAll(const Ray& ray, f32 maxDistance) {
    std::vector<RaycastHit> hits;

    if (!m_World) return hits;

    for (ECS::Entity entity : m_World->GetAllEntities()) {
        AABB aabb = GetEntityAABB(entity);
        if (aabb.GetSize().x <= 0) continue;

        // Ray-AABB intersection
        f32 tmin = 0.0f;
        f32 tmax = maxDistance;
        bool hit = true;

        for (int i = 0; i < 3 && hit; ++i) {
            f32 origin = (i == 0) ? ray.origin.x : (i == 1) ? ray.origin.y : ray.origin.z;
            f32 dir = (i == 0) ? ray.direction.x : (i == 1) ? ray.direction.y : ray.direction.z;
            f32 bmin = (i == 0) ? aabb.min.x : (i == 1) ? aabb.min.y : aabb.min.z;
            f32 bmax = (i == 0) ? aabb.max.x : (i == 1) ? aabb.max.y : aabb.max.z;

            if (Math::Abs(dir) < 0.0001f) {
                if (origin < bmin || origin > bmax) hit = false;
            } else {
                f32 t1 = (bmin - origin) / dir;
                f32 t2 = (bmax - origin) / dir;
                if (t1 > t2) { f32 temp = t1; t1 = t2; t2 = temp; }
                tmin = Math::Max(tmin, t1);
                tmax = Math::Min(tmax, t2);
                if (tmin > tmax) hit = false;
            }
        }

        if (hit && tmin >= 0.0f) {
            RaycastHit hitInfo;
            hitInfo.hit = true;
            hitInfo.distance = tmin;
            hitInfo.point = ray.GetPoint(tmin);
            hitInfo.entity = entity;
            hits.push_back(hitInfo);
        }
    }

    // Sort by distance
    std::sort(hits.begin(), hits.end(), [](const RaycastHit& a, const RaycastHit& b) {
        return a.distance < b.distance;
    });

    return hits;
}

bool SimplePhysics::CheckGround(const Math::Vector3& position, f32 checkDistance, RaycastHit& hit) {
    Ray ray;
    ray.origin = position;
    ray.direction = Math::Vector3(0, -1, 0);

    hit = Raycast(ray, checkDistance);
    return hit.hit;
}

Math::Vector3 SimplePhysics::MoveAndSlide(const Math::Vector3& position, const Math::Vector3& velocity,
                                           const AABB& collider, f32 deltaTime) {
    if (!m_World) return position + velocity * deltaTime;

    Math::Vector3 newPos = position + velocity * deltaTime;
    AABB movedCollider = AABB::FromCenterSize(newPos, collider.GetSize());

    // Check collisions with all other entities
    for (ECS::Entity entity : m_World->GetAllEntities()) {
        AABB otherAABB = GetEntityAABB(entity);
        if (otherAABB.GetSize().x <= 0) continue;

        CollisionResult result;
        if (CheckAABBCollision(movedCollider, otherAABB, result)) {
            // Push out of collision
            newPos = newPos + result.normal * result.penetration;
            movedCollider = AABB::FromCenterSize(newPos, collider.GetSize());
        }
    }

    return newPos;
}

std::vector<ECS::Entity> SimplePhysics::GetCollidersInRadius(const Math::Vector3& center, f32 radius) {
    std::vector<ECS::Entity> result;

    if (!m_World) return result;

    f32 radiusSq = radius * radius;

    for (ECS::Entity entity : m_World->GetAllEntities()) {
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        Math::Vector3 diff = transform->position - center;
        f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

        if (distSq <= radiusSq) {
            result.push_back(entity);
        }
    }

    return result;
}

AABB SimplePhysics::GetEntityAABB(ECS::Entity entity) {
    AABB aabb;

    auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
    if (!transform) return aabb;

    // Check for box collider
    if (m_World->HasComponent<ECS::BoxColliderComponent>(entity)) {
        auto* box = m_World->GetComponent<ECS::BoxColliderComponent>(entity);
        Math::Vector3 worldCenter = transform->position + box->center;
        Math::Vector3 worldSize = Math::Vector3(
            box->size.x * transform->scale.x,
            box->size.y * transform->scale.y,
            box->size.z * transform->scale.z
        );
        return AABB::FromCenterSize(worldCenter, worldSize);
    }

    // Check for sphere collider (approximate with AABB)
    if (m_World->HasComponent<ECS::SphereColliderComponent>(entity)) {
        auto* sphere = m_World->GetComponent<ECS::SphereColliderComponent>(entity);
        Math::Vector3 worldCenter = transform->position + sphere->center;
        f32 worldRadius = sphere->radius * Math::Max(transform->scale.x, Math::Max(transform->scale.y, transform->scale.z));
        Math::Vector3 size(worldRadius * 2.0f, worldRadius * 2.0f, worldRadius * 2.0f);
        return AABB::FromCenterSize(worldCenter, size);
    }

    // Check for capsule collider (approximate with AABB)
    if (m_World->HasComponent<ECS::CapsuleColliderComponent>(entity)) {
        auto* capsule = m_World->GetComponent<ECS::CapsuleColliderComponent>(entity);
        Math::Vector3 worldCenter = transform->position + capsule->center;
        f32 worldRadius = capsule->radius * Math::Max(transform->scale.x, transform->scale.z);
        f32 worldHeight = capsule->height * transform->scale.y;
        Math::Vector3 size(worldRadius * 2.0f, worldHeight, worldRadius * 2.0f);
        return AABB::FromCenterSize(worldCenter, size);
    }

    // Default: use mesh bounds or unit cube
    if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
        // Approximate with scaled unit cube
        return AABB::FromCenterSize(transform->position, transform->scale);
    }

    return aabb;
}

} // namespace Physics
} // namespace Enjin
