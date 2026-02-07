#include "Enjin/VisualScript/NodeRegistry.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace VisualScript {

// ============================================================================
// SINGLETON
// ============================================================================

NodeRegistry& NodeRegistry::Instance() {
    static NodeRegistry instance;
    return instance;
}

NodeRegistry::NodeRegistry() {
    RegisterBuiltinNodes();
    m_Initialized = true;
}

// ============================================================================
// REGISTRATION
// ============================================================================

void NodeRegistry::RegisterNode(const NodeDefinition& def) {
    m_Nodes[def.typeId] = def;
}

// ============================================================================
// LOOKUP
// ============================================================================

const NodeDefinition* NodeRegistry::FindNode(const std::string& typeId) const {
    auto it = m_Nodes.find(typeId);
    return it != m_Nodes.end() ? &it->second : nullptr;
}

std::vector<const NodeDefinition*> NodeRegistry::GetNodesByCategory(NodeCategory category) const {
    std::vector<const NodeDefinition*> result;
    for (const auto& [id, def] : m_Nodes) {
        if (def.category == category) {
            result.push_back(&def);
        }
    }
    // Sort by display name
    std::sort(result.begin(), result.end(),
        [](const NodeDefinition* a, const NodeDefinition* b) {
            return a->displayName < b->displayName;
        });
    return result;
}

std::vector<const NodeDefinition*> NodeRegistry::GetAllNodes() const {
    std::vector<const NodeDefinition*> result;
    result.reserve(m_Nodes.size());
    for (const auto& [id, def] : m_Nodes) {
        result.push_back(&def);
    }
    std::sort(result.begin(), result.end(),
        [](const NodeDefinition* a, const NodeDefinition* b) {
            return a->displayName < b->displayName;
        });
    return result;
}

std::vector<const NodeDefinition*> NodeRegistry::SearchNodes(const std::string& query) const {
    if (query.empty()) return GetAllNodes();

    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    std::vector<std::pair<const NodeDefinition*, int>> scored;

    for (const auto& [id, def] : m_Nodes) {
        int score = 0;

        // Check display name
        std::string lowerName = def.displayName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        if (lowerName == lowerQuery) {
            score = 100;  // Exact match
        } else if (lowerName.find(lowerQuery) == 0) {
            score = 90;   // Prefix match
        } else if (lowerName.find(lowerQuery) != std::string::npos) {
            score = 70;   // Substring match
        }

        // Check keywords
        for (const auto& kw : def.keywords) {
            std::string lowerKw = kw;
            std::transform(lowerKw.begin(), lowerKw.end(), lowerKw.begin(), ::tolower);
            if (lowerKw.find(lowerQuery) != std::string::npos) {
                score = std::max(score, 50);
            }
        }

        // Check type ID
        std::string lowerId = def.typeId;
        std::transform(lowerId.begin(), lowerId.end(), lowerId.begin(), ::tolower);
        if (lowerId.find(lowerQuery) != std::string::npos) {
            score = std::max(score, 40);
        }

        if (score > 0) {
            scored.push_back({&def, score});
        }
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<const NodeDefinition*> result;
    result.reserve(scored.size());
    for (const auto& [def, score] : scored) {
        result.push_back(def);
    }
    return result;
}

std::vector<NodeCategory> NodeRegistry::GetActiveCategories() const {
    std::vector<NodeCategory> result;
    std::unordered_map<NodeCategory, bool> seen;

    for (const auto& [id, def] : m_Nodes) {
        if (!seen[def.category]) {
            seen[def.category] = true;
            result.push_back(def.category);
        }
    }

    // Sort by enum order
    std::sort(result.begin(), result.end());
    return result;
}

// ============================================================================
// BUILT-IN NODE DEFINITIONS
// ============================================================================

void NodeRegistry::RegisterBuiltinNodes() {
    using namespace PinDefs;
    using PK = Editor::PinKind;

    // ========================================================================
    // EVENT NODES
    // ========================================================================

    // On Start
    {
        NodeDefinition def;
        def.typeId = NodeTypes::OnStart;
        def.displayName = "On Start";
        def.description = "Called once when play mode begins";
        def.category = NodeCategory::Events;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.3f);  // Green
        def.flags = NodeDefFlags::Event;
        def.outputs = {FlowOut()};
        def.keywords = {"start", "begin", "init", "awake"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ctx.nextFlowIndex = 0;  // Continue to first flow output
        };
        RegisterNode(def);
    }

    // On Update
    {
        NodeDefinition def;
        def.typeId = NodeTypes::OnUpdate;
        def.displayName = "On Update";
        def.description = "Called every frame. Outputs Delta Time.";
        def.category = NodeCategory::Events;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.3f);
        def.flags = NodeDefFlags::Event;
        def.outputs = {
            FlowOut(),
            Float("Delta Time", PK::Output)
        };
        def.keywords = {"update", "tick", "frame", "loop"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            outputs[0] = ctx.deltaTime;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // FLOW CONTROL NODES
    // ========================================================================

    // Branch (If)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Branch;
        def.displayName = "Branch";
        def.description = "If/Else flow control. Executes True or False path based on Condition.";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.2f);  // Orange-brown
        def.inputs = {
            FlowIn(),
            Bool("Condition", PK::Input, false)
        };
        def.outputs = {
            FlowOut("True"),
            FlowOut("False")
        };
        def.keywords = {"if", "else", "condition", "branch"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            bool condition = false;
            if (inputs.size() > 0) {
                if (std::holds_alternative<bool>(inputs[0])) {
                    condition = std::get<bool>(inputs[0]);
                }
            }
            ctx.nextFlowIndex = condition ? 0 : 1;  // 0 = True, 1 = False
        };
        RegisterNode(def);
    }

    // Sequence
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Sequence;
        def.displayName = "Sequence";
        def.description = "Executes multiple paths in order (Then 0, Then 1, etc.)";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.2f);
        def.inputs = {FlowIn()};
        def.outputs = {
            FlowOut("Then 0"),
            FlowOut("Then 1"),
            FlowOut("Then 2")
        };
        def.keywords = {"sequence", "order", "series", "chain"};
        // Note: Sequence is special - executor handles executing all outputs sequentially
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // Executor handles multi-output for Sequence specially
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // VARIABLE NODES
    // ========================================================================

    // Get Variable
    {
        NodeDefinition def;
        def.typeId = NodeTypes::GetVariable;
        def.displayName = "Get Variable";
        def.description = "Read a variable's value. Variable name is set in node properties.";
        def.category = NodeCategory::Variables;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.4f);  // Muted green
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Any("Value", PK::Output)};
        def.keywords = {"variable", "get", "read", "load"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            // Variable name comes from node metadata (properties["variableName"])
            // This is handled specially by the executor
            return false;  // Default
        };
        RegisterNode(def);
    }

    // Set Variable
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SetVariable;
        def.displayName = "Set Variable";
        def.description = "Write a value to a variable. Variable name is set in node properties.";
        def.category = NodeCategory::Variables;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.4f);
        def.inputs = {
            FlowIn(),
            Any("Value", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"variable", "set", "write", "store", "assign"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // Variable name from node metadata, handled by executor
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Get Self
    {
        NodeDefinition def;
        def.typeId = NodeTypes::GetSelf;
        def.displayName = "Get Self";
        def.description = "Returns the entity this script is attached to";
        def.category = NodeCategory::Variables;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.4f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.outputs = {EntityPin("Self", PK::Output)};
        def.keywords = {"self", "this", "entity", "owner"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            return ctx.entity;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // MATH NODES (Pure)
    // ========================================================================

    // Add
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Add;
        def.displayName = "Add";
        def.description = "A + B (works with Float, Int, or Vector3)";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 0.0f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"add", "plus", "sum", "+"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 0.0f;

            // Handle different types
            if (std::holds_alternative<f32>(inputs[0]) && std::holds_alternative<f32>(inputs[1])) {
                return std::get<f32>(inputs[0]) + std::get<f32>(inputs[1]);
            }
            if (std::holds_alternative<i32>(inputs[0]) && std::holds_alternative<i32>(inputs[1])) {
                return std::get<i32>(inputs[0]) + std::get<i32>(inputs[1]);
            }
            if (std::holds_alternative<Math::Vector3>(inputs[0]) && std::holds_alternative<Math::Vector3>(inputs[1])) {
                auto a = std::get<Math::Vector3>(inputs[0]);
                auto b = std::get<Math::Vector3>(inputs[1]);
                return Math::Vector3(a.x + b.x, a.y + b.y, a.z + b.z);
            }
            // Default: try as float
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            return a + b;
        };
        RegisterNode(def);
    }

    // Subtract
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Subtract;
        def.displayName = "Subtract";
        def.description = "A - B";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 0.0f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"subtract", "minus", "difference", "-"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 0.0f;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            return a - b;
        };
        RegisterNode(def);
    }

    // Multiply
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Multiply;
        def.displayName = "Multiply";
        def.description = "A * B";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 1.0f),
            Float("B", PK::Input, 1.0f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"multiply", "times", "product", "*"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 0.0f;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 1.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 1.0f;
            return a * b;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // TRANSFORM NODES
    // ========================================================================

    // Get Position
    {
        NodeDefinition def;
        def.typeId = NodeTypes::GetPosition;
        def.displayName = "Get Position";
        def.description = "Get an entity's world position";
        def.category = NodeCategory::Transform;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);  // Blue-ish
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Vec3("Position", PK::Output)};
        def.keywords = {"position", "location", "transform", "xyz"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;  // Default to self
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) {
                    return transform->position;
                }
            }
            return Math::Vector3(0, 0, 0);
        };
        RegisterNode(def);
    }

    // Set Position
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SetPosition;
        def.displayName = "Set Position";
        def.description = "Set an entity's world position";
        def.category = NodeCategory::Transform;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Position", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"position", "location", "move", "teleport", "transform"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            Math::Vector3 pos(0, 0, 0);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                pos = std::get<Math::Vector3>(inputs[1]);
            }

            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) {
                    transform->position = pos;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // DEBUG NODES
    // ========================================================================

    // Print String
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Print;
        def.displayName = "Print String";
        def.description = "Print a message to the console";
        def.category = NodeCategory::Debug;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.5f);  // Purple
        def.flags = NodeDefFlags::Development;
        def.inputs = {
            FlowIn(),
            String("Message", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"print", "log", "debug", "console", "output"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string msg = "[VisualScript] ";
            if (inputs.size() > 0) {
                if (std::holds_alternative<std::string>(inputs[0])) {
                    msg += std::get<std::string>(inputs[0]);
                } else if (std::holds_alternative<f32>(inputs[0])) {
                    msg += std::to_string(std::get<f32>(inputs[0]));
                } else if (std::holds_alternative<i32>(inputs[0])) {
                    msg += std::to_string(std::get<i32>(inputs[0]));
                } else if (std::holds_alternative<bool>(inputs[0])) {
                    msg += std::get<bool>(inputs[0]) ? "true" : "false";
                } else if (std::holds_alternative<Math::Vector3>(inputs[0])) {
                    auto v = std::get<Math::Vector3>(inputs[0]);
                    msg += "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
                }
            }
            ENJIN_LOG_INFO(Script, "%s", msg.c_str());
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // LOGIC NODES (Pure)
    // ========================================================================

    // Greater Than
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Greater;
        def.displayName = "Greater Than";
        def.description = "A > B";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 0.0f)
        };
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"greater", "more", "larger", ">", "compare"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return false;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            return a > b;
        };
        RegisterNode(def);
    }

    // Less Than
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Less;
        def.displayName = "Less Than";
        def.description = "A < B";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 0.0f)
        };
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"less", "smaller", "<", "compare"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return false;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            return a < b;
        };
        RegisterNode(def);
    }

    // Not
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Not;
        def.displayName = "Not";
        def.description = "Boolean NOT (inverts true/false)";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Bool("Value", PK::Input, false)};
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"not", "invert", "negate", "!", "opposite"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return true;
            bool val = std::holds_alternative<bool>(inputs[0]) ? std::get<bool>(inputs[0]) : false;
            return !val;
        };
        RegisterNode(def);
    }
}

} // namespace VisualScript
} // namespace Enjin
