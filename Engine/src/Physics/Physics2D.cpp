#include "Enjin/Physics/Physics2D.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace Enjin {
namespace Physics {

// ============================================================================
// Helper utilities
// ============================================================================

static u64 MakePairKey(ECS::Entity a, ECS::Entity b) {
    u64 lo = static_cast<u64>(std::min(a, b));
    u64 hi = static_cast<u64>(std::max(a, b));
    return (lo << 32) | hi;
}

static bool PassesCollisionFilter(const Body2DComponent& a, const Body2DComponent& b) {
    return (a.categoryBits & b.collisionMask) != 0 &&
           (b.categoryBits & a.collisionMask) != 0;
}

// Rotate a 2D vector by angle (radians)
static Math::Vector2 RotateVec(const Math::Vector2& v, f32 angle) {
    f32 c = Math::Cos(angle);
    f32 s = Math::Sin(angle);
    return Math::Vector2(v.x * c - v.y * s, v.x * s + v.y * c);
}

// Inverse-rotate a 2D vector by angle (radians)
static Math::Vector2 InvRotateVec(const Math::Vector2& v, f32 angle) {
    return RotateVec(v, -angle);
}

// 2D cross product (scalar result): a x b
static f32 Cross2D(const Math::Vector2& a, const Math::Vector2& b) {
    return a.x * b.y - a.y * b.x;
}

// Cross of scalar and vector: s x v (perpendicular)
static Math::Vector2 CrossSV(f32 s, const Math::Vector2& v) {
    return Math::Vector2(-s * v.y, s * v.x);
}

// Cross of vector and scalar: v x s
static Math::Vector2 CrossVS(const Math::Vector2& v, f32 s) {
    return Math::Vector2(s * v.y, -s * v.x);
}

// Get the Z-axis Euler angle (radians) from a TransformComponent quaternion
static f32 GetRotationZ(const ECS::TransformComponent& t) {
    return t.rotation.ToEuler().z;
}

// Get world-space position as Vector2 from transform
static Math::Vector2 GetPosition2D(const ECS::TransformComponent& t) {
    return Math::Vector2(t.position.x, t.position.y);
}

// Compute AABB for a 2D body in world space
struct AABB2D {
    Math::Vector2 min;
    Math::Vector2 max;
};

static AABB2D ComputeAABB(const Math::Vector2& worldPos, f32 worldAngle,
                           const Body2DComponent& body) {
    AABB2D aabb;

    switch (body.shapeType) {
        case Shape2DType::Circle: {
            Math::Vector2 center = worldPos + RotateVec(body.circle.offset, worldAngle);
            f32 r = body.circle.radius;
            aabb.min = Math::Vector2(center.x - r, center.y - r);
            aabb.max = Math::Vector2(center.x + r, center.y + r);
            break;
        }
        case Shape2DType::Box: {
            f32 totalAngle = worldAngle + body.box.rotation;
            Math::Vector2 center = worldPos + RotateVec(body.box.offset, worldAngle);
            Math::Vector2 he = body.box.halfExtents;

            // Compute rotated corners and find extents
            Math::Vector2 corners[4] = {
                center + RotateVec(Math::Vector2(-he.x, -he.y), totalAngle),
                center + RotateVec(Math::Vector2( he.x, -he.y), totalAngle),
                center + RotateVec(Math::Vector2( he.x,  he.y), totalAngle),
                center + RotateVec(Math::Vector2(-he.x,  he.y), totalAngle)
            };

            aabb.min = corners[0];
            aabb.max = corners[0];
            for (u32 i = 1; i < 4; ++i) {
                aabb.min.x = Math::Min(aabb.min.x, corners[i].x);
                aabb.min.y = Math::Min(aabb.min.y, corners[i].y);
                aabb.max.x = Math::Max(aabb.max.x, corners[i].x);
                aabb.max.y = Math::Max(aabb.max.y, corners[i].y);
            }
            break;
        }
        case Shape2DType::Polygon: {
            Math::Vector2 center = worldPos + RotateVec(body.polygon.offset, worldAngle);
            if (body.polygon.vertices.empty()) {
                aabb.min = center;
                aabb.max = center;
                break;
            }

            Math::Vector2 first = center + RotateVec(body.polygon.vertices[0], worldAngle);
            aabb.min = first;
            aabb.max = first;
            for (usize i = 1; i < body.polygon.vertices.size(); ++i) {
                Math::Vector2 v = center + RotateVec(body.polygon.vertices[i], worldAngle);
                aabb.min.x = Math::Min(aabb.min.x, v.x);
                aabb.min.y = Math::Min(aabb.min.y, v.y);
                aabb.max.x = Math::Max(aabb.max.x, v.x);
                aabb.max.y = Math::Max(aabb.max.y, v.y);
            }
            break;
        }
    }

    return aabb;
}

static bool AABBOverlap(const AABB2D& a, const AABB2D& b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y;
}

// ============================================================================
// PhysicsWorld2D
// ============================================================================

PhysicsWorld2D::PhysicsWorld2D() = default;
PhysicsWorld2D::~PhysicsWorld2D() = default;

void PhysicsWorld2D::Initialize(ECS::World* world) {
    m_World = world;
    if (!m_World) return;

    // Compute mass and inertia for all Body2D components
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<Body2DComponent>()) {
        auto* body = m_World->GetComponent<Body2DComponent>(entity);
        if (!body) continue;

        if (body->isStatic) {
            body->mass = 0.0f;
            body->inverseMass = 0.0f;
            body->inertia = 0.0f;
            body->inverseInertia = 0.0f;
            continue;
        }

        f32 density = body->material.density;

        switch (body->shapeType) {
            case Shape2DType::Circle: {
                f32 r = body->circle.radius;
                f32 area = Math::PI * r * r;
                body->mass = area * density;
                body->inverseMass = (body->mass > Math::EPSILON) ? 1.0f / body->mass : 0.0f;
                // I = 0.5 * m * r^2 for solid circle
                body->inertia = 0.5f * body->mass * r * r;
                body->inverseInertia = (body->inertia > Math::EPSILON) ? 1.0f / body->inertia : 0.0f;
                break;
            }
            case Shape2DType::Box: {
                f32 w = body->box.halfExtents.x * 2.0f;
                f32 h = body->box.halfExtents.y * 2.0f;
                f32 area = w * h;
                body->mass = area * density;
                body->inverseMass = (body->mass > Math::EPSILON) ? 1.0f / body->mass : 0.0f;
                // I = (1/12) * m * (w^2 + h^2) for solid rectangle
                body->inertia = (1.0f / 12.0f) * body->mass * (w * w + h * h);
                body->inverseInertia = (body->inertia > Math::EPSILON) ? 1.0f / body->inertia : 0.0f;
                break;
            }
            case Shape2DType::Polygon: {
                // Approximate: compute area and inertia from polygon vertices
                const auto& verts = body->polygon.vertices;
                if (verts.size() < 3) {
                    body->mass = density;
                    body->inverseMass = 1.0f / body->mass;
                    body->inertia = 1.0f;
                    body->inverseInertia = 1.0f;
                    break;
                }

                f32 area = 0.0f;
                f32 I = 0.0f;
                for (usize i = 0; i < verts.size(); ++i) {
                    usize j = (i + 1) % verts.size();
                    f32 cross = Cross2D(verts[i], verts[j]);
                    area += cross;
                    I += cross * (verts[i].Dot(verts[i]) + verts[i].Dot(verts[j]) + verts[j].Dot(verts[j]));
                }
                area = Math::Abs(area) * 0.5f;
                I = Math::Abs(I) / 6.0f;

                body->mass = area * density;
                body->inverseMass = (body->mass > Math::EPSILON) ? 1.0f / body->mass : 0.0f;
                body->inertia = I * density;
                body->inverseInertia = (body->inertia > Math::EPSILON) ? 1.0f / body->inertia : 0.0f;
                break;
            }
        }
    }
}

void PhysicsWorld2D::Shutdown() {
    m_World = nullptr;
    m_ActiveContacts.clear();
}

void PhysicsWorld2D::SetGravity(const Math::Vector2& gravity) {
    m_Gravity = gravity;
}

// ============================================================================
// Main update loop
// ============================================================================

void PhysicsWorld2D::Update(f32 deltaTime) {
    if (!m_World) return;
    if (deltaTime <= 0.0f) return;

    // 1. Integrate velocities (apply gravity, damping)
    IntegrateVelocities(deltaTime);

    // 2. Broad phase: generate candidate pairs
    std::vector<std::pair<ECS::Entity, ECS::Entity>> pairs;
    BroadPhase(pairs);

    // 3. Narrow phase: generate manifolds
    std::vector<Manifold2D> manifolds;
    NarrowPhase(pairs, manifolds);

    // 4. Resolve collisions (impulse solver, iterated)
    for (u32 iter = 0; iter < m_VelocityIterations; ++iter) {
        ResolveCollisions(manifolds, deltaTime);
    }

    // 5. Solve joints
    for (u32 iter = 0; iter < m_VelocityIterations; ++iter) {
        SolveJoints(deltaTime);
    }

    // 6. Integrate positions
    IntegratePositions(deltaTime);

    // 7. Position correction (Baumgarte stabilization) for remaining penetration
    for (u32 iter = 0; iter < m_PositionIterations; ++iter) {
        for (auto& m : manifolds) {
            auto* bodyA = m_World->GetComponent<Body2DComponent>(m.entityA);
            auto* bodyB = m_World->GetComponent<Body2DComponent>(m.entityB);
            auto* tA = m_World->GetComponent<ECS::TransformComponent>(m.entityA);
            auto* tB = m_World->GetComponent<ECS::TransformComponent>(m.entityB);
            if (!bodyA || !bodyB || !tA || !tB) continue;
            if (bodyA->isSensor || bodyB->isSensor) continue;

            f32 totalInvMass = bodyA->inverseMass + bodyB->inverseMass;
            if (totalInvMass <= Math::EPSILON) continue;

            const f32 slop = 0.005f;
            const f32 baumgarte = 0.2f;
            f32 correction = Math::Max(m.penetration - slop, 0.0f) * baumgarte / totalInvMass;

            Math::Vector2 corrVec = m.normal * correction;
            tA->position.x -= corrVec.x * bodyA->inverseMass;
            tA->position.y -= corrVec.y * bodyA->inverseMass;
            tB->position.x += corrVec.x * bodyB->inverseMass;
            tB->position.y += corrVec.y * bodyB->inverseMass;
        }
    }

    // 8. CCD pass if enabled
    if (m_CCDEnabled) {
        PerformCCD(deltaTime);
    }

    // 9. Contact tracking for enter/exit callbacks
    std::unordered_set<u64> newContacts;
    for (const auto& m : manifolds) {
        u64 key = MakePairKey(m.entityA, m.entityB);
        newContacts.insert(key);

        auto* bodyA = m_World->GetComponent<Body2DComponent>(m.entityA);
        auto* bodyB = m_World->GetComponent<Body2DComponent>(m.entityB);
        bool isSensor = (bodyA && bodyA->isSensor) || (bodyB && bodyB->isSensor);

        // Collision enter
        if (m_ActiveContacts.find(key) == m_ActiveContacts.end()) {
            Contact2D contact;
            contact.entityA = m.entityA;
            contact.entityB = m.entityB;
            contact.normal = m.normal;
            contact.penetration = m.penetration;
            contact.point = (m.contactCount > 0) ? m.contactPoints[0] : Math::Vector2();

            if (isSensor) {
                if (m_OnSensorEnter) m_OnSensorEnter(contact);
            } else {
                if (m_OnCollisionEnter) m_OnCollisionEnter(contact);
            }
        }
    }

    // Collision exit: contacts that were active but no longer present
    for (u64 key : m_ActiveContacts) {
        if (newContacts.find(key) == newContacts.end()) {
            ECS::Entity a = static_cast<ECS::Entity>(key >> 32);
            ECS::Entity b = static_cast<ECS::Entity>(key & 0xFFFFFFFF);

            auto* bodyA = m_World->GetComponent<Body2DComponent>(a);
            auto* bodyB = m_World->GetComponent<Body2DComponent>(b);
            bool isSensor = (bodyA && bodyA->isSensor) || (bodyB && bodyB->isSensor);

            Contact2D contact;
            contact.entityA = a;
            contact.entityB = b;

            if (isSensor) {
                if (m_OnSensorExit) m_OnSensorExit(contact);
            } else {
                if (m_OnCollisionExit) m_OnCollisionExit(contact);
            }
        }
    }

    m_ActiveContacts = std::move(newContacts);
}

// ============================================================================
// Integrate velocities: apply gravity and damping
// ============================================================================

void PhysicsWorld2D::IntegrateVelocities(f32 dt) {
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<Body2DComponent>()) {
        auto* body = m_World->GetComponent<Body2DComponent>(entity);
        if (!body || body->isStatic) continue;

        // Apply gravity
        body->velocity += m_Gravity * body->gravityScale * dt;

        // Apply linear damping
        body->velocity *= (1.0f / (1.0f + body->linearDamping * dt));

        // Apply angular damping
        if (!body->fixedRotation) {
            body->angularVelocity *= (1.0f / (1.0f + body->angularDamping * dt));
        } else {
            body->angularVelocity = 0.0f;
        }
    }
}

// ============================================================================
// Integrate positions: write velocities back to TransformComponent
// ============================================================================

void PhysicsWorld2D::IntegratePositions(f32 dt) {
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<Body2DComponent>()) {
        auto* body = m_World->GetComponent<Body2DComponent>(entity);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!body || !transform || body->isStatic) continue;

        // Update position
        transform->position.x += body->velocity.x * dt;
        transform->position.y += body->velocity.y * dt;

        // Update rotation (Z axis for 2D)
        if (!body->fixedRotation) {
            // Extract current Z rotation, add angular velocity, write back as quaternion
            Math::Vector3 euler = transform->rotation.ToEuler();
            euler.z += body->angularVelocity * dt;
            transform->rotation = Math::Quaternion::FromEuler(euler);
        }
    }
}

// ============================================================================
// Broad phase: AABB overlap test for all Body2D pairs
// ============================================================================

void PhysicsWorld2D::BroadPhase(std::vector<std::pair<ECS::Entity, ECS::Entity>>& pairs) {
    pairs.clear();

    // Gather all entities with Body2D
    std::vector<ECS::Entity> entities;
    for (ECS::Entity e : m_World->GetEntitiesWithComponent<Body2DComponent>()) {
        entities.push_back(e);
    }

    if (entities.size() < 2) return;

    // Precompute AABBs
    struct EntityAABB {
        ECS::Entity entity;
        AABB2D aabb;
        Body2DComponent* body;
    };

    std::vector<EntityAABB> entries;
    entries.reserve(entities.size());

    for (ECS::Entity e : entities) {
        auto* body = m_World->GetComponent<Body2DComponent>(e);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(e);
        if (!body || !transform) continue;

        Math::Vector2 pos = GetPosition2D(*transform);
        f32 angle = GetRotationZ(*transform);

        EntityAABB entry;
        entry.entity = e;
        entry.body = body;
        entry.aabb = ComputeAABB(pos, angle, *body);
        entries.push_back(entry);
    }

    // Sort by AABB min.x for sweep-and-prune optimization
    std::sort(entries.begin(), entries.end(), [](const EntityAABB& a, const EntityAABB& b) {
        return a.aabb.min.x < b.aabb.min.x;
    });

    // Sweep and prune along X, then check Y overlap
    for (usize i = 0; i < entries.size(); ++i) {
        for (usize j = i + 1; j < entries.size(); ++j) {
            // Early exit: if the next AABB starts beyond our max.x, no more overlaps
            if (entries[j].aabb.min.x > entries[i].aabb.max.x) break;

            // Check Y overlap
            if (entries[i].aabb.min.y > entries[j].aabb.max.y ||
                entries[i].aabb.max.y < entries[j].aabb.min.y) {
                continue;
            }

            // Skip if both static
            if (entries[i].body->isStatic && entries[j].body->isStatic) continue;

            // Check collision filter bitmasks
            if (!PassesCollisionFilter(*entries[i].body, *entries[j].body)) continue;

            pairs.push_back({entries[i].entity, entries[j].entity});
        }
    }
}

// ============================================================================
// Narrow phase: dispatch to correct shape-vs-shape test
// ============================================================================

void PhysicsWorld2D::NarrowPhase(const std::vector<std::pair<ECS::Entity, ECS::Entity>>& pairs,
                                  std::vector<Manifold2D>& manifolds) {
    manifolds.clear();

    for (const auto& [eA, eB] : pairs) {
        auto* bodyA = m_World->GetComponent<Body2DComponent>(eA);
        auto* bodyB = m_World->GetComponent<Body2DComponent>(eB);
        auto* tA = m_World->GetComponent<ECS::TransformComponent>(eA);
        auto* tB = m_World->GetComponent<ECS::TransformComponent>(eB);
        if (!bodyA || !bodyB || !tA || !tB) continue;

        Math::Vector2 posA = GetPosition2D(*tA);
        Math::Vector2 posB = GetPosition2D(*tB);
        f32 angleA = GetRotationZ(*tA);
        f32 angleB = GetRotationZ(*tB);

        Manifold2D manifold;
        manifold.entityA = eA;
        manifold.entityB = eB;
        bool hit = false;

        Shape2DType sA = bodyA->shapeType;
        Shape2DType sB = bodyB->shapeType;

        if (sA == Shape2DType::Circle && sB == Shape2DType::Circle) {
            hit = TestCircleCircle(posA, bodyA->circle, posB, bodyB->circle, manifold);
        }
        else if (sA == Shape2DType::Circle && sB == Shape2DType::Box) {
            hit = TestCircleBox(posA, bodyA->circle, posB, bodyB->box, angleB + bodyB->box.rotation, manifold);
        }
        else if (sA == Shape2DType::Box && sB == Shape2DType::Circle) {
            hit = TestCircleBox(posB, bodyB->circle, posA, bodyA->box, angleA + bodyA->box.rotation, manifold);
            if (hit) {
                // Flip normal and swap entities since we swapped arguments
                manifold.normal = -manifold.normal;
                std::swap(manifold.entityA, manifold.entityB);
            }
        }
        else if (sA == Shape2DType::Box && sB == Shape2DType::Box) {
            hit = TestBoxBox(posA, bodyA->box, angleA, posB, bodyB->box, angleB, manifold);
        }
        else if (sA == Shape2DType::Circle && sB == Shape2DType::Polygon) {
            hit = TestCirclePolygon(posA, bodyA->circle, posB, bodyB->polygon, angleB, manifold);
        }
        else if (sA == Shape2DType::Polygon && sB == Shape2DType::Circle) {
            hit = TestCirclePolygon(posB, bodyB->circle, posA, bodyA->polygon, angleA, manifold);
            if (hit) {
                manifold.normal = -manifold.normal;
                std::swap(manifold.entityA, manifold.entityB);
            }
        }
        // Polygon-Box and Polygon-Polygon omitted for now (use AABB approximation)

        if (hit) {
            manifolds.push_back(manifold);
        }
    }
}

// ============================================================================
// Shape collision tests
// ============================================================================

bool PhysicsWorld2D::TestCircleCircle(const Math::Vector2& posA, const CircleShape2D& a,
                                       const Math::Vector2& posB, const CircleShape2D& b,
                                       Manifold2D& manifold) const {
    Math::Vector2 centerA = posA + a.offset;
    Math::Vector2 centerB = posB + b.offset;
    Math::Vector2 diff = centerB - centerA;
    f32 distSq = diff.LengthSquared();
    f32 radiusSum = a.radius + b.radius;

    if (distSq >= radiusSum * radiusSum) return false;

    f32 dist = Math::Sqrt(distSq);

    if (dist > Math::EPSILON) {
        manifold.normal = diff * (1.0f / dist);
    } else {
        // Circles are coincident, pick arbitrary normal
        manifold.normal = Math::Vector2(1.0f, 0.0f);
    }

    manifold.penetration = radiusSum - dist;
    manifold.contactPoints[0] = centerA + manifold.normal * a.radius;
    manifold.contactCount = 1;

    return true;
}

bool PhysicsWorld2D::TestCircleBox(const Math::Vector2& posA, const CircleShape2D& circle,
                                    const Math::Vector2& posB, const BoxShape2D& box, f32 boxAngle,
                                    Manifold2D& manifold) const {
    // Transform circle center into box-local space
    Math::Vector2 circleCenter = posA + circle.offset;
    Math::Vector2 boxCenter = posB + RotateVec(box.offset, boxAngle - box.rotation);

    Math::Vector2 localCircle = InvRotateVec(circleCenter - boxCenter, boxAngle);

    // Clamp circle center to box
    Math::Vector2 closest;
    closest.x = Math::Clamp(localCircle.x, -box.halfExtents.x, box.halfExtents.x);
    closest.y = Math::Clamp(localCircle.y, -box.halfExtents.y, box.halfExtents.y);

    Math::Vector2 diff = localCircle - closest;
    f32 distSq = diff.LengthSquared();

    if (distSq >= circle.radius * circle.radius) return false;

    f32 dist = Math::Sqrt(distSq);

    Math::Vector2 localNormal;
    if (dist > Math::EPSILON) {
        localNormal = diff * (1.0f / dist);
    } else {
        // Circle center is inside box, push out along shortest axis
        f32 dx = box.halfExtents.x - Math::Abs(localCircle.x);
        f32 dy = box.halfExtents.y - Math::Abs(localCircle.y);
        if (dx < dy) {
            localNormal = Math::Vector2(localCircle.x >= 0.0f ? 1.0f : -1.0f, 0.0f);
            manifold.penetration = dx + circle.radius;
        } else {
            localNormal = Math::Vector2(0.0f, localCircle.y >= 0.0f ? 1.0f : -1.0f);
            manifold.penetration = dy + circle.radius;
        }

        manifold.normal = RotateVec(localNormal, boxAngle);
        manifold.contactPoints[0] = circleCenter - manifold.normal * circle.radius;
        manifold.contactCount = 1;
        return true;
    }

    manifold.penetration = circle.radius - dist;
    manifold.normal = RotateVec(localNormal, boxAngle);
    manifold.contactPoints[0] = circleCenter - manifold.normal * circle.radius;
    manifold.contactCount = 1;

    return true;
}

bool PhysicsWorld2D::TestBoxBox(const Math::Vector2& posA, const BoxShape2D& a, f32 angleA,
                                 const Math::Vector2& posB, const BoxShape2D& b, f32 angleB,
                                 Manifold2D& manifold) const {
    // SAT (Separating Axis Theorem) with 4 axes (2 per box)
    f32 totalAngleA = angleA + a.rotation;
    f32 totalAngleB = angleB + b.rotation;

    Math::Vector2 centerA = posA + RotateVec(a.offset, angleA);
    Math::Vector2 centerB = posB + RotateVec(b.offset, angleB);

    // Get axes for both boxes (the two edge normals)
    Math::Vector2 axesA[2] = {
        RotateVec(Math::Vector2(1.0f, 0.0f), totalAngleA),
        RotateVec(Math::Vector2(0.0f, 1.0f), totalAngleA)
    };
    Math::Vector2 axesB[2] = {
        RotateVec(Math::Vector2(1.0f, 0.0f), totalAngleB),
        RotateVec(Math::Vector2(0.0f, 1.0f), totalAngleB)
    };

    f32 minOverlap = std::numeric_limits<f32>::max();
    Math::Vector2 minAxis;

    // Get corners for both boxes
    Math::Vector2 cornersA[4], cornersB[4];
    {
        Math::Vector2 he = a.halfExtents;
        cornersA[0] = centerA + RotateVec(Math::Vector2(-he.x, -he.y), totalAngleA);
        cornersA[1] = centerA + RotateVec(Math::Vector2( he.x, -he.y), totalAngleA);
        cornersA[2] = centerA + RotateVec(Math::Vector2( he.x,  he.y), totalAngleA);
        cornersA[3] = centerA + RotateVec(Math::Vector2(-he.x,  he.y), totalAngleA);
    }
    {
        Math::Vector2 he = b.halfExtents;
        cornersB[0] = centerB + RotateVec(Math::Vector2(-he.x, -he.y), totalAngleB);
        cornersB[1] = centerB + RotateVec(Math::Vector2( he.x, -he.y), totalAngleB);
        cornersB[2] = centerB + RotateVec(Math::Vector2( he.x,  he.y), totalAngleB);
        cornersB[3] = centerB + RotateVec(Math::Vector2(-he.x,  he.y), totalAngleB);
    }

    // Test each axis
    auto testAxis = [&](const Math::Vector2& axis) -> bool {
        // Project all corners of A onto axis
        f32 minAProj = std::numeric_limits<f32>::max();
        f32 maxAProj = std::numeric_limits<f32>::lowest();
        for (u32 i = 0; i < 4; ++i) {
            f32 proj = cornersA[i].Dot(axis);
            minAProj = Math::Min(minAProj, proj);
            maxAProj = Math::Max(maxAProj, proj);
        }

        f32 minBProj = std::numeric_limits<f32>::max();
        f32 maxBProj = std::numeric_limits<f32>::lowest();
        for (u32 i = 0; i < 4; ++i) {
            f32 proj = cornersB[i].Dot(axis);
            minBProj = Math::Min(minBProj, proj);
            maxBProj = Math::Max(maxBProj, proj);
        }

        // Check for gap
        if (maxAProj < minBProj || maxBProj < minAProj) return false;

        // Compute overlap
        f32 overlap = Math::Min(maxAProj, maxBProj) - Math::Max(minAProj, minBProj);
        if (overlap < minOverlap) {
            minOverlap = overlap;
            minAxis = axis;

            // Ensure normal points from A to B
            Math::Vector2 d = centerB - centerA;
            if (d.Dot(minAxis) < 0.0f) {
                minAxis = -minAxis;
            }
        }
        return true;
    };

    // Test all 4 axes
    if (!testAxis(axesA[0])) return false;
    if (!testAxis(axesA[1])) return false;
    if (!testAxis(axesB[0])) return false;
    if (!testAxis(axesB[1])) return false;

    manifold.normal = minAxis;
    manifold.penetration = minOverlap;

    // Contact point: midpoint of the overlapping region projected onto the contact axis
    // Use the deepest penetrating corner as a simple approximation
    f32 bestDist = std::numeric_limits<f32>::lowest();
    Math::Vector2 bestPoint;

    for (u32 i = 0; i < 4; ++i) {
        f32 d = manifold.normal.Dot(cornersA[i]);
        if (d > bestDist) {
            bestDist = d;
            bestPoint = cornersA[i];
        }
    }
    manifold.contactPoints[0] = bestPoint;

    // Check if there is a second contact point (edge-edge contact)
    manifold.contactCount = 1;
    for (u32 i = 0; i < 4; ++i) {
        f32 d = manifold.normal.Dot(cornersA[i]);
        if (Math::Abs(d - bestDist) < 0.01f && (cornersA[i] - bestPoint).LengthSquared() > 0.0001f) {
            manifold.contactPoints[1] = cornersA[i];
            manifold.contactCount = 2;
            break;
        }
    }

    return true;
}

bool PhysicsWorld2D::TestCirclePolygon(const Math::Vector2& posA, const CircleShape2D& circle,
                                        const Math::Vector2& posB, const PolygonShape2D& poly, f32 polyAngle,
                                        Manifold2D& manifold) const {
    Math::Vector2 circleCenter = posA + circle.offset;
    Math::Vector2 polyCenter = posB + RotateVec(poly.offset, polyAngle);

    const auto& verts = poly.vertices;
    if (verts.size() < 3) return false;

    // Transform circle center into polygon local space
    Math::Vector2 localCircle = InvRotateVec(circleCenter - polyCenter, polyAngle);

    // Find closest edge or vertex
    f32 maxSep = std::numeric_limits<f32>::lowest();
    usize closestEdge = 0;

    for (usize i = 0; i < verts.size(); ++i) {
        usize j = (i + 1) % verts.size();
        Math::Vector2 edge = verts[j] - verts[i];
        // Edge normal (outward, for CCW winding)
        Math::Vector2 normal = Math::Vector2(edge.y, -edge.x).Normalized();

        f32 separation = normal.Dot(localCircle - verts[i]);
        if (separation > maxSep) {
            maxSep = separation;
            closestEdge = i;
        }
    }

    // If the circle center is farther than radius from all edges, no collision
    if (maxSep > circle.radius) return false;

    // Determine which feature is closest: vertex or edge
    usize i = closestEdge;
    usize j = (i + 1) % verts.size();
    Math::Vector2 edgeVec = verts[j] - verts[i];
    f32 edgeLen = edgeVec.Length();
    if (edgeLen < Math::EPSILON) return false;

    Math::Vector2 edgeDir = edgeVec * (1.0f / edgeLen);
    f32 t = (localCircle - verts[i]).Dot(edgeDir);

    Math::Vector2 closestPoint;
    Math::Vector2 localNormal;

    if (t <= 0.0f) {
        // Closest to vertex i
        closestPoint = verts[i];
        Math::Vector2 diff = localCircle - closestPoint;
        f32 dist = diff.Length();
        if (dist > circle.radius) return false;
        if (dist < Math::EPSILON) {
            localNormal = Math::Vector2(edgeVec.y, -edgeVec.x).Normalized();
        } else {
            localNormal = diff * (1.0f / dist);
        }
        manifold.penetration = circle.radius - dist;
    } else if (t >= edgeLen) {
        // Closest to vertex j
        closestPoint = verts[j];
        Math::Vector2 diff = localCircle - closestPoint;
        f32 dist = diff.Length();
        if (dist > circle.radius) return false;
        if (dist < Math::EPSILON) {
            localNormal = Math::Vector2(edgeVec.y, -edgeVec.x).Normalized();
        } else {
            localNormal = diff * (1.0f / dist);
        }
        manifold.penetration = circle.radius - dist;
    } else {
        // Closest to edge face
        closestPoint = verts[i] + edgeDir * t;
        localNormal = Math::Vector2(edgeVec.y, -edgeVec.x).Normalized();
        f32 dist = (localCircle - closestPoint).Dot(localNormal);
        if (dist > circle.radius) return false;
        manifold.penetration = circle.radius - dist;
    }

    // Transform back to world space
    manifold.normal = RotateVec(localNormal, polyAngle);
    manifold.contactPoints[0] = circleCenter - manifold.normal * circle.radius;
    manifold.contactCount = 1;

    return true;
}

// ============================================================================
// Collision resolution: impulse-based with friction
// ============================================================================

void PhysicsWorld2D::ResolveCollisions(std::vector<Manifold2D>& manifolds, f32 /*dt*/) {
    for (auto& m : manifolds) {
        auto* bodyA = m_World->GetComponent<Body2DComponent>(m.entityA);
        auto* bodyB = m_World->GetComponent<Body2DComponent>(m.entityB);
        if (!bodyA || !bodyB) continue;

        // Sensors trigger callbacks but no physical response
        if (bodyA->isSensor || bodyB->isSensor) continue;

        f32 invMassA = bodyA->inverseMass;
        f32 invMassB = bodyB->inverseMass;
        f32 invInertiaA = bodyA->fixedRotation ? 0.0f : bodyA->inverseInertia;
        f32 invInertiaB = bodyB->fixedRotation ? 0.0f : bodyB->inverseInertia;

        if (invMassA + invMassB <= Math::EPSILON) continue;

        // Average restitution and friction
        f32 restitution = Math::Min(bodyA->material.restitution, bodyB->material.restitution);
        f32 friction = Math::Sqrt(bodyA->material.friction * bodyB->material.friction);

        for (u32 c = 0; c < m.contactCount; ++c) {
            Math::Vector2 cp = m.contactPoints[c];

            // Radii from center of mass to contact point
            auto* tA = m_World->GetComponent<ECS::TransformComponent>(m.entityA);
            auto* tB = m_World->GetComponent<ECS::TransformComponent>(m.entityB);
            if (!tA || !tB) continue;

            Math::Vector2 rA = cp - GetPosition2D(*tA);
            Math::Vector2 rB = cp - GetPosition2D(*tB);

            // Relative velocity at contact point
            Math::Vector2 velA = bodyA->velocity + CrossSV(bodyA->angularVelocity, rA);
            Math::Vector2 velB = bodyB->velocity + CrossSV(bodyB->angularVelocity, rB);
            Math::Vector2 relVel = velB - velA;

            // Relative velocity along the normal
            f32 velAlongNormal = relVel.Dot(m.normal);

            // Do not resolve if objects are separating
            if (velAlongNormal > 0.0f) continue;

            // Compute impulse scalar
            f32 rACrossN = Cross2D(rA, m.normal);
            f32 rBCrossN = Cross2D(rB, m.normal);

            f32 denom = invMassA + invMassB +
                        rACrossN * rACrossN * invInertiaA +
                        rBCrossN * rBCrossN * invInertiaB;

            if (denom <= Math::EPSILON) continue;

            f32 j = -(1.0f + restitution) * velAlongNormal / denom;
            j /= static_cast<f32>(m.contactCount);

            // Apply normal impulse
            Math::Vector2 impulse = m.normal * j;
            bodyA->velocity -= impulse * invMassA;
            bodyB->velocity += impulse * invMassB;
            bodyA->angularVelocity -= Cross2D(rA, impulse) * invInertiaA;
            bodyB->angularVelocity += Cross2D(rB, impulse) * invInertiaB;

            // Friction impulse (tangent)
            // Recompute relative velocity after normal impulse
            velA = bodyA->velocity + CrossSV(bodyA->angularVelocity, rA);
            velB = bodyB->velocity + CrossSV(bodyB->angularVelocity, rB);
            relVel = velB - velA;

            Math::Vector2 tangent = relVel - m.normal * relVel.Dot(m.normal);
            f32 tangentLen = tangent.Length();
            if (tangentLen > Math::EPSILON) {
                tangent = tangent * (1.0f / tangentLen);
            } else {
                continue;
            }

            f32 rACrossT = Cross2D(rA, tangent);
            f32 rBCrossT = Cross2D(rB, tangent);

            f32 denomT = invMassA + invMassB +
                         rACrossT * rACrossT * invInertiaA +
                         rBCrossT * rBCrossT * invInertiaB;

            if (denomT <= Math::EPSILON) continue;

            f32 jt = -relVel.Dot(tangent) / denomT;
            jt /= static_cast<f32>(m.contactCount);

            // Coulomb friction: clamp tangent impulse
            Math::Vector2 frictionImpulse;
            if (Math::Abs(jt) < j * friction) {
                frictionImpulse = tangent * jt;
            } else {
                frictionImpulse = tangent * (-j * friction);
            }

            bodyA->velocity -= frictionImpulse * invMassA;
            bodyB->velocity += frictionImpulse * invMassB;
            bodyA->angularVelocity -= Cross2D(rA, frictionImpulse) * invInertiaA;
            bodyB->angularVelocity += Cross2D(rB, frictionImpulse) * invInertiaB;
        }
    }
}

// ============================================================================
// Joint solving
// ============================================================================

void PhysicsWorld2D::SolveJoints(f32 dt) {
    if (!m_World || dt <= Math::EPSILON) return;

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<Joint2DComponent>()) {
        auto* joint = m_World->GetComponent<Joint2DComponent>(entity);
        auto* bodyA = m_World->GetComponent<Body2DComponent>(entity);
        auto* tA = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!joint || !bodyA || !tA) continue;
        if (joint->connectedEntity == 0) continue;

        auto* bodyB = m_World->GetComponent<Body2DComponent>(joint->connectedEntity);
        auto* tB = m_World->GetComponent<ECS::TransformComponent>(joint->connectedEntity);
        if (!tB) continue;

        // bodyB may be null if the connected entity is just a transform (static anchor)
        f32 invMassA = bodyA->inverseMass;
        f32 invMassB = bodyB ? bodyB->inverseMass : 0.0f;
        f32 invInertiaA = (bodyA->fixedRotation) ? 0.0f : bodyA->inverseInertia;
        f32 invInertiaB = (bodyB && !bodyB->fixedRotation) ? bodyB->inverseInertia : 0.0f;

        Math::Vector2 posA = GetPosition2D(*tA);
        Math::Vector2 posB = GetPosition2D(*tB);
        f32 angleA = GetRotationZ(*tA);
        f32 angleB = GetRotationZ(*tB);

        Math::Vector2 worldAnchorA = posA + RotateVec(joint->anchorA, angleA);
        Math::Vector2 worldAnchorB = posB + RotateVec(joint->anchorB, angleB);

        switch (joint->type) {
            case Joint2DType::Revolute: {
                // Revolute: anchors must coincide
                Math::Vector2 rA = worldAnchorA - posA;
                Math::Vector2 rB = worldAnchorB - posB;
                Math::Vector2 error = worldAnchorB - worldAnchorA;

                f32 totalInvMass = invMassA + invMassB +
                    Cross2D(rA, error) * invInertiaA + // simplified for position correction
                    Cross2D(rB, error) * invInertiaB;

                // Position correction as velocity impulse
                if (error.LengthSquared() > Math::EPSILON) {
                    f32 effectiveInvMass = invMassA + invMassB;
                    if (effectiveInvMass > Math::EPSILON) {
                        Math::Vector2 correction = error * (0.5f / dt) * 0.2f; // Baumgarte factor
                        bodyA->velocity += correction * invMassA;
                        if (bodyB) bodyB->velocity -= correction * invMassB;
                    }
                }

                // Angle limits
                if (joint->enableLimit) {
                    f32 relAngle = angleB - angleA;
                    // Normalize to -PI..PI
                    while (relAngle > Math::PI) relAngle -= Math::PI_2;
                    while (relAngle < -Math::PI) relAngle += Math::PI_2;

                    if (relAngle < joint->lowerAngle) {
                        f32 angCorr = (joint->lowerAngle - relAngle) * 0.2f / dt;
                        bodyA->angularVelocity -= angCorr * invInertiaA;
                        if (bodyB) bodyB->angularVelocity += angCorr * invInertiaB;
                    } else if (relAngle > joint->upperAngle) {
                        f32 angCorr = (relAngle - joint->upperAngle) * 0.2f / dt;
                        bodyA->angularVelocity += angCorr * invInertiaA;
                        if (bodyB) bodyB->angularVelocity -= angCorr * invInertiaB;
                    }
                }

                // Motor
                if (joint->enableMotor) {
                    f32 currentSpeed = (bodyB ? bodyB->angularVelocity : 0.0f) - bodyA->angularVelocity;
                    f32 motorImpulse = (joint->motorSpeed - currentSpeed);
                    f32 maxImpulse = joint->maxMotorTorque * dt;
                    motorImpulse = Math::Clamp(motorImpulse, -maxImpulse, maxImpulse);

                    bodyA->angularVelocity -= motorImpulse * invInertiaA;
                    if (bodyB) bodyB->angularVelocity += motorImpulse * invInertiaB;
                }
                break;
            }

            case Joint2DType::Distance: {
                Math::Vector2 diff = worldAnchorB - worldAnchorA;
                f32 currentLen = diff.Length();
                if (currentLen < Math::EPSILON) break;

                Math::Vector2 dir = diff * (1.0f / currentLen);
                f32 error = currentLen - joint->length;

                if (joint->stiffness > 0.0f) {
                    // Spring: apply spring force as velocity change
                    f32 springForce = error * joint->stiffness;

                    // Relative velocity along direction
                    Math::Vector2 velA = bodyA->velocity;
                    Math::Vector2 velB = bodyB ? bodyB->velocity : Math::Vector2();
                    f32 relVelAlongDir = (velB - velA).Dot(dir);

                    f32 dampingForce = relVelAlongDir * joint->damping;
                    f32 totalForce = springForce + dampingForce;

                    Math::Vector2 impulse = dir * (totalForce * dt);
                    bodyA->velocity += impulse * invMassA;
                    if (bodyB) bodyB->velocity -= impulse * invMassB;
                } else {
                    // Rigid distance constraint
                    f32 effectiveInvMass = invMassA + invMassB;
                    if (effectiveInvMass > Math::EPSILON) {
                        Math::Vector2 impulse = dir * (error / effectiveInvMass * 0.5f / dt);
                        bodyA->velocity += impulse * invMassA;
                        if (bodyB) bodyB->velocity -= impulse * invMassB;
                    }
                }
                break;
            }

            case Joint2DType::Rope: {
                Math::Vector2 diff = worldAnchorB - worldAnchorA;
                f32 currentLen = diff.Length();
                f32 maxLen = joint->maxLength > 0.0f ? joint->maxLength : joint->length;

                if (currentLen > maxLen && currentLen > Math::EPSILON) {
                    Math::Vector2 dir = diff * (1.0f / currentLen);
                    f32 error = currentLen - maxLen;

                    f32 effectiveInvMass = invMassA + invMassB;
                    if (effectiveInvMass > Math::EPSILON) {
                        Math::Vector2 impulse = dir * (error / effectiveInvMass * 0.5f / dt);
                        bodyA->velocity += impulse * invMassA;
                        if (bodyB) bodyB->velocity -= impulse * invMassB;
                    }

                    // Also clamp relative velocity so they don't separate further
                    Math::Vector2 velA = bodyA->velocity;
                    Math::Vector2 velB = bodyB ? bodyB->velocity : Math::Vector2();
                    f32 relVel = (velB - velA).Dot(dir);
                    if (relVel > 0.0f) {
                        Math::Vector2 velCorrection = dir * (relVel / (invMassA + invMassB));
                        bodyA->velocity += velCorrection * invMassA;
                        if (bodyB) bodyB->velocity -= velCorrection * invMassB;
                    }
                }
                break;
            }

            case Joint2DType::Weld: {
                // Position constraint (same as revolute)
                Math::Vector2 error = worldAnchorB - worldAnchorA;
                if (error.LengthSquared() > Math::EPSILON) {
                    f32 effectiveInvMass = invMassA + invMassB;
                    if (effectiveInvMass > Math::EPSILON) {
                        Math::Vector2 correction = error * (0.5f / dt) * 0.3f;
                        bodyA->velocity += correction * invMassA;
                        if (bodyB) bodyB->velocity -= correction * invMassB;
                    }
                }

                // Angle constraint: relative angle should be zero
                f32 relAngle = angleB - angleA;
                while (relAngle > Math::PI) relAngle -= Math::PI_2;
                while (relAngle < -Math::PI) relAngle += Math::PI_2;

                if (Math::Abs(relAngle) > Math::EPSILON) {
                    f32 totalInvI = invInertiaA + invInertiaB;
                    if (totalInvI > Math::EPSILON) {
                        f32 angCorr = relAngle * 0.3f / dt;
                        bodyA->angularVelocity += angCorr * invInertiaA / totalInvI;
                        if (bodyB) bodyB->angularVelocity -= angCorr * invInertiaB / totalInvI;
                    }
                }
                break;
            }

            case Joint2DType::Prismatic: {
                // Constrain movement to the joint axis
                Math::Vector2 worldAxis = RotateVec(joint->axis, angleA).Normalized();
                Math::Vector2 perp = Math::Vector2(-worldAxis.y, worldAxis.x);

                Math::Vector2 diff = worldAnchorB - worldAnchorA;

                // Remove perpendicular component (constrain to axis)
                f32 perpError = diff.Dot(perp);
                if (Math::Abs(perpError) > Math::EPSILON) {
                    f32 effectiveInvMass = invMassA + invMassB;
                    if (effectiveInvMass > Math::EPSILON) {
                        Math::Vector2 correction = perp * (perpError * 0.5f / dt * 0.2f);
                        bodyA->velocity += correction * invMassA;
                        if (bodyB) bodyB->velocity -= correction * invMassB;
                    }
                }

                // Translation limits along axis
                if (joint->enableLimit) {
                    f32 axisPos = diff.Dot(worldAxis);
                    if (axisPos < joint->lowerTranslation) {
                        f32 error = joint->lowerTranslation - axisPos;
                        Math::Vector2 correction = worldAxis * (error * 0.2f / dt);
                        f32 effectiveInvMass = invMassA + invMassB;
                        if (effectiveInvMass > Math::EPSILON) {
                            bodyA->velocity -= correction * (invMassA / effectiveInvMass);
                            if (bodyB) bodyB->velocity += correction * (invMassB / effectiveInvMass);
                        }
                    } else if (axisPos > joint->upperTranslation) {
                        f32 error = axisPos - joint->upperTranslation;
                        Math::Vector2 correction = worldAxis * (error * 0.2f / dt);
                        f32 effectiveInvMass = invMassA + invMassB;
                        if (effectiveInvMass > Math::EPSILON) {
                            bodyA->velocity += correction * (invMassA / effectiveInvMass);
                            if (bodyB) bodyB->velocity -= correction * (invMassB / effectiveInvMass);
                        }
                    }
                }

                // Motor along axis
                if (joint->enableMotor) {
                    Math::Vector2 velA = bodyA->velocity;
                    Math::Vector2 velB = bodyB ? bodyB->velocity : Math::Vector2();
                    f32 currentSpeed = (velB - velA).Dot(worldAxis);
                    f32 motorImpulse = (joint->motorSpeed - currentSpeed);
                    f32 maxImpulse = joint->maxMotorTorque * dt; // reusing maxMotorTorque as maxForce
                    motorImpulse = Math::Clamp(motorImpulse, -maxImpulse, maxImpulse);

                    Math::Vector2 impulse = worldAxis * motorImpulse;
                    bodyA->velocity -= impulse * invMassA;
                    if (bodyB) bodyB->velocity += impulse * invMassB;
                }
                break;
            }
        }
    }
}

// ============================================================================
// CCD (Continuous Collision Detection)
// ============================================================================

void PhysicsWorld2D::PerformCCD(f32 dt) {
    if (!m_World) return;

    // For fast-moving objects, check if they would tunnel through thin objects
    // by subdividing the timestep and performing intermediate collision checks
    const f32 ccdThreshold = 2.0f; // Speed threshold for CCD (units/sec)

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<Body2DComponent>()) {
        auto* body = m_World->GetComponent<Body2DComponent>(entity);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!body || !transform || body->isStatic) continue;

        f32 speed = body->velocity.Length();
        if (speed < ccdThreshold) continue;

        // Compute the distance traveled this frame
        f32 distThisFrame = speed * dt;

        // Get effective radius of the shape
        f32 shapeRadius = 0.0f;
        switch (body->shapeType) {
            case Shape2DType::Circle: shapeRadius = body->circle.radius; break;
            case Shape2DType::Box:
                shapeRadius = Math::Sqrt(body->box.halfExtents.x * body->box.halfExtents.x +
                                          body->box.halfExtents.y * body->box.halfExtents.y);
                break;
            case Shape2DType::Polygon:
                for (const auto& v : body->polygon.vertices) {
                    f32 len = v.Length();
                    shapeRadius = Math::Max(shapeRadius, len);
                }
                break;
        }

        // If displacement is larger than the shape radius, we risk tunneling
        if (distThisFrame <= shapeRadius) continue;

        // Cast a ray along the velocity direction to find the earliest contact
        Math::Vector2 origin = GetPosition2D(*transform);
        Math::Vector2 dir = body->velocity.Normalized();
        RayHit2D hit;

        if (Raycast(origin, dir, distThisFrame, hit, body->collisionMask)) {
            // Snap back to just before the collision point
            f32 safeDistance = Math::Max(hit.distance - shapeRadius, 0.0f);
            Math::Vector2 safePos = origin + dir * safeDistance;
            transform->position.x = safePos.x;
            transform->position.y = safePos.y;

            // Reflect velocity along hit normal
            f32 velDotN = body->velocity.Dot(hit.normal);
            if (velDotN < 0.0f) {
                body->velocity = body->velocity - hit.normal * (velDotN * (1.0f + body->material.restitution));
            }
        }
    }
}

// ============================================================================
// Spatial queries
// ============================================================================

bool PhysicsWorld2D::Raycast(const Math::Vector2& origin, const Math::Vector2& direction,
                              f32 maxDistance, RayHit2D& outHit, u32 layerMask) const {
    if (!m_World) return false;

    Math::Vector2 dir = direction.Normalized();
    f32 closestDist = maxDistance;
    bool found = false;

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<Body2DComponent>()) {
        auto* body = m_World->GetComponent<Body2DComponent>(entity);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!body || !transform) continue;

        if (!(body->categoryBits & layerMask)) continue;

        Math::Vector2 pos = GetPosition2D(*transform);
        f32 angle = GetRotationZ(*transform);

        switch (body->shapeType) {
            case Shape2DType::Circle: {
                Math::Vector2 center = pos + RotateVec(body->circle.offset, angle);
                Math::Vector2 oc = origin - center;
                f32 r = body->circle.radius;

                f32 a = dir.Dot(dir);
                f32 b = 2.0f * oc.Dot(dir);
                f32 c = oc.Dot(oc) - r * r;
                f32 disc = b * b - 4.0f * a * c;

                if (disc >= 0.0f) {
                    f32 sqrtDisc = Math::Sqrt(disc);
                    f32 t = (-b - sqrtDisc) / (2.0f * a);
                    if (t < 0.0f) t = (-b + sqrtDisc) / (2.0f * a);

                    if (t >= 0.0f && t < closestDist) {
                        closestDist = t;
                        outHit.entity = entity;
                        outHit.distance = t;
                        outHit.point = origin + dir * t;
                        outHit.normal = (outHit.point - center).Normalized();
                        found = true;
                    }
                }
                break;
            }
            case Shape2DType::Box: {
                f32 totalAngle = angle + body->box.rotation;
                Math::Vector2 center = pos + RotateVec(body->box.offset, angle);

                // Transform ray into box-local space
                Math::Vector2 localOrigin = InvRotateVec(origin - center, totalAngle);
                Math::Vector2 localDir = InvRotateVec(dir, totalAngle);

                Math::Vector2 he = body->box.halfExtents;

                // Ray-AABB intersection in local space (slab method)
                f32 tmin = 0.0f;
                f32 tmax = closestDist;
                Math::Vector2 hitNormalLocal;

                for (i32 axis = 0; axis < 2; ++axis) {
                    f32 orig = (axis == 0) ? localOrigin.x : localOrigin.y;
                    f32 d = (axis == 0) ? localDir.x : localDir.y;
                    f32 bmin = (axis == 0) ? -he.x : -he.y;
                    f32 bmax = (axis == 0) ? he.x : he.y;

                    if (Math::Abs(d) < 0.0001f) {
                        if (orig < bmin || orig > bmax) { tmin = closestDist + 1.0f; break; }
                    } else {
                        f32 t1 = (bmin - orig) / d;
                        f32 t2 = (bmax - orig) / d;

                        Math::Vector2 n1, n2;
                        if (axis == 0) { n1 = Math::Vector2(-1, 0); n2 = Math::Vector2(1, 0); }
                        else { n1 = Math::Vector2(0, -1); n2 = Math::Vector2(0, 1); }

                        if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }

                        if (t1 > tmin) { tmin = t1; hitNormalLocal = n1; }
                        tmax = Math::Min(tmax, t2);

                        if (tmin > tmax) break;
                    }
                }

                if (tmin <= tmax && tmin < closestDist && tmin >= 0.0f) {
                    closestDist = tmin;
                    outHit.entity = entity;
                    outHit.distance = tmin;
                    outHit.point = origin + dir * tmin;
                    outHit.normal = RotateVec(hitNormalLocal, totalAngle);
                    found = true;
                }
                break;
            }
            case Shape2DType::Polygon: {
                // Ray vs polygon: test each edge
                Math::Vector2 polyCenter = pos + RotateVec(body->polygon.offset, angle);
                const auto& verts = body->polygon.vertices;
                if (verts.size() < 3) break;

                for (usize i = 0; i < verts.size(); ++i) {
                    usize j = (i + 1) % verts.size();
                    Math::Vector2 v0 = polyCenter + RotateVec(verts[i], angle);
                    Math::Vector2 v1 = polyCenter + RotateVec(verts[j], angle);

                    Math::Vector2 edge = v1 - v0;
                    Math::Vector2 toOrigin = origin - v0;

                    f32 denom = Cross2D(dir, edge);
                    if (Math::Abs(denom) < Math::EPSILON) continue;

                    f32 t = Cross2D(toOrigin, edge) / denom;
                    f32 u = Cross2D(toOrigin, dir) / denom;

                    if (t >= 0.0f && t < closestDist && u >= 0.0f && u <= 1.0f) {
                        closestDist = t;
                        outHit.entity = entity;
                        outHit.distance = t;
                        outHit.point = origin + dir * t;
                        // Normal perpendicular to edge, pointing outward
                        Math::Vector2 edgeNorm = Math::Vector2(edge.y, -edge.x).Normalized();
                        if (edgeNorm.Dot(dir) > 0.0f) edgeNorm = -edgeNorm;
                        outHit.normal = edgeNorm;
                        found = true;
                    }
                }
                break;
            }
        }
    }

    return found;
}

std::vector<RayHit2D> PhysicsWorld2D::RaycastAll(const Math::Vector2& origin, const Math::Vector2& direction,
                                                   f32 maxDistance, u32 layerMask) const {
    std::vector<RayHit2D> hits;
    if (!m_World) return hits;

    Math::Vector2 dir = direction.Normalized();

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<Body2DComponent>()) {
        auto* body = m_World->GetComponent<Body2DComponent>(entity);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!body || !transform) continue;

        if (!(body->categoryBits & layerMask)) continue;

        // Use single raycast logic per entity
        Math::Vector2 pos = GetPosition2D(*transform);

        switch (body->shapeType) {
            case Shape2DType::Circle: {
                Math::Vector2 center = pos + body->circle.offset;
                Math::Vector2 oc = origin - center;
                f32 r = body->circle.radius;

                f32 a = dir.Dot(dir);
                f32 b = 2.0f * oc.Dot(dir);
                f32 c = oc.Dot(oc) - r * r;
                f32 disc = b * b - 4.0f * a * c;

                if (disc >= 0.0f) {
                    f32 sqrtDisc = Math::Sqrt(disc);
                    f32 t = (-b - sqrtDisc) / (2.0f * a);
                    if (t < 0.0f) t = (-b + sqrtDisc) / (2.0f * a);

                    if (t >= 0.0f && t <= maxDistance) {
                        RayHit2D hit;
                        hit.entity = entity;
                        hit.distance = t;
                        hit.point = origin + dir * t;
                        hit.normal = (hit.point - center).Normalized();
                        hits.push_back(hit);
                    }
                }
                break;
            }
            case Shape2DType::Box: {
                f32 angle = GetRotationZ(*transform) + body->box.rotation;
                Math::Vector2 center = pos + RotateVec(body->box.offset, GetRotationZ(*transform));
                Math::Vector2 localOrigin = InvRotateVec(origin - center, angle);
                Math::Vector2 localDir = InvRotateVec(dir, angle);
                Math::Vector2 he = body->box.halfExtents;

                f32 tmin = 0.0f;
                f32 tmax = maxDistance;

                for (i32 axis = 0; axis < 2; ++axis) {
                    f32 orig = (axis == 0) ? localOrigin.x : localOrigin.y;
                    f32 d = (axis == 0) ? localDir.x : localDir.y;
                    f32 bmin = (axis == 0) ? -he.x : -he.y;
                    f32 bmax = (axis == 0) ? he.x : he.y;

                    if (Math::Abs(d) < 0.0001f) {
                        if (orig < bmin || orig > bmax) { tmin = maxDistance + 1.0f; break; }
                    } else {
                        f32 t1 = (bmin - orig) / d;
                        f32 t2 = (bmax - orig) / d;
                        if (t1 > t2) std::swap(t1, t2);
                        tmin = Math::Max(tmin, t1);
                        tmax = Math::Min(tmax, t2);
                        if (tmin > tmax) break;
                    }
                }

                if (tmin <= tmax && tmin >= 0.0f) {
                    RayHit2D hit;
                    hit.entity = entity;
                    hit.distance = tmin;
                    hit.point = origin + dir * tmin;
                    hit.normal = Math::Vector2(0, 1); // Simplified
                    hits.push_back(hit);
                }
                break;
            }
            default: break;
        }
    }

    // Sort by distance
    std::sort(hits.begin(), hits.end(), [](const RayHit2D& a, const RayHit2D& b) {
        return a.distance < b.distance;
    });

    return hits;
}

bool PhysicsWorld2D::OverlapCircle(const Math::Vector2& center, f32 radius,
                                    std::vector<ECS::Entity>& outEntities, u32 layerMask) const {
    outEntities.clear();
    if (!m_World) return false;

    f32 radiusSq = radius * radius;

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<Body2DComponent>()) {
        auto* body = m_World->GetComponent<Body2DComponent>(entity);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!body || !transform) continue;

        if (!(body->categoryBits & layerMask)) continue;

        Math::Vector2 pos = GetPosition2D(*transform);
        f32 angle = GetRotationZ(*transform);
        AABB2D aabb = ComputeAABB(pos, angle, *body);

        // Quick AABB test against the circle bounding box
        if (aabb.min.x > center.x + radius || aabb.max.x < center.x - radius ||
            aabb.min.y > center.y + radius || aabb.max.y < center.y - radius) {
            continue;
        }

        // More precise test based on shape type
        switch (body->shapeType) {
            case Shape2DType::Circle: {
                Math::Vector2 shapeCenter = pos + RotateVec(body->circle.offset, angle);
                f32 totalRadius = radius + body->circle.radius;
                if ((shapeCenter - center).LengthSquared() <= totalRadius * totalRadius) {
                    outEntities.push_back(entity);
                }
                break;
            }
            case Shape2DType::Box: {
                // Clamp query center to box and check distance
                f32 totalAngle = angle + body->box.rotation;
                Math::Vector2 boxCenter = pos + RotateVec(body->box.offset, angle);
                Math::Vector2 localCenter = InvRotateVec(center - boxCenter, totalAngle);

                Math::Vector2 closest;
                closest.x = Math::Clamp(localCenter.x, -body->box.halfExtents.x, body->box.halfExtents.x);
                closest.y = Math::Clamp(localCenter.y, -body->box.halfExtents.y, body->box.halfExtents.y);

                if ((localCenter - closest).LengthSquared() <= radiusSq) {
                    outEntities.push_back(entity);
                }
                break;
            }
            case Shape2DType::Polygon: {
                // Simple distance test from center to polygon center (conservative)
                Math::Vector2 polyCenter = pos + RotateVec(body->polygon.offset, angle);
                f32 maxVert = 0.0f;
                for (const auto& v : body->polygon.vertices) {
                    maxVert = Math::Max(maxVert, v.Length());
                }
                f32 totalRadius = radius + maxVert;
                if ((polyCenter - center).LengthSquared() <= totalRadius * totalRadius) {
                    outEntities.push_back(entity);
                }
                break;
            }
        }
    }

    return !outEntities.empty();
}

bool PhysicsWorld2D::OverlapBox(const Math::Vector2& center, const Math::Vector2& halfExtents,
                                 std::vector<ECS::Entity>& outEntities, u32 layerMask) const {
    outEntities.clear();
    if (!m_World) return false;

    AABB2D queryAABB;
    queryAABB.min = Math::Vector2(center.x - halfExtents.x, center.y - halfExtents.y);
    queryAABB.max = Math::Vector2(center.x + halfExtents.x, center.y + halfExtents.y);

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<Body2DComponent>()) {
        auto* body = m_World->GetComponent<Body2DComponent>(entity);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!body || !transform) continue;

        if (!(body->categoryBits & layerMask)) continue;

        Math::Vector2 pos = GetPosition2D(*transform);
        f32 angle = GetRotationZ(*transform);
        AABB2D entityAABB = ComputeAABB(pos, angle, *body);

        if (AABBOverlap(queryAABB, entityAABB)) {
            outEntities.push_back(entity);
        }
    }

    return !outEntities.empty();
}

} // namespace Physics
} // namespace Enjin
