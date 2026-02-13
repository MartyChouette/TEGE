#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace Enjin {
namespace Physics {

// 2D shape types
enum class Shape2DType : u8 {
    Circle,
    Box,
    Polygon
};

// Physics material
struct PhysicsMaterial2D {
    f32 friction = 0.3f;
    f32 restitution = 0.2f;     // Bounciness (0-1)
    f32 density = 1.0f;
};

// 2D shapes
struct CircleShape2D {
    f32 radius = 0.5f;
    Math::Vector2 offset;       // Local offset from entity center
};

struct BoxShape2D {
    Math::Vector2 halfExtents = Math::Vector2(0.5f, 0.5f);
    Math::Vector2 offset;
    f32 rotation = 0.0f;        // Local rotation in radians
};

struct PolygonShape2D {
    std::vector<Math::Vector2> vertices;  // CCW winding, max 8 vertices
    Math::Vector2 offset;
};

// 2D body component (add alongside existing RigidbodyComponent for 2D mode)
struct ENJIN_API Body2DComponent {
    Shape2DType shapeType = Shape2DType::Box;
    CircleShape2D circle;
    BoxShape2D box;
    PolygonShape2D polygon;

    PhysicsMaterial2D material;

    // Body properties
    bool isStatic = false;
    bool isSensor = false;          // Triggers callbacks but no physical response
    bool fixedRotation = false;
    f32 gravityScale = 1.0f;
    f32 linearDamping = 0.1f;
    f32 angularDamping = 0.1f;

    // Runtime state
    Math::Vector2 velocity;
    f32 angularVelocity = 0.0f;
    f32 mass = 1.0f;
    f32 inverseMass = 1.0f;
    f32 inertia = 1.0f;
    f32 inverseInertia = 1.0f;

    // Collision filtering (reuse 3D bitmask system)
    u32 categoryBits = 1;
    u32 collisionMask = 0xFFFFFFFF;
};

// 2D joint types
enum class Joint2DType : u8 {
    Revolute,       // Hinge/pin joint
    Prismatic,      // Slider joint
    Distance,       // Fixed distance spring
    Rope,           // Max distance constraint
    Weld            // Rigid attachment
};

struct ENJIN_API Joint2DComponent {
    Joint2DType type = Joint2DType::Revolute;
    ECS::Entity connectedEntity = 0;

    Math::Vector2 anchorA;      // Local anchor on this entity
    Math::Vector2 anchorB;      // Local anchor on connected entity

    // Revolute joint
    bool enableLimit = false;
    f32 lowerAngle = 0.0f;     // Radians
    f32 upperAngle = 0.0f;
    bool enableMotor = false;
    f32 motorSpeed = 0.0f;
    f32 maxMotorTorque = 0.0f;

    // Prismatic joint
    Math::Vector2 axis = Math::Vector2(1, 0);
    f32 lowerTranslation = 0.0f;
    f32 upperTranslation = 0.0f;

    // Distance/Rope joint
    f32 length = 1.0f;
    f32 minLength = 0.0f;
    f32 maxLength = 0.0f;
    f32 stiffness = 0.0f;      // 0 = rigid
    f32 damping = 0.0f;

    bool collideConnected = false;
};

// Contact info for collision callbacks
struct Contact2D {
    ECS::Entity entityA;
    ECS::Entity entityB;
    Math::Vector2 point;
    Math::Vector2 normal;
    f32 penetration = 0.0f;
};

// 2D raycast hit
struct RayHit2D {
    ECS::Entity entity;
    Math::Vector2 point;
    Math::Vector2 normal;
    f32 distance = 0.0f;
};

// Collision manifold (internal)
struct Manifold2D {
    ECS::Entity entityA;
    ECS::Entity entityB;
    Math::Vector2 normal;
    f32 penetration = 0.0f;
    Math::Vector2 contactPoints[2];
    u32 contactCount = 0;
};

// Main 2D physics world
class ENJIN_API PhysicsWorld2D {
public:
    PhysicsWorld2D();
    ~PhysicsWorld2D();

    void Initialize(ECS::World* world);
    void Update(f32 deltaTime);
    void Shutdown();

    // Gravity
    void SetGravity(const Math::Vector2& gravity);
    Math::Vector2 GetGravity() const { return m_Gravity; }

    // Iteration settings
    void SetVelocityIterations(u32 iterations) { m_VelocityIterations = iterations; }
    void SetPositionIterations(u32 iterations) { m_PositionIterations = iterations; }

    // Queries
    bool Raycast(const Math::Vector2& origin, const Math::Vector2& direction,
                 f32 maxDistance, RayHit2D& outHit, u32 layerMask = 0xFFFFFFFF) const;
    std::vector<RayHit2D> RaycastAll(const Math::Vector2& origin, const Math::Vector2& direction,
                                      f32 maxDistance, u32 layerMask = 0xFFFFFFFF) const;
    bool OverlapCircle(const Math::Vector2& center, f32 radius,
                       std::vector<ECS::Entity>& outEntities, u32 layerMask = 0xFFFFFFFF) const;
    bool OverlapBox(const Math::Vector2& center, const Math::Vector2& halfExtents,
                    std::vector<ECS::Entity>& outEntities, u32 layerMask = 0xFFFFFFFF) const;

    // Callbacks
    using CollisionCallback = std::function<void(const Contact2D&)>;
    void SetOnCollisionEnter(CollisionCallback cb) { m_OnCollisionEnter = cb; }
    void SetOnCollisionExit(CollisionCallback cb) { m_OnCollisionExit = cb; }
    void SetOnSensorEnter(CollisionCallback cb) { m_OnSensorEnter = cb; }
    void SetOnSensorExit(CollisionCallback cb) { m_OnSensorExit = cb; }

    // CCD
    void SetCCDEnabled(bool enabled) { m_CCDEnabled = enabled; }

private:
    // Physics pipeline
    void BroadPhase(std::vector<std::pair<ECS::Entity, ECS::Entity>>& pairs);
    void NarrowPhase(const std::vector<std::pair<ECS::Entity, ECS::Entity>>& pairs,
                     std::vector<Manifold2D>& manifolds);
    void ResolveCollisions(std::vector<Manifold2D>& manifolds, f32 dt);
    void IntegrateVelocities(f32 dt);
    void IntegratePositions(f32 dt);
    void SolveJoints(f32 dt);
    void PerformCCD(f32 dt);

    // Shape vs shape collision detection
    bool TestCircleCircle(const Math::Vector2& posA, const CircleShape2D& a,
                          const Math::Vector2& posB, const CircleShape2D& b,
                          Manifold2D& manifold) const;
    bool TestCircleBox(const Math::Vector2& posA, const CircleShape2D& circle,
                       const Math::Vector2& posB, const BoxShape2D& box, f32 boxAngle,
                       Manifold2D& manifold) const;
    bool TestBoxBox(const Math::Vector2& posA, const BoxShape2D& a, f32 angleA,
                    const Math::Vector2& posB, const BoxShape2D& b, f32 angleB,
                    Manifold2D& manifold) const;
    bool TestCirclePolygon(const Math::Vector2& posA, const CircleShape2D& circle,
                           const Math::Vector2& posB, const PolygonShape2D& poly, f32 polyAngle,
                           Manifold2D& manifold) const;

    ECS::World* m_World = nullptr;
    Math::Vector2 m_Gravity = Math::Vector2(0.0f, -9.81f);
    u32 m_VelocityIterations = 8;
    u32 m_PositionIterations = 3;
    bool m_CCDEnabled = false;

    // Reusable per-frame buffers (avoid heap allocation every frame)
    std::vector<std::pair<ECS::Entity, ECS::Entity>> m_CachedPairs;
    std::vector<Manifold2D> m_CachedManifolds;

    // Contact tracking for enter/exit callbacks
    std::unordered_set<u64> m_ActiveContacts;  // Pack entity pair into u64
    std::unordered_set<u64> m_NewContactsCache;  // Reused per-frame to avoid alloc

    CollisionCallback m_OnCollisionEnter;
    CollisionCallback m_OnCollisionExit;
    CollisionCallback m_OnSensorEnter;
    CollisionCallback m_OnSensorExit;
};

} // namespace Physics
} // namespace Enjin
