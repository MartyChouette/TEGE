#include "Enjin/VisualScript/NodeRegistry.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>
#include <random>

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

    // Divide
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Divide;
        def.displayName = "Divide";
        def.description = "A / B (returns 0 if B is zero)";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 1.0f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"divide", "quotient", "slash", "/"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 0.0f;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 1.0f;
            return (b != 0.0f) ? (a / b) : 0.0f;
        };
        RegisterNode(def);
    }

    // Modulo
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Modulo;
        def.displayName = "Modulo";
        def.description = "A % B (integer remainder)";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Int("A", PK::Input, 0),
            Int("B", PK::Input, 1)
        };
        def.outputs = {Int("Result", PK::Output)};
        def.keywords = {"modulo", "remainder", "mod", "%"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 0;
            i32 a = std::holds_alternative<i32>(inputs[0]) ? std::get<i32>(inputs[0]) : 0;
            i32 b = std::holds_alternative<i32>(inputs[1]) ? std::get<i32>(inputs[1]) : 1;
            return (b != 0) ? (a % b) : 0;
        };
        RegisterNode(def);
    }

    // Power
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Power;
        def.displayName = "Power";
        def.description = "Base raised to Exponent";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("Base", PK::Input, 1.0f),
            Float("Exponent", PK::Input, 2.0f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"power", "pow", "exponent", "^"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 1.0f;
            f32 base = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 1.0f;
            f32 exp = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 2.0f;
            return std::pow(base, exp);
        };
        RegisterNode(def);
    }

    // Sqrt
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Sqrt;
        def.displayName = "Square Root";
        def.description = "Square root of Value";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Float("Value", PK::Input, 0.0f)};
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"sqrt", "square", "root"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return 0.0f;
            f32 val = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            return (val >= 0.0f) ? std::sqrt(val) : 0.0f;
        };
        RegisterNode(def);
    }

    // Abs
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Abs;
        def.displayName = "Absolute";
        def.description = "Absolute value of input";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Float("Value", PK::Input, 0.0f)};
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"abs", "absolute", "magnitude"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return 0.0f;
            f32 val = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            return std::abs(val);
        };
        RegisterNode(def);
    }

    // Min
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Min;
        def.displayName = "Min";
        def.description = "Returns the smaller of A and B";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 0.0f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"min", "minimum", "smaller"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 0.0f;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            return std::min(a, b);
        };
        RegisterNode(def);
    }

    // Max
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Max;
        def.displayName = "Max";
        def.description = "Returns the larger of A and B";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 0.0f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"max", "maximum", "larger"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 0.0f;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            return std::max(a, b);
        };
        RegisterNode(def);
    }

    // Clamp
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Clamp;
        def.displayName = "Clamp";
        def.description = "Clamps Value between Min and Max";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Float("Value", PK::Input, 0.0f),
            Float("Min", PK::Input, 0.0f),
            Float("Max", PK::Input, 1.0f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"clamp", "limit", "constrain", "range"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 3) return 0.0f;
            f32 val = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 minVal = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            f32 maxVal = std::holds_alternative<f32>(inputs[2]) ? std::get<f32>(inputs[2]) : 1.0f;
            return std::max(minVal, std::min(maxVal, val));
        };
        RegisterNode(def);
    }

    // Lerp
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Lerp;
        def.displayName = "Lerp";
        def.description = "Linear interpolation between A and B by Alpha";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 1.0f),
            Float("Alpha", PK::Input, 0.5f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"lerp", "interpolate", "blend", "mix"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 3) return 0.0f;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 1.0f;
            f32 alpha = std::holds_alternative<f32>(inputs[2]) ? std::get<f32>(inputs[2]) : 0.5f;
            return a + (b - a) * alpha;
        };
        RegisterNode(def);
    }

    // Floor
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Floor;
        def.displayName = "Floor";
        def.description = "Rounds down to nearest integer";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Float("Value", PK::Input, 0.0f)};
        def.outputs = {Int("Result", PK::Output)};
        def.keywords = {"floor", "round", "down", "integer"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return 0;
            f32 val = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            return static_cast<i32>(std::floor(val));
        };
        RegisterNode(def);
    }

    // Ceil
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Ceil;
        def.displayName = "Ceil";
        def.description = "Rounds up to nearest integer";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Float("Value", PK::Input, 0.0f)};
        def.outputs = {Int("Result", PK::Output)};
        def.keywords = {"ceil", "ceiling", "round", "up", "integer"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return 0;
            f32 val = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            return static_cast<i32>(std::ceil(val));
        };
        RegisterNode(def);
    }

    // Round
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Round;
        def.displayName = "Round";
        def.description = "Rounds to nearest integer";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Float("Value", PK::Input, 0.0f)};
        def.outputs = {Int("Result", PK::Output)};
        def.keywords = {"round", "nearest", "integer"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return 0;
            f32 val = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            return static_cast<i32>(std::round(val));
        };
        RegisterNode(def);
    }

    // Sin
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Sin;
        def.displayName = "Sin";
        def.description = "Sine of angle (radians)";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Float("Angle", PK::Input, 0.0f)};
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"sin", "sine", "trig", "angle"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return 0.0f;
            f32 val = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            return std::sin(val);
        };
        RegisterNode(def);
    }

    // Cos
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Cos;
        def.displayName = "Cos";
        def.description = "Cosine of angle (radians)";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Float("Angle", PK::Input, 0.0f)};
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"cos", "cosine", "trig", "angle"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return 1.0f;
            f32 val = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            return std::cos(val);
        };
        RegisterNode(def);
    }

    // Tan
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Tan;
        def.displayName = "Tan";
        def.description = "Tangent of angle (radians)";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Float("Angle", PK::Input, 0.0f)};
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"tan", "tangent", "trig", "angle"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return 0.0f;
            f32 val = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            return std::tan(val);
        };
        RegisterNode(def);
    }

    // Atan2
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Atan2;
        def.displayName = "Atan2";
        def.description = "Arctangent of Y/X (returns radians)";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Float("Y", PK::Input, 0.0f),
            Float("X", PK::Input, 1.0f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"atan2", "arctangent", "angle", "trig"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 0.0f;
            f32 y = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 x = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 1.0f;
            return std::atan2(y, x);
        };
        RegisterNode(def);
    }

    // Random Float
    {
        NodeDefinition def;
        def.typeId = NodeTypes::RandomFloat;
        def.displayName = "Random Float";
        def.description = "Random float between Min and Max";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Float("Min", PK::Input, 0.0f),
            Float("Max", PK::Input, 1.0f)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"random", "rand", "float", "range"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            f32 minVal = 0.0f, maxVal = 1.0f;
            if (inputs.size() >= 1 && std::holds_alternative<f32>(inputs[0])) minVal = std::get<f32>(inputs[0]);
            if (inputs.size() >= 2 && std::holds_alternative<f32>(inputs[1])) maxVal = std::get<f32>(inputs[1]);
            static std::mt19937 gen(std::random_device{}());
            std::uniform_real_distribution<f32> dist(minVal, maxVal);
            return dist(gen);
        };
        RegisterNode(def);
    }

    // Random Int
    {
        NodeDefinition def;
        def.typeId = NodeTypes::RandomInt;
        def.displayName = "Random Int";
        def.description = "Random integer between Min and Max (inclusive)";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Int("Min", PK::Input, 0),
            Int("Max", PK::Input, 100)
        };
        def.outputs = {Int("Result", PK::Output)};
        def.keywords = {"random", "rand", "int", "integer", "range"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            i32 minVal = 0, maxVal = 100;
            if (inputs.size() >= 1 && std::holds_alternative<i32>(inputs[0])) minVal = std::get<i32>(inputs[0]);
            if (inputs.size() >= 2 && std::holds_alternative<i32>(inputs[1])) maxVal = std::get<i32>(inputs[1]);
            static std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<i32> dist(minVal, maxVal);
            return dist(gen);
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

    // And
    {
        NodeDefinition def;
        def.typeId = NodeTypes::And;
        def.displayName = "And";
        def.description = "A AND B (both must be true)";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Bool("A", PK::Input, false),
            Bool("B", PK::Input, false)
        };
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"and", "both", "&&"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return false;
            bool a = std::holds_alternative<bool>(inputs[0]) ? std::get<bool>(inputs[0]) : false;
            bool b = std::holds_alternative<bool>(inputs[1]) ? std::get<bool>(inputs[1]) : false;
            return a && b;
        };
        RegisterNode(def);
    }

    // Or
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Or;
        def.displayName = "Or";
        def.description = "A OR B (either can be true)";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Bool("A", PK::Input, false),
            Bool("B", PK::Input, false)
        };
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"or", "either", "||"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return false;
            bool a = std::holds_alternative<bool>(inputs[0]) ? std::get<bool>(inputs[0]) : false;
            bool b = std::holds_alternative<bool>(inputs[1]) ? std::get<bool>(inputs[1]) : false;
            return a || b;
        };
        RegisterNode(def);
    }

    // Nand
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Nand;
        def.displayName = "Nand";
        def.description = "NOT (A AND B)";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Bool("A", PK::Input, false),
            Bool("B", PK::Input, false)
        };
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"nand", "not and"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return true;
            bool a = std::holds_alternative<bool>(inputs[0]) ? std::get<bool>(inputs[0]) : false;
            bool b = std::holds_alternative<bool>(inputs[1]) ? std::get<bool>(inputs[1]) : false;
            return !(a && b);
        };
        RegisterNode(def);
    }

    // Xor
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Xor;
        def.displayName = "Xor";
        def.description = "A XOR B (exactly one must be true)";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Bool("A", PK::Input, false),
            Bool("B", PK::Input, false)
        };
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"xor", "exclusive", "^"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return false;
            bool a = std::holds_alternative<bool>(inputs[0]) ? std::get<bool>(inputs[0]) : false;
            bool b = std::holds_alternative<bool>(inputs[1]) ? std::get<bool>(inputs[1]) : false;
            return a != b;
        };
        RegisterNode(def);
    }

    // Equal
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Equal;
        def.displayName = "Equal";
        def.description = "A == B";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 0.0f)
        };
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"equal", "equals", "same", "==", "compare"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return false;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            return std::abs(a - b) < 0.0001f;
        };
        RegisterNode(def);
    }

    // Not Equal
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NotEqual;
        def.displayName = "Not Equal";
        def.description = "A != B";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 0.0f)
        };
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"not equal", "different", "!=", "compare"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return false;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            return std::abs(a - b) >= 0.0001f;
        };
        RegisterNode(def);
    }

    // Greater Equal
    {
        NodeDefinition def;
        def.typeId = NodeTypes::GreaterEqual;
        def.displayName = "Greater or Equal";
        def.description = "A >= B";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 0.0f)
        };
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"greater", "equal", ">=", "compare"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return false;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            return a >= b;
        };
        RegisterNode(def);
    }

    // Less Equal
    {
        NodeDefinition def;
        def.typeId = NodeTypes::LessEqual;
        def.displayName = "Less or Equal";
        def.description = "A <= B";
        def.category = NodeCategory::Logic;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {
            Float("A", PK::Input, 0.0f),
            Float("B", PK::Input, 0.0f)
        };
        def.outputs = {Bool("Result", PK::Output)};
        def.keywords = {"less", "equal", "<=", "compare"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return false;
            f32 a = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 b = std::holds_alternative<f32>(inputs[1]) ? std::get<f32>(inputs[1]) : 0.0f;
            return a <= b;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // VECTOR NODES (Pure)
    // ========================================================================

    // Make Vector3
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MakeVector3;
        def.displayName = "Make Vector3";
        def.description = "Create a Vector3 from X, Y, Z components";
        def.category = NodeCategory::Vector;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Float("X", PK::Input, 0.0f),
            Float("Y", PK::Input, 0.0f),
            Float("Z", PK::Input, 0.0f)
        };
        def.outputs = {Vec3("Vector", PK::Output)};
        def.keywords = {"make", "create", "vector", "construct"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            f32 x = (inputs.size() > 0 && std::holds_alternative<f32>(inputs[0])) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 y = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.0f;
            f32 z = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ? std::get<f32>(inputs[2]) : 0.0f;
            return Math::Vector3(x, y, z);
        };
        RegisterNode(def);
    }

    // Break Vector3
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BreakVector3;
        def.displayName = "Break Vector3";
        def.description = "Extract X, Y, Z components from a Vector3";
        def.category = NodeCategory::Vector;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {Vec3("Vector", PK::Input)};
        def.outputs = {
            Float("X", PK::Output),
            Float("Y", PK::Output),
            Float("Z", PK::Output)
        };
        def.keywords = {"break", "split", "decompose", "vector"};
        // Note: Pure nodes with multiple outputs need special handling
        // The executor needs to cache all outputs; we return the first (X) here
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty() || !std::holds_alternative<Math::Vector3>(inputs[0])) return 0.0f;
            auto vec = std::get<Math::Vector3>(inputs[0]);
            return vec.x;  // Executor handles multi-output pure nodes specially
        };
        RegisterNode(def);
    }

    // Vector Length
    {
        NodeDefinition def;
        def.typeId = NodeTypes::VectorLength;
        def.displayName = "Vector Length";
        def.description = "Magnitude (length) of a vector";
        def.category = NodeCategory::Vector;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Vec3("Vector", PK::Input)};
        def.outputs = {Float("Length", PK::Output)};
        def.keywords = {"length", "magnitude", "size", "distance"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty() || !std::holds_alternative<Math::Vector3>(inputs[0])) return 0.0f;
            auto vec = std::get<Math::Vector3>(inputs[0]);
            return std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
        };
        RegisterNode(def);
    }

    // Normalize
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Normalize;
        def.displayName = "Normalize";
        def.description = "Unit vector (length = 1)";
        def.category = NodeCategory::Vector;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Vec3("Vector", PK::Input)};
        def.outputs = {Vec3("Normalized", PK::Output)};
        def.keywords = {"normalize", "unit", "direction"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty() || !std::holds_alternative<Math::Vector3>(inputs[0])) {
                return Math::Vector3(0, 0, 0);
            }
            auto vec = std::get<Math::Vector3>(inputs[0]);
            f32 len = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
            if (len < 0.0001f) return Math::Vector3(0, 0, 0);
            return Math::Vector3(vec.x / len, vec.y / len, vec.z / len);
        };
        RegisterNode(def);
    }

    // Dot Product
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DotProduct;
        def.displayName = "Dot Product";
        def.description = "Dot product of two vectors";
        def.category = NodeCategory::Vector;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Vec3("A", PK::Input),
            Vec3("B", PK::Input)
        };
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"dot", "product", "scalar"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 0.0f;
            if (!std::holds_alternative<Math::Vector3>(inputs[0]) ||
                !std::holds_alternative<Math::Vector3>(inputs[1])) return 0.0f;
            auto a = std::get<Math::Vector3>(inputs[0]);
            auto b = std::get<Math::Vector3>(inputs[1]);
            return a.x * b.x + a.y * b.y + a.z * b.z;
        };
        RegisterNode(def);
    }

    // Cross Product
    {
        NodeDefinition def;
        def.typeId = NodeTypes::CrossProduct;
        def.displayName = "Cross Product";
        def.description = "Cross product of two vectors";
        def.category = NodeCategory::Vector;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Vec3("A", PK::Input),
            Vec3("B", PK::Input)
        };
        def.outputs = {Vec3("Result", PK::Output)};
        def.keywords = {"cross", "product", "perpendicular"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return Math::Vector3(0, 0, 0);
            if (!std::holds_alternative<Math::Vector3>(inputs[0]) ||
                !std::holds_alternative<Math::Vector3>(inputs[1])) return Math::Vector3(0, 0, 0);
            auto a = std::get<Math::Vector3>(inputs[0]);
            auto b = std::get<Math::Vector3>(inputs[1]);
            return Math::Vector3(
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            );
        };
        RegisterNode(def);
    }

    // Distance
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Distance;
        def.displayName = "Distance";
        def.description = "Distance between two points";
        def.category = NodeCategory::Vector;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Vec3("A", PK::Input),
            Vec3("B", PK::Input)
        };
        def.outputs = {Float("Distance", PK::Output)};
        def.keywords = {"distance", "length", "between"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 2) return 0.0f;
            if (!std::holds_alternative<Math::Vector3>(inputs[0]) ||
                !std::holds_alternative<Math::Vector3>(inputs[1])) return 0.0f;
            auto a = std::get<Math::Vector3>(inputs[0]);
            auto b = std::get<Math::Vector3>(inputs[1]);
            f32 dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        };
        RegisterNode(def);
    }

    // Lerp Vector
    {
        NodeDefinition def;
        def.typeId = NodeTypes::LerpVector;
        def.displayName = "Lerp Vector";
        def.description = "Linear interpolation between two vectors";
        def.category = NodeCategory::Vector;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Vec3("A", PK::Input),
            Vec3("B", PK::Input),
            Float("Alpha", PK::Input, 0.5f)
        };
        def.outputs = {Vec3("Result", PK::Output)};
        def.keywords = {"lerp", "interpolate", "blend", "mix"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() < 3) return Math::Vector3(0, 0, 0);
            if (!std::holds_alternative<Math::Vector3>(inputs[0]) ||
                !std::holds_alternative<Math::Vector3>(inputs[1])) return Math::Vector3(0, 0, 0);
            auto a = std::get<Math::Vector3>(inputs[0]);
            auto b = std::get<Math::Vector3>(inputs[1]);
            f32 alpha = std::holds_alternative<f32>(inputs[2]) ? std::get<f32>(inputs[2]) : 0.5f;
            return Math::Vector3(
                a.x + (b.x - a.x) * alpha,
                a.y + (b.y - a.y) * alpha,
                a.z + (b.z - a.z) * alpha
            );
        };
        RegisterNode(def);
    }

    // ========================================================================
    // ADDITIONAL FLOW CONTROL NODES
    // ========================================================================

    // For Loop
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ForLoop;
        def.displayName = "For Loop";
        def.description = "Execute body from First to Last index. Note: uses runtime state.";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.2f);
        def.inputs = {
            FlowIn(),
            Int("First", PK::Input, 0),
            Int("Last", PK::Input, 5)
        };
        def.outputs = {
            FlowOut("Loop Body"),
            Int("Index", PK::Output),
            FlowOut("Completed")
        };
        def.keywords = {"for", "loop", "iterate", "repeat", "count"};
        // Note: ForLoop requires special executor handling for iteration state
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // The executor handles ForLoop specially by maintaining iteration state
            ctx.nextFlowIndex = 0;  // Start with loop body
        };
        RegisterNode(def);
    }

    // While Loop
    {
        NodeDefinition def;
        def.typeId = NodeTypes::WhileLoop;
        def.displayName = "While Loop";
        def.description = "Execute body while condition is true (max 1000 iterations)";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.2f);
        def.inputs = {
            FlowIn(),
            Bool("Condition", PK::Input, false)
        };
        def.outputs = {
            FlowOut("Loop Body"),
            FlowOut("Completed")
        };
        def.keywords = {"while", "loop", "repeat", "condition"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            bool condition = false;
            if (!inputs.empty() && std::holds_alternative<bool>(inputs[0])) {
                condition = std::get<bool>(inputs[0]);
            }
            ctx.nextFlowIndex = condition ? 0 : 1;  // 0 = loop body, 1 = completed
        };
        RegisterNode(def);
    }

    // Do Once
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DoOnce;
        def.displayName = "Do Once";
        def.description = "Executes once until Reset is triggered";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.2f);
        def.inputs = {
            FlowIn(),
            FlowIn("Reset")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"once", "single", "first", "reset"};
        // Note: DoOnce requires per-node state tracking in the script
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // The executor handles state tracking
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Gate
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Gate;
        def.displayName = "Gate";
        def.description = "Only passes flow when Open is true";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.2f);
        def.inputs = {
            FlowIn(),
            Bool("Open", PK::Input, true)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"gate", "open", "close", "filter"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            bool open = true;
            if (!inputs.empty() && std::holds_alternative<bool>(inputs[0])) {
                open = std::get<bool>(inputs[0]);
            }
            ctx.nextFlowIndex = open ? 0 : -1;  // -1 stops flow
        };
        RegisterNode(def);
    }

    // Flip Flop
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FlipFlop;
        def.displayName = "Flip Flop";
        def.description = "Alternates between A and B outputs";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.2f);
        def.inputs = {FlowIn()};
        def.outputs = {
            FlowOut("A"),
            FlowOut("B"),
            Bool("Is A", PK::Output)
        };
        def.keywords = {"flip", "flop", "toggle", "alternate"};
        // Note: FlipFlop requires per-node state tracking
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // The executor handles state tracking; this just sets a default
            outputs.resize(1);
            outputs[0] = true;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // ADDITIONAL TRANSFORM NODES
    // ========================================================================

    // Get Rotation
    {
        NodeDefinition def;
        def.typeId = NodeTypes::GetRotation;
        def.displayName = "Get Rotation";
        def.description = "Get an entity's rotation (Euler angles in degrees)";
        def.category = NodeCategory::Transform;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Vec3("Rotation", PK::Output)};
        def.keywords = {"rotation", "euler", "angle", "transform"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) return transform->rotation.ToEuler();
            }
            return Math::Vector3(0, 0, 0);
        };
        RegisterNode(def);
    }

    // Set Rotation
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SetRotation;
        def.displayName = "Set Rotation";
        def.description = "Set an entity's rotation (Euler angles in degrees)";
        def.category = NodeCategory::Transform;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Rotation", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"rotation", "euler", "angle", "transform"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 rot(0, 0, 0);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                rot = std::get<Math::Vector3>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) transform->rotation = Math::Quaternion::FromEuler(rot);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Get Scale
    {
        NodeDefinition def;
        def.typeId = NodeTypes::GetScale;
        def.displayName = "Get Scale";
        def.description = "Get an entity's scale";
        def.category = NodeCategory::Transform;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Vec3("Scale", PK::Output)};
        def.keywords = {"scale", "size", "transform"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) return transform->scale;
            }
            return Math::Vector3(1, 1, 1);
        };
        RegisterNode(def);
    }

    // Set Scale
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SetScale;
        def.displayName = "Set Scale";
        def.description = "Set an entity's scale";
        def.category = NodeCategory::Transform;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Scale", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"scale", "size", "transform"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 scale(1, 1, 1);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                scale = std::get<Math::Vector3>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) transform->scale = scale;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Translate
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Translate;
        def.displayName = "Translate";
        def.description = "Move entity by Delta amount";
        def.category = NodeCategory::Transform;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Delta", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"translate", "move", "offset", "delta"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 delta(0, 0, 0);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                delta = std::get<Math::Vector3>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) {
                    transform->position.x += delta.x;
                    transform->position.y += delta.y;
                    transform->position.z += delta.z;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Rotate (add to rotation)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Rotate;
        def.displayName = "Rotate";
        def.description = "Add Euler angles to entity's rotation";
        def.category = NodeCategory::Transform;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Euler", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"rotate", "turn", "spin", "euler"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 euler(0, 0, 0);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                euler = std::get<Math::Vector3>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) {
                    transform->rotation.x += euler.x;
                    transform->rotation.y += euler.y;
                    transform->rotation.z += euler.z;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Look At
    {
        NodeDefinition def;
        def.typeId = NodeTypes::LookAt;
        def.displayName = "Look At";
        def.description = "Rotate entity to face target position";
        def.category = NodeCategory::Transform;
        def.headerColor = Math::Vector3(0.4f, 0.4f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Target", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"look", "face", "aim", "direction"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 lookTarget(0, 0, 0);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                lookTarget = std::get<Math::Vector3>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) {
                    Math::Vector3 dir(
                        lookTarget.x - transform->position.x,
                        lookTarget.y - transform->position.y,
                        lookTarget.z - transform->position.z
                    );
                    // Calculate yaw (Y rotation) from XZ direction
                    f32 yaw = std::atan2(dir.x, dir.z) * (180.0f / 3.14159265f);
                    transform->rotation.y = yaw;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // ENTITY NODES
    // ========================================================================

    // Find Entity
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FindEntity;
        def.displayName = "Find Entity";
        def.description = "Find entity by name";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.5f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {String("Name", PK::Input, "")};
        def.outputs = {EntityPin("Entity", PK::Output)};
        def.keywords = {"find", "get", "name", "entity"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (!ctx.world || inputs.empty()) return ECS::INVALID_ENTITY;
            std::string name = std::holds_alternative<std::string>(inputs[0]) ?
                               std::get<std::string>(inputs[0]) : "";
            if (name.empty()) return ECS::INVALID_ENTITY;

            for (auto entity : ctx.world->GetAllEntities()) {
                if (ctx.world->HasComponent<ECS::NameComponent>(entity)) {
                    auto* nameComp = ctx.world->GetComponent<ECS::NameComponent>(entity);
                    if (nameComp && nameComp->name == name) return entity;
                }
            }
            return ECS::INVALID_ENTITY;
        };
        RegisterNode(def);
    }

    // Destroy Entity
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DestroyEntity;
        def.displayName = "Destroy Entity";
        def.description = "Destroy an entity";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.5f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"destroy", "delete", "remove", "kill"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                ctx.world->DestroyEntity(target);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Spawn Entity
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SpawnEntity;
        def.displayName = "Spawn Entity";
        def.description = "Create a new entity at position";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.5f);
        def.inputs = {
            FlowIn(),
            String("Name", PK::Input, "NewEntity"),
            Vec3("Position", PK::Input)
        };
        def.outputs = {
            FlowOut(),
            EntityPin("Entity", PK::Output)
        };
        def.keywords = {"spawn", "create", "instantiate", "new"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string name = "NewEntity";
            if (!inputs.empty() && std::holds_alternative<std::string>(inputs[0])) {
                name = std::get<std::string>(inputs[0]);
            }
            Math::Vector3 pos(0, 0, 0);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                pos = std::get<Math::Vector3>(inputs[1]);
            }
            ECS::Entity newEntity = ECS::INVALID_ENTITY;
            if (ctx.world) {
                newEntity = ctx.world->CreateEntity();
                ctx.world->AddComponent<ECS::TransformComponent>(newEntity);
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(newEntity);
                if (transform) transform->position = pos;
                ctx.world->AddComponent<ECS::NameComponent>(newEntity);
                auto* nameComp = ctx.world->GetComponent<ECS::NameComponent>(newEntity);
                if (nameComp) nameComp->name = name;
            }
            outputs.resize(1);
            outputs[0] = newEntity;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Is Valid
    {
        NodeDefinition def;
        def.typeId = NodeTypes::IsValid;
        def.displayName = "Is Valid";
        def.description = "Check if entity reference is valid";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.5f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Bool("Valid", PK::Output)};
        def.keywords = {"valid", "null", "exists", "check"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return false;
            if (!std::holds_alternative<ECS::Entity>(inputs[0])) return false;
            ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
            if (e == ECS::INVALID_ENTITY) return false;
            if (!ctx.world) return false;
            return ctx.world->IsValid(e);
        };
        RegisterNode(def);
    }

    // Get Name
    {
        NodeDefinition def;
        def.typeId = NodeTypes::GetName;
        def.displayName = "Get Name";
        def.description = "Get entity's name";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.5f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {String("Name", PK::Output, "")};
        def.keywords = {"name", "get", "entity"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                if (ctx.world->HasComponent<ECS::NameComponent>(target)) {
                    auto* nameComp = ctx.world->GetComponent<ECS::NameComponent>(target);
                    if (nameComp) return nameComp->name;
                }
            }
            return std::string("");
        };
        RegisterNode(def);
    }

    // ========================================================================
    // PHYSICS NODES
    // ========================================================================

    // Add Force
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AddForce;
        def.displayName = "Add Force";
        def.description = "Apply force to rigidbody";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.5f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Force", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"force", "push", "physics", "rigidbody"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 force(0, 0, 0);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                force = std::get<Math::Vector3>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                if (ctx.world->HasComponent<ECS::RigidbodyComponent>(target)) {
                    auto* rb = ctx.world->GetComponent<ECS::RigidbodyComponent>(target);
                    if (rb) {
                        rb->velocity.x += force.x;
                        rb->velocity.y += force.y;
                        rb->velocity.z += force.z;
                    }
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Add Impulse
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AddImpulse;
        def.displayName = "Add Impulse";
        def.description = "Apply instant impulse to rigidbody";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.5f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Impulse", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"impulse", "instant", "physics", "rigidbody"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 impulse(0, 0, 0);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                impulse = std::get<Math::Vector3>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                if (ctx.world->HasComponent<ECS::RigidbodyComponent>(target)) {
                    auto* rb = ctx.world->GetComponent<ECS::RigidbodyComponent>(target);
                    if (rb) {
                        rb->velocity.x += impulse.x;
                        rb->velocity.y += impulse.y;
                        rb->velocity.z += impulse.z;
                    }
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Set Velocity
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SetVelocity;
        def.displayName = "Set Velocity";
        def.description = "Set rigidbody velocity directly";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.5f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Velocity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"velocity", "speed", "physics", "rigidbody"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 vel(0, 0, 0);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                vel = std::get<Math::Vector3>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                if (ctx.world->HasComponent<ECS::RigidbodyComponent>(target)) {
                    auto* rb = ctx.world->GetComponent<ECS::RigidbodyComponent>(target);
                    if (rb) rb->velocity = vel;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Get Velocity
    {
        NodeDefinition def;
        def.typeId = NodeTypes::GetVelocity;
        def.displayName = "Get Velocity";
        def.description = "Get rigidbody velocity";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.5f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Vec3("Velocity", PK::Output)};
        def.keywords = {"velocity", "speed", "physics", "rigidbody"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                if (ctx.world->HasComponent<ECS::RigidbodyComponent>(target)) {
                    auto* rb = ctx.world->GetComponent<ECS::RigidbodyComponent>(target);
                    if (rb) return rb->velocity;
                }
            }
            return Math::Vector3(0, 0, 0);
        };
        RegisterNode(def);
    }

    // Set Gravity Scale
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SetGravityScale;
        def.displayName = "Set Gravity Scale";
        def.description = "Set rigidbody gravity multiplier";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.5f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Scale", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"gravity", "scale", "physics", "rigidbody"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            f32 scale = 1.0f;
            if (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) {
                scale = std::get<f32>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                if (ctx.world->HasComponent<ECS::RigidbodyComponent>(target)) {
                    auto* rb = ctx.world->GetComponent<ECS::RigidbodyComponent>(target);
                    if (rb) rb->gravityScale = scale;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // COMPONENT ACCESS NODES
    // ========================================================================

    // Get Health
    {
        NodeDefinition def;
        def.typeId = NodeTypes::GetHealth;
        def.displayName = "Get Health";
        def.description = "Get entity's current and max health";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {
            Float("Current", PK::Output),
            Float("Max", PK::Output)
        };
        def.keywords = {"health", "hp", "life", "damage"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                if (ctx.world->HasComponent<ECS::HealthComponent>(target)) {
                    auto* health = ctx.world->GetComponent<ECS::HealthComponent>(target);
                    if (health) return health->currentHealth;
                }
            }
            return 0.0f;
        };
        RegisterNode(def);
    }

    // Set Health
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SetHealth;
        def.displayName = "Set Health";
        def.description = "Set entity's current health";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.3f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Health", PK::Input, 100.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"health", "hp", "life", "set"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            f32 health = 100.0f;
            if (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) {
                health = std::get<f32>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                if (ctx.world->HasComponent<ECS::HealthComponent>(target)) {
                    auto* healthComp = ctx.world->GetComponent<ECS::HealthComponent>(target);
                    if (healthComp) healthComp->currentHealth = health;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Damage
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Damage;
        def.displayName = "Damage";
        def.description = "Apply damage to entity (subtracts from health)";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.3f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Amount", PK::Input, 10.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"damage", "hurt", "health", "attack"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            f32 amount = 10.0f;
            if (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) {
                amount = std::get<f32>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                if (ctx.world->HasComponent<ECS::HealthComponent>(target)) {
                    auto* health = ctx.world->GetComponent<ECS::HealthComponent>(target);
                    if (health) {
                        health->currentHealth -= amount;
                        if (health->currentHealth < 0.0f) health->currentHealth = 0.0f;
                    }
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // COLLISION EVENT NODES
    // ========================================================================

    // On Collision Enter
    {
        NodeDefinition def;
        def.typeId = NodeTypes::OnCollisionEnter;
        def.displayName = "On Collision Enter";
        def.description = "Called when entity starts colliding with another";
        def.category = NodeCategory::Events;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.3f);
        def.flags = NodeDefFlags::Event;
        def.outputs = {
            FlowOut(),
            EntityPin("Other Entity", PK::Output)
        };
        def.keywords = {"collision", "enter", "hit", "contact"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            outputs[0] = ctx.otherEntity;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // On Collision Exit
    {
        NodeDefinition def;
        def.typeId = NodeTypes::OnCollisionExit;
        def.displayName = "On Collision Exit";
        def.description = "Called when entity stops colliding with another";
        def.category = NodeCategory::Events;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.3f);
        def.flags = NodeDefFlags::Event;
        def.outputs = {
            FlowOut(),
            EntityPin("Other Entity", PK::Output)
        };
        def.keywords = {"collision", "exit", "leave", "separate"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            outputs[0] = ctx.otherEntity;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // On Trigger Enter
    {
        NodeDefinition def;
        def.typeId = NodeTypes::OnTriggerEnter;
        def.displayName = "On Trigger Enter";
        def.description = "Called when entity enters a trigger volume";
        def.category = NodeCategory::Events;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.3f);
        def.flags = NodeDefFlags::Event;
        def.outputs = {
            FlowOut(),
            EntityPin("Other Entity", PK::Output)
        };
        def.keywords = {"trigger", "enter", "volume", "zone"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            outputs[0] = ctx.otherEntity;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // On Trigger Exit
    {
        NodeDefinition def;
        def.typeId = NodeTypes::OnTriggerExit;
        def.displayName = "On Trigger Exit";
        def.description = "Called when entity exits a trigger volume";
        def.category = NodeCategory::Events;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.3f);
        def.flags = NodeDefFlags::Event;
        def.outputs = {
            FlowOut(),
            EntityPin("Other Entity", PK::Output)
        };
        def.keywords = {"trigger", "exit", "leave", "volume"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            outputs[0] = ctx.otherEntity;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
}

} // namespace VisualScript
} // namespace Enjin
