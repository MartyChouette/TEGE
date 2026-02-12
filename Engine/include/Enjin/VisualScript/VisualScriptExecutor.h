#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/VisualScript/NodeDefinition.h"
#include "Enjin/VisualScript/NodeRegistry.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/ECS/World.h"
#include <stack>

namespace Enjin {
namespace VisualScript {

// ============================================================================
// VISUAL SCRIPT EXECUTOR
// ============================================================================

// Executes visual script graphs
class ENJIN_API VisualScriptExecutor {
public:
    VisualScriptExecutor() = default;

    // Execute an event on a script
    void ExecuteEvent(ECS::World* world, ECS::Entity entity,
                      ECS::VisualScriptComponent* script,
                      ECS::VisualScriptEvent event,
                      f32 deltaTime,
                      ECS::Entity otherEntity = ECS::INVALID_ENTITY);

    // Execute a custom event by name
    void ExecuteCustomEvent(ECS::World* world, ECS::Entity entity,
                            ECS::VisualScriptComponent* script,
                            const std::string& eventName,
                            f32 deltaTime);

    // Execute starting from a specific node (for manual triggering)
    void ExecuteFromNode(ECS::World* world, ECS::Entity entity,
                         ECS::VisualScriptComponent* script,
                         Editor::NodeId startNode,
                         f32 deltaTime,
                         ECS::Entity otherEntity = ECS::INVALID_ENTITY);

    // Update latent nodes (called each frame for Delay, etc.)
    void UpdateLatentNodes(ECS::World* world, ECS::Entity entity,
                           ECS::VisualScriptComponent* script,
                           f32 deltaTime);

    // Get execution statistics (for debugging)
    struct ExecutionStats {
        u32 nodesExecuted = 0;
        u32 pureNodesEvaluated = 0;
        f32 executionTimeMs = 0.0f;
    };
    const ExecutionStats& GetLastStats() const { return m_LastStats; }

    // Physics system for physics query nodes
    void SetPhysics(Physics::IPhysicsBackend* physics) { m_Physics = physics; }

    // 2D physics system for 2D physics query nodes
    void SetPhysics2D(Physics::IPhysicsBackend2D* physics2d) { m_Physics2D = physics2d; }

    // Script engine for interop nodes
    void SetScriptEngine(Scripting::ScriptEngine* engine) { m_ScriptEngine = engine; }

    // Networking system for multiplayer nodes
    void SetNetworking(Networking::NetworkSystem* net) { m_Networking = net; }

    // Configuration
    void SetMaxIterations(u32 max) { m_MaxIterations = max; }
    u32 GetMaxIterations() const { return m_MaxIterations; }

private:
    // Execute flow from a node, following flow links
    void ExecuteFlow(ExecutionContext& ctx, Editor::NodeId nodeId);

    // Evaluate a pure node (recursive, cached)
    ECS::VariableValue EvaluatePureNode(const ExecutionContext& ctx,
                                         const ECS::VisualScriptComponent* script,
                                         Editor::NodeId nodeId);

    // Get the value for an input pin (resolve connections or use default)
    ECS::VariableValue GetInputPinValue(const ExecutionContext& ctx,
                                         const ECS::VisualScriptComponent* script,
                                         Editor::PinId pinId);

    // Get all input values for a node's data pins (excluding flow pins)
    std::vector<ECS::VariableValue> GatherInputValues(const ExecutionContext& ctx,
                                                       const ECS::VisualScriptComponent* script,
                                                       const Editor::GraphNode* node,
                                                       const NodeDefinition* def);

    // Find the next node to execute via flow link
    Editor::NodeId FollowFlowLink(const ECS::VisualScriptComponent* script,
                                   Editor::PinId outputPinId);

    // Get flow output pin by index from a node
    Editor::PinId GetFlowOutputPin(const Editor::GraphNode* node, i32 index);

    ExecutionStats m_LastStats;
    u32 m_MaxIterations = 10000;  // Safety limit to prevent infinite loops
    Physics::IPhysicsBackend* m_Physics = nullptr;
    Physics::IPhysicsBackend2D* m_Physics2D = nullptr;
    Scripting::ScriptEngine* m_ScriptEngine = nullptr;
    Networking::NetworkSystem* m_Networking = nullptr;
    u32 m_FunctionCallDepth = 0;
    static constexpr u32 MAX_CALL_DEPTH = 32;
};

} // namespace VisualScript
} // namespace Enjin
