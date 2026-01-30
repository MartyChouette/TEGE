#include "Enjin/AI/AIBehaviors.h"
#include "Enjin/Math/Math.h"
#include <memory>

namespace Enjin {
namespace AI {

const char* AIStateToString(AIState state) {
    switch (state) {
        case AIState::Idle: return "Idle";
        case AIState::Patrol: return "Patrol";
        case AIState::Chase: return "Chase";
        case AIState::Attack: return "Attack";
        case AIState::Flee: return "Flee";
        case AIState::Investigate: return "Investigate";
        case AIState::Return: return "Return";
        case AIState::Dead: return "Dead";
        case AIState::Custom: return "Custom";
        default: return "Unknown";
    }
}

// ============================================================================
// IdleBehavior
// ============================================================================

void IdleBehavior::Enter(AIAgent* agent) {
    agent->Stop();
    m_ScanTimer = 0.0f;
}

void IdleBehavior::Update(AIAgent* agent, f32 deltaTime) {
    m_ScanTimer += deltaTime;

    // Periodically scan for targets
    if (m_ScanTimer >= m_ScanInterval) {
        m_ScanTimer = 0.0f;
        // If we can see the target, transition to chase
        if (agent->HasTarget() && agent->CanSeeTarget()) {
            agent->TransitionTo(AIState::Chase);
        }
    }
}

void IdleBehavior::Exit(AIAgent* /*agent*/) {
}

// ============================================================================
// PatrolBehavior
// ============================================================================

PatrolBehavior::PatrolBehavior(const std::vector<Math::Vector3>& waypoints)
    : m_Mode(PatrolMode::Waypoints), m_Waypoints(waypoints) {
}

PatrolBehavior::PatrolBehavior(const Math::Spline& spline, f32 speed)
    : m_Mode(PatrolMode::Spline), m_Spline(spline), m_SplineSpeed(speed) {
}

void PatrolBehavior::SetWaypoints(const std::vector<Math::Vector3>& waypoints) {
    m_Mode = PatrolMode::Waypoints;
    m_Waypoints = waypoints;
    m_CurrentWaypoint = 0;
}

void PatrolBehavior::SetSpline(const Math::Spline& spline) {
    m_Mode = PatrolMode::Spline;
    m_Spline = spline;
    m_SplineT = 0.0f;
}

void PatrolBehavior::Enter(AIAgent* agent) {
    m_IsWaiting = false;
    m_WaitTimer = 0.0f;

    if (m_Mode == PatrolMode::Waypoints && !m_Waypoints.empty()) {
        agent->MoveTo(m_Waypoints[m_CurrentWaypoint]);
    } else if (m_Mode == PatrolMode::Spline) {
        m_SplineT = 0.0f;
    }
}

void PatrolBehavior::Update(AIAgent* agent, f32 deltaTime) {
    // Check for target - interrupt patrol if we see something
    if (agent->HasTarget() && agent->CanSeeTarget()) {
        agent->TransitionTo(AIState::Chase);
        return;
    }

    if (m_Mode == PatrolMode::Waypoints) {
        if (m_Waypoints.empty()) return;

        if (m_IsWaiting) {
            m_WaitTimer += deltaTime;
            if (m_WaitTimer >= agent->GetMovement().patrolWaitTime) {
                m_IsWaiting = false;

                // Move to next waypoint
                if (agent->GetMovement().patrolLoop) {
                    m_CurrentWaypoint = (m_CurrentWaypoint + 1) % m_Waypoints.size();
                } else {
                    // Ping-pong
                    if (m_MovingForward) {
                        if (m_CurrentWaypoint < m_Waypoints.size() - 1) {
                            m_CurrentWaypoint++;
                        } else {
                            m_MovingForward = false;
                            m_CurrentWaypoint--;
                        }
                    } else {
                        if (m_CurrentWaypoint > 0) {
                            m_CurrentWaypoint--;
                        } else {
                            m_MovingForward = true;
                            m_CurrentWaypoint++;
                        }
                    }
                }

                agent->MoveTo(m_Waypoints[m_CurrentWaypoint]);
            }
        } else if (agent->HasReachedDestination()) {
            m_IsWaiting = true;
            m_WaitTimer = 0.0f;
            agent->Stop();
        }
    } else {
        // Spline patrol
        f32 totalLength = m_Spline.GetTotalLength();
        if (totalLength > 0.001f) {
            m_SplineT += (m_SplineSpeed * deltaTime) / totalLength;

            if (m_Spline.IsClosed()) {
                while (m_SplineT >= 1.0f) m_SplineT -= 1.0f;
            } else {
                if (m_SplineT >= 1.0f) {
                    m_SplineT = 1.0f;
                    m_SplineSpeed = -m_SplineSpeed; // Reverse
                } else if (m_SplineT <= 0.0f) {
                    m_SplineT = 0.0f;
                    m_SplineSpeed = -m_SplineSpeed;
                }
            }

            Math::Vector3 targetPos = m_Spline.Evaluate(Math::Abs(m_SplineT));
            agent->SetPosition(targetPos);
            agent->SetForward(m_Spline.EvaluateTangent(Math::Abs(m_SplineT)));
        }
    }
}

void PatrolBehavior::Exit(AIAgent* /*agent*/) {
}

// ============================================================================
// ChaseBehavior
// ============================================================================

void ChaseBehavior::Enter(AIAgent* agent) {
    m_LostSightTimer = 0.0f;
    if (agent->HasTarget()) {
        m_LastKnownPosition = agent->GetTarget().position;
        agent->MoveTo(m_LastKnownPosition);
    }
}

void ChaseBehavior::Update(AIAgent* agent, f32 deltaTime) {
    if (!agent->HasTarget()) {
        agent->TransitionTo(AIState::Return);
        return;
    }

    const AITarget& target = agent->GetTarget();
    f32 distance = agent->GetDistanceToTarget();

    // Check if close enough to attack
    if (distance <= agent->GetMovement().attackRange) {
        agent->TransitionTo(AIState::Attack);
        return;
    }

    if (agent->CanSeeTarget()) {
        m_LostSightTimer = 0.0f;
        m_LastKnownPosition = target.position;
        agent->MoveTo(m_LastKnownPosition);
    } else {
        m_LostSightTimer += deltaTime;

        // Give up chase after losing sight for too long
        if (m_LostSightTimer >= agent->GetMovement().chaseGiveUpTime) {
            agent->TransitionTo(AIState::Investigate);
            return;
        }

        // Continue moving to last known position
        if (!agent->IsMoving() || agent->HasReachedDestination()) {
            // Lost the target, investigate last known position
            agent->TransitionTo(AIState::Investigate);
        }
    }

    // Give up if target is too far
    if (distance > agent->GetMovement().chaseGiveUpDistance) {
        agent->TransitionTo(AIState::Return);
    }
}

void ChaseBehavior::Exit(AIAgent* /*agent*/) {
}

// ============================================================================
// AttackBehavior
// ============================================================================

void AttackBehavior::Enter(AIAgent* agent) {
    agent->Stop();
    m_AttackCooldown = 0.0f;
    m_AttackWindup = 0.0f;
    m_IsAttacking = false;
}

void AttackBehavior::Update(AIAgent* agent, f32 deltaTime) {
    if (!agent->HasTarget()) {
        agent->TransitionTo(AIState::Idle);
        return;
    }

    f32 distance = agent->GetDistanceToTarget();

    // Face the target
    agent->LookAt(agent->GetTarget().position);

    // If target moved out of range, chase them
    if (distance > agent->GetMovement().attackRange * 1.5f) {
        agent->TransitionTo(AIState::Chase);
        return;
    }

    // Attack logic
    if (m_AttackCooldown > 0.0f) {
        m_AttackCooldown -= deltaTime;
    } else if (!m_IsAttacking) {
        // Start attack windup
        m_IsAttacking = true;
        m_AttackWindup = 0.3f; // Windup time
    } else {
        m_AttackWindup -= deltaTime;
        if (m_AttackWindup <= 0.0f) {
            // Execute attack
            agent->TriggerAttack();
            m_IsAttacking = false;
            m_AttackCooldown = 1.0f; // Cooldown between attacks
        }
    }
}

void AttackBehavior::Exit(AIAgent* /*agent*/) {
}

// ============================================================================
// FleeBehavior
// ============================================================================

void FleeBehavior::Enter(AIAgent* agent) {
    m_FleeTimer = 0.0f;

    if (agent->HasTarget()) {
        // Run opposite direction from target
        m_FleeDirection = (agent->GetPosition() - agent->GetTarget().position).Normalized();
    } else {
        // Random direction
        m_FleeDirection = Math::Vector3(
            Math::Random(-1.0f, 1.0f),
            0.0f,
            Math::Random(-1.0f, 1.0f)
        ).Normalized();
    }
}

void FleeBehavior::Update(AIAgent* agent, f32 deltaTime) {
    m_FleeTimer += deltaTime;

    // Run at full speed
    agent->MoveInDirection(m_FleeDirection, agent->GetMovement().runSpeed);

    // Update flee direction if target is still visible
    if (agent->HasTarget() && agent->CanSeeTarget()) {
        m_FleeDirection = (agent->GetPosition() - agent->GetTarget().position).Normalized();
        m_FleeTimer = 0.0f; // Reset timer while target visible
    }

    // Stop fleeing after some time
    if (m_FleeTimer >= 3.0f) {
        agent->TransitionTo(AIState::Idle);
    }
}

void FleeBehavior::Exit(AIAgent* agent) {
    agent->Stop();
}

// ============================================================================
// InvestigateBehavior
// ============================================================================

void InvestigateBehavior::SetInvestigatePosition(const Math::Vector3& position) {
    m_InvestigatePosition = position;
}

void InvestigateBehavior::Enter(AIAgent* agent) {
    m_InvestigateTimer = 0.0f;
    m_ReachedPosition = false;

    // Use target's last known position if available
    if (agent->HasTarget()) {
        m_InvestigatePosition = agent->GetTarget().position;
    }

    agent->MoveTo(m_InvestigatePosition);
}

void InvestigateBehavior::Update(AIAgent* agent, f32 deltaTime) {
    // If we see the target, chase!
    if (agent->HasTarget() && agent->CanSeeTarget()) {
        agent->TransitionTo(AIState::Chase);
        return;
    }

    if (!m_ReachedPosition) {
        if (agent->HasReachedDestination()) {
            m_ReachedPosition = true;
            agent->Stop();
        }
    } else {
        m_InvestigateTimer += deltaTime;

        // Look around (could add rotation logic here)

        if (m_InvestigateTimer >= agent->GetPerception().investigateTime) {
            // Nothing found, return to patrol
            agent->TransitionTo(AIState::Return);
        }
    }
}

void InvestigateBehavior::Exit(AIAgent* /*agent*/) {
}

// ============================================================================
// ReturnBehavior
// ============================================================================

void ReturnBehavior::Enter(AIAgent* agent) {
    agent->MoveTo(agent->GetSpawnPosition());
}

void ReturnBehavior::Update(AIAgent* agent, f32 deltaTime) {
    (void)deltaTime;

    // Check for target on the way back
    if (agent->HasTarget() && agent->CanSeeTarget()) {
        agent->TransitionTo(AIState::Chase);
        return;
    }

    if (agent->HasReachedDestination()) {
        agent->TransitionTo(AIState::Patrol);
    }
}

void ReturnBehavior::Exit(AIAgent* /*agent*/) {
}

// ============================================================================
// AIAgent
// ============================================================================

AIAgent::AIAgent() {
    // Set up default behaviors
    m_Behaviors[AIState::Idle] = std::make_unique<IdleBehavior>();
    m_Behaviors[AIState::Patrol] = std::make_unique<PatrolBehavior>();
    m_Behaviors[AIState::Chase] = std::make_unique<ChaseBehavior>();
    m_Behaviors[AIState::Attack] = std::make_unique<AttackBehavior>();
    m_Behaviors[AIState::Flee] = std::make_unique<FleeBehavior>();
    m_Behaviors[AIState::Investigate] = std::make_unique<InvestigateBehavior>();
    m_Behaviors[AIState::Return] = std::make_unique<ReturnBehavior>();

    m_CurrentBehavior = m_Behaviors[AIState::Idle].get();
}

AIAgent::~AIAgent() = default;

void AIAgent::Initialize(u64 entityId) {
    m_EntityId = entityId;
}

void AIAgent::SetSpawnPosition(const Math::Vector3& position) {
    m_SpawnPosition = position;
    m_Position = position;
}

void AIAgent::SetBehavior(AIState state, std::unique_ptr<AIBehavior> behavior) {
    m_Behaviors[state] = std::move(behavior);
}

void AIAgent::TransitionTo(AIState state) {
    if (state == m_CurrentState) return;

    auto it = m_Behaviors.find(state);
    if (it == m_Behaviors.end()) return;

    AIState oldState = m_CurrentState;

    if (m_CurrentBehavior) {
        m_CurrentBehavior->Exit(this);
    }

    m_CurrentState = state;
    m_CurrentBehavior = it->second.get();

    if (m_CurrentBehavior) {
        m_CurrentBehavior->Enter(this);
    }

    if (m_OnStateChange) {
        m_OnStateChange(oldState, state);
    }
}

void AIAgent::Update(f32 deltaTime) {
    // Update target tracking
    if (m_Target.isValid && !m_Target.isVisible) {
        m_Target.lastSeenTime += deltaTime;
        if (m_Target.lastSeenTime > m_Perception.memoryDuration) {
            m_Target.Clear();
        }
    }

    // Update current behavior
    if (m_CurrentBehavior) {
        m_CurrentBehavior->Update(this, deltaTime);
    }

    // Update movement
    UpdateMovement(deltaTime);
}

void AIAgent::MoveTo(const Math::Vector3& target) {
    m_MoveTarget = target;
    m_IsMoving = true;
}

void AIAgent::MoveInDirection(const Math::Vector3& direction, f32 speed) {
    m_MoveTarget = m_Position + direction * 100.0f; // Far ahead
    m_IsMoving = true;
    m_CurrentSpeed = speed;
}

void AIAgent::Stop() {
    m_IsMoving = false;
    m_Velocity = Math::Vector3(0, 0, 0);
    m_CurrentSpeed = 0.0f;
}

void AIAgent::LookAt(const Math::Vector3& target) {
    Math::Vector3 dir = target - m_Position;
    dir.y = 0; // Keep horizontal
    if (dir.LengthSquared() > 0.001f) {
        m_Forward = dir.Normalized();
    }
}

bool AIAgent::HasReachedDestination() const {
    if (!m_IsMoving) return true;
    Math::Vector3 diff = m_MoveTarget - m_Position;
    diff.y = 0; // Ignore height
    return diff.LengthSquared() < 0.5f * 0.5f;
}

bool AIAgent::CanSeeTarget() const {
    if (!m_Target.isValid) return false;

    Math::Vector3 toTarget = m_Target.position - m_Position;
    f32 distance = toTarget.Length();

    // Close range - peripheral vision
    if (distance <= m_Perception.peripheralRange) {
        return true;
    }

    // Out of sight range
    if (distance > m_Perception.sightRange) {
        return false;
    }

    // Check angle
    Math::Vector3 toTargetNorm = toTarget.Normalized();
    f32 dot = m_Forward.Dot(toTargetNorm);
    f32 angle = Math::Degrees(Math::Acos(Math::Clamp(dot, -1.0f, 1.0f)));

    return angle <= m_Perception.sightAngle * 0.5f;
}

bool AIAgent::CanHearNoise(const Math::Vector3& noisePosition, f32 noiseLevel) const {
    if (noiseLevel < m_Perception.noiseThreshold) {
        return false;
    }

    f32 distance = (noisePosition - m_Position).Length();
    return distance <= m_Perception.hearingRange * noiseLevel;
}

f32 AIAgent::GetDistanceToTarget() const {
    if (!m_Target.isValid) return 999999.0f;
    return (m_Target.position - m_Position).Length();
}

void AIAgent::UpdateMovement(f32 deltaTime) {
    if (!m_IsMoving) {
        // Decelerate
        f32 speed = m_Velocity.Length();
        if (speed > 0.001f) {
            f32 decel = m_Movement.deceleration * deltaTime;
            if (decel >= speed) {
                m_Velocity = Math::Vector3(0, 0, 0);
            } else {
                m_Velocity = m_Velocity * ((speed - decel) / speed);
            }
        }
        return;
    }

    Math::Vector3 toTarget = m_MoveTarget - m_Position;
    toTarget.y = 0; // Keep on ground plane
    f32 distToTarget = toTarget.Length();

    if (distToTarget < 0.1f) {
        m_IsMoving = false;
        return;
    }

    Math::Vector3 desiredDir = toTarget.Normalized();

    // Turn towards target
    f32 currentAngle = Math::Atan2(m_Forward.x, m_Forward.z);
    f32 targetAngle = Math::Atan2(desiredDir.x, desiredDir.z);
    f32 angleDiff = targetAngle - currentAngle;

    // Normalize angle difference
    while (angleDiff > Math::PI) angleDiff -= 2.0f * Math::PI;
    while (angleDiff < -Math::PI) angleDiff += 2.0f * Math::PI;

    f32 maxTurn = Math::Radians(m_Movement.turnSpeed) * deltaTime;
    f32 turn = Math::Clamp(angleDiff, -maxTurn, maxTurn);
    f32 newAngle = currentAngle + turn;

    m_Forward = Math::Vector3(Math::Sin(newAngle), 0, Math::Cos(newAngle));

    // Accelerate towards target
    f32 targetSpeed = m_Movement.walkSpeed;
    if (m_CurrentState == AIState::Chase || m_CurrentState == AIState::Flee) {
        targetSpeed = m_Movement.runSpeed;
    }

    f32 currentSpeed = m_Velocity.Length();
    f32 speedDiff = targetSpeed - currentSpeed;
    f32 accel = (speedDiff > 0 ? m_Movement.acceleration : m_Movement.deceleration) * deltaTime;

    if (Math::Abs(speedDiff) <= accel) {
        currentSpeed = targetSpeed;
    } else {
        currentSpeed += (speedDiff > 0 ? accel : -accel);
    }

    m_Velocity = m_Forward * currentSpeed;
    m_Position = m_Position + m_Velocity * deltaTime;
}

// ============================================================================
// AIPresets
// ============================================================================

namespace AIPresets {

void SetupBasicEnemy(AIAgent& agent, const std::vector<Math::Vector3>& patrolPoints) {
    auto patrol = std::make_unique<PatrolBehavior>(patrolPoints);
    agent.SetBehavior(AIState::Patrol, std::move(patrol));
    agent.TransitionTo(AIState::Patrol);
}

void SetupAggressive(AIAgent& agent) {
    AIMovementSettings movement = agent.GetMovement();
    movement.runSpeed = 7.0f;
    movement.attackRange = 2.5f;
    movement.chaseGiveUpDistance = 50.0f;
    movement.chaseGiveUpTime = 10.0f;
    agent.SetMovementSettings(movement);

    AIPerceptionSettings perception = agent.GetPerception();
    perception.sightRange = 25.0f;
    perception.sightAngle = 180.0f;
    agent.SetPerceptionSettings(perception);
}

void SetupCoward(AIAgent& agent, f32 fleeDistance) {
    AIPerceptionSettings perception = agent.GetPerception();
    perception.sightRange = fleeDistance;
    perception.peripheralRange = fleeDistance * 0.5f;
    agent.SetPerceptionSettings(perception);

    // Override idle to flee when target seen
    // (In practice you'd create a custom behavior for this)
}

void SetupRanged(AIAgent& agent, f32 preferredDistance) {
    AIMovementSettings movement = agent.GetMovement();
    movement.attackRange = preferredDistance;
    movement.preferredDistance = preferredDistance;
    agent.SetMovementSettings(movement);

    AIPerceptionSettings perception = agent.GetPerception();
    perception.sightRange = preferredDistance * 2.0f;
    agent.SetPerceptionSettings(perception);
}

void SetupGuard(AIAgent& agent, f32 alertRange) {
    AIPerceptionSettings perception = agent.GetPerception();
    perception.sightRange = alertRange;
    perception.sightAngle = 360.0f; // Can see all around
    agent.SetPerceptionSettings(perception);

    AIMovementSettings movement = agent.GetMovement();
    movement.chaseGiveUpDistance = alertRange * 1.5f;
    agent.SetMovementSettings(movement);

    agent.TransitionTo(AIState::Idle);
}

void SetupTurret(AIAgent& agent, f32 range) {
    AIPerceptionSettings perception = agent.GetPerception();
    perception.sightRange = range;
    perception.sightAngle = 360.0f;
    agent.SetPerceptionSettings(perception);

    AIMovementSettings movement = agent.GetMovement();
    movement.walkSpeed = 0.0f;
    movement.runSpeed = 0.0f;
    movement.attackRange = range;
    movement.turnSpeed = 90.0f;
    agent.SetMovementSettings(movement);
}

} // namespace AIPresets

} // namespace AI
} // namespace Enjin
