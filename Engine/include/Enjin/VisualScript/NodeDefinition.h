#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Editor/NodeGraph.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include <string>
#include <vector>
#include <functional>

namespace Enjin {

// Forward declarations
namespace ECS {
    class World;
}
namespace Physics {
    class IPhysicsBackend;
}
namespace Scripting {
    class ScriptEngine;
}

namespace VisualScript {

// ============================================================================
// EXECUTION CONTEXT
// ============================================================================

// Context passed to node execution functions
struct ExecutionContext {
    ECS::World* world = nullptr;
    ECS::Entity entity = ECS::INVALID_ENTITY;
    ECS::VisualScriptComponent* script = nullptr;
    f32 deltaTime = 0.0f;

    // For collision/trigger events
    ECS::Entity otherEntity = ECS::INVALID_ENTITY;

    // Flow control output - which flow output to follow next
    // -1 means execution stops, 0 = first flow output, 1 = second, etc.
    i32 nextFlowIndex = 0;

    // Physics system for query nodes
    Physics::IPhysicsBackend* physics = nullptr;

    // Script engine for interop nodes
    Scripting::ScriptEngine* scriptEngine = nullptr;

    // For event nodes, additional data
    std::string customEventName;
};

// ============================================================================
// PIN DEFINITION
// ============================================================================

struct PinDefinition {
    std::string name;
    Editor::PinType type = Editor::PinType::Any;
    Editor::PinKind kind = Editor::PinKind::Input;

    // Default value for input pins (used when not connected)
    ECS::VariableValue defaultValue;

    // True if this is an optional input (can be unconnected)
    bool optional = false;
};

// ============================================================================
// NODE CATEGORY
// ============================================================================

enum class NodeCategory : u8 {
    Events,        // Entry points (OnStart, OnUpdate, etc.)
    FlowControl,   // Branch, Sequence, ForLoop, etc.
    Variables,     // Get/Set Variable, Get Self
    Math,          // Add, Subtract, Multiply, etc.
    Logic,         // And, Or, Not, Comparison
    Vector,        // MakeVector3, BreakVector3, Normalize, etc.
    Transform,     // Position, Rotation, Scale operations
    Entity,        // Spawn, Destroy, FindByName
    Physics,       // Raycast, AddForce, etc.
    Components,    // GetHealth, SetHealth, Damage, etc.
    Audio,         // Play/Stop sound
    Debug,         // Print, DrawLine, etc.
    Utility,       // Delay, Random, etc.
    Gameplay,      // Save/Load, Checkpoint, Meta-progression
    Custom         // User-defined nodes
};

inline const char* NodeCategoryToString(NodeCategory cat) {
    switch (cat) {
        case NodeCategory::Events:      return "Events";
        case NodeCategory::FlowControl: return "Flow Control";
        case NodeCategory::Variables:   return "Variables";
        case NodeCategory::Math:        return "Math";
        case NodeCategory::Logic:       return "Logic";
        case NodeCategory::Vector:      return "Vector";
        case NodeCategory::Transform:   return "Transform";
        case NodeCategory::Entity:      return "Entity";
        case NodeCategory::Physics:     return "Physics";
        case NodeCategory::Components:  return "Components";
        case NodeCategory::Audio:       return "Audio";
        case NodeCategory::Debug:       return "Debug";
        case NodeCategory::Utility:     return "Utility";
        case NodeCategory::Gameplay:    return "Gameplay";
        case NodeCategory::Custom:      return "Custom";
        default:                        return "Unknown";
    }
}

// ============================================================================
// NODE FLAGS
// ============================================================================

enum class NodeDefFlags : u32 {
    None          = 0,
    Pure          = 1 << 0,   // No flow pins, evaluated on-demand
    Event         = 1 << 1,   // Entry point node (only has flow output)
    Latent        = 1 << 2,   // Execution spans multiple frames (e.g., Delay)
    Development   = 1 << 3,   // Only available in editor/debug builds
    Deprecated    = 1 << 4,   // Shown with warning, may be removed
    Compact       = 1 << 5,   // Rendered in compact form (single row)
};

inline NodeDefFlags operator|(NodeDefFlags a, NodeDefFlags b) {
    return static_cast<NodeDefFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline NodeDefFlags operator&(NodeDefFlags a, NodeDefFlags b) {
    return static_cast<NodeDefFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}

inline bool HasDefFlag(NodeDefFlags flags, NodeDefFlags flag) {
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

// ============================================================================
// NODE DEFINITION
// ============================================================================

// Type for node execution function
// Returns the index of the flow output to follow (-1 to stop, 0 for first output)
using NodeExecuteFunc = std::function<void(ExecutionContext& ctx,
                                           const std::vector<ECS::VariableValue>& inputs,
                                           std::vector<ECS::VariableValue>& outputs)>;

// Type for pure node evaluation (no flow, just compute output from inputs)
using NodeEvaluateFunc = std::function<ECS::VariableValue(const ExecutionContext& ctx,
                                                          const std::vector<ECS::VariableValue>& inputs)>;

struct NodeDefinition {
    // Unique identifier for this node type
    std::string typeId;

    // Display name shown in editor
    std::string displayName;

    // Tooltip/description
    std::string description;

    // Category for organization
    NodeCategory category = NodeCategory::Utility;

    // Header color (RGB)
    Math::Vector3 headerColor = Math::Vector3(0.3f, 0.3f, 0.6f);

    // Node behavior flags
    NodeDefFlags flags = NodeDefFlags::None;

    // Pin definitions
    std::vector<PinDefinition> inputs;
    std::vector<PinDefinition> outputs;

    // Execution function (for impure nodes with flow pins)
    NodeExecuteFunc execute;

    // Evaluation function (for pure nodes)
    NodeEvaluateFunc evaluate;

    // Keywords for search (in addition to display name)
    std::vector<std::string> keywords;

    // ========== Helper methods ==========

    bool IsPure() const { return HasDefFlag(flags, NodeDefFlags::Pure); }
    bool IsEvent() const { return HasDefFlag(flags, NodeDefFlags::Event); }
    bool IsLatent() const { return HasDefFlag(flags, NodeDefFlags::Latent); }
    bool IsDevelopment() const { return HasDefFlag(flags, NodeDefFlags::Development); }
    bool IsDeprecated() const { return HasDefFlag(flags, NodeDefFlags::Deprecated); }
    bool IsCompact() const { return HasDefFlag(flags, NodeDefFlags::Compact); }

    // Check if this node has flow input pin
    bool HasFlowInput() const {
        for (const auto& pin : inputs) {
            if (pin.type == Editor::PinType::Flow) return true;
        }
        return false;
    }

    // Check if this node has flow output pin
    bool HasFlowOutput() const {
        for (const auto& pin : outputs) {
            if (pin.type == Editor::PinType::Flow) return true;
        }
        return false;
    }

    // Count flow outputs (for nodes like Branch that have multiple)
    usize CountFlowOutputs() const {
        usize count = 0;
        for (const auto& pin : outputs) {
            if (pin.type == Editor::PinType::Flow) count++;
        }
        return count;
    }
};

// ============================================================================
// HELPER: CREATE COMMON PIN DEFINITIONS
// ============================================================================

namespace PinDefs {

inline PinDefinition FlowIn(const std::string& name = "") {
    return {name.empty() ? "" : name, Editor::PinType::Flow, Editor::PinKind::Input, {}, false};
}

inline PinDefinition FlowOut(const std::string& name = "") {
    return {name.empty() ? "" : name, Editor::PinType::Flow, Editor::PinKind::Output, {}, false};
}

inline PinDefinition Bool(const std::string& name, Editor::PinKind kind, bool defaultVal = false) {
    PinDefinition p;
    p.name = name;
    p.type = Editor::PinType::Bool;
    p.kind = kind;
    p.defaultValue = defaultVal;
    return p;
}

inline PinDefinition Int(const std::string& name, Editor::PinKind kind, i32 defaultVal = 0) {
    PinDefinition p;
    p.name = name;
    p.type = Editor::PinType::Int;
    p.kind = kind;
    p.defaultValue = defaultVal;
    return p;
}

inline PinDefinition Float(const std::string& name, Editor::PinKind kind, f32 defaultVal = 0.0f) {
    PinDefinition p;
    p.name = name;
    p.type = Editor::PinType::Float;
    p.kind = kind;
    p.defaultValue = defaultVal;
    return p;
}

inline PinDefinition String(const std::string& name, Editor::PinKind kind, const std::string& defaultVal = "") {
    PinDefinition p;
    p.name = name;
    p.type = Editor::PinType::String;
    p.kind = kind;
    p.defaultValue = defaultVal;
    return p;
}

inline PinDefinition Vec3(const std::string& name, Editor::PinKind kind, Math::Vector3 defaultVal = Math::Vector3(0,0,0)) {
    PinDefinition p;
    p.name = name;
    p.type = Editor::PinType::Vector3;
    p.kind = kind;
    p.defaultValue = defaultVal;
    return p;
}

inline PinDefinition EntityPin(const std::string& name, Editor::PinKind kind) {
    PinDefinition p;
    p.name = name;
    p.type = Editor::PinType::Entity;
    p.kind = kind;
    p.defaultValue = ECS::INVALID_ENTITY;
    return p;
}

inline PinDefinition Any(const std::string& name, Editor::PinKind kind) {
    PinDefinition p;
    p.name = name;
    p.type = Editor::PinType::Any;
    p.kind = kind;
    return p;
}

} // namespace PinDefs

} // namespace VisualScript
} // namespace Enjin
