#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <memory>
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace Physics {

// Forward declarations
class RigidBody;
class Collider;

// Collision pair result
struct CollisionPair {
    usize bodyA = 0;
    usize bodyB = 0;
    Math::Vector3 normal;       // From A to B
    f32 penetration = 0.0f;
    Math::Vector3 contactPoint;
};

/**
 * @brief Physics World
 * 
 * Simple physics engine for:
 * - Rigid body dynamics
 * - Collision detection
 * - Gravity simulation
 * - Basic constraints
 * 
 * @note Designed to be simple and performant
 */
class ENJIN_API PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    /**
     * @brief Initialize physics world
     */
    bool Initialize();

    /**
     * @brief Shutdown physics world
     */
    void Shutdown();

    /**
     * @brief Set gravity
     */
    void SetGravity(const Math::Vector3& gravity);

    /**
     * @brief Get gravity
     */
    Math::Vector3 GetGravity() const { return m_Gravity; }

    /**
     * @brief Add rigid body to world
     */
    void AddRigidBody(std::shared_ptr<RigidBody> body);

    /**
     * @brief Remove rigid body from world
     */
    void RemoveRigidBody(std::shared_ptr<RigidBody> body);

    /**
     * @brief Step physics simulation
     * @param deltaTime Time step
     */
    void Step(f32 deltaTime);

    /**
     * @brief Get all rigid bodies
     */
    const std::vector<std::shared_ptr<RigidBody>>& GetRigidBodies() const {
        return m_RigidBodies;
    }

    /**
     * @brief Set the ECS world for physics-ECS synchronization
     * @param world Pointer to the ECS world (nullptr to disable sync)
     */
    void SetECSWorld(ECS::World* world) { m_ECSWorld = world; }

    /**
     * @brief Sync ECS components into PhysicsWorld rigid bodies
     *
     * Reads RigidbodyComponent, TransformComponent, and collider components
     * from the ECS world, creating or updating RigidBody objects as needed.
     * Removes bodies for entities that no longer have a RigidbodyComponent.
     */
    void SyncFromECS();

    /**
     * @brief Write PhysicsWorld simulation results back to ECS components
     *
     * Copies RigidBody positions and velocities back into
     * TransformComponent and RigidbodyComponent, applying freeze
     * constraints and linear drag.
     */
    void SyncToECS();

private:
    Math::Vector3 m_Gravity = Math::Vector3(0.0f, -9.81f, 0.0f);
    std::vector<std::shared_ptr<RigidBody>> m_RigidBodies;
    std::vector<CollisionPair> m_CollisionPairs;

    void Integrate(f32 deltaTime);
    void DetectCollisions();
    void ResolveCollisions();

    bool TestSphereSphere(usize a, usize b, CollisionPair& pair);
    bool TestAABBAABB(usize a, usize b, CollisionPair& pair);
    bool TestSphereAABB(usize sphereIdx, usize boxIdx, CollisionPair& pair);

    // ECS integration
    ECS::World* m_ECSWorld = nullptr;
    std::unordered_map<ECS::Entity, std::shared_ptr<RigidBody>> m_EntityBodyMap;
    f32 m_LastDeltaTime = 0.0f;
};

class ENJIN_API RigidBody {
public:
    RigidBody();
    ~RigidBody();

    Math::Vector3 GetPosition() const { return m_Position; }
    void SetPosition(const Math::Vector3& pos) { m_Position = pos; }

    Math::Vector3 GetVelocity() const { return m_Velocity; }
    void SetVelocity(const Math::Vector3& vel) { m_Velocity = vel; }

    f32 GetMass() const { return m_Mass; }
    void SetMass(f32 mass) { m_Mass = mass; }

    f32 GetInverseMass() const { return m_IsStatic ? 0.0f : (m_Mass > 0.0f ? 1.0f / m_Mass : 0.0f); }

    bool IsStatic() const { return m_IsStatic; }
    void SetStatic(bool isStatic) { m_IsStatic = isStatic; }

    f32 GetBoundingRadius() const { return m_BoundingRadius; }
    void SetBoundingRadius(f32 radius) { m_BoundingRadius = radius; }

    Math::Vector3 GetHalfExtents() const { return m_HalfExtents; }
    void SetHalfExtents(const Math::Vector3& half) { m_HalfExtents = half; }

    f32 GetRestitution() const { return m_Restitution; }
    void SetRestitution(f32 e) { m_Restitution = e; }

    f32 GetFriction() const { return m_Friction; }
    void SetFriction(f32 f) { m_Friction = f; }

    enum class Shape : u8 { Sphere, Box };
    Shape GetShape() const { return m_Shape; }
    void SetShape(Shape s) { m_Shape = s; }

private:
    Math::Vector3 m_Position = Math::Vector3(0.0f);
    Math::Vector3 m_Velocity = Math::Vector3(0.0f);
    f32 m_Mass = 1.0f;
    bool m_IsStatic = false;

    Shape m_Shape = Shape::Sphere;
    f32 m_BoundingRadius = 0.5f;
    Math::Vector3 m_HalfExtents = Math::Vector3(0.5f, 0.5f, 0.5f);
    f32 m_Restitution = 0.3f;
    f32 m_Friction = 0.5f;
};

} // namespace Physics
} // namespace Enjin
