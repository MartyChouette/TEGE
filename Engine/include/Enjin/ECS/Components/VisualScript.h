#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Editor/NodeGraph.h"
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>
#include <unordered_set>

namespace Enjin {
namespace ECS {

// ============================================================================
// VISUAL SCRIPT VARIABLE SYSTEM
// ============================================================================

// Variant type for storing variable values
using VariableValue = std::variant<
    bool,
    i32,
    f32,
    std::string,
    Math::Vector2,
    Math::Vector3,
    Math::Vector4,
    Entity
>;

// A variable defined in a visual script graph
struct VisualScriptVariable {
    std::string name;
    Editor::PinType type = Editor::PinType::Float;
    VariableValue value;
    bool exposed = false;  // Show in inspector for external editing

    // Default constructors for each type
    static VisualScriptVariable Bool(const std::string& name, bool val = false) {
        VisualScriptVariable v;
        v.name = name;
        v.type = Editor::PinType::Bool;
        v.value = val;
        return v;
    }

    static VisualScriptVariable Int(const std::string& name, i32 val = 0) {
        VisualScriptVariable v;
        v.name = name;
        v.type = Editor::PinType::Int;
        v.value = val;
        return v;
    }

    static VisualScriptVariable Float(const std::string& name, f32 val = 0.0f) {
        VisualScriptVariable v;
        v.name = name;
        v.type = Editor::PinType::Float;
        v.value = val;
        return v;
    }

    static VisualScriptVariable String(const std::string& name, const std::string& val = "") {
        VisualScriptVariable v;
        v.name = name;
        v.type = Editor::PinType::String;
        v.value = val;
        return v;
    }

    static VisualScriptVariable Vec3(const std::string& name, Math::Vector3 val = Math::Vector3(0, 0, 0)) {
        VisualScriptVariable v;
        v.name = name;
        v.type = Editor::PinType::Vector3;
        v.value = val;
        return v;
    }

    static VisualScriptVariable EntityRef(const std::string& name, Entity val = INVALID_ENTITY) {
        VisualScriptVariable v;
        v.name = name;
        v.type = Editor::PinType::Entity;
        v.value = val;
        return v;
    }
};

// ============================================================================
// VISUAL SCRIPT EVENTS
// ============================================================================

// Event types that can trigger graph execution
enum class VisualScriptEvent : u8 {
    OnCreate = 0,        // Called once when entity is created/spawned
    OnStart,             // Called once on first frame of play mode
    OnUpdate,            // Called every frame
    OnFixedUpdate,       // Called at fixed time intervals (physics)
    OnLateUpdate,        // Called after all Update calls
    OnCollisionEnter,    // Called when collision begins
    OnCollisionExit,     // Called when collision ends
    OnTriggerEnter,      // Called when entering a trigger zone
    OnTriggerExit,       // Called when exiting a trigger zone
    OnDestroy,           // Called when entity is about to be destroyed
    Custom,              // User-defined event (triggered via event name)
    COUNT
};

// Convert event to string for display
inline const char* VisualScriptEventToString(VisualScriptEvent e) {
    switch (e) {
        case VisualScriptEvent::OnCreate:         return "On Create";
        case VisualScriptEvent::OnStart:          return "On Start";
        case VisualScriptEvent::OnUpdate:         return "On Update";
        case VisualScriptEvent::OnFixedUpdate:    return "On Fixed Update";
        case VisualScriptEvent::OnLateUpdate:     return "On Late Update";
        case VisualScriptEvent::OnCollisionEnter: return "On Collision Enter";
        case VisualScriptEvent::OnCollisionExit:  return "On Collision Exit";
        case VisualScriptEvent::OnTriggerEnter:   return "On Trigger Enter";
        case VisualScriptEvent::OnTriggerExit:    return "On Trigger Exit";
        case VisualScriptEvent::OnDestroy:        return "On Destroy";
        case VisualScriptEvent::Custom:           return "Custom Event";
        default:                                   return "Unknown";
    }
}

// ============================================================================
// LATENT NODE STATE (for multi-frame execution like Delay)
// ============================================================================

struct LatentNodeState {
    f32 timeRemaining = 0.0f;
    bool isActive = false;
    Editor::PinId resumeFlowPin = 0;  // Which output pin to resume from
    Entity targetEntity = INVALID_ENTITY;  // For audio/animation waits
    std::string targetName;  // Animation name for WaitForAnimation
};

// ============================================================================
// EXECUTION RECORD (for profiler/timeline visualization)
// ============================================================================

struct ExecutionRecord {
    Editor::NodeId nodeId = 0;
    f32 timestamp = 0.0f;       // Time since play started
    f32 duration = 0.0f;        // Execution time in ms
    std::string nodeType;
};

// ============================================================================
// VISUAL SCRIPT NODE METADATA
// ============================================================================

// Metadata stored per-node for execution context
struct VisualScriptNodeMeta {
    std::string nodeType;        // Type identifier (e.g., "OnUpdate", "Branch", "PrintString")
    std::string customEventName; // For Custom event nodes

    // Node-specific settings (stored as key-value pairs)
    std::unordered_map<std::string, std::string> properties;

    // Cached pin values for data inputs (updated during execution)
    std::unordered_map<Editor::PinId, VariableValue> pinValues;
};

// ============================================================================
// VISUAL SCRIPT COMPONENT
// ============================================================================

struct VisualScriptComponent {
    // The node graph data (nodes, pins, links)
    Editor::NodeGraphData graph;

    // Local variables defined in this script
    std::vector<VisualScriptVariable> variables;

    // Map from event type to entry node ID
    // This allows quick lookup of which node to start execution from
    std::unordered_map<u8, Editor::NodeId> eventNodes;

    // Custom event mappings (event name -> node ID)
    std::unordered_map<std::string, Editor::NodeId> customEventNodes;

    // Per-node metadata (node ID -> metadata)
    std::unordered_map<Editor::NodeId, VisualScriptNodeMeta> nodeMeta;

    // ========== Runtime state (not serialized) ==========

    bool enabled = true;           // Is this script active?
    bool initialized = false;      // Has OnCreate been called?
    bool started = false;          // Has OnStart been called?

    // Cached values for pure nodes (cleared each frame)
    mutable std::unordered_map<Editor::NodeId, VariableValue> pureNodeCache;

    // Latent node states (for multi-frame execution like Delay)
    std::unordered_map<Editor::NodeId, LatentNodeState> latentStates;

    // Currently executing node ID (for visualization, 0 when not executing)
    Editor::NodeId currentlyExecutingNode = 0;

    // ========== Debug state (for breakpoints and step-through) ==========

    std::unordered_set<Editor::NodeId> breakpoints;  // Nodes with breakpoints set
    bool isPaused = false;                            // Script paused at breakpoint
    Editor::NodeId pausedAtNode = 0;                  // Which node we're paused at
    bool stepRequested = false;                       // User requested single step

    // ========== Profiler state (for execution timeline) ==========

    std::vector<ExecutionRecord> executionHistory;   // Recorded execution events
    f32 playStartTime = 0.0f;                        // When play mode started
    bool recordingEnabled = true;                    // Whether to record execution
    static constexpr usize MAX_HISTORY = 1000;       // Max history entries

    // ========== Helper methods ==========

    // Find a variable by name
    VisualScriptVariable* FindVariable(const std::string& name) {
        for (auto& v : variables) {
            if (v.name == name) return &v;
        }
        return nullptr;
    }

    const VisualScriptVariable* FindVariable(const std::string& name) const {
        for (const auto& v : variables) {
            if (v.name == name) return &v;
        }
        return nullptr;
    }

    // Register an event node
    void SetEventNode(VisualScriptEvent event, Editor::NodeId nodeId) {
        eventNodes[static_cast<u8>(event)] = nodeId;
    }

    // Get the entry node for an event
    Editor::NodeId GetEventNode(VisualScriptEvent event) const {
        auto it = eventNodes.find(static_cast<u8>(event));
        return it != eventNodes.end() ? it->second : 0;
    }

    // Register a custom event node
    void SetCustomEventNode(const std::string& eventName, Editor::NodeId nodeId) {
        customEventNodes[eventName] = nodeId;
    }

    // Get the entry node for a custom event
    Editor::NodeId GetCustomEventNode(const std::string& eventName) const {
        auto it = customEventNodes.find(eventName);
        return it != customEventNodes.end() ? it->second : 0;
    }

    // Clear runtime caches (called each frame before execution)
    void ClearRuntimeCache() const {
        pureNodeCache.clear();
    }

    // Reset runtime state (called when stopping play mode)
    void ResetRuntimeState() {
        initialized = false;
        started = false;
        pureNodeCache.clear();
        latentStates.clear();
        currentlyExecutingNode = 0;

        // Reset debug state
        isPaused = false;
        pausedAtNode = 0;
        stepRequested = false;

        // Clear profiler history
        executionHistory.clear();
        playStartTime = 0.0f;
    }
};

} // namespace ECS
} // namespace Enjin
