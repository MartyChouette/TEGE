#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Spline.h"
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>

namespace Enjin {
namespace AI {

// Forward declarations
class AIAgent;
class AIBehavior;
class Pathfinder;
class Navmesh;

// ============================================================================
// AI State - Base states for AI agents
// ============================================================================

enum class AIState : u8 {
    Idle,           // Standing still, scanning for targets
    Patrol,         // Following a path
    Chase,          // Pursuing a target
    Attack,         // Attacking a target
    Flee,           // Running away from danger
    Investigate,    // Moving to a point of interest
    Return,         // Returning to patrol/spawn point
    Dead,           // No longer active
    Custom          // User-defined state
};

const char* AIStateToString(AIState state);

// ============================================================================
// AITarget - What the AI is tracking
// ============================================================================

struct AITarget {
    Math::Vector3 position;
    Math::Vector3 velocity;
    u64 entityId = 0;
    f32 lastSeenTime = 0.0f;
    bool isVisible = false;
    bool isValid = false;

    void Clear() {
        entityId = 0;
        isVisible = false;
        isValid = false;
    }
};

// ============================================================================
// AIPerception - Sensing the environment
// ============================================================================

struct AIPerceptionSettings {
    // Vision
    f32 sightRange = 15.0f;
    f32 sightAngle = 120.0f;    // Degrees (cone of vision)
    f32 peripheralRange = 5.0f;  // Close range, any angle

    // Hearing
    f32 hearingRange = 10.0f;
    f32 noiseThreshold = 0.5f;   // Minimum noise level to react

    // Memory
    f32 memoryDuration = 5.0f;   // How long to remember last seen position
    f32 investigateTime = 3.0f;  // Time spent investigating
};

// ============================================================================
// AIMovement - Movement capabilities
// ============================================================================

struct AIMovementSettings {
    f32 walkSpeed = 2.0f;
    f32 runSpeed = 5.0f;
    f32 turnSpeed = 180.0f;     // Degrees per second
    f32 acceleration = 10.0f;
    f32 deceleration = 15.0f;

    // Patrol settings
    f32 patrolWaitTime = 2.0f;  // Time to wait at each patrol point
    bool patrolLoop = true;     // Loop patrol or ping-pong

    // Chase settings
    f32 chaseGiveUpDistance = 25.0f;  // Stop chasing if target is this far
    f32 chaseGiveUpTime = 5.0f;       // Stop chasing after losing sight for this long

    // Combat settings
    f32 attackRange = 2.0f;
    f32 preferredDistance = 3.0f;  // Try to maintain this distance
};

// ============================================================================
// AIBehavior - Base class for behaviors
// ============================================================================

class ENJIN_API AIBehavior {
public:
    virtual ~AIBehavior() = default;

    virtual void Enter(AIAgent* agent) = 0;
    virtual void Update(AIAgent* agent, f32 deltaTime) = 0;
    virtual void Exit(AIAgent* agent) = 0;

    virtual AIState GetState() const = 0;
};

// ============================================================================
// Built-in Behaviors
// ============================================================================

// Idle - Stand still, scan for targets
class ENJIN_API IdleBehavior : public AIBehavior {
public:
    void Enter(AIAgent* agent) override;
    void Update(AIAgent* agent, f32 deltaTime) override;
    void Exit(AIAgent* agent) override;
    AIState GetState() const override { return AIState::Idle; }

private:
    f32 m_ScanTimer = 0.0f;
    f32 m_ScanInterval = 1.0f;
};

// Patrol - Follow a path of waypoints
class ENJIN_API PatrolBehavior : public AIBehavior {
public:
    PatrolBehavior() = default;
    PatrolBehavior(const std::vector<Math::Vector3>& waypoints);
    PatrolBehavior(const Math::Spline& spline, f32 speed = 1.0f);

    void SetWaypoints(const std::vector<Math::Vector3>& waypoints);
    void SetSpline(const Math::Spline& spline);

    void Enter(AIAgent* agent) override;
    void Update(AIAgent* agent, f32 deltaTime) override;
    void Exit(AIAgent* agent) override;
    AIState GetState() const override { return AIState::Patrol; }

private:
    enum class PatrolMode { Waypoints, Spline };
    PatrolMode m_Mode = PatrolMode::Waypoints;

    std::vector<Math::Vector3> m_Waypoints;
    usize m_CurrentWaypoint = 0;
    bool m_MovingForward = true;  // For ping-pong mode

    Math::Spline m_Spline;
    f32 m_SplineT = 0.0f;
    f32 m_SplineSpeed = 1.0f;

    f32 m_WaitTimer = 0.0f;
    bool m_IsWaiting = false;
};

// Chase - Pursue a target
class ENJIN_API ChaseBehavior : public AIBehavior {
public:
    void Enter(AIAgent* agent) override;
    void Update(AIAgent* agent, f32 deltaTime) override;
    void Exit(AIAgent* agent) override;
    AIState GetState() const override { return AIState::Chase; }

private:
    f32 m_LostSightTimer = 0.0f;
    Math::Vector3 m_LastKnownPosition;
};

// Attack - Attack the target
class ENJIN_API AttackBehavior : public AIBehavior {
public:
    void Enter(AIAgent* agent) override;
    void Update(AIAgent* agent, f32 deltaTime) override;
    void Exit(AIAgent* agent) override;
    AIState GetState() const override { return AIState::Attack; }

private:
    f32 m_AttackCooldown = 0.0f;
    f32 m_AttackWindup = 0.0f;
    bool m_IsAttacking = false;
};

// Flee - Run away from danger
class ENJIN_API FleeBehavior : public AIBehavior {
public:
    void Enter(AIAgent* agent) override;
    void Update(AIAgent* agent, f32 deltaTime) override;
    void Exit(AIAgent* agent) override;
    AIState GetState() const override { return AIState::Flee; }

private:
    f32 m_FleeTimer = 0.0f;
    Math::Vector3 m_FleeDirection;
};

// Investigate - Check out a suspicious location
class ENJIN_API InvestigateBehavior : public AIBehavior {
public:
    void SetInvestigatePosition(const Math::Vector3& position);

    void Enter(AIAgent* agent) override;
    void Update(AIAgent* agent, f32 deltaTime) override;
    void Exit(AIAgent* agent) override;
    AIState GetState() const override { return AIState::Investigate; }

private:
    Math::Vector3 m_InvestigatePosition;
    f32 m_InvestigateTimer = 0.0f;
    bool m_ReachedPosition = false;
};

// Return - Go back to spawn/patrol start
class ENJIN_API ReturnBehavior : public AIBehavior {
public:
    void Enter(AIAgent* agent) override;
    void Update(AIAgent* agent, f32 deltaTime) override;
    void Exit(AIAgent* agent) override;
    AIState GetState() const override { return AIState::Return; }
};

// ============================================================================
// AIAgent - The AI controller
// ============================================================================

class ENJIN_API AIAgent {
public:
    AIAgent();
    ~AIAgent();

    // Setup
    void Initialize(u64 entityId);
    void SetSpawnPosition(const Math::Vector3& position);
    void SetPerceptionSettings(const AIPerceptionSettings& settings) { m_Perception = settings; }
    void SetMovementSettings(const AIMovementSettings& settings) { m_Movement = settings; }

    // Behavior management
    void SetBehavior(AIState state, std::unique_ptr<AIBehavior> behavior);
    void TransitionTo(AIState state);
    AIState GetCurrentState() const { return m_CurrentState; }
    AIBehavior* GetCurrentBehavior() const { return m_CurrentBehavior; }

    // Update
    void Update(f32 deltaTime);

    // Position and movement
    Math::Vector3 GetPosition() const { return m_Position; }
    void SetPosition(const Math::Vector3& position) { m_Position = position; }
    Math::Vector3 GetForward() const { return m_Forward; }
    void SetForward(const Math::Vector3& forward) { m_Forward = forward.Normalized(); }
    Math::Vector3 GetVelocity() const { return m_Velocity; }

    Math::Vector3 GetSpawnPosition() const { return m_SpawnPosition; }

    // Movement commands
    void MoveTo(const Math::Vector3& target);
    void MoveInDirection(const Math::Vector3& direction, f32 speed);
    void Stop();
    void LookAt(const Math::Vector3& target);
    bool IsMoving() const { return m_IsMoving; }
    bool HasReachedDestination() const;

    // Target management
    void SetTarget(const AITarget& target) { m_Target = target; }
    const AITarget& GetTarget() const { return m_Target; }
    void ClearTarget() { m_Target.Clear(); }
    bool HasTarget() const { return m_Target.isValid; }

    // Perception queries (call from game code with actual world data)
    bool CanSeeTarget() const;
    bool CanHearNoise(const Math::Vector3& noisePosition, f32 noiseLevel) const;
    f32 GetDistanceToTarget() const;

    // Events/callbacks
    using StateChangeCallback = std::function<void(AIState oldState, AIState newState)>;
    using AttackCallback = std::function<void()>;
    using DamageCallback = std::function<void(f32 damage)>;

    void SetOnStateChange(StateChangeCallback callback) { m_OnStateChange = callback; }
    void SetOnAttack(AttackCallback callback) { m_OnAttack = callback; }
    void SetOnDamage(DamageCallback callback) { m_OnDamage = callback; }

    // Trigger callbacks (call from behaviors)
    void TriggerAttack() { if (m_OnAttack) m_OnAttack(); }
    void TriggerDamage(f32 damage) { if (m_OnDamage) m_OnDamage(damage); }

    // Access settings
    const AIPerceptionSettings& GetPerception() const { return m_Perception; }
    const AIMovementSettings& GetMovement() const { return m_Movement; }
    AIMovementSettings& GetMovementMutable() { return m_Movement; }

    // Entity ID
    u64 GetEntityId() const { return m_EntityId; }

    // Navmesh pathfinding integration
    void SetPathfinder(class Pathfinder* pathfinder) { m_Pathfinder = pathfinder; }
    Pathfinder* GetPathfinder() const { return m_Pathfinder; }
    void SetNavmesh(const class Navmesh* navmesh) { m_Navmesh = navmesh; }
    const Navmesh* GetNavmesh() const { return m_Navmesh; }
    bool HasNavmesh() const { return m_Navmesh != nullptr && m_Pathfinder != nullptr; }

    /// Move to target using navmesh A* pathfinding when available, direct otherwise.
    void MoveToNav(const Math::Vector3& target);

    /// Check if currently following a navmesh path
    bool IsFollowingNavPath() const { return m_HasNavPath; }

    /// Get current navmesh path (for debug drawing)
    const std::vector<Math::Vector3>& GetCurrentNavPath() const { return m_NavPathWaypoints; }
    u32 GetCurrentNavWaypointIndex() const { return m_NavPathIndex; }

private:
    void UpdateMovement(f32 deltaTime);
    void UpdateNavPathMovement(f32 deltaTime);

    u64 m_EntityId = 0;
    Math::Vector3 m_Position;
    Math::Vector3 m_Forward = Math::Vector3(0, 0, -1);
    Math::Vector3 m_Velocity;
    Math::Vector3 m_SpawnPosition;

    // Movement
    Math::Vector3 m_MoveTarget;
    bool m_IsMoving = false;
    f32 m_CurrentSpeed = 0.0f;

    // Navmesh path following
    Pathfinder* m_Pathfinder = nullptr;
    const Navmesh* m_Navmesh = nullptr;
    std::vector<Math::Vector3> m_NavPathWaypoints;
    u32 m_NavPathIndex = 0;
    bool m_HasNavPath = false;
    f32 m_NavArrivalRadius = 0.5f;

    // Target
    AITarget m_Target;

    // Settings
    AIPerceptionSettings m_Perception;
    AIMovementSettings m_Movement;

    // State machine
    AIState m_CurrentState = AIState::Idle;
    AIBehavior* m_CurrentBehavior = nullptr;
    std::unordered_map<AIState, std::unique_ptr<AIBehavior>> m_Behaviors;

    // Callbacks
    StateChangeCallback m_OnStateChange;
    AttackCallback m_OnAttack;
    DamageCallback m_OnDamage;
};

// ============================================================================
// Preset AI Configurations
// ============================================================================

namespace AIPresets {
    // Basic enemy that patrols and chases player
    ENJIN_API void SetupBasicEnemy(AIAgent& agent, const std::vector<Math::Vector3>& patrolPoints);

    // Aggressive enemy that charges at player
    ENJIN_API void SetupAggressive(AIAgent& agent);

    // Cowardly enemy that flees when player gets close
    ENJIN_API void SetupCoward(AIAgent& agent, f32 fleeDistance = 8.0f);

    // Ranged enemy that maintains distance
    ENJIN_API void SetupRanged(AIAgent& agent, f32 preferredDistance = 10.0f);

    // Guard that stays in one spot but attacks if approached
    ENJIN_API void SetupGuard(AIAgent& agent, f32 alertRange = 5.0f);

    // Turret - doesn't move, just rotates and attacks
    ENJIN_API void SetupTurret(AIAgent& agent, f32 range = 20.0f);
}

} // namespace AI
} // namespace Enjin
