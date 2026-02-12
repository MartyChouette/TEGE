#include "Enjin/Physics/PhysicsWorld.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"
#include <algorithm>
#include <unordered_set>

namespace Enjin {
namespace Physics {

PhysicsWorld::PhysicsWorld() {
}

PhysicsWorld::~PhysicsWorld() {
    Shutdown();
}

bool PhysicsWorld::Initialize() {
    ENJIN_LOG_INFO(Renderer, "Initializing Physics World...");
    return true;
}

void PhysicsWorld::Shutdown() {
    m_RigidBodies.clear();
    m_EntityBodyMap.clear();
    m_ECSWorld = nullptr;
}

void PhysicsWorld::SetGravity(const Math::Vector3& gravity) {
    m_Gravity = gravity;
}

void PhysicsWorld::AddRigidBody(std::shared_ptr<RigidBody> body) {
    if (body) {
        m_RigidBodies.push_back(body);
    }
}

void PhysicsWorld::RemoveRigidBody(std::shared_ptr<RigidBody> body) {
    m_RigidBodies.erase(
        std::remove(m_RigidBodies.begin(), m_RigidBodies.end(), body),
        m_RigidBodies.end()
    );
}

void PhysicsWorld::Step(f32 deltaTime) {
    if (m_ECSWorld) {
        SyncFromECS();
    }

    Integrate(deltaTime);
    DetectCollisions();
    ResolveCollisions();

    if (m_ECSWorld) {
        SyncToECS();
    }

    m_LastDeltaTime = deltaTime;
}

void PhysicsWorld::Integrate(f32 deltaTime) {
    for (auto& body : m_RigidBodies) {
        if (body->IsStatic()) {
            continue;
        }

        // Apply gravity
        Math::Vector3 acceleration = m_Gravity;
        
        // Update velocity
        body->SetVelocity(body->GetVelocity() + acceleration * deltaTime);
        
        // Update position
        body->SetPosition(body->GetPosition() + body->GetVelocity() * deltaTime);
    }
}

void PhysicsWorld::DetectCollisions() {
    m_CollisionPairs.clear();

    for (usize i = 0; i < m_RigidBodies.size(); ++i) {
        for (usize j = i + 1; j < m_RigidBodies.size(); ++j) {
            // Skip if both are static
            if (m_RigidBodies[i]->IsStatic() && m_RigidBodies[j]->IsStatic()) {
                continue;
            }

            // Bilateral collision filtering: both entities must agree to collide
            if (!(m_RigidBodies[i]->GetCategoryBits() & m_RigidBodies[j]->GetCollisionMask()) ||
                !(m_RigidBodies[j]->GetCategoryBits() & m_RigidBodies[i]->GetCollisionMask())) {
                continue;
            }

            CollisionPair pair;
            bool collided = false;

            auto shapeA = m_RigidBodies[i]->GetShape();
            auto shapeB = m_RigidBodies[j]->GetShape();

            if (shapeA == RigidBody::Shape::Sphere && shapeB == RigidBody::Shape::Sphere) {
                collided = TestSphereSphere(i, j, pair);
            } else if (shapeA == RigidBody::Shape::Box && shapeB == RigidBody::Shape::Box) {
                collided = TestAABBAABB(i, j, pair);
            } else if (shapeA == RigidBody::Shape::Sphere && shapeB == RigidBody::Shape::Box) {
                collided = TestSphereAABB(i, j, pair);
            } else {
                // Box vs Sphere: swap and negate normal
                collided = TestSphereAABB(j, i, pair);
                if (collided) {
                    std::swap(pair.bodyA, pair.bodyB);
                    pair.normal = pair.normal * -1.0f;
                }
            }

            if (collided) {
                m_CollisionPairs.push_back(pair);
            }
        }
    }
}

void PhysicsWorld::ResolveCollisions() {
    for (const auto& pair : m_CollisionPairs) {
        auto& bodyA = m_RigidBodies[pair.bodyA];
        auto& bodyB = m_RigidBodies[pair.bodyB];

        f32 invMassA = bodyA->GetInverseMass();
        f32 invMassB = bodyB->GetInverseMass();
        f32 invMassSum = invMassA + invMassB;

        if (invMassSum <= 0.0f) continue;

        // Positional correction (push bodies apart)
        const f32 correctionPercent = 0.8f;
        const f32 slop = 0.01f;
        f32 correctionMag = Math::Max(pair.penetration - slop, 0.0f) / invMassSum * correctionPercent;
        Math::Vector3 correction = pair.normal * correctionMag;

        bodyA->SetPosition(bodyA->GetPosition() - correction * invMassA);
        bodyB->SetPosition(bodyB->GetPosition() + correction * invMassB);

        // Relative velocity
        Math::Vector3 relVel = bodyB->GetVelocity() - bodyA->GetVelocity();
        f32 velAlongNormal = relVel.x * pair.normal.x + relVel.y * pair.normal.y + relVel.z * pair.normal.z;

        // Don't resolve if velocities are separating
        if (velAlongNormal > 0.0f) continue;

        // Restitution (take minimum)
        f32 e = Math::Min(bodyA->GetRestitution(), bodyB->GetRestitution());

        // Impulse magnitude
        f32 j = -(1.0f + e) * velAlongNormal / invMassSum;

        // Apply impulse
        Math::Vector3 impulse = pair.normal * j;
        bodyA->SetVelocity(bodyA->GetVelocity() - impulse * invMassA);
        bodyB->SetVelocity(bodyB->GetVelocity() + impulse * invMassB);

        // Friction impulse
        Math::Vector3 tangent = relVel - pair.normal * velAlongNormal;
        f32 tangentLen = Math::Sqrt(tangent.x * tangent.x + tangent.y * tangent.y + tangent.z * tangent.z);
        if (tangentLen > Math::EPSILON) {
            tangent = tangent * (1.0f / tangentLen);

            f32 jt = -(relVel.x * tangent.x + relVel.y * tangent.y + relVel.z * tangent.z) / invMassSum;
            f32 mu = Math::Sqrt(bodyA->GetFriction() * bodyB->GetFriction());

            // Coulomb's law: clamp friction to cone
            Math::Vector3 frictionImpulse;
            if (Math::Abs(jt) < j * mu) {
                frictionImpulse = tangent * jt;
            } else {
                frictionImpulse = tangent * (-j * mu);
            }

            bodyA->SetVelocity(bodyA->GetVelocity() - frictionImpulse * invMassA);
            bodyB->SetVelocity(bodyB->GetVelocity() + frictionImpulse * invMassB);
        }
    }
}

bool PhysicsWorld::TestSphereSphere(usize a, usize b, CollisionPair& pair) {
    const auto& bodyA = m_RigidBodies[a];
    const auto& bodyB = m_RigidBodies[b];

    Math::Vector3 diff = bodyB->GetPosition() - bodyA->GetPosition();
    f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    f32 radiusSum = bodyA->GetBoundingRadius() + bodyB->GetBoundingRadius();

    if (distSq >= radiusSum * radiusSum) return false;

    f32 dist = Math::Sqrt(distSq);
    pair.bodyA = a;
    pair.bodyB = b;
    pair.penetration = radiusSum - dist;

    if (dist > Math::EPSILON) {
        pair.normal = diff * (1.0f / dist);
    } else {
        pair.normal = Math::Vector3(0, 1, 0);
    }

    pair.contactPoint = bodyA->GetPosition() + pair.normal * bodyA->GetBoundingRadius();
    return true;
}

bool PhysicsWorld::TestAABBAABB(usize a, usize b, CollisionPair& pair) {
    const auto& bodyA = m_RigidBodies[a];
    const auto& bodyB = m_RigidBodies[b];

    Math::Vector3 posA = bodyA->GetPosition();
    Math::Vector3 posB = bodyB->GetPosition();
    Math::Vector3 halfA = bodyA->GetHalfExtents();
    Math::Vector3 halfB = bodyB->GetHalfExtents();

    Math::Vector3 diff = posB - posA;
    Math::Vector3 overlap(
        halfA.x + halfB.x - Math::Abs(diff.x),
        halfA.y + halfB.y - Math::Abs(diff.y),
        halfA.z + halfB.z - Math::Abs(diff.z)
    );

    if (overlap.x <= 0 || overlap.y <= 0 || overlap.z <= 0) return false;

    pair.bodyA = a;
    pair.bodyB = b;

    // Minimum penetration axis
    if (overlap.x <= overlap.y && overlap.x <= overlap.z) {
        pair.penetration = overlap.x;
        pair.normal = Math::Vector3(diff.x < 0 ? -1.0f : 1.0f, 0, 0);
    } else if (overlap.y <= overlap.x && overlap.y <= overlap.z) {
        pair.penetration = overlap.y;
        pair.normal = Math::Vector3(0, diff.y < 0 ? -1.0f : 1.0f, 0);
    } else {
        pair.penetration = overlap.z;
        pair.normal = Math::Vector3(0, 0, diff.z < 0 ? -1.0f : 1.0f);
    }

    pair.contactPoint = (posA + posB) * 0.5f;
    return true;
}

bool PhysicsWorld::TestSphereAABB(usize sphereIdx, usize boxIdx, CollisionPair& pair) {
    const auto& sphere = m_RigidBodies[sphereIdx];
    const auto& box = m_RigidBodies[boxIdx];

    Math::Vector3 spherePos = sphere->GetPosition();
    Math::Vector3 boxPos = box->GetPosition();
    Math::Vector3 halfExt = box->GetHalfExtents();

    // Clamp sphere center to AABB to find closest point
    Math::Vector3 closest(
        Math::Clamp(spherePos.x, boxPos.x - halfExt.x, boxPos.x + halfExt.x),
        Math::Clamp(spherePos.y, boxPos.y - halfExt.y, boxPos.y + halfExt.y),
        Math::Clamp(spherePos.z, boxPos.z - halfExt.z, boxPos.z + halfExt.z)
    );

    Math::Vector3 diff = spherePos - closest;
    f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    f32 radius = sphere->GetBoundingRadius();

    if (distSq >= radius * radius) return false;

    f32 dist = Math::Sqrt(distSq);
    pair.bodyA = sphereIdx;
    pair.bodyB = boxIdx;
    pair.penetration = radius - dist;
    pair.contactPoint = closest;

    if (dist > Math::EPSILON) {
        pair.normal = diff * (1.0f / dist);
    } else {
        // Sphere center inside box - use AABB face normal
        Math::Vector3 localPos = spherePos - boxPos;
        f32 minDist = halfExt.x - Math::Abs(localPos.x);
        pair.normal = Math::Vector3(localPos.x < 0 ? -1.0f : 1.0f, 0, 0);
        pair.penetration = minDist + radius;

        f32 dy = halfExt.y - Math::Abs(localPos.y);
        if (dy < minDist) {
            minDist = dy;
            pair.normal = Math::Vector3(0, localPos.y < 0 ? -1.0f : 1.0f, 0);
            pair.penetration = dy + radius;
        }

        f32 dz = halfExt.z - Math::Abs(localPos.z);
        if (dz < minDist) {
            pair.normal = Math::Vector3(0, 0, localPos.z < 0 ? -1.0f : 1.0f);
            pair.penetration = dz + radius;
        }
    }

    return true;
}

void PhysicsWorld::SyncFromECS() {
    if (!m_ECSWorld) return;

    auto entities = m_ECSWorld->GetEntitiesWithComponent<ECS::RigidbodyComponent>();

    // Track which entities are still alive so we can prune stale bodies
    std::unordered_set<ECS::Entity> aliveEntities;
    aliveEntities.reserve(entities.size());

    for (auto entity : entities) {
        aliveEntities.insert(entity);

        auto* rb = m_ECSWorld->GetComponent<ECS::RigidbodyComponent>(entity);
        auto* transform = m_ECSWorld->GetComponent<ECS::TransformComponent>(entity);
        if (!rb || !transform) continue;

        // Create body if this entity is new to the physics world
        auto it = m_EntityBodyMap.find(entity);
        if (it == m_EntityBodyMap.end()) {
            auto body = std::make_shared<RigidBody>();
            AddRigidBody(body);
            m_EntityBodyMap[entity] = body;
            it = m_EntityBodyMap.find(entity);
            ENJIN_LOG_INFO(Physics, "Created RigidBody for entity %llu", static_cast<unsigned long long>(entity));
        }

        auto& body = it->second;

        // Determine collider center offset and configure shape
        Math::Vector3 colliderOffset(0.0f, 0.0f, 0.0f);

        if (auto* box = m_ECSWorld->GetComponent<ECS::BoxColliderComponent>(entity)) {
            body->SetShape(RigidBody::Shape::Box);
            colliderOffset = box->center;

            // Half extents scaled by entity transform scale
            Math::Vector3 halfExtents(
                box->size.x * 0.5f * transform->scale.x,
                box->size.y * 0.5f * transform->scale.y,
                box->size.z * 0.5f * transform->scale.z
            );
            body->SetHalfExtents(halfExtents);
            body->SetFriction(box->friction);
            body->SetRestitution(box->bounciness);
            body->SetCategoryBits(box->categoryBits);
            body->SetCollisionMask(box->collisionMask);
        } else if (auto* sphere = m_ECSWorld->GetComponent<ECS::SphereColliderComponent>(entity)) {
            body->SetShape(RigidBody::Shape::Sphere);
            colliderOffset = sphere->center;

            // Bounding radius scaled by the largest scale axis
            f32 maxScale = Math::Max(transform->scale.x, Math::Max(transform->scale.y, transform->scale.z));
            body->SetBoundingRadius(sphere->radius * maxScale);
            body->SetFriction(sphere->friction);
            body->SetRestitution(sphere->bounciness);
            body->SetCategoryBits(sphere->categoryBits);
            body->SetCollisionMask(sphere->collisionMask);
        } else if (auto* capsule = m_ECSWorld->GetComponent<ECS::CapsuleColliderComponent>(entity)) {
            body->SetShape(RigidBody::Shape::Sphere);  // Approximate capsule as sphere
            colliderOffset = capsule->center;

            f32 maxScale = Math::Max(transform->scale.x, Math::Max(transform->scale.y, transform->scale.z));
            body->SetBoundingRadius(capsule->radius * maxScale);
            body->SetFriction(capsule->friction);
            body->SetRestitution(capsule->bounciness);
            body->SetCategoryBits(capsule->categoryBits);
            body->SetCollisionMask(capsule->collisionMask);
        }

        // Set body position from transform position + collider center offset
        body->SetPosition(transform->position + colliderOffset);

        // Set mass and static flag from RigidbodyComponent
        body->SetMass(rb->mass);
        body->SetStatic(rb->bodyType == ECS::RigidbodyComponent::BodyType::Static);

        // Set velocity from ECS component
        body->SetVelocity(rb->velocity);
    }

    // Remove bodies for entities that no longer have a RigidbodyComponent
    for (auto it = m_EntityBodyMap.begin(); it != m_EntityBodyMap.end(); ) {
        if (aliveEntities.find(it->first) == aliveEntities.end()) {
            RemoveRigidBody(it->second);
            ENJIN_LOG_INFO(Physics, "Removed RigidBody for entity %llu", static_cast<unsigned long long>(it->first));
            it = m_EntityBodyMap.erase(it);
        } else {
            ++it;
        }
    }
}

void PhysicsWorld::SyncToECS() {
    if (!m_ECSWorld) return;

    for (auto& [entity, body] : m_EntityBodyMap) {
        if (!m_ECSWorld->IsValid(entity)) continue;

        auto* rb = m_ECSWorld->GetComponent<ECS::RigidbodyComponent>(entity);
        auto* transform = m_ECSWorld->GetComponent<ECS::TransformComponent>(entity);
        if (!rb || !transform) continue;

        // Static and kinematic bodies are not driven by physics results
        if (rb->bodyType != ECS::RigidbodyComponent::BodyType::Dynamic) continue;

        // Determine collider center offset to subtract when writing back
        Math::Vector3 colliderOffset(0.0f, 0.0f, 0.0f);
        if (auto* box = m_ECSWorld->GetComponent<ECS::BoxColliderComponent>(entity)) {
            colliderOffset = box->center;
        } else if (auto* sphere = m_ECSWorld->GetComponent<ECS::SphereColliderComponent>(entity)) {
            colliderOffset = sphere->center;
        } else if (auto* capsule = m_ECSWorld->GetComponent<ECS::CapsuleColliderComponent>(entity)) {
            colliderOffset = capsule->center;
        }

        // Write position back (subtract collider offset)
        transform->position = body->GetPosition() - colliderOffset;

        // Get velocity from physics body
        Math::Vector3 velocity = body->GetVelocity();

        // Apply freeze constraints: zero out frozen axes
        if (rb->freezePositionX) velocity.x = 0.0f;
        if (rb->freezePositionY) velocity.y = 0.0f;
        if (rb->freezePositionZ) velocity.z = 0.0f;

        // Apply linear drag: velocity *= (1.0 - drag * deltaTime)
        if (rb->drag > 0.0f && m_LastDeltaTime > 0.0f) {
            f32 dragFactor = 1.0f - rb->drag * m_LastDeltaTime;
            if (dragFactor < 0.0f) dragFactor = 0.0f;
            velocity = velocity * dragFactor;
        }

        // Write velocity back to both the RigidBody and the ECS component
        body->SetVelocity(velocity);
        rb->velocity = velocity;
    }
}

RigidBody::RigidBody() {
}

RigidBody::~RigidBody() {
}

} // namespace Physics
} // namespace Enjin
