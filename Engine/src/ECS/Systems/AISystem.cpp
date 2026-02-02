#include "Enjin/ECS/Systems/AISystem.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace ECS {

// ============================================================================
// Navmesh Management
// ============================================================================

void AISystem::SetNavmesh(const AI::Navmesh* navmesh) {
    m_Navmesh = navmesh;
    m_Pathfinder.SetNavmesh(navmesh);
    m_OwnsNavmesh = false;
    ENJIN_LOG_INFO(AI, "AISystem: External navmesh set (%zu polygons)",
                   navmesh ? navmesh->GetPolygons().size() : 0);
}

void AISystem::GenerateGridNavmesh(const Math::Vector3& min, const Math::Vector3& max,
                                    f32 cellSize, f32 height) {
    m_Generator.GenerateGrid(min, max, cellSize, height);
    m_Navmesh = &m_Generator.GetNavmesh();
    m_Pathfinder.SetNavmesh(m_Navmesh);
    m_OwnsNavmesh = true;
    ENJIN_LOG_INFO(AI, "AISystem: Generated grid navmesh (%zu polygons)",
                   m_Navmesh->GetPolygons().size());
}

void AISystem::GenerateNavmeshFromGeometry(const std::vector<Math::Vector3>& vertices,
                                            const std::vector<u32>& indices,
                                            const AI::NavmeshGenSettings& settings) {
    if (m_Generator.Generate(vertices, indices, settings)) {
        m_Navmesh = &m_Generator.GetNavmesh();
        m_Pathfinder.SetNavmesh(m_Navmesh);
        m_OwnsNavmesh = true;
        ENJIN_LOG_INFO(AI, "AISystem: Generated navmesh from geometry (%zu polygons)",
                       m_Navmesh->GetPolygons().size());
    } else {
        ENJIN_LOG_ERROR(AI, "AISystem: Failed to generate navmesh from geometry");
    }
}

// ============================================================================
// Main Update Loop
// ============================================================================

void AISystem::Update(f32 deltaTime) {
    if (!m_Enabled || !m_World) return;

    // Clear debug lines from previous frame
    m_DebugLines.clear();

    // Draw navmesh overlay if debug is enabled
    if (m_DebugDraw && m_Navmesh) {
        DrawNavmeshDebug();
    }

    // Iterate all entities with AIControllerComponent + TransformComponent
    for (Entity entity : m_World->GetAllEntities()) {
        if (!m_World->HasComponent<AIControllerComponent>(entity) ||
            !m_World->HasComponent<TransformComponent>(entity)) {
            continue;
        }

        auto* ai = m_World->GetComponent<AIControllerComponent>(entity);
        auto* transform = m_World->GetComponent<TransformComponent>(entity);

        // Get or create persistent agent state
        AgentState& state = m_AgentStates[entity];

        // Resolve target position from targetEntity (if any)
        if (ai->targetEntity != 0 && ai->targetEntity != INVALID_ENTITY && m_World->IsValid(ai->targetEntity)) {
            auto* targetTransform = m_World->GetComponent<TransformComponent>(ai->targetEntity);
            if (targetTransform) {
                ai->targetPosition = targetTransform->position;
            }
        }

        // Update state timer
        ai->stateTimer += deltaTime;

        // Process current AI state
        switch (ai->currentState) {
            case AIControllerComponent::AIState::Idle:
                ProcessIdle(entity, *ai, *transform, state, deltaTime);
                break;
            case AIControllerComponent::AIState::Patrol:
                ProcessPatrol(entity, *ai, *transform, state, deltaTime);
                break;
            case AIControllerComponent::AIState::Chase:
                ProcessChase(entity, *ai, *transform, state, deltaTime);
                break;
            case AIControllerComponent::AIState::Attack:
                ProcessAttack(entity, *ai, *transform, state, deltaTime);
                break;
            case AIControllerComponent::AIState::Flee:
                ProcessFlee(entity, *ai, *transform, state, deltaTime);
                break;
            case AIControllerComponent::AIState::Dead:
                // Dead entities do not update
                break;
        }

        // Debug drawing per agent
        if (m_DebugDraw) {
            DrawAgentDebug(*transform, *ai);
            if (state.isFollowingPath && state.pathValid) {
                DrawPathDebug(state, transform->position);
            }
        }
    }

    // Clean up agent states for destroyed entities
    for (auto it = m_AgentStates.begin(); it != m_AgentStates.end(); ) {
        if (!m_World->IsValid(it->first)) {
            it = m_AgentStates.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// State Processors
// ============================================================================

void AISystem::ProcessIdle(Entity entity, AIControllerComponent& ai, TransformComponent& transform,
                           AgentState& state, f32 dt) {
    (void)entity;
    (void)state;
    (void)dt;

    // Scan for target
    if (ai.targetEntity != 0 && ai.targetEntity != INVALID_ENTITY) {
        f32 dist = DistanceTo(transform.position, ai.targetPosition);
        if (dist <= ai.detectionRange && CanSeeTarget(transform, ai, ai.targetPosition)) {
            ai.currentState = AIControllerComponent::AIState::Chase;
            ai.stateTimer = 0.0f;
            state.lostTargetTimer = 0.0f;
            state.isFollowingPath = false;
            state.pathValid = false;
            return;
        }
    }

    // If we have patrol points, transition to patrol
    if (!ai.patrolPoints.empty()) {
        ai.currentState = AIControllerComponent::AIState::Patrol;
        ai.stateTimer = 0.0f;
        state.isFollowingPath = false;
        state.pathValid = false;
    }
}

void AISystem::ProcessPatrol(Entity entity, AIControllerComponent& ai, TransformComponent& transform,
                             AgentState& state, f32 dt) {
    (void)entity;

    // Check for target - interrupt patrol if we see something
    if (ai.targetEntity != 0 && ai.targetEntity != INVALID_ENTITY) {
        f32 dist = DistanceTo(transform.position, ai.targetPosition);
        if (dist <= ai.detectionRange && CanSeeTarget(transform, ai, ai.targetPosition)) {
            ai.currentState = AIControllerComponent::AIState::Chase;
            ai.stateTimer = 0.0f;
            state.lostTargetTimer = 0.0f;
            state.isFollowingPath = false;
            state.pathValid = false;
            return;
        }
    }

    if (ai.patrolPoints.empty()) {
        ai.currentState = AIControllerComponent::AIState::Idle;
        ai.stateTimer = 0.0f;
        return;
    }

    // Waiting at a patrol point?
    if (state.isWaiting) {
        state.patrolWaitTimer += dt;
        if (state.patrolWaitTimer >= ai.patrolWaitTime) {
            state.isWaiting = false;

            // Advance to next patrol point
            if (ai.patrolLoop) {
                ai.currentPatrolIndex = (ai.currentPatrolIndex + 1) % ai.patrolPoints.size();
            } else {
                // Ping-pong: reverse direction at endpoints
                if (state.patrolForward) {
                    if (ai.currentPatrolIndex + 1 >= ai.patrolPoints.size()) {
                        state.patrolForward = false;
                        if (ai.currentPatrolIndex > 0) ai.currentPatrolIndex--;
                    } else {
                        ai.currentPatrolIndex++;
                    }
                } else {
                    if (ai.currentPatrolIndex == 0) {
                        state.patrolForward = true;
                        if (ai.patrolPoints.size() > 1) ai.currentPatrolIndex++;
                    } else {
                        ai.currentPatrolIndex--;
                    }
                }
            }

            // Request a new path to the next patrol point
            state.isFollowingPath = false;
            state.pathValid = false;
        }
        return;
    }

    // If we don't have a valid path, compute one to the current patrol waypoint
    if (!state.isFollowingPath || !state.pathValid) {
        Math::Vector3 target = ai.patrolPoints[ai.currentPatrolIndex];
        if (ai.useNavmesh && m_Navmesh) {
            if (RequestPath(state, transform.position, target)) {
                state.isFollowingPath = true;
            } else {
                // Fallback: direct movement (no navmesh path available)
                state.currentPath.waypoints.clear();
                state.currentPath.waypoints.push_back(transform.position);
                state.currentPath.waypoints.push_back(target);
                state.currentPath.success = true;
                state.currentWaypointIndex = 0;
                state.pathValid = true;
                state.isFollowingPath = true;
            }
        } else {
            // Direct movement (no navmesh)
            state.currentPath.waypoints.clear();
            state.currentPath.waypoints.push_back(transform.position);
            state.currentPath.waypoints.push_back(target);
            state.currentPath.success = true;
            state.currentWaypointIndex = 0;
            state.pathValid = true;
            state.isFollowingPath = true;
        }
    }

    // Move along the path
    bool arrived = MoveAlongPath(transform, ai, state, ai.moveSpeed, dt);
    if (arrived) {
        // Arrived at patrol point, start waiting
        state.isWaiting = true;
        state.patrolWaitTimer = 0.0f;
        state.isFollowingPath = false;
        state.pathValid = false;
    }
}

void AISystem::ProcessChase(Entity entity, AIControllerComponent& ai, TransformComponent& transform,
                            AgentState& state, f32 dt) {
    (void)entity;

    // No target? Return to patrol or idle
    if (ai.targetEntity == 0 || ai.targetEntity == INVALID_ENTITY) {
        ai.currentState = ai.patrolPoints.empty()
            ? AIControllerComponent::AIState::Idle
            : AIControllerComponent::AIState::Patrol;
        ai.stateTimer = 0.0f;
        state.isFollowingPath = false;
        state.pathValid = false;
        return;
    }

    f32 distToTarget = DistanceTo(transform.position, ai.targetPosition);

    // Close enough to attack?
    if (distToTarget <= ai.attackRange) {
        ai.currentState = AIControllerComponent::AIState::Attack;
        ai.stateTimer = 0.0f;
        state.isFollowingPath = false;
        state.pathValid = false;
        return;
    }

    // Lost target (too far)?
    if (distToTarget > ai.loseTargetRange) {
        state.lostTargetTimer += dt;
        if (state.lostTargetTimer > 3.0f) {
            // Give up chase
            ai.currentState = ai.patrolPoints.empty()
                ? AIControllerComponent::AIState::Idle
                : AIControllerComponent::AIState::Patrol;
            ai.stateTimer = 0.0f;
            state.isFollowingPath = false;
            state.pathValid = false;
            state.lostTargetTimer = 0.0f;
            return;
        }
    } else {
        state.lostTargetTimer = 0.0f;
    }

    // Re-path periodically (target moves)
    state.repathTimer += dt;
    bool needRepath = !state.pathValid || !state.isFollowingPath ||
                      state.repathTimer >= ai.repathInterval;

    if (needRepath) {
        state.repathTimer = 0.0f;

        if (ai.useNavmesh && m_Navmesh) {
            if (RequestPath(state, transform.position, ai.targetPosition)) {
                state.isFollowingPath = true;
            } else {
                // Fallback: direct movement
                state.currentPath.waypoints.clear();
                state.currentPath.waypoints.push_back(transform.position);
                state.currentPath.waypoints.push_back(ai.targetPosition);
                state.currentPath.success = true;
                state.currentWaypointIndex = 0;
                state.pathValid = true;
                state.isFollowingPath = true;
            }
        } else {
            // Direct movement
            state.currentPath.waypoints.clear();
            state.currentPath.waypoints.push_back(transform.position);
            state.currentPath.waypoints.push_back(ai.targetPosition);
            state.currentPath.success = true;
            state.currentWaypointIndex = 0;
            state.pathValid = true;
            state.isFollowingPath = true;
        }
    }

    // Move along path at chase speed
    MoveAlongPath(transform, ai, state, ai.chaseSpeed, dt);
}

void AISystem::ProcessAttack(Entity entity, AIControllerComponent& ai, TransformComponent& transform,
                             AgentState& state, f32 dt) {
    (void)entity;

    // No target? Return
    if (ai.targetEntity == 0 || ai.targetEntity == INVALID_ENTITY) {
        ai.currentState = AIControllerComponent::AIState::Idle;
        ai.stateTimer = 0.0f;
        return;
    }

    f32 distToTarget = DistanceTo(transform.position, ai.targetPosition);

    // Face the target
    FaceTarget(transform, ai.targetPosition, ai.turnSpeed, dt);

    // If target moved out of attack range, chase again
    if (distToTarget > ai.attackRange * 1.5f) {
        ai.currentState = AIControllerComponent::AIState::Chase;
        ai.stateTimer = 0.0f;
        state.isFollowingPath = false;
        state.pathValid = false;
        return;
    }

    // Attack cooldown
    state.attackCooldownTimer -= dt;
    if (state.attackCooldownTimer <= 0.0f) {
        // Execute attack: apply damage to target entity if it has HealthComponent
        if (m_World && m_World->IsValid(ai.targetEntity)) {
            auto* targetHealth = m_World->GetComponent<HealthComponent>(ai.targetEntity);
            if (targetHealth && !targetHealth->isDead && !targetHealth->isInvulnerable) {
                // Apply damage (check resistance)
                f32 damage = ai.attackDamage;
                auto* resistance = m_World->GetComponent<DamageResistanceComponent>(ai.targetEntity);
                if (resistance) {
                    damage *= resistance->physicalMult; // AI does physical damage by default
                }

                // Absorb with shield first
                if (targetHealth->currentShield > 0.0f) {
                    f32 shieldAbsorb = Math::Min(targetHealth->currentShield, damage);
                    targetHealth->currentShield -= shieldAbsorb;
                    damage -= shieldAbsorb;
                }

                targetHealth->currentHealth -= damage;
                targetHealth->timeSinceLastDamage = 0.0f;
                if (targetHealth->currentHealth <= 0.0f) {
                    targetHealth->currentHealth = 0.0f;
                    targetHealth->isDead = true;
                }
            }
        }

        state.attackCooldownTimer = ai.attackCooldown;
    }
}

void AISystem::ProcessFlee(Entity entity, AIControllerComponent& ai, TransformComponent& transform,
                           AgentState& state, f32 dt) {
    (void)entity;

    // Compute flee destination: opposite direction from target
    if (!state.isFollowingPath || !state.pathValid) {
        Math::Vector3 fleeDir;
        if (ai.targetEntity != 0 && ai.targetEntity != INVALID_ENTITY) {
            fleeDir = transform.position - ai.targetPosition;
        } else {
            // Random direction
            fleeDir = Math::Vector3(1.0f, 0.0f, 0.0f);
        }

        // Normalize and scale
        f32 len = fleeDir.Length();
        if (len > 0.001f) {
            fleeDir = fleeDir * (1.0f / len);
        } else {
            fleeDir = Math::Vector3(0.0f, 0.0f, 1.0f);
        }

        Math::Vector3 fleeTarget = transform.position + fleeDir * ai.fleeDistance;

        if (ai.useNavmesh && m_Navmesh) {
            // Clamp flee target to navmesh
            fleeTarget = m_Navmesh->GetNearestPoint(fleeTarget);
            if (RequestPath(state, transform.position, fleeTarget)) {
                state.isFollowingPath = true;
            } else {
                // Direct flee
                state.currentPath.waypoints.clear();
                state.currentPath.waypoints.push_back(transform.position);
                state.currentPath.waypoints.push_back(fleeTarget);
                state.currentPath.success = true;
                state.currentWaypointIndex = 0;
                state.pathValid = true;
                state.isFollowingPath = true;
            }
        } else {
            state.currentPath.waypoints.clear();
            state.currentPath.waypoints.push_back(transform.position);
            state.currentPath.waypoints.push_back(fleeTarget);
            state.currentPath.success = true;
            state.currentWaypointIndex = 0;
            state.pathValid = true;
            state.isFollowingPath = true;
        }
    }

    bool arrived = MoveAlongPath(transform, ai, state, ai.fleeSpeed, dt);
    if (arrived || ai.stateTimer > 5.0f) {
        // Done fleeing, return to idle
        ai.currentState = AIControllerComponent::AIState::Idle;
        ai.stateTimer = 0.0f;
        state.isFollowingPath = false;
        state.pathValid = false;
    }
}

// ============================================================================
// Movement Helpers
// ============================================================================

bool AISystem::MoveAlongPath(TransformComponent& transform, AIControllerComponent& ai,
                              AgentState& state, f32 speed, f32 dt) {
    if (!state.pathValid || state.currentPath.waypoints.empty()) return true;

    // Skip waypoint 0 (it is the start position)
    if (state.currentWaypointIndex == 0 && state.currentPath.waypoints.size() > 1) {
        state.currentWaypointIndex = 1;
    }

    if (state.currentWaypointIndex >= state.currentPath.waypoints.size()) {
        state.isFollowingPath = false;
        return true; // Arrived at end of path
    }

    const Math::Vector3& target = state.currentPath.waypoints[state.currentWaypointIndex];

    // Move towards current waypoint
    MoveTowards(transform, ai, target, speed, dt);

    // Check if we reached the waypoint
    f32 dist = DistanceTo(transform.position, target);
    if (dist < ai.arrivalRadius) {
        state.currentWaypointIndex++;
        if (state.currentWaypointIndex >= state.currentPath.waypoints.size()) {
            state.isFollowingPath = false;
            return true; // Arrived at final destination
        }
    }

    return false;
}

void AISystem::MoveTowards(TransformComponent& transform, AIControllerComponent& ai,
                            const Math::Vector3& target, f32 speed, f32 dt) {
    Math::Vector3 toTarget = target - transform.position;
    toTarget.y = 0.0f; // Stay on ground plane

    f32 dist = Math::Sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
    if (dist < 0.001f) return;

    Math::Vector3 direction = toTarget * (1.0f / dist);

    // Slow down near arrival
    f32 effectiveSpeed = speed;
    if (dist < ai.stoppingDistance) {
        effectiveSpeed *= dist / ai.stoppingDistance;
    }

    // Apply movement
    f32 moveAmount = effectiveSpeed * dt;
    if (moveAmount > dist) moveAmount = dist;

    transform.position.x += direction.x * moveAmount;
    transform.position.z += direction.z * moveAmount;

    // Face movement direction
    FaceTarget(transform, target, ai.turnSpeed, dt);
}

void AISystem::FaceTarget(TransformComponent& transform, const Math::Vector3& target,
                           f32 turnSpeed, f32 dt) {
    Math::Vector3 dir = target - transform.position;
    dir.y = 0.0f;
    f32 len = Math::Sqrt(dir.x * dir.x + dir.z * dir.z);
    if (len < 0.001f) return;

    f32 targetAngle = Math::Atan2(dir.x, dir.z);

    // Extract current yaw from quaternion rotation matrix
    Math::Matrix4 mat = transform.rotation.ToMatrix();
    f32 currentAngle = Math::Atan2(mat.m[8], mat.m[10]);

    // Compute angle difference
    f32 angleDiff = targetAngle - currentAngle;
    while (angleDiff > Math::PI) angleDiff -= 2.0f * Math::PI;
    while (angleDiff < -Math::PI) angleDiff += 2.0f * Math::PI;

    // Clamp turn rate
    f32 maxTurn = Math::Radians(turnSpeed) * dt;
    if (Math::Abs(angleDiff) > maxTurn) {
        angleDiff = (angleDiff > 0.0f) ? maxTurn : -maxTurn;
    }

    f32 newAngle = currentAngle + angleDiff;
    transform.rotation = Math::Quaternion(Math::Vector3(0.0f, 1.0f, 0.0f), newAngle);
}

// ============================================================================
// Pathfinding Helpers
// ============================================================================

bool AISystem::RequestPath(AgentState& state, const Math::Vector3& from, const Math::Vector3& to) {
    if (!m_Navmesh) return false;

    AI::PathResult result = m_Pathfinder.FindPath(from, to);
    if (result.success && !result.waypoints.empty()) {
        state.currentPath = result;
        state.currentWaypointIndex = 0;
        state.pathValid = true;
        return true;
    }

    // Try partial path as fallback
    AI::PathResult partial = m_Pathfinder.FindPartialPath(from, to, 200);
    if (partial.success && !partial.waypoints.empty()) {
        state.currentPath = partial;
        state.currentWaypointIndex = 0;
        state.pathValid = true;
        return true;
    }

    state.pathValid = false;
    return false;
}

// ============================================================================
// Detection / Perception
// ============================================================================

bool AISystem::CanSeeTarget(const TransformComponent& transform, const AIControllerComponent& ai,
                             const Math::Vector3& targetPos) const {
    Math::Vector3 toTarget = targetPos - transform.position;
    toTarget.y = 0.0f;
    f32 dist = Math::Sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

    if (dist > ai.detectionRange) return false;
    if (dist < 0.001f) return true; // On top of target

    // Check field of view angle
    Math::Vector3 dir = toTarget * (1.0f / dist);

    // Extract forward direction from rotation
    Math::Matrix4 mat = transform.rotation.ToMatrix();
    Math::Vector3 forward(mat.m[8], 0.0f, mat.m[10]);
    f32 fwdLen = Math::Sqrt(forward.x * forward.x + forward.z * forward.z);
    if (fwdLen > 0.001f) {
        forward = forward * (1.0f / fwdLen);
    } else {
        forward = Math::Vector3(0.0f, 0.0f, -1.0f);
    }

    f32 dot = forward.x * dir.x + forward.z * dir.z;
    dot = Math::Clamp(dot, -1.0f, 1.0f);
    f32 angle = Math::Degrees(Math::Acos(dot));

    return angle <= ai.fieldOfView * 0.5f;
}

f32 AISystem::DistanceTo(const Math::Vector3& a, const Math::Vector3& b) const {
    f32 dx = b.x - a.x;
    f32 dy = b.y - a.y;
    f32 dz = b.z - a.z;
    return Math::Sqrt(dx * dx + dy * dy + dz * dz);
}

// ============================================================================
// Debug Visualization
// ============================================================================

void AISystem::DrawNavmeshDebug() {
    if (!m_Navmesh) return;

    const Math::Vector3 navColor(0.2f, 0.6f, 0.9f);       // Blue for walkable
    const Math::Vector3 unwalkableColor(0.8f, 0.2f, 0.2f); // Red for unwalkable

    for (const auto& poly : m_Navmesh->GetPolygons()) {
        const Math::Vector3& color = poly.walkable ? navColor : unwalkableColor;

        // Draw polygon edges
        for (usize i = 0; i < poly.vertices.size(); ++i) {
            usize j = (i + 1) % poly.vertices.size();
            // Lift slightly above ground for visibility
            Math::Vector3 a = poly.vertices[i];
            Math::Vector3 b = poly.vertices[j];
            a.y += 0.05f;
            b.y += 0.05f;
            m_DebugLines.push_back({a, b, color});
        }

        // Draw connectivity lines (center to neighbor center)
        const Math::Vector3 connectColor(0.4f, 0.4f, 0.4f);
        for (u32 neighborId : poly.neighbors) {
            const AI::NavPolygon* neighbor = m_Navmesh->GetPolygon(neighborId);
            if (neighbor && neighbor->id > poly.id) { // Avoid duplicates
                Math::Vector3 a = poly.center;
                Math::Vector3 b = neighbor->center;
                a.y += 0.1f;
                b.y += 0.1f;
                m_DebugLines.push_back({a, b, connectColor});
            }
        }
    }
}

void AISystem::DrawPathDebug(const AgentState& state, const Math::Vector3& agentPos) {
    if (!state.pathValid || state.currentPath.waypoints.empty()) return;

    const Math::Vector3 pathColor(1.0f, 1.0f, 0.0f);       // Yellow for path
    const Math::Vector3 waypointColor(1.0f, 0.5f, 0.0f);    // Orange for waypoint markers
    const Math::Vector3 activeColor(0.0f, 1.0f, 0.0f);      // Green for active segment

    // Draw line from agent to current waypoint
    if (state.currentWaypointIndex < state.currentPath.waypoints.size()) {
        Math::Vector3 a = agentPos;
        Math::Vector3 b = state.currentPath.waypoints[state.currentWaypointIndex];
        a.y += 0.15f;
        b.y += 0.15f;
        m_DebugLines.push_back({a, b, activeColor});
    }

    // Draw full path
    for (usize i = 0; i + 1 < state.currentPath.waypoints.size(); ++i) {
        Math::Vector3 a = state.currentPath.waypoints[i];
        Math::Vector3 b = state.currentPath.waypoints[i + 1];
        a.y += 0.1f;
        b.y += 0.1f;

        const Math::Vector3& color = (i >= state.currentWaypointIndex) ? pathColor : Math::Vector3(0.5f, 0.5f, 0.5f);
        m_DebugLines.push_back({a, b, color});
    }

    // Draw waypoint markers (small crosses)
    for (usize i = 0; i < state.currentPath.waypoints.size(); ++i) {
        Math::Vector3 wp = state.currentPath.waypoints[i];
        wp.y += 0.1f;
        f32 size = 0.15f;
        m_DebugLines.push_back({Math::Vector3(wp.x - size, wp.y, wp.z),
                                 Math::Vector3(wp.x + size, wp.y, wp.z), waypointColor});
        m_DebugLines.push_back({Math::Vector3(wp.x, wp.y, wp.z - size),
                                 Math::Vector3(wp.x, wp.y, wp.z + size), waypointColor});
    }
}

void AISystem::DrawAgentDebug(const TransformComponent& transform, const AIControllerComponent& ai) {
    // Draw detection range circle (approximated with line segments)
    if (ai.debugDrawDetection) {
        const Math::Vector3 detColor(0.0f, 1.0f, 0.0f);  // Green
        const Math::Vector3 atkColor(1.0f, 0.0f, 0.0f);   // Red for attack range

        const i32 segments = 24;
        for (i32 i = 0; i < segments; ++i) {
            f32 angle0 = (static_cast<f32>(i) / segments) * 2.0f * Math::PI;
            f32 angle1 = (static_cast<f32>(i + 1) / segments) * 2.0f * Math::PI;

            // Detection range
            Math::Vector3 a(transform.position.x + Math::Cos(angle0) * ai.detectionRange,
                            transform.position.y + 0.1f,
                            transform.position.z + Math::Sin(angle0) * ai.detectionRange);
            Math::Vector3 b(transform.position.x + Math::Cos(angle1) * ai.detectionRange,
                            transform.position.y + 0.1f,
                            transform.position.z + Math::Sin(angle1) * ai.detectionRange);
            m_DebugLines.push_back({a, b, detColor});

            // Attack range
            Math::Vector3 c(transform.position.x + Math::Cos(angle0) * ai.attackRange,
                            transform.position.y + 0.1f,
                            transform.position.z + Math::Sin(angle0) * ai.attackRange);
            Math::Vector3 d(transform.position.x + Math::Cos(angle1) * ai.attackRange,
                            transform.position.y + 0.1f,
                            transform.position.z + Math::Sin(angle1) * ai.attackRange);
            m_DebugLines.push_back({c, d, atkColor});
        }

        // Draw field of view cone
        Math::Matrix4 mat = transform.rotation.ToMatrix();
        Math::Vector3 forward(mat.m[8], 0.0f, mat.m[10]);
        f32 fwdLen = Math::Sqrt(forward.x * forward.x + forward.z * forward.z);
        if (fwdLen > 0.001f) {
            forward = forward * (1.0f / fwdLen);
        }

        f32 halfFov = Math::Radians(ai.fieldOfView * 0.5f);
        f32 fwdAngle = Math::Atan2(forward.x, forward.z);

        Math::Vector3 center = transform.position;
        center.y += 0.1f;

        Math::Vector3 leftEdge(
            center.x + Math::Sin(fwdAngle - halfFov) * ai.detectionRange,
            center.y,
            center.z + Math::Cos(fwdAngle - halfFov) * ai.detectionRange
        );
        Math::Vector3 rightEdge(
            center.x + Math::Sin(fwdAngle + halfFov) * ai.detectionRange,
            center.y,
            center.z + Math::Cos(fwdAngle + halfFov) * ai.detectionRange
        );

        const Math::Vector3 fovColor(0.8f, 0.8f, 0.0f);
        m_DebugLines.push_back({center, leftEdge, fovColor});
        m_DebugLines.push_back({center, rightEdge, fovColor});
    }

    // Draw current state label position (just a small up-line indicator)
    if (ai.debugDrawPath) {
        Math::Vector3 base = transform.position;
        base.y += 0.1f;
        Math::Vector3 top = base;
        top.y += 1.5f;

        Math::Vector3 stateColor;
        switch (ai.currentState) {
            case AIControllerComponent::AIState::Idle:    stateColor = Math::Vector3(0.5f, 0.5f, 0.5f); break;
            case AIControllerComponent::AIState::Patrol:  stateColor = Math::Vector3(0.0f, 0.5f, 1.0f); break;
            case AIControllerComponent::AIState::Chase:   stateColor = Math::Vector3(1.0f, 0.5f, 0.0f); break;
            case AIControllerComponent::AIState::Attack:  stateColor = Math::Vector3(1.0f, 0.0f, 0.0f); break;
            case AIControllerComponent::AIState::Flee:    stateColor = Math::Vector3(1.0f, 1.0f, 0.0f); break;
            case AIControllerComponent::AIState::Dead:    stateColor = Math::Vector3(0.3f, 0.3f, 0.3f); break;
        }
        m_DebugLines.push_back({base, top, stateColor});
    }
}

} // namespace ECS
} // namespace Enjin
