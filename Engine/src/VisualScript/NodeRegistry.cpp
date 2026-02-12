#include "Enjin/VisualScript/NodeRegistry.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Gameplay/QuestSystem.h"
#include "Enjin/Gameplay/CinematicSystem.h"
#include "Enjin/Gameplay/ObjectPool.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Destructible.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/Localization.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>
#include <random>

// Global pointer for visual script save system access (set by PlayMode)
Enjin::Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem = nullptr;

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

    // Delay (Latent)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Delay;
        def.displayName = "Delay";
        def.description = "Wait for a specified duration before continuing. Execution pauses at this node.";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.2f);
        def.flags = NodeDefFlags::Latent;
        def.inputs = {
            FlowIn(),
            Float("Duration", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut("Completed")};
        def.keywords = {"delay", "wait", "pause", "sleep", "time", "timer"};
        // Note: Delay is handled specially by the executor (latent action)
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // Executor handles Delay specially - starts latent action
            ctx.nextFlowIndex = -1;  // Stop synchronous execution
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
            return ctx.world->FindEntityByName(name);
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

    // ========================================================================
    // AUDIO NODES
    // ========================================================================

    // Audio Play
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AudioPlay;
        def.displayName = "Play Audio";
        def.description = "Start playing audio on an entity with AudioSourceComponent";
        def.category = NodeCategory::Audio;
        def.headerColor = Math::Vector3(0.6f, 0.2f, 0.6f);  // Magenta
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"audio", "sound", "play", "music", "sfx"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* audio = ctx.world->GetComponent<ECS::AudioSourceComponent>(target);
                if (audio && !audio->clipPath.empty()) {
                    audio->playOnAwake = true;  // Trigger play on next audio system update
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Audio Stop
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AudioStop;
        def.displayName = "Stop Audio";
        def.description = "Stop playing audio on an entity";
        def.category = NodeCategory::Audio;
        def.headerColor = Math::Vector3(0.6f, 0.2f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"audio", "sound", "stop", "mute", "silence"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* audio = ctx.world->GetComponent<ECS::AudioSourceComponent>(target);
                if (audio) {
                    audio->playOnAwake = false;  // Stop on next audio system update
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Audio Is Playing (Pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AudioIsPlaying;
        def.displayName = "Is Audio Playing";
        def.description = "Check if audio is playing on an entity";
        def.category = NodeCategory::Audio;
        def.headerColor = Math::Vector3(0.6f, 0.2f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Bool("Playing", PK::Output)};
        def.keywords = {"audio", "sound", "playing", "active", "check"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* audio = ctx.world->GetComponent<ECS::AudioSourceComponent>(target);
                if (audio) {
                    return audio->playOnAwake;  // Simplified check
                }
            }
            return false;
        };
        RegisterNode(def);
    }

    // Audio Set Volume
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AudioSetVolume;
        def.displayName = "Set Audio Volume";
        def.description = "Set the volume of an audio source (0.0 to 1.0)";
        def.category = NodeCategory::Audio;
        def.headerColor = Math::Vector3(0.6f, 0.2f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Volume", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"audio", "sound", "volume", "loudness", "level"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            f32 volume = 1.0f;
            if (inputs.size() > 1) {
                if (std::holds_alternative<f32>(inputs[1])) {
                    volume = std::get<f32>(inputs[1]);
                } else if (std::holds_alternative<i32>(inputs[1])) {
                    volume = static_cast<f32>(std::get<i32>(inputs[1]));
                }
            }
            volume = Math::Clamp(volume, 0.0f, 1.0f);

            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* audio = ctx.world->GetComponent<ECS::AudioSourceComponent>(target);
                if (audio) {
                    audio->volume = volume;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // ANIMATION NODES
    // ========================================================================

    // Animator Play
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AnimatorPlay;
        def.displayName = "Play Animation";
        def.description = "Play an animation on an entity with AnimatorComponent";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);  // Blue
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            String("Animation", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"animator", "animation", "play", "skeletal", "anim"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            std::string animName;
            if (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) {
                animName = std::get<std::string>(inputs[1]);
            }

            if (ctx.world && target != ECS::INVALID_ENTITY && !animName.empty()) {
                auto* animator = ctx.world->GetComponent<ECS::AnimatorComponent>(target);
                if (animator) {
                    animator->animator.Play(animName);
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Animator Set Speed
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AnimatorSetSpeed;
        def.displayName = "Set Animation Speed";
        def.description = "Set the playback speed of an animator (1.0 = normal)";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Speed", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"animator", "animation", "speed", "playback", "rate"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            f32 speed = 1.0f;
            if (inputs.size() > 1) {
                if (std::holds_alternative<f32>(inputs[1])) {
                    speed = std::get<f32>(inputs[1]);
                } else if (std::holds_alternative<i32>(inputs[1])) {
                    speed = static_cast<f32>(std::get<i32>(inputs[1]));
                }
            }

            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* animator = ctx.world->GetComponent<ECS::AnimatorComponent>(target);
                if (animator) {
                    animator->animator.SetSpeed(speed);
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Animator Get Speed (Pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AnimatorGetSpeed;
        def.displayName = "Get Animation Speed";
        def.description = "Get the playback speed of an animator";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Float("Speed", PK::Output)};
        def.keywords = {"animator", "animation", "speed", "playback", "rate"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* animator = ctx.world->GetComponent<ECS::AnimatorComponent>(target);
                if (animator) {
                    return animator->animator.GetSpeed();
                }
            }
            return 1.0f;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // LATENT NODES (Wait for completion)
    // ========================================================================

    // Wait For Audio Complete (Latent)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::WaitForAudioComplete;
        def.displayName = "Wait For Audio";
        def.description = "Wait until audio finishes playing on an entity before continuing";
        def.category = NodeCategory::Audio;
        def.headerColor = Math::Vector3(0.6f, 0.2f, 0.6f);  // Magenta
        def.flags = NodeDefFlags::Latent;
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut("Completed")};
        def.keywords = {"audio", "wait", "complete", "finish", "sound", "done"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // Latent action - handled by executor
            ctx.nextFlowIndex = -1;
        };
        RegisterNode(def);
    }

    // Wait For Animation Complete (Latent)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::WaitForAnimationComplete;
        def.displayName = "Wait For Animation";
        def.description = "Wait until animation finishes playing on an entity before continuing";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);  // Blue
        def.flags = NodeDefFlags::Latent;
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            String("Animation", PK::Input, "")
        };
        def.outputs = {FlowOut("Completed")};
        def.keywords = {"animation", "wait", "complete", "finish", "animator", "done"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // Latent action - handled by executor
            ctx.nextFlowIndex = -1;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // PHYSICS QUERY NODES
    // ========================================================================

    // Physics Raycast
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Raycast;
        def.displayName = "Raycast";
        def.description = "Cast a ray and check for collisions";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.3f);
        def.inputs = {
            FlowIn(),
            Vec3("Origin", PK::Input),
            Vec3("Direction", PK::Input, Math::Vector3(0, 0, -1)),
            Float("Max Distance", PK::Input, 1000.0f),
            Int("Layer Mask", PK::Input, static_cast<i32>(0x7FFFFFFF))
        };
        def.outputs = {
            FlowOut("Hit"),
            FlowOut("Miss"),
            Bool("Did Hit", PK::Output),
            Vec3("Hit Point", PK::Output),
            Vec3("Hit Normal", PK::Output),
            Float("Distance", PK::Output),
            EntityPin("Hit Entity", PK::Output)
        };
        def.keywords = {"raycast", "ray", "cast", "trace", "line", "physics"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(5);

            if (!ctx.physics) {
                outputs[0] = false;
                outputs[1] = Math::Vector3(0, 0, 0);
                outputs[2] = Math::Vector3(0, 0, 0);
                outputs[3] = 0.0f;
                outputs[4] = ECS::INVALID_ENTITY;
                ctx.nextFlowIndex = 1; // Miss
                return;
            }

            Math::Vector3 origin = (inputs.size() > 0 && std::holds_alternative<Math::Vector3>(inputs[0]))
                ? std::get<Math::Vector3>(inputs[0]) : Math::Vector3(0, 0, 0);
            Math::Vector3 direction = (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1]))
                ? std::get<Math::Vector3>(inputs[1]) : Math::Vector3(0, 0, -1);
            f32 maxDist = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 1000.0f;
            u32 layerMask = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3]))
                ? static_cast<u32>(std::get<i32>(inputs[3])) : 0xFFFFFFFF;

            // Normalize direction
            f32 len = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
            if (len > 0.0001f) {
                direction.x /= len;
                direction.y /= len;
                direction.z /= len;
            }

            Physics::Ray ray;
            ray.origin = origin;
            ray.direction = direction;

            Physics::RaycastHit hit = ctx.physics->Raycast(ray, maxDist, layerMask);

            outputs[0] = hit.hit;
            outputs[1] = hit.hit ? hit.point : Math::Vector3(0, 0, 0);
            outputs[2] = hit.hit ? hit.normal : Math::Vector3(0, 0, 0);
            outputs[3] = hit.hit ? hit.distance : 0.0f;
            outputs[4] = hit.hit ? hit.entity : ECS::INVALID_ENTITY;

            ctx.nextFlowIndex = hit.hit ? 0 : 1;
        };
        RegisterNode(def);
    }

    // Physics Sphere Check
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SphereCheck;
        def.displayName = "Sphere Check";
        def.description = "Find all entities within a sphere radius";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.3f);
        def.inputs = {
            FlowIn(),
            Vec3("Center", PK::Input),
            Float("Radius", PK::Input, 5.0f),
            Int("Layer Mask", PK::Input, static_cast<i32>(0x7FFFFFFF))
        };
        def.outputs = {
            FlowOut(),
            Bool("Has Hits", PK::Output),
            EntityPin("First Entity", PK::Output)
        };
        def.keywords = {"sphere", "overlap", "check", "radius", "physics"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(2);

            if (!ctx.physics) {
                outputs[0] = false;
                outputs[1] = ECS::INVALID_ENTITY;
                ctx.nextFlowIndex = 0;
                return;
            }

            Math::Vector3 center = (inputs.size() > 0 && std::holds_alternative<Math::Vector3>(inputs[0]))
                ? std::get<Math::Vector3>(inputs[0]) : Math::Vector3(0, 0, 0);
            f32 radius = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1]))
                ? std::get<f32>(inputs[1]) : 5.0f;
            u32 layerMask = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2]))
                ? static_cast<u32>(std::get<i32>(inputs[2])) : 0xFFFFFFFF;

            auto entities = ctx.physics->GetCollidersInRadius(center, radius, layerMask);
            bool hasHits = !entities.empty();

            outputs[0] = hasHits;
            outputs[1] = hasHits ? entities[0] : ECS::INVALID_ENTITY;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Physics Box Check
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BoxCheck;
        def.displayName = "Box Check";
        def.description = "Find all entities within a box volume";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.3f);
        def.inputs = {
            FlowIn(),
            Vec3("Center", PK::Input),
            Vec3("Half Extents", PK::Input, Math::Vector3(1, 1, 1)),
            Int("Layer Mask", PK::Input, static_cast<i32>(0x7FFFFFFF))
        };
        def.outputs = {
            FlowOut(),
            Bool("Has Hits", PK::Output),
            EntityPin("First Entity", PK::Output)
        };
        def.keywords = {"box", "overlap", "check", "aabb", "physics"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(2);

            if (!ctx.physics) {
                outputs[0] = false;
                outputs[1] = ECS::INVALID_ENTITY;
                ctx.nextFlowIndex = 0;
                return;
            }

            Math::Vector3 center = (inputs.size() > 0 && std::holds_alternative<Math::Vector3>(inputs[0]))
                ? std::get<Math::Vector3>(inputs[0]) : Math::Vector3(0, 0, 0);
            Math::Vector3 halfExtents = (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1]))
                ? std::get<Math::Vector3>(inputs[1]) : Math::Vector3(1, 1, 1);
            u32 layerMask = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2]))
                ? static_cast<u32>(std::get<i32>(inputs[2])) : 0xFFFFFFFF;

            auto entities = ctx.physics->OverlapBox(center, halfExtents, layerMask);
            bool hasHits = !entities.empty();

            outputs[0] = hasHits;
            outputs[1] = hasHits ? entities[0] : ECS::INVALID_ENTITY;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // MISSING STUB NODES
    // ========================================================================

    // Math Negate (Pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Negate;
        def.displayName = "Negate";
        def.description = "Returns -Value";
        def.category = NodeCategory::Math;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure | NodeDefFlags::Compact;
        def.inputs = {Float("Value", PK::Input, 0.0f)};
        def.outputs = {Float("Result", PK::Output)};
        def.keywords = {"negate", "negative", "minus", "invert"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.empty()) return 0.0f;
            f32 val = std::holds_alternative<f32>(inputs[0]) ? std::get<f32>(inputs[0]) : 0.0f;
            return -val;
        };
        RegisterNode(def);
    }

    // Component Has (Pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::HasComponent;
        def.displayName = "Has Component";
        def.description = "Check if an entity has a specific component type by name";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            EntityPin("Entity", PK::Input),
            String("Component", PK::Input, "HealthComponent")
        };
        def.outputs = {Bool("Has", PK::Output)};
        def.keywords = {"has", "component", "check", "exists"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (!ctx.world || inputs.size() < 2) return false;

            ECS::Entity entity = ctx.entity;
            if (std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) entity = e;
            }

            std::string compName;
            if (std::holds_alternative<std::string>(inputs[1])) {
                compName = std::get<std::string>(inputs[1]);
            }

            if (compName == "HealthComponent" || compName == "Health") {
                return ctx.world->HasComponent<ECS::HealthComponent>(entity);
            }
            if (compName == "TransformComponent" || compName == "Transform") {
                return ctx.world->HasComponent<ECS::TransformComponent>(entity);
            }
            if (compName == "NameComponent" || compName == "Name") {
                return ctx.world->HasComponent<ECS::NameComponent>(entity);
            }
            if (compName == "AnimatorComponent" || compName == "Animator") {
                return ctx.world->HasComponent<ECS::AnimatorComponent>(entity);
            }

            return false;
        };
        RegisterNode(def);
    }

    // Debug Print Warning
    {
        NodeDefinition def;
        def.typeId = NodeTypes::PrintWarning;
        def.displayName = "Print Warning";
        def.description = "Print a warning message to the console";
        def.category = NodeCategory::Debug;
        def.headerColor = Math::Vector3(0.6f, 0.5f, 0.2f);  // Yellow-ish
        def.flags = NodeDefFlags::Development;
        def.inputs = {
            FlowIn(),
            String("Message", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"print", "warn", "warning", "log", "debug"};
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
                }
            }
            ENJIN_LOG_WARN(Script, "%s", msg.c_str());
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Debug Print Error
    {
        NodeDefinition def;
        def.typeId = NodeTypes::PrintError;
        def.displayName = "Print Error";
        def.description = "Print an error message to the console";
        def.category = NodeCategory::Debug;
        def.headerColor = Math::Vector3(0.7f, 0.2f, 0.2f);  // Red
        def.flags = NodeDefFlags::Development;
        def.inputs = {
            FlowIn(),
            String("Message", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"print", "error", "log", "debug"};
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
                }
            }
            ENJIN_LOG_ERROR(Script, "%s", msg.c_str());
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // FUNCTION NODES (Subgraphs)
    // ========================================================================

    // Function Entry (event-style node inside a function subgraph)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FunctionEntry;
        def.displayName = "Function Entry";
        def.description = "Entry point for a function subgraph. Outputs flow and parameters.";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.2f, 0.7f, 0.4f);
        def.flags = NodeDefFlags::Event;
        def.outputs = {FlowOut()};
        def.keywords = {"function", "entry", "subgraph", "start"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Function Return (terminates function execution)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FunctionReturn;
        def.displayName = "Function Return";
        def.description = "Returns from a function subgraph";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.7f, 0.3f, 0.3f);
        def.inputs = {FlowIn()};
        def.keywords = {"function", "return", "end"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ctx.nextFlowIndex = -1; // Stop execution (return to caller)
        };
        RegisterNode(def);
    }

    // Function Call (calls a user-defined function)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FunctionCall;
        def.displayName = "Call Function";
        def.description = "Call a user-defined function by name";
        def.category = NodeCategory::FlowControl;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            String("Function", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"function", "call", "invoke", "subgraph"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // Execution is handled specially in VisualScriptExecutor
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // SCRIPT INTEROP
    // ========================================================================

    // Script Call (call an AngelScript function from visual script)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ScriptCall;
        def.displayName = "Call Script";
        def.description = "Call an AngelScript function by module and name";
        def.category = NodeCategory::Utility;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.6f);
        def.inputs = {
            FlowIn(),
            String("Module", PK::Input, ""),
            String("Function", PK::Input, ""),
            Float("Arg1", PK::Input, 0.0f),
            Float("Arg2", PK::Input, 0.0f)
        };
        def.outputs = {
            FlowOut(),
            Float("Return", PK::Output)
        };
        def.keywords = {"script", "call", "angelscript", "function"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            outputs[0] = 0.0f;

            std::string module = (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            std::string func = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            f32 arg1 = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 0.0f;
            f32 arg2 = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3]))
                ? std::get<f32>(inputs[3]) : 0.0f;

            if (ctx.scriptEngine) {
                asIScriptEngine* asEngine = ctx.scriptEngine->GetASEngine();
                if (asEngine) {
                    asIScriptModule* mod = asEngine->GetModule(module.c_str());
                    if (mod) {
                        // Try to find function by declaration pattern
                        std::string decl = "float " + func + "(float, float)";
                        asIScriptFunction* asFunc = mod->GetFunctionByDecl(decl.c_str());
                        if (!asFunc) {
                            // Try without args
                            decl = "float " + func + "()";
                            asFunc = mod->GetFunctionByDecl(decl.c_str());
                        }

                        if (asFunc) {
                            asIScriptContext* asCtx = ctx.scriptEngine->AcquireContext();
                            if (asCtx) {
                                asCtx->Prepare(asFunc);
                                if (asFunc->GetParamCount() >= 2) {
                                    asCtx->SetArgFloat(0, arg1);
                                    asCtx->SetArgFloat(1, arg2);
                                } else if (asFunc->GetParamCount() >= 1) {
                                    asCtx->SetArgFloat(0, arg1);
                                }

                                int r = asCtx->Execute();
                                if (r == asEXECUTION_FINISHED) {
                                    outputs[0] = asCtx->GetReturnFloat();
                                } else {
                                    ENJIN_LOG_WARN(Script, "[VisualScript] Script call failed (exec error): %s::%s",
                                                   module.c_str(), func.c_str());
                                }
                                ctx.scriptEngine->ReturnContext(asCtx);
                            }
                        } else {
                            ENJIN_LOG_WARN(Script, "[VisualScript] Script function not found: %s::%s",
                                           module.c_str(), func.c_str());
                        }
                    } else {
                        ENJIN_LOG_WARN(Script, "[VisualScript] Script module not found: %s", module.c_str());
                    }
                }
            } else {
                ENJIN_LOG_WARN(Script, "[VisualScript] Script call failed: no script engine available");
            }

            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // DATA ASSETS
    // ========================================================================

    // DataAsset Load (impure — check if asset is available)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DataAssetLoad;
        def.displayName = "Load Data Asset";
        def.description = "Check if a data asset is loaded by name";
        def.category = NodeCategory::Utility;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.5f);
        def.inputs = {
            FlowIn(),
            String("Asset Name", PK::Input, "")
        };
        def.outputs = {
            FlowOut(),
            Bool("Found", PK::Output)
        };
        def.keywords = {"data", "asset", "load", "scriptable", "object"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            std::string assetName = (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            bool found = Assets::DataAssetRegistry::Get().FindAsset(assetName) != nullptr;
            outputs[0] = found;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // DataAsset GetFloat (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DataAssetGetFloat;
        def.displayName = "Get Data Float";
        def.description = "Get a float field from a data asset";
        def.category = NodeCategory::Utility;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.5f);
        def.inputs = {
            String("Asset Name", PK::Input, ""),
            String("Field", PK::Input, "")
        };
        def.outputs = {
            Float("Value", PK::Output)
        };
        def.keywords = {"data", "asset", "get", "float", "field"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            std::string assetName = (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            std::string field = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            return Assets::DataAssetRegistry::Get().GetFloat(assetName, field);
        };
        RegisterNode(def);
    }

    // DataAsset GetString (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DataAssetGetString;
        def.displayName = "Get Data String";
        def.description = "Get a string field from a data asset";
        def.category = NodeCategory::Utility;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.5f);
        def.inputs = {
            String("Asset Name", PK::Input, ""),
            String("Field", PK::Input, "")
        };
        def.outputs = {
            String("Value", PK::Output, "")
        };
        def.keywords = {"data", "asset", "get", "string", "field"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            std::string assetName = (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            std::string field = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            return Assets::DataAssetRegistry::Get().GetString(assetName, field);
        };
        RegisterNode(def);
    }

    // ========================================================================
    // GAMEPLAY — SAVE SYSTEM
    // ========================================================================

    // Save to Slot (flow node)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SaveToSlot;
        def.displayName = "Save To Slot";
        def.description = "Save current game state to a numbered slot (0-19)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.7f, 0.4f);
        def.inputs = {
            FlowIn(),
            Int("Slot", PK::Input, 0)
        };
        def.outputs = {
            {PinDefinition{"Success", Editor::PinType::Flow, Editor::PinKind::Output, {}, false}},
            {PinDefinition{"Failed", Editor::PinType::Flow, Editor::PinKind::Output, {}, false}}
        };
        def.keywords = {"save", "slot", "persist", "game"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 slot = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1]))
                ? std::get<i32>(inputs[1]) : 0;
            // Access save system via script engine bindings
            extern Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
            bool ok = false;
            if (s_VisualScriptSaveSystem && ctx.world) {
                ok = s_VisualScriptSaveSystem->SaveToSlot(static_cast<u32>(slot), ctx.world,
                    s_VisualScriptSaveSystem->GetCurrentScene());
            }
            ctx.nextFlowIndex = ok ? 0 : 1;
        };
        RegisterNode(def);
    }

    // Load from Slot (flow node)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::LoadFromSlot;
        def.displayName = "Load From Slot";
        def.description = "Load game state from a numbered save slot";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.7f, 0.4f);
        def.inputs = {
            FlowIn(),
            Int("Slot", PK::Input, 0)
        };
        def.outputs = {
            {PinDefinition{"Success", Editor::PinType::Flow, Editor::PinKind::Output, {}, false}},
            {PinDefinition{"Failed", Editor::PinType::Flow, Editor::PinKind::Output, {}, false}}
        };
        def.keywords = {"load", "slot", "restore", "game"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 slot = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1]))
                ? std::get<i32>(inputs[1]) : 0;
            extern Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
            bool ok = false;
            if (s_VisualScriptSaveSystem && ctx.world) {
                ok = s_VisualScriptSaveSystem->LoadFromSlot(static_cast<u32>(slot), ctx.world);
            }
            ctx.nextFlowIndex = ok ? 0 : 1;
        };
        RegisterNode(def);
    }

    // Delete Slot (flow node)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DeleteSlot;
        def.displayName = "Delete Save Slot";
        def.description = "Delete save data in the specified slot";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.7f, 0.3f, 0.3f);
        def.inputs = {
            FlowIn(),
            Int("Slot", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"delete", "slot", "erase", "save"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 slot = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1]))
                ? std::get<i32>(inputs[1]) : 0;
            extern Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
            if (s_VisualScriptSaveSystem) {
                s_VisualScriptSaveSystem->DeleteSlot(static_cast<u32>(slot));
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Checkpoint (flow node)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Checkpoint;
        def.displayName = "Checkpoint";
        def.description = "Save a checkpoint to rotating auto-save slots";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.7f, 0.4f);
        def.inputs = {FlowIn()};
        def.outputs = {FlowOut()};
        def.keywords = {"checkpoint", "autosave", "save"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            extern Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
            if (s_VisualScriptSaveSystem && ctx.world) {
                s_VisualScriptSaveSystem->Checkpoint(ctx.world,
                    s_VisualScriptSaveSystem->GetCurrentScene());
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Meta Set Float (flow node)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MetaSetFloat;
        def.displayName = "Set Meta Float";
        def.description = "Set a permanent meta-progression float value";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.inputs = {
            FlowIn(),
            String("Key", PK::Input, ""),
            Float("Value", PK::Input, 0.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"meta", "progression", "set", "float", "permanent"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string key = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            f32 val = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 0.0f;
            extern Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
            if (s_VisualScriptSaveSystem && !key.empty()) {
                s_VisualScriptSaveSystem->SetMetaFloat(key, val);
                s_VisualScriptSaveSystem->SaveMeta();
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Meta Get Float (pure node)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MetaGetFloat;
        def.displayName = "Get Meta Float";
        def.description = "Get a permanent meta-progression float value";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.inputs = {
            String("Key", PK::Input, ""),
            Float("Fallback", PK::Input, 0.0f)
        };
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"meta", "progression", "get", "float", "permanent"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            std::string key = (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            f32 fallback = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1]))
                ? std::get<f32>(inputs[1]) : 0.0f;
            extern Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
            if (s_VisualScriptSaveSystem && !key.empty()) {
                return s_VisualScriptSaveSystem->GetMetaFloat(key, fallback);
            }
            return fallback;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // WEATHER
    // ========================================================================

    {
        NodeDefinition def;
        def.typeId = NodeTypes::WeatherSet;
        def.displayName = "Set Weather";
        def.description = "Change the weather type with transition time";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.8f);
        def.inputs = {
            FlowIn(),
            Int("Type", PK::Input, 0),
            Float("Transition", PK::Input, 2.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"weather", "rain", "snow", "fog", "storm", "clear"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 type = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1]))
                ? std::get<i32>(inputs[1]) : 0;
            f32 transition = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 2.0f;
            // Weather system accessed via script bindings static pointer
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::WeatherSetFog;
        def.displayName = "Set Fog";
        def.description = "Set fog density and range";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.8f);
        def.inputs = {
            FlowIn(),
            Float("Density", PK::Input, 0.5f),
            Float("Start", PK::Input, 20.0f),
            Float("End", PK::Input, 100.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"fog", "weather", "atmosphere"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // Fog nodes are placeholders until WeatherSystem VS bridge is set
        };
        RegisterNode(def);
    }

    // ========================================================================
    // QUEST SYSTEM
    // ========================================================================

    {
        NodeDefinition def;
        def.typeId = NodeTypes::QuestStart;
        def.displayName = "Start Quest";
        def.description = "Start a quest by ID";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.7f, 0.5f, 0.2f);
        def.inputs = {
            FlowIn(),
            String("Quest ID", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"quest", "start", "objective", "mission"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // Quest nodes delegate to the ScriptBindings static pointers
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::QuestComplete;
        def.displayName = "Complete Objective";
        def.description = "Complete a quest objective by quest ID and objective index";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.7f, 0.5f, 0.2f);
        def.inputs = {
            FlowIn(),
            String("Quest ID", PK::Input, ""),
            Int("Objective", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"quest", "complete", "objective"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::QuestIsActive;
        def.displayName = "Is Quest Active";
        def.description = "Check if a quest is currently active";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.7f, 0.5f, 0.2f);
        def.inputs = {
            String("Quest ID", PK::Input, "")
        };
        def.outputs = {
            Bool("Active", PK::Output)
        };
        def.keywords = {"quest", "active", "check"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            outputs[0] = false;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // CINEMATIC
    // ========================================================================

    {
        NodeDefinition def;
        def.typeId = NodeTypes::CinematicPlay;
        def.displayName = "Play Cinematic";
        def.description = "Start playing a cinematic camera on the given entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"cinematic", "cutscene", "camera", "play"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::CinematicStop;
        def.displayName = "Stop Cinematic";
        def.description = "Stop a playing cinematic camera";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"cinematic", "cutscene", "stop"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
        };
        RegisterNode(def);
    }

    // ========================================================================
    // PARTICLES
    // ========================================================================

    {
        NodeDefinition def;
        def.typeId = NodeTypes::ParticlePlay;
        def.displayName = "Play Particles";
        def.description = "Start a particle emitter on an entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.8f, 0.5f, 0.2f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"particle", "emitter", "play", "start", "fx"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1]) && ctx.world) {
                auto* emitter = ctx.world->GetComponent<ECS::ParticleEmitterComponent>(
                    std::get<ECS::Entity>(inputs[1]));
                if (emitter) emitter->isPlaying = true;
            }
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::ParticleStop;
        def.displayName = "Stop Particles";
        def.description = "Stop a particle emitter on an entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.8f, 0.5f, 0.2f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"particle", "emitter", "stop", "fx"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1]) && ctx.world) {
                auto* emitter = ctx.world->GetComponent<ECS::ParticleEmitterComponent>(
                    std::get<ECS::Entity>(inputs[1]));
                if (emitter) emitter->isPlaying = false;
            }
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::ParticleBurst;
        def.displayName = "Particle Burst";
        def.description = "Emit a burst of particles from an entity's emitter";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.8f, 0.5f, 0.2f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Int("Count", PK::Input, 10)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"particle", "burst", "emit", "fx"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1]) && ctx.world) {
                i32 count = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2]))
                    ? std::get<i32>(inputs[2]) : 10;
                auto* emitter = ctx.world->GetComponent<ECS::ParticleEmitterComponent>(
                    std::get<ECS::Entity>(inputs[1]));
                if (emitter) emitter->burstCount = count;
            }
        };
        RegisterNode(def);
    }

    // ========================================================================
    // DESTRUCTIBLE
    // ========================================================================

    {
        NodeDefinition def;
        def.typeId = NodeTypes::DestructibleDamage;
        def.displayName = "Apply Damage";
        def.description = "Apply damage to a destructible entity (triggers destruction if health depleted)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.8f, 0.2f, 0.2f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Damage", PK::Input, 10.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"destructible", "damage", "destroy", "break"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // Destructible system accessed via script bindings static pointer
        };
        RegisterNode(def);
    }

    // ========================================================================
    // PREFAB
    // ========================================================================

    {
        NodeDefinition def;
        def.typeId = NodeTypes::PrefabInstantiate;
        def.displayName = "Instantiate Prefab";
        def.description = "Instantiate a prefab at a position";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.3f, 0.7f, 0.5f);
        def.inputs = {
            FlowIn(),
            String("Path", PK::Input, ""),
            Vec3("Position", PK::Input)
        };
        def.outputs = {
            FlowOut(),
            EntityPin("Entity", PK::Output)
        };
        def.keywords = {"prefab", "instantiate", "spawn", "create"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            std::string path = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            Math::Vector3 pos = (inputs.size() > 2 && std::holds_alternative<Math::Vector3>(inputs[2]))
                ? std::get<Math::Vector3>(inputs[2]) : Math::Vector3(0,0,0);
            ECS::Entity result = ECS::INVALID_ENTITY;
            if (ctx.world && !path.empty()) {
                auto prefab = Assets::PrefabManager::Get().LoadPrefab(path);
                if (prefab) {
                    result = Assets::PrefabManager::Get().Instantiate(ctx.world, *prefab, pos);
                }
            }
            outputs[0] = result;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // UI
    // ========================================================================

    {
        NodeDefinition def;
        def.typeId = NodeTypes::UISetText;
        def.displayName = "UI Set Text";
        def.description = "Set the text of a UI element by entity and element ID";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Canvas Entity", PK::Input),
            Int("Element ID", PK::Input, 1),
            String("Text", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"ui", "text", "label", "set", "canvas"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 3 && ctx.world) {
                ECS::Entity entity = std::holds_alternative<ECS::Entity>(inputs[1])
                    ? std::get<ECS::Entity>(inputs[1]) : ECS::INVALID_ENTITY;
                i32 elemId = std::holds_alternative<i32>(inputs[2])
                    ? std::get<i32>(inputs[2]) : 0;
                std::string text = std::holds_alternative<std::string>(inputs[3])
                    ? std::get<std::string>(inputs[3]) : "";
                auto* canvas = ctx.world->GetComponent<GUI::UICanvasComponent>(entity);
                if (canvas) {
                    auto* el = canvas->GetElement(static_cast<u32>(elemId));
                    if (el) el->data.text = text;
                }
            }
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::UISetProgress;
        def.displayName = "UI Set Progress";
        def.description = "Set the fill value of a progress bar element (0-1)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Canvas Entity", PK::Input),
            Int("Element ID", PK::Input, 1),
            Float("Value", PK::Input, 0.5f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"ui", "progress", "bar", "fill", "health"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 3 && ctx.world) {
                ECS::Entity entity = std::holds_alternative<ECS::Entity>(inputs[1])
                    ? std::get<ECS::Entity>(inputs[1]) : ECS::INVALID_ENTITY;
                i32 elemId = std::holds_alternative<i32>(inputs[2])
                    ? std::get<i32>(inputs[2]) : 0;
                f32 value = std::holds_alternative<f32>(inputs[3])
                    ? std::get<f32>(inputs[3]) : 0.5f;
                auto* canvas = ctx.world->GetComponent<GUI::UICanvasComponent>(entity);
                if (canvas) {
                    auto* el = canvas->GetElement(static_cast<u32>(elemId));
                    if (el) el->data.progressValue = Math::Clamp(value, 0.0f, 1.0f);
                }
            }
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::UISetVisible;
        def.displayName = "UI Set Visible";
        def.description = "Show or hide a UI element";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Canvas Entity", PK::Input),
            Int("Element ID", PK::Input, 1),
            Bool("Visible", PK::Input, true)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"ui", "visible", "show", "hide", "element"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 3 && ctx.world) {
                ECS::Entity entity = std::holds_alternative<ECS::Entity>(inputs[1])
                    ? std::get<ECS::Entity>(inputs[1]) : ECS::INVALID_ENTITY;
                i32 elemId = std::holds_alternative<i32>(inputs[2])
                    ? std::get<i32>(inputs[2]) : 0;
                bool visible = std::holds_alternative<bool>(inputs[3])
                    ? std::get<bool>(inputs[3]) : true;
                auto* canvas = ctx.world->GetComponent<GUI::UICanvasComponent>(entity);
                if (canvas) {
                    auto* el = canvas->GetElement(static_cast<u32>(elemId));
                    if (el) el->visible = visible;
                }
            }
        };
        RegisterNode(def);
    }

    // UI Set Focus
    {
        NodeDefinition def;
        def.typeId = NodeTypes::UISetFocus;
        def.displayName = "UI Set Focus";
        def.description = "Set keyboard/gamepad focus to a UI element";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Canvas Entity", PK::Input),
            Int("Element ID", PK::Input, 1)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"ui", "focus", "keyboard", "gamepad", "navigation"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 2 && ctx.world) {
                ECS::Entity entity = std::holds_alternative<ECS::Entity>(inputs[1])
                    ? std::get<ECS::Entity>(inputs[1]) : ECS::INVALID_ENTITY;
                i32 elemId = std::holds_alternative<i32>(inputs[2])
                    ? std::get<i32>(inputs[2]) : 0;
                auto* canvas = ctx.world->GetComponent<GUI::UICanvasComponent>(entity);
                if (canvas) canvas->focusedElementId = static_cast<u32>(elemId);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // UI Clear Focus
    {
        NodeDefinition def;
        def.typeId = NodeTypes::UIClearFocus;
        def.displayName = "UI Clear Focus";
        def.description = "Clear keyboard/gamepad focus from a UI canvas";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Canvas Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"ui", "focus", "clear", "unfocus"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 1 && ctx.world) {
                ECS::Entity entity = std::holds_alternative<ECS::Entity>(inputs[1])
                    ? std::get<ECS::Entity>(inputs[1]) : ECS::INVALID_ENTITY;
                auto* canvas = ctx.world->GetComponent<GUI::UICanvasComponent>(entity);
                if (canvas) canvas->focusedElementId = 0;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // UI Get Focused Element (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::UIGetFocusedElement;
        def.displayName = "UI Get Focused Element";
        def.description = "Get the currently focused element ID in a UI canvas";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            EntityPin("Canvas Entity", PK::Input)
        };
        def.outputs = {Int("Element ID", PK::Output)};
        def.keywords = {"ui", "focus", "get", "focused", "element"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (inputs.size() > 0 && ctx.world) {
                ECS::Entity entity = std::holds_alternative<ECS::Entity>(inputs[0])
                    ? std::get<ECS::Entity>(inputs[0]) : ECS::INVALID_ENTITY;
                auto* canvas = ctx.world->GetComponent<GUI::UICanvasComponent>(entity);
                if (canvas) return static_cast<i32>(canvas->focusedElementId);
            }
            return 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // LOCALIZATION
    // ========================================================================

    {
        NodeDefinition def;
        def.typeId = NodeTypes::LocGetString;
        def.displayName = "Get Localized String";
        def.description = "Look up a localized string by key";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.5f);
        def.inputs = {
            String("Key", PK::Input, "")
        };
        def.outputs = {
            String("Text", PK::Output)
        };
        def.keywords = {"localization", "string", "translate", "i18n", "text"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            std::string key = (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            outputs[0] = key.empty() ? std::string("") :
                GUI::LocalizationManager::Get().GetString(key);
        };
        RegisterNode(def);
    }

    // ========================================================================
    // FLOWER
    // ========================================================================

    // Get Tension (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FlowerGetTension;
        def.displayName = "Get Flower Tension";
        def.description = "Get the current tension (0-1) of a tethered flower part";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.9f, 0.4f, 0.6f);
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Float("Tension", PK::Output)};
        def.keywords = {"flower", "tension", "tether", "spring", "stress"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* tether = ctx.world->GetComponent<ECS::TetherComponent>(target);
            return tether ? tether->currentTension : 0.0f;
        };
        RegisterNode(def);
    }

    // Is Broken (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FlowerIsBroken;
        def.displayName = "Is Flower Broken";
        def.description = "Check if a flower part's tether has broken off";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.9f, 0.4f, 0.6f);
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Bool("Broken", PK::Output)};
        def.keywords = {"flower", "broken", "tether", "pluck"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* tether = ctx.world->GetComponent<ECS::TetherComponent>(target);
            return tether ? tether->isBroken : false;
        };
        RegisterNode(def);
    }

    // Is Grabbed (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FlowerIsGrabbed;
        def.displayName = "Is Flower Grabbed";
        def.description = "Check if a flower part is currently being held by the player";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.9f, 0.4f, 0.6f);
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Bool("Grabbed", PK::Output)};
        def.keywords = {"flower", "grab", "held", "pick"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* grab = ctx.world->GetComponent<ECS::GrabbableComponent>(target);
            return grab ? grab->isGrabbed : false;
        };
        RegisterNode(def);
    }

    // Get Score (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FlowerGetScore;
        def.displayName = "Get Flower Score";
        def.description = "Get the evaluation score of a flower stem";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.9f, 0.4f, 0.6f);
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Float("Score", PK::Output)};
        def.keywords = {"flower", "score", "evaluate", "stem"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* stem = ctx.world->GetComponent<ECS::FlowerStemComponent>(target);
            return stem ? stem->score : 0.0f;
        };
        RegisterNode(def);
    }

    // Set Break Force (action)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FlowerSetBreakForce;
        def.displayName = "Set Break Force";
        def.description = "Set the break force threshold for a tethered flower part";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.9f, 0.4f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Force", PK::Input, 25.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"flower", "break", "force", "tether", "strength"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            f32 force = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 25.0f;
            auto* tether = ctx.world->GetComponent<ECS::TetherComponent>(target);
            if (tether) tether->autoBreakForce = force;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // PHYSICS 2D
    // ========================================================================

    // Raycast2D
    {
        NodeDefinition def;
        def.typeId = NodeTypes::Raycast2D;
        def.displayName = "Raycast 2D";
        def.description = "Cast a 2D ray and return the hit entity";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.8f);
        def.inputs = {
            FlowIn(),
            Vec2("Origin", PK::Input),
            Vec2("Direction", PK::Input, Math::Vector2(1.0f, 0.0f)),
            Float("Distance", PK::Input, 100.0f),
            Int("Layer Mask", PK::Input, static_cast<i32>(0xFFFF))
        };
        def.outputs = {
            FlowOut("Hit"),
            {PinDefinition{"Miss", Editor::PinType::Flow, Editor::PinKind::Output, {}, false}},
            EntityPin("Entity", PK::Output),
            Vec2("Point", PK::Output),
            Vec2("Normal", PK::Output)
        };
        def.keywords = {"raycast", "2d", "physics", "ray", "hit", "collision"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(3);
            Math::Vector2 origin = (inputs.size() > 1 && std::holds_alternative<Math::Vector2>(inputs[1]))
                ? std::get<Math::Vector2>(inputs[1]) : Math::Vector2();
            Math::Vector2 dir = (inputs.size() > 2 && std::holds_alternative<Math::Vector2>(inputs[2]))
                ? std::get<Math::Vector2>(inputs[2]) : Math::Vector2(1.0f, 0.0f);
            f32 dist = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3]))
                ? std::get<f32>(inputs[3]) : 100.0f;
            u32 mask = (inputs.size() > 4 && std::holds_alternative<i32>(inputs[4]))
                ? static_cast<u32>(std::get<i32>(inputs[4])) : 0xFFFFFFFF;

            Physics::RayHit2D hit;
            bool didHit = ctx.physics2d && ctx.physics2d->Raycast(origin, dir, dist, hit, mask);
            if (didHit) {
                outputs[0] = static_cast<ECS::Entity>(hit.entity);
                outputs[1] = hit.point;
                outputs[2] = hit.normal;
                ctx.nextFlowIndex = 0; // Hit
            } else {
                outputs[0] = ECS::INVALID_ENTITY;
                outputs[1] = Math::Vector2();
                outputs[2] = Math::Vector2();
                ctx.nextFlowIndex = 1; // Miss
            }
        };
        RegisterNode(def);
    }

    // OverlapCircle2D
    {
        NodeDefinition def;
        def.typeId = NodeTypes::OverlapCircle2D;
        def.displayName = "Overlap Circle 2D";
        def.description = "Check if any 2D physics bodies overlap a circle";
        def.category = NodeCategory::Physics;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.8f);
        def.inputs = {
            Vec2("Center", PK::Input),
            Float("Radius", PK::Input, 1.0f)
        };
        def.outputs = {Bool("Hit", PK::Output)};
        def.keywords = {"overlap", "circle", "2d", "physics", "check"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            Math::Vector2 center = (inputs.size() > 0 && std::holds_alternative<Math::Vector2>(inputs[0]))
                ? std::get<Math::Vector2>(inputs[0]) : Math::Vector2();
            f32 radius = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1]))
                ? std::get<f32>(inputs[1]) : 1.0f;
            if (!ctx.physics2d) return false;
            std::vector<ECS::Entity> entities;
            return ctx.physics2d->OverlapCircle(center, radius, entities);
        };
        RegisterNode(def);
    }

    // OverlapBox2D
    {
        NodeDefinition def;
        def.typeId = NodeTypes::OverlapBox2D;
        def.displayName = "Overlap Box 2D";
        def.description = "Check if any 2D physics bodies overlap a box";
        def.category = NodeCategory::Physics;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.8f);
        def.inputs = {
            Vec2("Center", PK::Input),
            Vec2("Half Extents", PK::Input, Math::Vector2(0.5f, 0.5f))
        };
        def.outputs = {Bool("Hit", PK::Output)};
        def.keywords = {"overlap", "box", "2d", "physics", "aabb"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            Math::Vector2 center = (inputs.size() > 0 && std::holds_alternative<Math::Vector2>(inputs[0]))
                ? std::get<Math::Vector2>(inputs[0]) : Math::Vector2();
            Math::Vector2 halfExtents = (inputs.size() > 1 && std::holds_alternative<Math::Vector2>(inputs[1]))
                ? std::get<Math::Vector2>(inputs[1]) : Math::Vector2(0.5f, 0.5f);
            if (!ctx.physics2d) return false;
            std::vector<ECS::Entity> entities;
            return ctx.physics2d->OverlapBox(center, halfExtents, entities);
        };
        RegisterNode(def);
    }

    // AddForce2D
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AddForce2D;
        def.displayName = "Add Force 2D";
        def.description = "Apply a 2D force to a rigidbody (scaled by inverse mass)";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.8f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec2("Force", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"force", "2d", "physics", "push", "rigidbody"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity entity = (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1]))
                ? std::get<ECS::Entity>(inputs[1]) : ctx.entity;
            Math::Vector2 force = (inputs.size() > 2 && std::holds_alternative<Math::Vector2>(inputs[2]))
                ? std::get<Math::Vector2>(inputs[2]) : Math::Vector2();
            if (ctx.world) {
                auto* rb = ctx.world->GetComponent<ECS::RigidbodyComponent>(entity);
                if (rb && rb->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) {
                    f32 invMass = (rb->mass > 0.0f) ? 1.0f / rb->mass : 0.0f;
                    rb->velocity.x += force.x * invMass;
                    rb->velocity.y += force.y * invMass;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // AddImpulse2D
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AddImpulse2D;
        def.displayName = "Add Impulse 2D";
        def.description = "Apply a 2D impulse (instant velocity change) to a rigidbody";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.8f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec2("Impulse", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"impulse", "2d", "physics", "jump", "rigidbody"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity entity = (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1]))
                ? std::get<ECS::Entity>(inputs[1]) : ctx.entity;
            Math::Vector2 impulse = (inputs.size() > 2 && std::holds_alternative<Math::Vector2>(inputs[2]))
                ? std::get<Math::Vector2>(inputs[2]) : Math::Vector2();
            if (ctx.world) {
                auto* rb = ctx.world->GetComponent<ECS::RigidbodyComponent>(entity);
                if (rb && rb->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic) {
                    f32 invMass = (rb->mass > 0.0f) ? 1.0f / rb->mass : 0.0f;
                    rb->velocity.x += impulse.x * invMass;
                    rb->velocity.y += impulse.y * invMass;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // NETWORKING
    // ========================================================================

    // Net Host Game
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetHostGame;
        def.displayName = "Host Game";
        def.description = "Start hosting a multiplayer game on the specified port";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.8f, 0.4f);
        def.inputs = {
            FlowIn(),
            Int("Port", PK::Input, 7777),
            String("Player Name", PK::Input, "Host")
        };
        def.outputs = {
            FlowOut("Success"),
            {PinDefinition{"Failed", Editor::PinType::Flow, Editor::PinKind::Output, {}, false}}
        };
        def.keywords = {"network", "host", "server", "multiplayer", "lan"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 port = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1]))
                ? std::get<i32>(inputs[1]) : 7777;
            std::string name = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                ? std::get<std::string>(inputs[2]) : "Host";
            bool ok = ctx.networking && ctx.networking->HostGame(static_cast<u16>(port), name);
            ctx.nextFlowIndex = ok ? 0 : 1;
        };
        RegisterNode(def);
    }

    // Net Join Game
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetJoinGame;
        def.displayName = "Join Game";
        def.description = "Join a multiplayer game at the given IP and port";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.8f, 0.4f);
        def.inputs = {
            FlowIn(),
            String("IP", PK::Input, "127.0.0.1"),
            Int("Port", PK::Input, 7777),
            String("Player Name", PK::Input, "Player")
        };
        def.outputs = {
            FlowOut("Success"),
            {PinDefinition{"Failed", Editor::PinType::Flow, Editor::PinKind::Output, {}, false}}
        };
        def.keywords = {"network", "join", "connect", "client", "multiplayer"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string ip = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "127.0.0.1";
            i32 port = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2]))
                ? std::get<i32>(inputs[2]) : 7777;
            std::string name = (inputs.size() > 3 && std::holds_alternative<std::string>(inputs[3]))
                ? std::get<std::string>(inputs[3]) : "Player";
            bool ok = ctx.networking && ctx.networking->JoinGame(ip, static_cast<u16>(port), name);
            ctx.nextFlowIndex = ok ? 0 : 1;
        };
        RegisterNode(def);
    }

    // Net Disconnect
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetDisconnect;
        def.displayName = "Disconnect";
        def.description = "Disconnect from the current multiplayer game";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.8f, 0.4f);
        def.inputs = {FlowIn()};
        def.outputs = {FlowOut()};
        def.keywords = {"network", "disconnect", "leave", "quit"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (ctx.networking) ctx.networking->Disconnect();
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Net Is Connected (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetIsConnected;
        def.displayName = "Is Connected";
        def.description = "Check if currently connected to a multiplayer game";
        def.category = NodeCategory::Networking;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.3f, 0.8f, 0.4f);
        def.inputs = {};
        def.outputs = {Bool("Connected", PK::Output)};
        def.keywords = {"network", "connected", "online", "status"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            return ctx.networking ? ctx.networking->IsConnected() : false;
        };
        RegisterNode(def);
    }

    // Net Get Ping (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetGetPing;
        def.displayName = "Get Ping";
        def.description = "Get the current network round-trip time in seconds";
        def.category = NodeCategory::Networking;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.3f, 0.8f, 0.4f);
        def.inputs = {};
        def.outputs = {Float("Ping", PK::Output)};
        def.keywords = {"network", "ping", "latency", "rtt"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            return ctx.networking ? ctx.networking->GetPing() : 0.0f;
        };
        RegisterNode(def);
    }

    // Net Call RPC
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetCallRPC;
        def.displayName = "Call RPC";
        def.description = "Call a remote procedure on all connected peers";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.8f, 0.4f);
        def.inputs = {
            FlowIn(),
            String("Name", PK::Input, ""),
            String("Data", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"network", "rpc", "remote", "call", "broadcast"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string name = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            std::string data = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                ? std::get<std::string>(inputs[2]) : "";
            if (ctx.networking && !name.empty()) {
                ctx.networking->CallRPCAll(name,
                    reinterpret_cast<const u8*>(data.data()), static_cast<u32>(data.size()));
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // TWEEN NODES
    // ========================================================================

    // Tween Position
    {
        NodeDefinition def;
        def.typeId = NodeTypes::TweenPosition;
        def.displayName = "Tween Position";
        def.description = "Start a position tween on an entity's TweenComponent";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.8f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Target", PK::Input),
            Float("Duration", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"tween", "position", "move", "animate", "lerp"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* tc = ctx.world->GetComponent<ECS::TweenComponent>(target);
            if (tc && !tc->tweens.empty()) {
                auto& entry = tc->tweens[0];
                entry.property = ECS::TweenProperty::Position;
                if (inputs.size() > 2 && std::holds_alternative<Math::Vector3>(inputs[2]))
                    entry.endValue = std::get<Math::Vector3>(inputs[2]);
                if (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3]))
                    entry.duration = std::get<f32>(inputs[3]);
                entry.useCurrentAsStart = true;
                entry.elapsed = 0.0f;
                entry.isPlaying = true;
                entry.isComplete = false;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Tween Scale
    {
        NodeDefinition def;
        def.typeId = NodeTypes::TweenScale;
        def.displayName = "Tween Scale";
        def.description = "Start a scale tween on an entity's TweenComponent";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.8f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Target Scale", PK::Input),
            Float("Duration", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"tween", "scale", "resize", "animate"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* tc = ctx.world->GetComponent<ECS::TweenComponent>(target);
            if (tc && !tc->tweens.empty()) {
                auto& entry = tc->tweens[0];
                entry.property = ECS::TweenProperty::Scale;
                if (inputs.size() > 2 && std::holds_alternative<Math::Vector3>(inputs[2]))
                    entry.endValue = std::get<Math::Vector3>(inputs[2]);
                if (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3]))
                    entry.duration = std::get<f32>(inputs[3]);
                entry.useCurrentAsStart = true;
                entry.elapsed = 0.0f;
                entry.isPlaying = true;
                entry.isComplete = false;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // DIALOGUE NODES
    // ========================================================================

    // Dialogue Start
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DialogueStart;
        def.displayName = "Start Dialogue";
        def.description = "Begin dialogue playback on an entity with DialogueComponent";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"dialogue", "talk", "conversation", "start", "npc"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* dlg = ctx.world->GetComponent<ECS::DialogueComponent>(target);
            if (dlg) {
                dlg->currentLine = 0;
                dlg->currentChar = 0;
                dlg->isTyping = true;
                dlg->waitingForInput = false;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Dialogue Advance
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DialogueAdvance;
        def.displayName = "Advance Dialogue";
        def.description = "Advance to next line or finish typing in active dialogue";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"dialogue", "advance", "next", "continue"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* dlg = ctx.world->GetComponent<ECS::DialogueComponent>(target);
            if (dlg && dlg->waitingForInput) {
                dlg->currentLine++;
                dlg->currentChar = 0;
                dlg->isTyping = true;
                dlg->waitingForInput = false;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Dialogue Is Active (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DialogueIsActive;
        def.displayName = "Is Dialogue Active";
        def.description = "Check if an entity has active dialogue in progress";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Bool("Active", PK::Output)};
        def.keywords = {"dialogue", "active", "talking", "check"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* dlg = ctx.world->GetComponent<ECS::DialogueComponent>(target);
            if (!dlg) return false;
            return !dlg->IsComplete();
        };
        RegisterNode(def);
    }

    // ========================================================================
    // ANIMATOR NODES (extended — Play/SetSpeed/GetSpeed already registered above)
    // ========================================================================

    // Animator Stop
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AnimatorStop;
        def.displayName = "Stop Animation";
        def.description = "Stop the currently playing animation on an entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.8f, 0.5f, 0.2f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"animator", "animation", "stop"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* ac = ctx.world->GetComponent<ECS::AnimatorComponent>(target);
            if (ac) ac->animator.Stop();
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Animator Is Playing (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AnimatorIsPlaying;
        def.displayName = "Is Animation Playing";
        def.description = "Check if an animation is currently playing on an entity";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.8f, 0.5f, 0.2f);
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Bool("Playing", PK::Output)};
        def.keywords = {"animator", "animation", "playing", "check"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* ac = ctx.world->GetComponent<ECS::AnimatorComponent>(target);
            return ac ? ac->animator.IsPlaying() : false;
        };
        RegisterNode(def);
    }
}

} // namespace VisualScript
} // namespace Enjin
