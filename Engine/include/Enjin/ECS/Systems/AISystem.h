#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/AI/Navmesh.h"
#include <unordered_map>
#include <memory>

namespace Enjin {
namespace ECS {

/**
 * @brief ECS system that processes AIControllerComponent entities.
 *
 * Manages AI agents, drives state transitions, and uses the navmesh
 * Pathfinder for patrol waypoint navigation and chase target tracking.
 */
class ENJIN_API AISystem {
public:
    AISystem() = default;
    ~AISystem() = default;

    void SetWorld(World* world) { m_World = world; }

    // Enable/disable all AI updates (disabled in editor mode, enabled in play mode)
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // --- Navmesh management ---

    /// Set the shared navmesh for pathfinding. The system does NOT take ownership.
    void SetNavmesh(const AI::Navmesh* navmesh);

    /// Get the current navmesh (may be null)
    const AI::Navmesh* GetNavmesh() const { return m_Navmesh; }

    /// Get the pathfinder (for external queries)
    AI::Pathfinder* GetPathfinder() { return &m_Pathfinder; }

    /// Generate a simple grid navmesh covering a world-space area.
    /// Convenience method that builds a navmesh internally and stores it.
    void GenerateGridNavmesh(const Math::Vector3& min, const Math::Vector3& max,
                             f32 cellSize, f32 height = 0.0f);

    /// Generate a navmesh from level geometry (triangle soup).
    void GenerateNavmeshFromGeometry(const std::vector<Math::Vector3>& vertices,
                                     const std::vector<u32>& indices,
                                     const AI::NavmeshGenSettings& settings = AI::NavmeshGenSettings{});

    // --- Per-frame update ---

    /// Main update - processes all entities with AIControllerComponent + TransformComponent
    void Update(f32 deltaTime);

    // --- Debug visualization ---

    /// Enable or disable debug overlay drawing
    void SetDebugDraw(bool enabled) { m_DebugDraw = enabled; }
    bool GetDebugDraw() const { return m_DebugDraw; }

    /// Get debug line segments for current frame (pairs: start, end, color repeated)
    struct DebugLine {
        Math::Vector3 start;
        Math::Vector3 end;
        Math::Vector3 color;
    };

    const std::vector<DebugLine>& GetDebugLines() const { return m_DebugLines; }

    // --- Settings ---

    /// How often (seconds) to re-path during chase. 0 = every frame.
    void SetRepathInterval(f32 interval) { m_RepathInterval = interval; }
    f32 GetRepathInterval() const { return m_RepathInterval; }

private:
    // Internal agent state that persists across frames (keyed by entity ID)
    struct AgentState {
        AI::PathResult currentPath;
        u32 currentWaypointIndex = 0;
        bool isFollowingPath = false;
        bool pathValid = false;
        f32 repathTimer = 0.0f;
        f32 patrolWaitTimer = 0.0f;
        bool isWaiting = false;
        f32 lostTargetTimer = 0.0f;
        f32 attackCooldownTimer = 0.0f;
        bool patrolForward = true;  // Direction for ping-pong patrol mode
    };

    // Process individual AI states
    void ProcessIdle(Entity entity, AIControllerComponent& ai, TransformComponent& transform,
                     AgentState& state, f32 dt);
    void ProcessPatrol(Entity entity, AIControllerComponent& ai, TransformComponent& transform,
                       AgentState& state, f32 dt);
    void ProcessChase(Entity entity, AIControllerComponent& ai, TransformComponent& transform,
                      AgentState& state, f32 dt);
    void ProcessAttack(Entity entity, AIControllerComponent& ai, TransformComponent& transform,
                       AgentState& state, f32 dt);
    void ProcessFlee(Entity entity, AIControllerComponent& ai, TransformComponent& transform,
                     AgentState& state, f32 dt);

    // Movement helpers
    bool MoveAlongPath(TransformComponent& transform, AIControllerComponent& ai,
                       AgentState& state, f32 speed, f32 dt);
    void MoveTowards(TransformComponent& transform, AIControllerComponent& ai,
                     const Math::Vector3& target, f32 speed, f32 dt);
    void FaceTarget(TransformComponent& transform, const Math::Vector3& target, f32 turnSpeed, f32 dt);

    // Pathfinding helpers
    bool RequestPath(AgentState& state, const Math::Vector3& from, const Math::Vector3& to);

    // Target visibility check
    bool CanSeeTarget(const TransformComponent& transform, const AIControllerComponent& ai,
                      const Math::Vector3& targetPos) const;
    f32 DistanceTo(const Math::Vector3& a, const Math::Vector3& b) const;

    // Debug drawing helpers
    void DrawNavmeshDebug();
    void DrawPathDebug(const AgentState& state, const Math::Vector3& agentPos);
    void DrawAgentDebug(const TransformComponent& transform, const AIControllerComponent& ai);

    World* m_World = nullptr;
    bool m_Enabled = false;

    // Navmesh and pathfinding
    const AI::Navmesh* m_Navmesh = nullptr;
    AI::Pathfinder m_Pathfinder;
    AI::NavmeshGenerator m_Generator;         // Owns generated navmesh data
    bool m_OwnsNavmesh = false;               // True if we generated the navmesh

    // Per-entity persistent state
    std::unordered_map<Entity, AgentState> m_AgentStates;

    // Settings
    f32 m_RepathInterval = 0.5f; // Re-path every 0.5s during chase

    // Debug
    bool m_DebugDraw = false;
    std::vector<DebugLine> m_DebugLines;
};

} // namespace ECS
} // namespace Enjin
