#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include <vector>

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

} // namespace Physics
} // namespace Enjin
