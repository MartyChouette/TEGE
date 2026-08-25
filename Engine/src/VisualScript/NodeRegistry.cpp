#include "Enjin/VisualScript/NodeRegistry.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Systems/DialogueSystem.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Gameplay/QuestSystem.h"
#include "Enjin/Gameplay/CinematicSystem.h"
#include "Enjin/Gameplay/ObjectPool.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/Water.h"
#include "Enjin/Effects/Destructible.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/Localization.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Math/Noise.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include "Enjin/Audio/AudioEventGraph.h"
#include "Enjin/Plugin/PluginSystem.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/ECS/Components/PostProcessVolume.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Effects/ElementalSystem.h"
#include "Enjin/ECS/Components/Elemental.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>
#include <random>

// Global pointers for visual script system access (set by PlayMode/EditorLayer).
// THREAD SAFETY: These are written at play-start and read during execution, both
// on the main thread. Do NOT access from worker threads without synchronization.
// TODO: Move these to ExecutionContext so nodes receive systems via context
//       rather than relying on global extern pointers.
Enjin::Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem = nullptr;
Enjin::Audio::SimpleAudio* s_VisualScriptAudio = nullptr;
Enjin::Audio::AudioEventGraphRuntime* s_VisualScriptAudioGraphRuntime = nullptr;
Enjin::Plugin::PluginSystem* s_VisualScriptPluginSystem = nullptr;
Enjin::Effects::WeatherSystem* s_VisualScriptWeather = nullptr;
Enjin::Effects::Water3D* s_VisualScriptWater = nullptr;
// HUDSystem is retired: the HUD nodes drive the unified UISystem
Enjin::GUI::UISystem* s_VisualScriptUI = nullptr;
Enjin::Accessibility::SubtitleSystem* s_VisualScriptSubtitleSystem = nullptr;
Enjin::Accessibility::AccessibilityAnnouncer* s_VisualScriptAnnouncer = nullptr;
#if !ENJIN_RENDERER_WEBGPU
Enjin::Renderer::PostProcessing* s_VisualScriptPostProcessing = nullptr;
#endif
Enjin::Gameplay::ObjectPool* s_VisualScriptObjectPool = nullptr;
Enjin::Effects::ElementalSystem* s_VisualScriptElemental = nullptr;
Enjin::Gameplay::QuestSystem* s_VisualScriptQuestSystem = nullptr;
Enjin::Gameplay::CinematicSystem* s_VisualScriptCinematic = nullptr;

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

    // Audio Set Channel Volume
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AudioSetChannelVolume;
        def.displayName = "Set Channel Volume";
        def.description = "Set volume for an audio channel (0=SFX, 1=Music, 2=UI, 3=Voice)";
        def.category = NodeCategory::Audio;
        def.headerColor = Math::Vector3(0.6f, 0.2f, 0.6f);
        def.inputs = {
            FlowIn(),
            Int("Channel", PK::Input, 0),
            Float("Volume", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"audio", "channel", "volume", "music", "sfx", "ui", "voice", "mix"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 channel = 0;
            if (inputs.size() > 0 && std::holds_alternative<i32>(inputs[0])) {
                channel = std::get<i32>(inputs[0]);
            }
            f32 volume = 1.0f;
            if (inputs.size() > 1) {
                if (std::holds_alternative<f32>(inputs[1])) volume = std::get<f32>(inputs[1]);
                else if (std::holds_alternative<i32>(inputs[1])) volume = static_cast<f32>(std::get<i32>(inputs[1]));
            }
            if (s_VisualScriptAudio && channel >= 0 && channel < 4) {
                s_VisualScriptAudio->SetChannelVolume(static_cast<Audio::AudioChannel>(channel),
                    Math::Clamp(volume, 0.0f, 1.0f));
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Audio Stop Channel
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AudioStopChannel;
        def.displayName = "Stop Audio Channel";
        def.description = "Stop all sounds on a channel (0=SFX, 1=Music, 2=UI, 3=Voice)";
        def.category = NodeCategory::Audio;
        def.headerColor = Math::Vector3(0.6f, 0.2f, 0.6f);
        def.inputs = {
            FlowIn(),
            Int("Channel", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"audio", "channel", "stop", "music", "sfx", "silence"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 channel = 0;
            if (inputs.size() > 0 && std::holds_alternative<i32>(inputs[0])) {
                channel = std::get<i32>(inputs[0]);
            }
            if (s_VisualScriptAudio && channel >= 0 && channel < 4) {
                s_VisualScriptAudio->StopChannel(static_cast<Audio::AudioChannel>(channel));
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
            if (s_VisualScriptWeather && type >= 0 && type <= 6) {
                s_VisualScriptWeather->SetWeather(static_cast<Effects::WeatherType>(type), transition);
            }
            ctx.nextFlowIndex = 0;
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
            f32 density = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.5f;
            f32 start = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ? std::get<f32>(inputs[2]) : 20.0f;
            f32 end = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3])) ? std::get<f32>(inputs[3]) : 100.0f;
            if (s_VisualScriptWeather) {
                s_VisualScriptWeather->SetFogDensity(density);
                s_VisualScriptWeather->SetFogStart(start);
                s_VisualScriptWeather->SetFogEnd(end);
            }
            ctx.nextFlowIndex = 0;
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
            std::string questId = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : std::string();
            if (s_VisualScriptQuestSystem && ctx.world && !questId.empty()) {
                s_VisualScriptQuestSystem->StartQuest(ctx.world, questId);
            }
            ctx.nextFlowIndex = 0;
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
            std::string questId = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : std::string();
            i32 objective = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2]))
                ? std::get<i32>(inputs[2]) : 0;
            if (s_VisualScriptQuestSystem && ctx.world && !questId.empty()) {
                s_VisualScriptQuestSystem->CompleteObjective(ctx.world, questId, objective);
            }
            ctx.nextFlowIndex = 0;
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
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            std::string questId = (!inputs.empty() && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : std::string();
            if (s_VisualScriptQuestSystem && ctx.world && !questId.empty()) {
                return s_VisualScriptQuestSystem->IsQuestActive(ctx.world, questId);
            }
            return false;
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
            ECS::Entity target = (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1]))
                ? std::get<ECS::Entity>(inputs[1]) : ctx.entity;
            if (s_VisualScriptCinematic && ctx.world) {
                s_VisualScriptCinematic->Play(ctx.world, target);
            }
            ctx.nextFlowIndex = 0;
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
            ECS::Entity target = (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1]))
                ? std::get<ECS::Entity>(inputs[1]) : ctx.entity;
            if (s_VisualScriptCinematic && ctx.world) {
                s_VisualScriptCinematic->Stop(ctx.world, target);
            }
            ctx.nextFlowIndex = 0;
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

    {
        NodeDefinition def;
        def.typeId = NodeTypes::ParticlePreset;
        def.displayName = "Apply Particle Preset";
        def.description = "Apply a named preset (Fire, Smoke, Sparks, etc.) to a particle emitter";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.8f, 0.5f, 0.2f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            String("Preset", PK::Input, "Fire")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"particle", "preset", "fire", "smoke", "sparks", "effect"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[1]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            std::string preset = "Fire";
            if (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                preset = std::get<std::string>(inputs[2]);
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* emitter = ctx.world->GetComponent<ECS::ParticleEmitterComponent>(target);
                if (emitter) ECS::ApplyParticlePreset(*emitter, preset);
            }
            ctx.nextFlowIndex = 0;
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

    // Set Max Distance (action)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FlowerSetBreakForce;
        def.displayName = "Set Max Distance";
        def.description = "Set the break distance threshold for a tethered flower part";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.9f, 0.4f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Distance", PK::Input, 0.75f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"flower", "break", "distance", "tether", "stretch"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            f32 dist = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 0.75f;
            auto* tether = ctx.world->GetComponent<ECS::TetherComponent>(target);
            if (tether) tether->maxDistance = dist;
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

    // Tween Rotation
    {
        NodeDefinition def;
        def.typeId = NodeTypes::TweenRotation;
        def.displayName = "Tween Rotation";
        def.description = "Start a rotation tween on an entity's TweenComponent";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.8f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Target Rotation", PK::Input),
            Float("Duration", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"tween", "rotation", "rotate", "animate", "spin"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* tc = ctx.world->GetComponent<ECS::TweenComponent>(target);
            if (tc && !tc->tweens.empty()) {
                auto& entry = tc->tweens[0];
                entry.property = ECS::TweenProperty::Rotation;
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

    // Tween Float
    {
        NodeDefinition def;
        def.typeId = NodeTypes::TweenFloat;
        def.displayName = "Tween Float";
        def.description = "Start a generic float tween on an entity's TweenComponent (value in currentValue.x)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.8f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Start", PK::Input),
            Float("End", PK::Input),
            Float("Duration", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"tween", "float", "interpolate", "lerp", "animate"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* tc = ctx.world->GetComponent<ECS::TweenComponent>(target);
            if (tc && !tc->tweens.empty()) {
                auto& entry = tc->tweens[0];
                entry.property = ECS::TweenProperty::Float;
                f32 startVal = 0.0f, endVal = 1.0f;
                if (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                    startVal = std::get<f32>(inputs[2]);
                if (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3]))
                    endVal = std::get<f32>(inputs[3]);
                entry.startValue = Math::Vector3(startVal, 0, 0);
                entry.endValue = Math::Vector3(endVal, 0, 0);
                entry.useCurrentAsStart = false;
                if (inputs.size() > 4 && std::holds_alternative<f32>(inputs[4]))
                    entry.duration = std::get<f32>(inputs[4]);
                entry.elapsed = 0.0f;
                entry.isPlaying = true;
                entry.isComplete = false;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // On Collision (generic — fires each frame while colliding)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::OnCollision;
        def.displayName = "On Collision";
        def.description = "Fires each frame while entity is colliding with another (use Enter/Exit for one-shot)";
        def.category = NodeCategory::Events;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.3f);
        def.flags = NodeDefFlags::Event;
        def.outputs = {
            FlowOut(),
            EntityPin("Other Entity", PK::Output)
        };
        def.keywords = {"collision", "overlap", "touching", "contact"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            outputs[0] = ctx.otherEntity;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Custom Event
    {
        NodeDefinition def;
        def.typeId = NodeTypes::CustomEvent;
        def.displayName = "Custom Event";
        def.description = "Fires when a named custom event is dispatched via script or visual script";
        def.category = NodeCategory::Events;
        def.headerColor = Math::Vector3(0.2f, 0.6f, 0.3f);
        def.flags = NodeDefFlags::Event;
        def.inputs = {
            String("Event Name", PK::Input)
        };
        def.outputs = {
            FlowOut(),
            String("Event Name", PK::Output)
        };
        def.keywords = {"custom", "event", "dispatch", "signal", "trigger"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            std::string eventName;
            if (!inputs.empty() && std::holds_alternative<std::string>(inputs[0]))
                eventName = std::get<std::string>(inputs[0]);
            outputs[0] = eventName;
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

    // ========================================================================
    // AI NODES
    // ========================================================================

    // AI Set Target
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AISetTarget;
        def.displayName = "AI Set Target";
        def.description = "Set the target entity for an AI controller";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.5f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            EntityPin("Target", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"ai", "target", "enemy", "chase", "controller"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            ECS::Entity targetEntity = ECS::INVALID_ENTITY;
            if (inputs.size() > 2 && std::holds_alternative<u64>(inputs[2]))
                targetEntity = static_cast<ECS::Entity>(std::get<u64>(inputs[2]));
            auto* ai = ctx.world->GetComponent<ECS::AIControllerComponent>(target);
            if (ai) ai->targetEntity = targetEntity;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // AI Get Target (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AIGetTarget;
        def.displayName = "AI Get Target";
        def.description = "Get the current target entity of an AI controller";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.5f);
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {EntityPin("Target", PK::Output)};
        def.keywords = {"ai", "target", "enemy", "get", "controller"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* ai = ctx.world->GetComponent<ECS::AIControllerComponent>(target);
            return ai ? static_cast<u64>(ai->targetEntity) : static_cast<u64>(ECS::INVALID_ENTITY);
        };
        RegisterNode(def);
    }

    // AI Set State
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AISetState;
        def.displayName = "AI Set State";
        def.description = "Set the AI controller state (Idle=0, Patrol=1, Chase=2, Attack=3, Flee=4, Dead=5)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.5f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Int("State", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"ai", "state", "idle", "patrol", "chase", "attack", "flee"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            i32 state = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2]))
                ? std::get<i32>(inputs[2]) : 0;
            if (state < 0 || state > 5) state = 0;
            auto* ai = ctx.world->GetComponent<ECS::AIControllerComponent>(target);
            if (ai) ai->currentState = static_cast<ECS::AIControllerComponent::AIState>(state);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // AI Get State (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AIGetState;
        def.displayName = "AI Get State";
        def.description = "Get the current AI controller state as integer";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.5f);
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Int("State", PK::Output)};
        def.keywords = {"ai", "state", "get", "controller"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* ai = ctx.world->GetComponent<ECS::AIControllerComponent>(target);
            return ai ? static_cast<i32>(ai->currentState) : 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // BEHAVIOR TREE NODES
    // ========================================================================

    // BT Enable
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTEnable;
        def.displayName = "BT Enable";
        def.description = "Enable a behavior tree component on an entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"behavior", "tree", "enable", "activate", "bt"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* bt = ctx.world->GetComponent<ECS::BehaviorTreeComponent>(target);
            if (bt) bt->enabled = true;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // BT Disable
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTDisable;
        def.displayName = "BT Disable";
        def.description = "Disable a behavior tree component on an entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"behavior", "tree", "disable", "deactivate", "bt"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* bt = ctx.world->GetComponent<ECS::BehaviorTreeComponent>(target);
            if (bt) bt->enabled = false;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // BT Reset
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTReset;
        def.displayName = "BT Reset";
        def.description = "Reset a behavior tree to its initial state";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"behavior", "tree", "reset", "restart", "bt"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* bt = ctx.world->GetComponent<ECS::BehaviorTreeComponent>(target);
            if (bt) bt->ResetRuntimeState();
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // BT Get Enabled (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTGetEnabled;
        def.displayName = "BT Is Enabled";
        def.description = "Check if a behavior tree is currently enabled";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.6f);
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Bool("Enabled", PK::Output)};
        def.keywords = {"behavior", "tree", "enabled", "active", "bt", "check"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* bt = ctx.world->GetComponent<ECS::BehaviorTreeComponent>(target);
            return bt ? bt->enabled : false;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // NAVMESH NODES
    // ========================================================================

    // Navmesh Is Point On (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NavmeshIsPointOn;
        def.displayName = "Is Point On Navmesh";
        def.description = "Check if a world position is on the navigation mesh";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.5f);
        def.inputs = {Vec3("Position", PK::Input)};
        def.outputs = {Bool("On Navmesh", PK::Output)};
        def.keywords = {"navmesh", "navigation", "point", "walkable", "pathfinding"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            (void)ctx;
            Math::Vector3 p = (!inputs.empty() && std::holds_alternative<Math::Vector3>(inputs[0]))
                ? std::get<Math::Vector3>(inputs[0]) : Math::Vector3(0, 0, 0);
            return Enjin::Scripting::VSNavmeshIsPointOn(p.x, p.y, p.z);
        };
        RegisterNode(def);
    }

    // ========================================================================
    // STATE MACHINE NODES (F19)
    // ========================================================================

    // StateMachine Set State
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SMSetState;
        def.displayName = "Set State Machine State";
        def.description = "Transition a state machine to a new state by name";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.3f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            String("State", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"state", "machine", "set", "transition", "fsm"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            std::string stateName;
            if (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                stateName = std::get<std::string>(inputs[2]);
            if (!stateName.empty()) {
                auto* sm = ctx.world->GetComponent<ECS::StateMachineComponent>(target);
                if (sm) sm->SetState(stateName);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // StateMachine Get State (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SMGetState;
        def.displayName = "Get State Machine State";
        def.description = "Get the current state name of a state machine";
        def.category = NodeCategory::Gameplay;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.3f);
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {String("State", PK::Output)};
        def.keywords = {"state", "machine", "get", "current", "fsm"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* sm = ctx.world->GetComponent<ECS::StateMachineComponent>(target);
            return sm ? sm->currentState : std::string("");
        };
        RegisterNode(def);
    }

    // ========================================================================
    // ACCESSIBILITY NODES (F20)
    // ========================================================================

    // Subtitle Show
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SubtitleShow;
        def.displayName = "Show Subtitle";
        def.description = "Display a subtitle or closed caption on screen";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.7f, 0.7f);
        def.inputs = {
            FlowIn(),
            String("Text", PK::Input, ""),
            String("Speaker", PK::Input, ""),
            Float("Duration", PK::Input, 3.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"subtitle", "caption", "text", "accessibility", "display"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (s_VisualScriptSubtitleSystem) {
                std::string text = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                    ? std::get<std::string>(inputs[1]) : "";
                std::string speaker = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                    ? std::get<std::string>(inputs[2]) : "";
                f32 duration = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3]))
                    ? std::get<f32>(inputs[3]) : 3.0f;
                // ED-C1 fix: null-check before dereference
                if (!text.empty() && s_VisualScriptSubtitleSystem) {
                    s_VisualScriptSubtitleSystem->ShowSubtitle(text, speaker,
                        Math::Vector3(1.0f, 1.0f, 1.0f), duration);
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Announcer Announce
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AnnouncerAnnounce;
        def.displayName = "Announce";
        def.description = "Queue an accessibility announcement for screen readers";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.7f, 0.7f);
        def.inputs = {
            FlowIn(),
            String("Text", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"announce", "screen reader", "accessibility", "tts"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (s_VisualScriptAnnouncer) {
                std::string text = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                    ? std::get<std::string>(inputs[1]) : "";
                if (!text.empty()) {
                    s_VisualScriptAnnouncer->Announce(text, Accessibility::AnnouncePriority::Normal);
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Colorblind Set Mode
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ColorblindSetMode;
        def.displayName = "Set Colorblind Mode";
        def.description = "Set the colorblind filter mode (0=Off, 1=Protanopia, 2=Deuteranopia, 3=Tritanopia, 4-6=anomaly, 7=Achromatopsia)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.7f, 0.7f);
        def.inputs = {
            FlowIn(),
            Int("Mode", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"colorblind", "accessibility", "filter", "vision", "color"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 mode = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1]))
                ? std::get<i32>(inputs[1]) : 0;
#if !ENJIN_RENDERER_WEBGPU
            if (s_VisualScriptPostProcessing) {
                auto& settings = s_VisualScriptPostProcessing->GetSettings();
                settings.colorblindMode = static_cast<u32>(std::clamp(mode, 0, 7));
            }
#endif
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // INPUT NODES
    // ========================================================================

    // Input Is Key Pressed (pure) - single frame press
    {
        NodeDefinition def;
        def.typeId = NodeTypes::InputIsKeyPressed;
        def.displayName = "Is Key Pressed";
        def.description = "Check if a key was pressed this frame (single frame)";
        def.category = NodeCategory::Input;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.9f, 0.7f, 0.2f);
        def.inputs = {Int("Key Code", PK::Input, 0)};
        def.outputs = {Bool("Pressed", PK::Output)};
        def.keywords = {"input", "key", "pressed", "keyboard", "down"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            i32 keyCode = (inputs.size() > 0 && std::holds_alternative<i32>(inputs[0]))
                ? std::get<i32>(inputs[0]) : 0;
            return Enjin::Input::IsKeyPressed(static_cast<Enjin::KeyCode>(keyCode));
        };
        RegisterNode(def);
    }

    // Input Is Key Down (pure) - held
    {
        NodeDefinition def;
        def.typeId = NodeTypes::InputIsKeyDown;
        def.displayName = "Is Key Down";
        def.description = "Check if a key is currently held down";
        def.category = NodeCategory::Input;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.9f, 0.7f, 0.2f);
        def.inputs = {Int("Key Code", PK::Input, 0)};
        def.outputs = {Bool("Down", PK::Output)};
        def.keywords = {"input", "key", "held", "keyboard", "down"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            i32 keyCode = (inputs.size() > 0 && std::holds_alternative<i32>(inputs[0]))
                ? std::get<i32>(inputs[0]) : 0;
            return Enjin::Input::IsKeyDown(static_cast<Enjin::KeyCode>(keyCode));
        };
        RegisterNode(def);
    }

    // Input Get Mouse Position (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::InputGetMousePosition;
        def.displayName = "Get Mouse Position";
        def.description = "Get the current mouse cursor position";
        def.category = NodeCategory::Input;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.9f, 0.7f, 0.2f);
        def.inputs = {};
        def.outputs = {
            Float("X", PK::Output),
            Float("Y", PK::Output)
        };
        def.keywords = {"input", "mouse", "position", "cursor", "x", "y"};
        // Pure node with multiple outputs: evaluate returns first (X)
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            auto pos = Enjin::Input::GetMousePosition();
            return pos.x;
        };
        RegisterNode(def);
    }

    // Input Get Axis (pure) - gamepad axis
    {
        NodeDefinition def;
        def.typeId = NodeTypes::InputGetAxis;
        def.displayName = "Get Gamepad Axis";
        def.description = "Get the value of a gamepad axis (-1 to 1)";
        def.category = NodeCategory::Input;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.9f, 0.7f, 0.2f);
        def.inputs = {
            Int("Axis", PK::Input, 0),
            Int("Gamepad Index", PK::Input, 0)
        };
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"input", "gamepad", "axis", "stick", "joystick", "controller"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            i32 axis = (inputs.size() > 0 && std::holds_alternative<i32>(inputs[0]))
                ? std::get<i32>(inputs[0]) : 0;
            i32 index = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1]))
                ? std::get<i32>(inputs[1]) : 0;
            return Enjin::Input::GetGamepadAxis(static_cast<Enjin::GamepadAxis>(axis), index);
        };
        RegisterNode(def);
    }

    // ========================================================================
    // SCENE NODES
    // ========================================================================

    // Scene Load Scene
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SceneLoadScene;
        def.displayName = "Load Scene";
        def.description = "Load a scene by name";
        def.category = NodeCategory::Scene;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.9f);
        def.inputs = {
            FlowIn(),
            String("Scene Name", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"scene", "load", "level", "change", "switch"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string sceneName = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            if (!sceneName.empty()) {
                if (ctx.sceneManager) {
                    ENJIN_LOG_INFO(Script, "Scene_LoadScene: loading '%s'", sceneName.c_str());
                    ctx.sceneManager->LoadScene(sceneName);
                } else {
                    ENJIN_LOG_WARN(Script, "Scene_LoadScene: no SceneManager bound, cannot load '%s'", sceneName.c_str());
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Scene Get Current Scene (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SceneGetCurrentScene;
        def.displayName = "Get Current Scene";
        def.description = "Get the name of the currently loaded scene";
        def.category = NodeCategory::Scene;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.9f);
        def.inputs = {};
        def.outputs = {String("Scene Name", PK::Output, "")};
        def.keywords = {"scene", "current", "name", "active", "loaded"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            // Scene name querying would need SceneManager access; return empty for now
            return std::string("");
        };
        RegisterNode(def);
    }

    // ========================================================================
    // NOISE NODES
    // ========================================================================

    // Perlin Noise 2D (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NoisePerlin2D;
        def.displayName = "Perlin Noise 2D";
        def.description = "Sample 2D Perlin noise at a position";
        def.category = NodeCategory::Noise;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.5f, 0.8f, 0.4f);
        def.inputs = {
            Float("X", PK::Input, 0.0f),
            Float("Y", PK::Input, 0.0f),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"noise", "perlin", "procedural", "random", "2d"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            f32 x = (inputs.size() > 0 && std::holds_alternative<f32>(inputs[0])) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 y = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.0f;
            u32 seed = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? static_cast<u32>(std::get<i32>(inputs[2])) : 0u;
            return Math::PerlinNoise2D(x, y, seed);
        };
        RegisterNode(def);
    }

    // Simplex Noise 2D (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NoiseSimplex2D;
        def.displayName = "Simplex Noise 2D";
        def.description = "Sample 2D Simplex noise at a position";
        def.category = NodeCategory::Noise;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.5f, 0.8f, 0.4f);
        def.inputs = {
            Float("X", PK::Input, 0.0f),
            Float("Y", PK::Input, 0.0f),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"noise", "simplex", "procedural", "random", "2d"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            f32 x = (inputs.size() > 0 && std::holds_alternative<f32>(inputs[0])) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 y = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.0f;
            u32 seed = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? static_cast<u32>(std::get<i32>(inputs[2])) : 0u;
            return Math::SimplexNoise2D(x, y, seed);
        };
        RegisterNode(def);
    }

    // Worley Noise 2D (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NoiseWorley2D;
        def.displayName = "Worley Noise 2D";
        def.description = "Sample 2D Worley (cellular) noise at a position";
        def.category = NodeCategory::Noise;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.5f, 0.8f, 0.4f);
        def.inputs = {
            Float("X", PK::Input, 0.0f),
            Float("Y", PK::Input, 0.0f),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"noise", "worley", "cellular", "voronoi", "2d"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            f32 x = (inputs.size() > 0 && std::holds_alternative<f32>(inputs[0])) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 y = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.0f;
            u32 seed = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? static_cast<u32>(std::get<i32>(inputs[2])) : 0u;
            return Math::WorleyNoise2D(x, y, seed);
        };
        RegisterNode(def);
    }

    // FBM 2D (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NoiseFBM2D;
        def.displayName = "FBM Noise 2D";
        def.description = "Fractal Brownian Motion noise (layered Perlin)";
        def.category = NodeCategory::Noise;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.5f, 0.8f, 0.4f);
        def.inputs = {
            Float("X", PK::Input, 0.0f),
            Float("Y", PK::Input, 0.0f),
            Int("Octaves", PK::Input, 6),
            Float("Frequency", PK::Input, 1.0f),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"noise", "fbm", "fractal", "terrain", "procedural"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            f32 x = (inputs.size() > 0 && std::holds_alternative<f32>(inputs[0])) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 y = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.0f;
            i32 oct = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? std::get<i32>(inputs[2]) : 6;
            f32 freq = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3])) ? std::get<f32>(inputs[3]) : 1.0f;
            u32 seed = (inputs.size() > 4 && std::holds_alternative<i32>(inputs[4])) ? static_cast<u32>(std::get<i32>(inputs[4])) : 0u;
            return Math::FBM2D(x, y, oct, 2.0f, 0.5f, freq, seed);
        };
        RegisterNode(def);
    }

    // Perlin Noise 3D (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NoisePerlin3D;
        def.displayName = "Perlin Noise 3D";
        def.description = "Sample 3D Perlin noise at a position";
        def.category = NodeCategory::Noise;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.5f, 0.8f, 0.4f);
        def.inputs = {
            Float("X", PK::Input, 0.0f),
            Float("Y", PK::Input, 0.0f),
            Float("Z", PK::Input, 0.0f),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"noise", "perlin", "procedural", "random", "3d"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            f32 x = (inputs.size() > 0 && std::holds_alternative<f32>(inputs[0])) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 y = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.0f;
            f32 z = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ? std::get<f32>(inputs[2]) : 0.0f;
            u32 seed = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? static_cast<u32>(std::get<i32>(inputs[3])) : 0u;
            return Math::PerlinNoise3D(x, y, z, seed);
        };
        RegisterNode(def);
    }

    // Simplex Noise 3D (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NoiseSimplex3D;
        def.displayName = "Simplex Noise 3D";
        def.description = "Sample 3D Simplex noise at a position";
        def.category = NodeCategory::Noise;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.5f, 0.8f, 0.4f);
        def.inputs = {
            Float("X", PK::Input, 0.0f),
            Float("Y", PK::Input, 0.0f),
            Float("Z", PK::Input, 0.0f),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"noise", "simplex", "procedural", "random", "3d"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            f32 x = (inputs.size() > 0 && std::holds_alternative<f32>(inputs[0])) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 y = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.0f;
            f32 z = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ? std::get<f32>(inputs[2]) : 0.0f;
            u32 seed = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? static_cast<u32>(std::get<i32>(inputs[3])) : 0u;
            return Math::SimplexNoise3D(x, y, z, seed);
        };
        RegisterNode(def);
    }

    // ========================================================================
    // STREAMING NODES
    // ========================================================================

    // Streaming Force Load
    {
        NodeDefinition def;
        def.typeId = NodeTypes::StreamingForceLoad;
        def.displayName = "Force Load Chunk";
        def.description = "Force-load a streaming chunk by ID";
        def.category = NodeCategory::Streaming;
        def.headerColor = Math::Vector3(0.3f, 0.7f, 0.6f);
        def.inputs = {
            FlowIn(),
            String("Chunk ID", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"streaming", "load", "chunk", "level"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string chunkId = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            if (!chunkId.empty() && ctx.streamingManager)
                ctx.streamingManager->ForceLoadChunk(chunkId);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Streaming Force Unload
    {
        NodeDefinition def;
        def.typeId = NodeTypes::StreamingForceUnload;
        def.displayName = "Force Unload Chunk";
        def.description = "Force-unload a streaming chunk by ID";
        def.category = NodeCategory::Streaming;
        def.headerColor = Math::Vector3(0.3f, 0.7f, 0.6f);
        def.inputs = {
            FlowIn(),
            String("Chunk ID", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"streaming", "unload", "chunk", "level"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string chunkId = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            if (!chunkId.empty() && ctx.streamingManager)
                ctx.streamingManager->ForceUnloadChunk(chunkId);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Streaming Get State (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::StreamingGetState;
        def.displayName = "Get Chunk State";
        def.description = "Get the loading state of a streaming chunk (0=Unloaded, 1=Loading, 2=Loaded, 3=Unloading)";
        def.category = NodeCategory::Streaming;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.3f, 0.7f, 0.6f);
        def.inputs = {String("Chunk ID", PK::Input, "")};
        def.outputs = {Int("State", PK::Output, 0)};
        def.keywords = {"streaming", "state", "chunk", "status"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            std::string chunkId = (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            if (chunkId.empty() || !ctx.streamingManager) return 0;
            return static_cast<i32>(ctx.streamingManager->GetChunkState(chunkId));
        };
        RegisterNode(def);
    }

    // Streaming Is Loaded (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::StreamingIsLoaded;
        def.displayName = "Is Chunk Loaded";
        def.description = "Check if a streaming chunk is fully loaded";
        def.category = NodeCategory::Streaming;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.3f, 0.7f, 0.6f);
        def.inputs = {String("Chunk ID", PK::Input, "")};
        def.outputs = {Bool("Loaded", PK::Output)};
        def.keywords = {"streaming", "loaded", "chunk", "check"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            std::string chunkId = (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            if (chunkId.empty() || !ctx.streamingManager) return false;
            return ctx.streamingManager->GetChunkState(chunkId) == Scene::ChunkState::Loaded;
        };
        RegisterNode(def);
    }

    // Streaming Get Loaded Count (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::StreamingGetLoadedCount;
        def.displayName = "Get Loaded Chunk Count";
        def.description = "Get the number of currently loaded streaming chunks";
        def.category = NodeCategory::Streaming;
        def.flags = NodeDefFlags::Pure;
        def.headerColor = Math::Vector3(0.3f, 0.7f, 0.6f);
        def.inputs = {};
        def.outputs = {Int("Count", PK::Output, 0)};
        def.keywords = {"streaming", "count", "loaded", "chunks"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (!ctx.streamingManager) return 0;
            return static_cast<i32>(ctx.streamingManager->GetLoadedChunkCount());
        };
        RegisterNode(def);
    }

    // Streaming Set Enabled
    {
        NodeDefinition def;
        def.typeId = NodeTypes::StreamingSetEnabled;
        def.displayName = "Set Streaming Enabled";
        def.description = "Enable or disable the level streaming system";
        def.category = NodeCategory::Streaming;
        def.headerColor = Math::Vector3(0.3f, 0.7f, 0.6f);
        def.inputs = {
            FlowIn(),
            Bool("Enabled", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"streaming", "enable", "disable", "toggle"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            bool enabled = (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1]))
                ? std::get<bool>(inputs[1]) : true;
            if (ctx.streamingManager)
                ctx.streamingManager->SetEnabled(enabled);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // PROCEDURAL GENERATION
    // ========================================================================

    // Cellular Automata
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ProceduralCellularAutomata;
        def.displayName = "Cellular Automata";
        def.description = "Generate a cave/organic layout using cellular automata";
        def.category = NodeCategory::Procedural;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.8f);
        def.inputs = {
            FlowIn(),
            Int("Width", PK::Input, 64),
            Int("Height", PK::Input, 64),
            Int("Fill %", PK::Input, 45),
            Int("Smooth Passes", PK::Input, 5),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"procedural", "cave", "cellular", "automata", "generate"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 w = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 64;
            i32 h = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? std::get<i32>(inputs[2]) : 64;
            i32 fill = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? std::get<i32>(inputs[3]) : 45;
            i32 smooth = (inputs.size() > 4 && std::holds_alternative<i32>(inputs[4])) ? std::get<i32>(inputs[4]) : 5;
            i32 seed = (inputs.size() > 5 && std::holds_alternative<i32>(inputs[5])) ? std::get<i32>(inputs[5]) : 0;
            Procedural::CellularAutomata::Params p;
            p.width = static_cast<u32>(w); p.height = static_cast<u32>(h);
            p.fillPercent = static_cast<u32>(fill); p.iterations = static_cast<u32>(smooth);
            p.seed = static_cast<u32>(seed);
            Procedural::CellularAutomata::Generate(p);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Random Walker
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ProceduralRandomWalker;
        def.displayName = "Random Walker";
        def.description = "Carve dungeon passages using a random walk algorithm";
        def.category = NodeCategory::Procedural;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.8f);
        def.inputs = {
            FlowIn(),
            Int("Width", PK::Input, 64),
            Int("Height", PK::Input, 64),
            Int("Steps", PK::Input, 2000),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"procedural", "random", "walker", "dungeon", "carve"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 w = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 64;
            i32 h = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? std::get<i32>(inputs[2]) : 64;
            i32 steps = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? std::get<i32>(inputs[3]) : 2000;
            i32 seed = (inputs.size() > 4 && std::holds_alternative<i32>(inputs[4])) ? std::get<i32>(inputs[4]) : 0;
            Procedural::RandomWalker::Params p;
            p.width = static_cast<u32>(w); p.height = static_cast<u32>(h);
            p.steps = static_cast<u32>(steps); p.seed = static_cast<u32>(seed);
            Procedural::RandomWalker::Generate(p);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // BSP
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ProceduralBSP;
        def.displayName = "BSP Dungeon";
        def.description = "Generate rooms and corridors using binary space partition";
        def.category = NodeCategory::Procedural;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.8f);
        def.inputs = {
            FlowIn(),
            Int("Width", PK::Input, 64),
            Int("Height", PK::Input, 64),
            Int("Min Room", PK::Input, 5),
            Int("Max Room", PK::Input, 15),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"procedural", "bsp", "dungeon", "rooms", "corridors"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 w = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 64;
            i32 h = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? std::get<i32>(inputs[2]) : 64;
            i32 minR = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? std::get<i32>(inputs[3]) : 5;
            i32 maxR = (inputs.size() > 4 && std::holds_alternative<i32>(inputs[4])) ? std::get<i32>(inputs[4]) : 15;
            i32 seed = (inputs.size() > 5 && std::holds_alternative<i32>(inputs[5])) ? std::get<i32>(inputs[5]) : 0;
            Procedural::BSPGenerator::Params p;
            p.width = static_cast<u32>(w); p.height = static_cast<u32>(h);
            p.minRoomSize = static_cast<u32>(minR); p.maxRoomSize = static_cast<u32>(maxR);
            p.seed = static_cast<u32>(seed);
            Procedural::BSPGenerator::Generate(p);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Diamond-Square
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ProceduralDiamondSquare;
        def.displayName = "Diamond-Square Heightmap";
        def.description = "Generate a heightmap terrain using diamond-square algorithm";
        def.category = NodeCategory::Procedural;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.8f);
        def.inputs = {
            FlowIn(),
            Int("Size", PK::Input, 129),
            Float("Roughness", PK::Input, 0.5f),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"procedural", "heightmap", "terrain", "diamond", "square"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 size = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 129;
            f32 rough = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ? std::get<f32>(inputs[2]) : 0.5f;
            i32 seed = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? std::get<i32>(inputs[3]) : 0;
            Procedural::DiamondSquare::Params p;
            p.size = static_cast<u32>(size); p.roughness = rough; p.seed = static_cast<u32>(seed);
            Procedural::DiamondSquare::Generate(p);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // L-System
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ProceduralLSystem;
        def.displayName = "L-System";
        def.description = "Generate an L-System string from axiom and rewriting rules";
        def.category = NodeCategory::Procedural;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.8f);
        def.inputs = {
            FlowIn(),
            String("Axiom", PK::Input, "F"),
            String("Rules", PK::Input, "F=F+F-F"),
            Int("Iterations", PK::Input, 4)
        };
        def.outputs = {
            FlowOut(),
            String("Result", PK::Output, "")
        };
        def.keywords = {"procedural", "lsystem", "fractal", "plant", "turtle"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string axiom = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) ? std::get<std::string>(inputs[1]) : "F";
            std::string rules = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2])) ? std::get<std::string>(inputs[2]) : "F=F+F-F";
            i32 iters = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? std::get<i32>(inputs[3]) : 4;
            if (iters < 0) iters = 0;
            if (iters > 10) iters = 10;

            Procedural::LSystemGenerator::Params p;
            p.axiom = axiom;
            p.iterations = static_cast<u32>(iters);
            // Parse rules
            size_t pos = 0;
            while (pos < rules.size()) {
                size_t eq = rules.find('=', pos);
                if (eq == std::string::npos || eq == pos) break;
                char sym = rules[pos];
                size_t semi = rules.find(';', eq);
                p.rules[sym] = rules.substr(eq + 1, (semi == std::string::npos) ? std::string::npos : semi - eq - 1);
                pos = (semi == std::string::npos) ? rules.size() : semi + 1;
            }

            // Expand
            std::string expanded = axiom;
            for (u32 i = 0; i < static_cast<u32>(iters); i++) {
                std::string next;
                next.reserve(expanded.size() * 2);
                for (char c : expanded) {
                    auto it = p.rules.find(c);
                    next += (it != p.rules.end()) ? it->second : std::string(1, c);
                }
                expanded = std::move(next);
                if (expanded.size() > 100000) break;
            }
            outputs.resize(2);
            outputs[1] = expanded;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Voronoi
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ProceduralVoronoi;
        def.displayName = "Voronoi Regions";
        def.description = "Partition space into regions using Voronoi diagram";
        def.category = NodeCategory::Procedural;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.8f);
        def.inputs = {
            FlowIn(),
            Int("Width", PK::Input, 128),
            Int("Height", PK::Input, 128),
            Int("Num Points", PK::Input, 20),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"procedural", "voronoi", "regions", "diagram", "partition"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 w = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 128;
            i32 h = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? std::get<i32>(inputs[2]) : 128;
            i32 pts = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? std::get<i32>(inputs[3]) : 20;
            i32 seed = (inputs.size() > 4 && std::holds_alternative<i32>(inputs[4])) ? std::get<i32>(inputs[4]) : 0;
            Procedural::VoronoiGenerator::Params p;
            p.width = static_cast<u32>(w); p.height = static_cast<u32>(h);
            p.numSeeds = static_cast<u32>(pts); p.seed = static_cast<u32>(seed);
            Procedural::VoronoiGenerator::Generate(p);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // WFC
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ProceduralWFC;
        def.displayName = "Wave Function Collapse";
        def.description = "Tile-based generation using wave function collapse";
        def.category = NodeCategory::Procedural;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.8f);
        def.inputs = {
            FlowIn(),
            Int("Width", PK::Input, 16),
            Int("Height", PK::Input, 16),
            Int("Tile Set Size", PK::Input, 4),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"procedural", "wfc", "wave", "function", "collapse", "tiles"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 w = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 16;
            i32 h = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? std::get<i32>(inputs[2]) : 16;
            i32 tiles = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? std::get<i32>(inputs[3]) : 4;
            i32 seed = (inputs.size() > 4 && std::holds_alternative<i32>(inputs[4])) ? std::get<i32>(inputs[4]) : 0;

            Procedural::WaveFunctionCollapse::Params p;
            p.width = static_cast<u32>(w); p.height = static_cast<u32>(h);
            p.seed = static_cast<u32>(seed);
            u32 ts = static_cast<u32>(tiles);
            for (u32 i = 0; i < ts; i++) {
                Procedural::WaveFunctionCollapse::WFCTile tile;
                tile.id = i;
                for (int dir = 0; dir < 4; dir++)
                    for (u32 j = 0; j < ts; j++) tile.allowedNeighbors[dir].push_back(j);
                p.tiles.push_back(tile);
            }
            Procedural::WaveFunctionCollapse::Generate(p);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Grammar
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ProceduralGrammar;
        def.displayName = "Shape Grammar";
        def.description = "Generate structures using shape grammar rules";
        def.category = NodeCategory::Procedural;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.8f);
        def.inputs = {
            FlowIn(),
            String("Rules", PK::Input, "building=wall wall roof"),
            String("Start Symbol", PK::Input, "building"),
            Int("Seed", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"procedural", "grammar", "building", "structure", "shape"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string rules = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) ? std::get<std::string>(inputs[1]) : "";
            std::string start = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2])) ? std::get<std::string>(inputs[2]) : "building";
            i32 seed = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? std::get<i32>(inputs[3]) : 0;
            Procedural::GrammarGenerator::Params p;
            p.startSymbol = start;
            p.seed = static_cast<u32>(seed);
            Procedural::GrammarGenerator::Generate(p);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Spawn Grid
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ProceduralSpawnGrid;
        def.displayName = "Spawn Grid Entities";
        def.description = "Create wall/floor entities from the last generated grid";
        def.category = NodeCategory::Procedural;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.8f);
        def.inputs = {
            FlowIn(),
            Float("Cell Size", PK::Input, 1.0f),
            Int("Wall Value", PK::Input, 1),
            Int("Floor Value", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"procedural", "spawn", "grid", "entities", "instantiate"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            // Spawn grid delegates to script binding function; VS nodes use the same cached grid
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // AUDIO EVENT GRAPH NODES
    // ========================================================================

    // Audio Graph: Trigger Event
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AudioGraphTriggerEvent;
        def.displayName = "Trigger Audio Event";
        def.description = "Trigger a named event in the active audio event graph";
        def.category = NodeCategory::Audio;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.3f);
        def.inputs = {
            FlowIn(),
            String("Event Name", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"audio", "graph", "event", "trigger", "sound"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string eventName;
            if (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0])) {
                eventName = std::get<std::string>(inputs[0]);
            }
            if (!eventName.empty() && s_VisualScriptAudioGraphRuntime) {
                s_VisualScriptAudioGraphRuntime->TriggerEvent(eventName);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Audio Graph: Set Parameter
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AudioGraphSetParameter;
        def.displayName = "Set Audio Parameter";
        def.description = "Set a named parameter on the active audio event graph";
        def.category = NodeCategory::Audio;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.3f);
        def.inputs = {
            FlowIn(),
            String("Parameter", PK::Input),
            Float("Value", PK::Input, 0.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"audio", "graph", "parameter", "set", "value"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string paramName;
            f32 value = 0.0f;
            if (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0])) {
                paramName = std::get<std::string>(inputs[0]);
            }
            if (inputs.size() > 1) {
                if (std::holds_alternative<f32>(inputs[1])) {
                    value = std::get<f32>(inputs[1]);
                } else if (std::holds_alternative<i32>(inputs[1])) {
                    value = static_cast<f32>(std::get<i32>(inputs[1]));
                }
            }
            if (!paramName.empty() && s_VisualScriptAudioGraphRuntime) {
                s_VisualScriptAudioGraphRuntime->SetParameter(paramName, value);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Audio Graph: Stop All
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AudioGraphStopAll;
        def.displayName = "Stop All Audio Events";
        def.description = "Stop all sounds playing from the audio event graph";
        def.category = NodeCategory::Audio;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.3f);
        def.inputs = {FlowIn()};
        def.outputs = {FlowOut()};
        def.keywords = {"audio", "graph", "stop", "all", "silence"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (s_VisualScriptAudioGraphRuntime) {
                s_VisualScriptAudioGraphRuntime->StopAll();
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // PLUGIN NODES
    // ========================================================================

    // Plugin: Is Loaded
    {
        NodeDefinition def;
        def.typeId = NodeTypes::PluginIsLoaded;
        def.displayName = "Plugin Is Loaded";
        def.description = "Check if a plugin is currently loaded";
        def.category = NodeCategory::Utility;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.6f);
        def.inputs = {
            String("Plugin Name", PK::Input)
        };
        def.outputs = {
            Bool("Is Loaded", PK::Output)
        };
        def.keywords = {"plugin", "loaded", "check", "query"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string name;
            if (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0])) {
                name = std::get<std::string>(inputs[0]);
            }
            bool loaded = false;
            if (!name.empty() && s_VisualScriptPluginSystem) {
                loaded = s_VisualScriptPluginSystem->IsLoaded(name);
            }
            outputs.push_back(loaded);
        };
        RegisterNode(def);
    }

    // Plugin: Load
    {
        NodeDefinition def;
        def.typeId = NodeTypes::PluginLoad;
        def.displayName = "Load Plugin";
        def.description = "Load a plugin by name";
        def.category = NodeCategory::Utility;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            String("Plugin Name", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"plugin", "load", "enable"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string name;
            if (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0])) {
                name = std::get<std::string>(inputs[0]);
            }
            if (!name.empty() && s_VisualScriptPluginSystem) {
                s_VisualScriptPluginSystem->LoadPlugin(name);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Plugin: Unload
    {
        NodeDefinition def;
        def.typeId = NodeTypes::PluginUnload;
        def.displayName = "Unload Plugin";
        def.description = "Unload a plugin by name";
        def.category = NodeCategory::Utility;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            String("Plugin Name", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"plugin", "unload", "disable"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string name;
            if (inputs.size() > 0 && std::holds_alternative<std::string>(inputs[0])) {
                name = std::get<std::string>(inputs[0]);
            }
            if (!name.empty() && s_VisualScriptPluginSystem) {
                s_VisualScriptPluginSystem->UnloadPlugin(name);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // WATER
    // ========================================================================

    {
        NodeDefinition def;
        def.typeId = NodeTypes::WaterSetStyle;
        def.displayName = "Set Water Style";
        def.description = "Change water rendering style (0=Flat, 1=Animated, 2=VertexWave, 3=Reflective, 4=Refractive)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = { FlowIn(), Int("Style", PK::Input, 2) };
        def.outputs = { FlowOut() };
        def.keywords = {"water", "style", "wave", "flat", "reflective"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            i32 style = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 2;
            if (s_VisualScriptWater && style >= 0 && style <= 4) {
                s_VisualScriptWater->GetSettings().style = static_cast<Effects::WaterStyle>(style);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::WaterSetWaveHeight;
        def.displayName = "Set Wave Height";
        def.description = "Set the water wave displacement height";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = { FlowIn(), Float("Height", PK::Input, 0.3f) };
        def.outputs = { FlowOut() };
        def.keywords = {"water", "wave", "height"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            f32 h = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.3f;
            if (s_VisualScriptWater) s_VisualScriptWater->GetSettings().waveHeight = h;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::WaterSetWaveSpeed;
        def.displayName = "Set Wave Speed";
        def.description = "Set the water wave animation speed";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = { FlowIn(), Float("Speed", PK::Input, 1.0f) };
        def.outputs = { FlowOut() };
        def.keywords = {"water", "wave", "speed"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            f32 s = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 1.0f;
            if (s_VisualScriptWater) s_VisualScriptWater->GetSettings().waveSpeed = s;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::WaterGetWaveHeight;
        def.displayName = "Get Wave Height At";
        def.description = "Sample water wave height at world X/Z position";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = { Float("X", PK::Input, 0.0f), Float("Z", PK::Input, 0.0f) };
        def.outputs = { Float("Height", PK::Output, 0.0f) };
        def.flags = NodeDefFlags::Pure;
        def.keywords = {"water", "wave", "height", "sample", "query"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            f32 x = (inputs.size() > 0 && std::holds_alternative<f32>(inputs[0])) ? std::get<f32>(inputs[0]) : 0.0f;
            f32 z = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.0f;
            if (s_VisualScriptWater) return s_VisualScriptWater->GetWaveHeight(x, z);
            return 0.0f;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::WaterSetOpacity;
        def.displayName = "Set Water Opacity";
        def.description = "Set water surface opacity (0-1)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = { FlowIn(), Float("Opacity", PK::Input, 0.7f) };
        def.outputs = { FlowOut() };
        def.keywords = {"water", "opacity", "transparent"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            f32 o = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.7f;
            if (s_VisualScriptWater) s_VisualScriptWater->GetSettings().opacity = Math::Clamp(o, 0.0f, 1.0f);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::WaterSetColor;
        def.displayName = "Set Water Color";
        def.description = "Set the shallow water color (RGB)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = { FlowIn(), Vec3("Color", PK::Input, Math::Vector3(0.1f, 0.4f, 0.5f)) };
        def.outputs = { FlowOut() };
        def.keywords = {"water", "color", "tint"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            Math::Vector3 c(0.1f, 0.4f, 0.5f);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) c = std::get<Math::Vector3>(inputs[1]);
            if (s_VisualScriptWater) s_VisualScriptWater->GetSettings().shallowColor = c;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // HUD
    // ========================================================================

    {
        NodeDefinition def;
        def.typeId = NodeTypes::HUDSetEnabled;
        def.displayName = "Set HUD Enabled";
        def.description = "Enable or disable the HUD overlay";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.7f, 0.3f);
        def.inputs = { FlowIn(), Bool("Enabled", PK::Input, true) };
        def.outputs = { FlowOut() };
        def.keywords = {"hud", "enable", "disable", "overlay", "ui"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            bool enabled = (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) ? std::get<bool>(inputs[1]) : true;
            if (s_VisualScriptUI) s_VisualScriptUI->SetHUDEnabled(enabled);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::HUDIsEnabled;
        def.displayName = "Is HUD Enabled";
        def.description = "Check if the HUD overlay is currently enabled";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.7f, 0.3f);
        def.inputs = {};
        def.outputs = { Bool("Enabled", PK::Output, false) };
        def.flags = NodeDefFlags::Pure;
        def.keywords = {"hud", "enabled", "active", "check"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (s_VisualScriptUI) return s_VisualScriptUI->IsHUDEnabled();
            return false;
        };
        RegisterNode(def);
    }

    // HUD Widget component nodes
    {
        NodeDefinition def;
        def.typeId = NodeTypes::HUDSetVisible;
        def.displayName = "HUD Set Visible";
        def.description = "Show or hide a HUD widget on an entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Bool("Visible", PK::Input, true)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"hud", "widget", "visible", "show", "hide"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[1]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            bool visible = (inputs.size() > 2 && std::holds_alternative<bool>(inputs[2]))
                ? std::get<bool>(inputs[2]) : true;
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* canvas = ctx.world->GetComponent<GUI::UICanvasComponent>(target);
                if (canvas) canvas->visible = visible;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::HUDSetText;
        def.displayName = "HUD Set Text";
        def.description = "Set the display text of a HUD widget";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            String("Text", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"hud", "widget", "text", "label", "set"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[1]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            std::string text;
            if (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                text = std::get<std::string>(inputs[2]);
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* canvas = ctx.world->GetComponent<GUI::UICanvasComponent>(target);
                if (canvas) {
                    for (auto& el : canvas->elements) {
                        if (el.type == GUI::UIWidgetType::Label) { el.data.text = text; break; }
                    }
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::HUDSetValue;
        def.displayName = "HUD Set Value";
        def.description = "Set the current and max values of a HUD widget (for bars)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Current", PK::Input, 1.0f),
            Float("Max", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"hud", "widget", "value", "health", "bar", "progress"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[1]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            f32 current = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 1.0f;
            f32 maxVal = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3]))
                ? std::get<f32>(inputs[3]) : 1.0f;
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* canvas = ctx.world->GetComponent<GUI::UICanvasComponent>(target);
                if (canvas) {
                    for (auto& el : canvas->elements) {
                        if (el.type == GUI::UIWidgetType::ProgressBar) {
                            el.data.bindMaxValue = maxVal;
                            el.data.progressValue = (maxVal > 0.0f) ? (current / maxVal) : 0.0f;
                            break;
                        }
                    }
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Text component nodes
    {
        NodeDefinition def;
        def.typeId = NodeTypes::TextSetContent;
        def.displayName = "Text Set Content";
        def.description = "Set the text content of a TextComponent and trigger re-rasterization";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            String("Text", PK::Input, "")
        };
        def.outputs = {FlowOut()};
        def.keywords = {"text", "content", "set", "string", "label"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[1]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            std::string text;
            if (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                text = std::get<std::string>(inputs[2]);
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* tc = ctx.world->GetComponent<ECS::TextComponent>(target);
                if (tc) { tc->text = text; tc->dirty = true; }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::TextSetColor;
        def.displayName = "Text Set Color";
        def.description = "Set the text color of a TextComponent";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.2f, 0.5f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Color", PK::Input, Math::Vector3(1.0f, 1.0f, 1.0f))
        };
        def.outputs = {FlowOut()};
        def.keywords = {"text", "color", "font", "set"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[1]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 color(1.0f, 1.0f, 1.0f);
            if (inputs.size() > 2 && std::holds_alternative<Math::Vector3>(inputs[2]))
                color = std::get<Math::Vector3>(inputs[2]);
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* tc = ctx.world->GetComponent<ECS::TextComponent>(target);
                if (tc) { tc->textColor = color; tc->dirty = true; }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // Post-Process Volume nodes
    // ========================================================================

    // ========================================================================
    // Screen-Space Effects
    // ========================================================================

#if !ENJIN_RENDERER_WEBGPU
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SSEffectSetSSAO;
        def.displayName = "Set SSAO Enabled";
        def.description = "Enable or disable screen-space ambient occlusion";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.7f);
        def.inputs = { FlowIn(), Bool("Enabled", PK::Input, true) };
        def.outputs = { FlowOut() };
        def.keywords = {"ssao", "ambient", "occlusion", "screen-space"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            bool enabled = (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) ? std::get<bool>(inputs[1]) : true;
            if (s_VisualScriptPostProcessing) s_VisualScriptPostProcessing->GetSettings().ssaoEnabled = enabled ? 1u : 0u;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::SSEffectSetContactShadows;
        def.displayName = "Set Contact Shadows Enabled";
        def.description = "Enable or disable screen-space contact shadows";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.7f);
        def.inputs = { FlowIn(), Bool("Enabled", PK::Input, true) };
        def.outputs = { FlowOut() };
        def.keywords = {"contact", "shadows", "screen-space"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            bool enabled = (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) ? std::get<bool>(inputs[1]) : true;
            if (s_VisualScriptPostProcessing) s_VisualScriptPostProcessing->GetSettings().contactShadowsEnabled = enabled ? 1u : 0u;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::SSEffectSetGodRays;
        def.displayName = "Set God Rays Enabled";
        def.description = "Enable or disable screen-space god rays (light shafts from the sun)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.7f);
        def.inputs = { FlowIn(), Bool("Enabled", PK::Input, true) };
        def.outputs = { FlowOut() };
        def.keywords = {"god", "rays", "light", "shafts", "sun"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            bool enabled = (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) ? std::get<bool>(inputs[1]) : true;
            if (s_VisualScriptPostProcessing) s_VisualScriptPostProcessing->GetSettings().godRaysEnabled = enabled ? 1u : 0u;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::SSEffectSetCaustics;
        def.displayName = "Set Caustics Enabled";
        def.description = "Enable or disable fake underwater caustics pattern";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.7f);
        def.inputs = { FlowIn(), Bool("Enabled", PK::Input, true) };
        def.outputs = { FlowOut() };
        def.keywords = {"caustics", "water", "underwater"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            bool enabled = (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) ? std::get<bool>(inputs[1]) : true;
            if (s_VisualScriptPostProcessing) s_VisualScriptPostProcessing->GetSettings().causticsEnabled = enabled ? 1u : 0u;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::SSEffectSetFogShafts;
        def.displayName = "Set Fog Shafts Enabled";
        def.description = "Enable or disable volumetric-look fog shaft ray march";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.7f);
        def.inputs = { FlowIn(), Bool("Enabled", PK::Input, true) };
        def.outputs = { FlowOut() };
        def.keywords = {"fog", "shafts", "volumetric", "atmosphere"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            bool enabled = (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) ? std::get<bool>(inputs[1]) : true;
            if (s_VisualScriptPostProcessing) s_VisualScriptPostProcessing->GetSettings().fogShaftsEnabled = enabled ? 1u : 0u;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
#endif // !ENJIN_RENDERER_WEBGPU

    {
        NodeDefinition def;
        def.typeId = NodeTypes::PPVolumeSetActive;
        def.displayName = "Set PP Volume Active";
        def.description = "Enable or disable a post-process volume on an entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Bool("Active", PK::Input, true)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"post-process", "volume", "active", "enable", "disable"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            auto entity = (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]))
                ? static_cast<ECS::Entity>(std::get<u64>(inputs[1])) : ECS::Entity(0);
            bool active = (inputs.size() > 2 && std::holds_alternative<bool>(inputs[2]))
                ? std::get<bool>(inputs[2]) : true;
            if (ctx.world && entity != 0) {
                auto* vol = ctx.world->GetComponent<ECS::PostProcessVolumeComponent>(entity);
                if (vol) vol->isActive = active;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::PPVolumeSetWeight;
        def.displayName = "Set PP Volume Weight";
        def.description = "Set the blend weight of a post-process volume (0-1)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Weight", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"post-process", "volume", "weight", "blend"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            auto entity = (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]))
                ? static_cast<ECS::Entity>(std::get<u64>(inputs[1])) : ECS::Entity(0);
            f32 weight = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 1.0f;
            if (ctx.world && entity != 0) {
                auto* vol = ctx.world->GetComponent<ECS::PostProcessVolumeComponent>(entity);
                if (vol) vol->weight = std::clamp(weight, 0.0f, 1.0f);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::PPVolumeSetBlendRadius;
        def.displayName = "Set PP Volume Blend Radius";
        def.description = "Set the blend radius for smooth transitions at volume edges";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Radius", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"post-process", "volume", "blend", "radius", "transition"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            auto entity = (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]))
                ? static_cast<ECS::Entity>(std::get<u64>(inputs[1])) : ECS::Entity(0);
            f32 radius = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 1.0f;
            if (ctx.world && entity != 0) {
                auto* vol = ctx.world->GetComponent<ECS::PostProcessVolumeComponent>(entity);
                if (vol) vol->blendRadius = std::max(radius, 0.0f);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    {
        NodeDefinition def;
        def.typeId = NodeTypes::PPVolumeSetPriority;
        def.displayName = "Set PP Volume Priority";
        def.description = "Set the priority of a post-process volume (higher overrides lower)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Int("Priority", PK::Input, 0)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"post-process", "volume", "priority", "order"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            auto entity = (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]))
                ? static_cast<ECS::Entity>(std::get<u64>(inputs[1])) : ECS::Entity(0);
            i32 priority = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2]))
                ? std::get<i32>(inputs[2]) : 0;
            if (ctx.world && entity != 0) {
                auto* vol = ctx.world->GetComponent<ECS::PostProcessVolumeComponent>(entity);
                if (vol) vol->priority = priority;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // SPRITE 2D NODES
    // ========================================================================

    // Set Sprite Texture
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SpriteSetTexture;
        def.displayName = "Set Sprite Texture";
        def.description = "Set the texture path of a Sprite2D component";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.4f, 0.7f, 0.3f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            String("Path", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"sprite", "texture", "image", "set"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            std::string path;
            if (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) {
                path = std::get<std::string>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* sprite = ctx.world->GetComponent<ECS::Sprite2DComponent>(target);
                if (sprite) sprite->texturePath = path;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Set Sprite Color
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SpriteSetColor;
        def.displayName = "Set Sprite Color";
        def.description = "Set the tint color and alpha of a Sprite2D component";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.4f, 0.7f, 0.3f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Tint", PK::Input),
            Float("Alpha", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"sprite", "color", "tint", "alpha", "opacity"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 tint(1.0f, 1.0f, 1.0f);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                tint = std::get<Math::Vector3>(inputs[1]);
            }
            f32 alpha = 1.0f;
            if (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) {
                alpha = std::get<f32>(inputs[2]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* sprite = ctx.world->GetComponent<ECS::Sprite2DComponent>(target);
                if (sprite) {
                    sprite->tint = tint;
                    sprite->alpha = alpha;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Set Sprite Flip
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SpriteSetFlip;
        def.displayName = "Set Sprite Flip";
        def.description = "Set horizontal and vertical flip of a Sprite2D component";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.4f, 0.7f, 0.3f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Bool("Flip X", PK::Input, false),
            Bool("Flip Y", PK::Input, false)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"sprite", "flip", "mirror", "horizontal", "vertical"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            bool flipX = false, flipY = false;
            if (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) {
                flipX = std::get<bool>(inputs[1]);
            }
            if (inputs.size() > 2 && std::holds_alternative<bool>(inputs[2])) {
                flipY = std::get<bool>(inputs[2]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* sprite = ctx.world->GetComponent<ECS::Sprite2DComponent>(target);
                if (sprite) {
                    sprite->flipX = flipX;
                    sprite->flipY = flipY;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Get Sprite Flip
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SpriteGetFlip;
        def.displayName = "Get Sprite Flip";
        def.description = "Get horizontal and vertical flip state of a Sprite2D component";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.4f, 0.7f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {
            Bool("Flip X", PK::Output),
            Bool("Flip Y", PK::Output)
        };
        def.keywords = {"sprite", "flip", "mirror", "get"};
        // Pure nodes with multiple outputs: executor handles indexing; we return Flip X
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* sprite = ctx.world->GetComponent<ECS::Sprite2DComponent>(target);
                if (sprite) return sprite->flipX;
            }
            return false;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // LIGHT NODES
    // ========================================================================

    // Set Light Color
    {
        NodeDefinition def;
        def.typeId = NodeTypes::LightSetColor;
        def.displayName = "Set Light Color";
        def.description = "Set the color of a light component";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.9f, 0.8f, 0.2f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Color", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"light", "color", "set"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 color(1.0f, 1.0f, 1.0f);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                color = std::get<Math::Vector3>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* light = ctx.world->GetComponent<ECS::LightComponent>(target);
                if (light) light->color = color;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Set Light Intensity
    {
        NodeDefinition def;
        def.typeId = NodeTypes::LightSetIntensity;
        def.displayName = "Set Light Intensity";
        def.description = "Set the intensity of a light component";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.9f, 0.8f, 0.2f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Intensity", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"light", "intensity", "brightness", "set"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            f32 intensity = 1.0f;
            if (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) {
                intensity = std::get<f32>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* light = ctx.world->GetComponent<ECS::LightComponent>(target);
                if (light) light->intensity = intensity;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Get Light Intensity
    {
        NodeDefinition def;
        def.typeId = NodeTypes::LightGetIntensity;
        def.displayName = "Get Light Intensity";
        def.description = "Get the intensity of a light component";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.9f, 0.8f, 0.2f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Float("Intensity", PK::Output)};
        def.keywords = {"light", "intensity", "brightness", "get"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* light = ctx.world->GetComponent<ECS::LightComponent>(target);
                if (light) return light->intensity;
            }
            return 0.0f;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // CAMERA NODES
    // ========================================================================

    // Set Camera FOV
    {
        NodeDefinition def;
        def.typeId = NodeTypes::CameraSetFOV;
        def.displayName = "Set Camera FOV";
        def.description = "Set the field of view of a camera component";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.8f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("FOV", PK::Input, 60.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"camera", "fov", "field of view", "zoom"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            f32 fov = 60.0f;
            if (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) {
                fov = std::get<f32>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* cam = ctx.world->GetComponent<ECS::CameraComponent>(target);
                if (cam) cam->fieldOfView = fov;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Set Camera Active
    {
        NodeDefinition def;
        def.typeId = NodeTypes::CameraSetActive;
        def.displayName = "Set Camera Active";
        def.description = "Enable or disable a camera component";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.8f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Bool("Active", PK::Input, true)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"camera", "active", "enable", "disable"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            bool active = true;
            if (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) {
                active = std::get<bool>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* cam = ctx.world->GetComponent<ECS::CameraComponent>(target);
                if (cam) cam->isActive = active;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // MATERIAL NODES
    // ========================================================================

    // Set Material Color
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MaterialSetColor;
        def.displayName = "Set Material Color";
        def.description = "Set the base color and opacity of a material";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Color", PK::Input),
            Float("Opacity", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"material", "color", "albedo", "opacity"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 color(1.0f, 1.0f, 1.0f);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                color = std::get<Math::Vector3>(inputs[1]);
            }
            f32 opacity = 1.0f;
            if (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) {
                opacity = std::get<f32>(inputs[2]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* mat = ctx.world->GetComponent<ECS::MaterialComponent>(target);
                if (mat) {
                    mat->baseColor = color;
                    mat->opacity = opacity;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Set Material Emissive
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MaterialSetEmissive;
        def.displayName = "Set Material Emissive";
        def.description = "Set the emissive color and strength of a material";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Vec3("Color", PK::Input),
            Float("Strength", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"material", "emissive", "glow", "emission"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            Math::Vector3 color(0.0f, 0.0f, 0.0f);
            if (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) {
                color = std::get<Math::Vector3>(inputs[1]);
            }
            f32 strength = 1.0f;
            if (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) {
                strength = std::get<f32>(inputs[2]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* mat = ctx.world->GetComponent<ECS::MaterialComponent>(target);
                if (mat) {
                    mat->emissiveColor = color;
                    mat->emissiveStrength = strength;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Set Material Transmission
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MaterialSetTransmission;
        def.displayName = "Set Material Transmission";
        def.description = "Set transmission, IOR, and thickness for glass/water/translucent materials";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Transmission", PK::Input, 0.0f),
            Float("IOR", PK::Input, 1.5f),
            Float("Thickness", PK::Input, 0.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"material", "transmission", "glass", "refraction", "ior", "translucent"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            f32 transmission = 0.0f, ior = 1.5f, thickness = 0.0f;
            if (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) transmission = std::get<f32>(inputs[1]);
            if (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ior = std::get<f32>(inputs[2]);
            if (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3])) thickness = std::get<f32>(inputs[3]);
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* mat = ctx.world->GetComponent<ECS::MaterialComponent>(target);
                if (mat) {
                    mat->transmission = transmission;
                    mat->ior = ior;
                    mat->thickness = thickness;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Set Material SSS
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MaterialSetSSS;
        def.displayName = "Set Material SSS";
        def.description = "Set subsurface scattering intensity, radius, and color";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.6f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Intensity", PK::Input, 0.0f),
            Float("Radius", PK::Input, 1.0f),
            Vec3("Color", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"material", "sss", "subsurface", "scattering", "skin", "wax"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            f32 intensity = 0.0f, radius = 1.0f;
            Math::Vector3 color(1.0f, 0.2f, 0.1f);
            if (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) intensity = std::get<f32>(inputs[1]);
            if (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) radius = std::get<f32>(inputs[2]);
            if (inputs.size() > 3 && std::holds_alternative<Math::Vector3>(inputs[3])) color = std::get<Math::Vector3>(inputs[3]);
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* mat = ctx.world->GetComponent<ECS::MaterialComponent>(target);
                if (mat) {
                    mat->sssIntensity = intensity;
                    mat->sssRadius = radius;
                    mat->sssColor = color;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // VISIBILITY NODES
    // ========================================================================

    // Set Visible
    {
        NodeDefinition def;
        def.typeId = NodeTypes::EntitySetVisible;
        def.displayName = "Set Visible";
        def.description = "Show or hide an entity by setting its transform visibility";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.5f, 0.5f, 0.5f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Bool("Visible", PK::Input, true)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"visible", "show", "hide", "visibility"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            bool visible = true;
            if (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) {
                visible = std::get<bool>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) transform->visible = visible;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Is Visible
    {
        NodeDefinition def;
        def.typeId = NodeTypes::EntityIsVisible;
        def.displayName = "Is Visible";
        def.description = "Check if an entity is visible";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.5f, 0.5f, 0.5f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Bool("Visible", PK::Output)};
        def.keywords = {"visible", "visibility", "shown", "hidden"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* transform = ctx.world->GetComponent<ECS::TransformComponent>(target);
                if (transform) return transform->visible;
            }
            return false;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // OBJECT POOL NODES
    // ========================================================================

    // Pool Acquire
    {
        NodeDefinition def;
        def.typeId = NodeTypes::PoolAcquire;
        def.displayName = "Pool Acquire";
        def.description = "Acquire an entity from an object pool";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.7f, 0.4f);
        def.inputs = {
            FlowIn(),
            String("Pool ID", PK::Input)
        };
        def.outputs = {
            {PinDefinition{"Success", Editor::PinType::Flow, Editor::PinKind::Output, {}, false}},
            {PinDefinition{"Failed", Editor::PinType::Flow, Editor::PinKind::Output, {}, false}},
            EntityPin("Entity", PK::Output)
        };
        def.keywords = {"pool", "acquire", "get", "spawn", "object"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string poolId;
            if (!inputs.empty() && std::holds_alternative<std::string>(inputs[0])) {
                poolId = std::get<std::string>(inputs[0]);
            }
            ECS::Entity entity = ECS::INVALID_ENTITY;
            if (s_VisualScriptObjectPool && !poolId.empty()) {
                entity = s_VisualScriptObjectPool->Acquire(poolId);
            }
            outputs.resize(1);
            outputs[0] = entity;
            ctx.nextFlowIndex = (entity != ECS::INVALID_ENTITY) ? 0 : 1;
        };
        RegisterNode(def);
    }

    // Pool Release
    {
        NodeDefinition def;
        def.typeId = NodeTypes::PoolRelease;
        def.displayName = "Pool Release";
        def.description = "Release an entity back to an object pool";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.7f, 0.4f);
        def.inputs = {
            FlowIn(),
            String("Pool ID", PK::Input),
            EntityPin("Entity", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"pool", "release", "return", "recycle", "object"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string poolId;
            if (!inputs.empty() && std::holds_alternative<std::string>(inputs[0])) {
                poolId = std::get<std::string>(inputs[0]);
            }
            ECS::Entity entity = ECS::INVALID_ENTITY;
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1])) {
                entity = std::get<ECS::Entity>(inputs[1]);
            }
            if (s_VisualScriptObjectPool && !poolId.empty() && entity != ECS::INVALID_ENTITY) {
                s_VisualScriptObjectPool->Release(poolId, entity);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // SAVE DATA NODES
    // ========================================================================

    // Save Data Set
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SaveDataSet;
        def.displayName = "Save Data Set";
        def.description = "Set a key-value pair on an entity's SaveDataComponent";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            String("Key", PK::Input),
            String("Value", PK::Input)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"save", "data", "set", "key", "value", "persist"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            std::string key, value;
            if (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) {
                key = std::get<std::string>(inputs[1]);
            }
            if (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2])) {
                value = std::get<std::string>(inputs[2]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY && !key.empty()) {
                auto* saveData = ctx.world->GetComponent<ECS::SaveDataComponent>(target);
                if (saveData) saveData->SetData(key, value);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Save Data Get
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SaveDataGet;
        def.displayName = "Save Data Get";
        def.description = "Get a value by key from an entity's SaveDataComponent";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            EntityPin("Entity", PK::Input),
            String("Key", PK::Input),
            String("Default", PK::Input)
        };
        def.outputs = {String("Value", PK::Output)};
        def.keywords = {"save", "data", "get", "key", "value", "read"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            std::string key, defaultVal;
            if (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) {
                key = std::get<std::string>(inputs[1]);
            }
            if (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2])) {
                defaultVal = std::get<std::string>(inputs[2]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* saveData = ctx.world->GetComponent<ECS::SaveDataComponent>(target);
                if (saveData) return saveData->GetData(key, defaultVal);
            }
            return defaultVal;
        };
        RegisterNode(def);
    }

    // Save Data Has Tag
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SaveDataHasTag;
        def.displayName = "Save Data Has Tag";
        def.description = "Check if an entity's SaveDataComponent has a specific tag";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            EntityPin("Entity", PK::Input),
            String("Tag", PK::Input)
        };
        def.outputs = {Bool("Has Tag", PK::Output)};
        def.keywords = {"save", "data", "tag", "has", "check"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            std::string tag;
            if (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) {
                tag = std::get<std::string>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* saveData = ctx.world->GetComponent<ECS::SaveDataComponent>(target);
                if (saveData) return saveData->HasTag(tag);
            }
            return false;
        };
        RegisterNode(def);
    }

    // ========================================================================
    // RIGIDBODY NODES
    // ========================================================================

    // Set Kinematic
    {
        NodeDefinition def;
        def.typeId = NodeTypes::PhysicsSetKinematic;
        def.displayName = "Set Kinematic";
        def.description = "Set whether a rigidbody is kinematic or dynamic";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.8f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Bool("Kinematic", PK::Input, false)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"physics", "kinematic", "dynamic", "rigidbody", "body type"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            bool kinematic = false;
            if (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) {
                kinematic = std::get<bool>(inputs[1]);
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* rb = ctx.world->GetComponent<ECS::RigidbodyComponent>(target);
                if (rb) {
                    rb->bodyType = kinematic ? ECS::RigidbodyComponent::BodyType::Kinematic
                                             : ECS::RigidbodyComponent::BodyType::Dynamic;
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // Get Angular Velocity
    {
        NodeDefinition def;
        def.typeId = NodeTypes::PhysicsGetAngularVelocity;
        def.displayName = "Get Angular Velocity";
        def.description = "Get the angular velocity of a rigidbody";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.8f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Vec3("Angular Velocity", PK::Output)};
        def.keywords = {"physics", "angular", "velocity", "rotation", "spin"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* rb = ctx.world->GetComponent<ECS::RigidbodyComponent>(target);
                if (rb) return rb->angularVelocity;
            }
            return Math::Vector3(0.0f, 0.0f, 0.0f);
        };
        RegisterNode(def);
    }

    // =========================================================================
    // ELEMENTAL SYSTEM NODES
    // =========================================================================

    // --- Elemental_SpawnFire ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalSpawnFire;
        def.displayName = "Spawn Fire";
        def.description = "Spawn fire particles at a world position";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.9f, 0.3f, 0.1f);
        def.inputs = {
            FlowIn(),
            Vec3("Position", PK::Input),
            Float("Intensity", PK::Input, 1.0f),
            Float("Lifetime", PK::Input, 3.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"elemental", "fire", "spawn", "flame", "burn"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            Math::Vector3 pos = (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1]))
                ? std::get<Math::Vector3>(inputs[1]) : Math::Vector3(0, 0, 0);
            f32 intensity = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 1.0f;
            f32 lifetime = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3]))
                ? std::get<f32>(inputs[3]) : 3.0f;
            if (s_VisualScriptElemental)
                s_VisualScriptElemental->SpawnFire(pos, intensity, lifetime);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // --- Elemental_SpawnWater ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalSpawnWater;
        def.displayName = "Spawn Water";
        def.description = "Spawn water particles at a world position with velocity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.4f, 0.9f);
        def.inputs = {
            FlowIn(),
            Vec3("Position", PK::Input),
            Vec3("Velocity", PK::Input),
            Float("Intensity", PK::Input, 1.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"elemental", "water", "spawn", "rain", "splash"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            Math::Vector3 pos = (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1]))
                ? std::get<Math::Vector3>(inputs[1]) : Math::Vector3(0, 0, 0);
            Math::Vector3 vel = (inputs.size() > 2 && std::holds_alternative<Math::Vector3>(inputs[2]))
                ? std::get<Math::Vector3>(inputs[2]) : Math::Vector3(0, -2, 0);
            f32 intensity = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3]))
                ? std::get<f32>(inputs[3]) : 1.0f;
            if (s_VisualScriptElemental)
                s_VisualScriptElemental->SpawnWater(pos, vel, intensity);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // --- Elemental_SpawnSnow ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalSpawnSnow;
        def.displayName = "Spawn Snow";
        def.description = "Spawn snow particles at a world position";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.8f, 0.9f, 1.0f);
        def.inputs = {
            FlowIn(),
            Vec3("Position", PK::Input),
            Float("Size", PK::Input, 0.15f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"elemental", "snow", "spawn", "ice", "cold"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            Math::Vector3 pos = (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1]))
                ? std::get<Math::Vector3>(inputs[1]) : Math::Vector3(0, 0, 0);
            f32 size = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 0.15f;
            if (s_VisualScriptElemental)
                s_VisualScriptElemental->SpawnSnow(pos, size);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // --- Elemental_SpawnSteam ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalSpawnSteam;
        def.displayName = "Spawn Steam";
        def.description = "Spawn steam particles at a world position";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.7f, 0.7f, 0.8f);
        def.inputs = {
            FlowIn(),
            Vec3("Position", PK::Input),
            Float("Intensity", PK::Input, 0.8f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"elemental", "steam", "spawn", "vapor"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            Math::Vector3 pos = (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1]))
                ? std::get<Math::Vector3>(inputs[1]) : Math::Vector3(0, 0, 0);
            f32 intensity = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                ? std::get<f32>(inputs[2]) : 0.8f;
            if (s_VisualScriptElemental)
                s_VisualScriptElemental->SpawnSteam(pos, intensity);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // --- Elemental_SpawnDebris ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalSpawnDebris;
        def.displayName = "Spawn Debris";
        def.description = "Spawn a burst of earth/debris particles";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.35f, 0.15f);
        def.inputs = {
            FlowIn(),
            Vec3("Center", PK::Input),
            Vec3("Force", PK::Input),
            Int("Count", PK::Input, 8)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"elemental", "earth", "debris", "spawn", "burst", "destruction"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            Math::Vector3 center = (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1]))
                ? std::get<Math::Vector3>(inputs[1]) : Math::Vector3(0, 0, 0);
            Math::Vector3 force = (inputs.size() > 2 && std::holds_alternative<Math::Vector3>(inputs[2]))
                ? std::get<Math::Vector3>(inputs[2]) : Math::Vector3(0, 5, 0);
            i32 count = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3]))
                ? std::get<i32>(inputs[3]) : 8;
            if (s_VisualScriptElemental && count > 0)
                s_VisualScriptElemental->SpawnDebrisBurst(center, force, static_cast<u32>(count));
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // --- Elemental_GetFireIntensity (pure) ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalGetFireIntensity;
        def.displayName = "Get Fire Intensity";
        def.description = "Sample fire intensity at a world position";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.9f, 0.3f, 0.1f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Vec3("Position", PK::Input),
            Float("Radius", PK::Input, 3.0f)
        };
        def.outputs = {Float("Intensity", PK::Output)};
        def.keywords = {"elemental", "fire", "query", "intensity", "heat"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            Math::Vector3 pos = (!inputs.empty() && std::holds_alternative<Math::Vector3>(inputs[0]))
                ? std::get<Math::Vector3>(inputs[0]) : Math::Vector3(0, 0, 0);
            f32 radius = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1]))
                ? std::get<f32>(inputs[1]) : 3.0f;
            if (s_VisualScriptElemental)
                return s_VisualScriptElemental->GetFireIntensityAt(pos, radius);
            return 0.0f;
        };
        RegisterNode(def);
    }

    // --- Elemental_GetMoisture (pure) ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalGetMoisture;
        def.displayName = "Get Moisture";
        def.description = "Sample moisture/water level at a world position";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.2f, 0.4f, 0.9f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {
            Vec3("Position", PK::Input),
            Float("Radius", PK::Input, 3.0f)
        };
        def.outputs = {Float("Moisture", PK::Output)};
        def.keywords = {"elemental", "water", "moisture", "query", "wet"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            Math::Vector3 pos = (!inputs.empty() && std::holds_alternative<Math::Vector3>(inputs[0]))
                ? std::get<Math::Vector3>(inputs[0]) : Math::Vector3(0, 0, 0);
            f32 radius = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1]))
                ? std::get<f32>(inputs[1]) : 3.0f;
            if (s_VisualScriptElemental)
                return s_VisualScriptElemental->GetMoistureAt(pos, radius);
            return 0.0f;
        };
        RegisterNode(def);
    }

    // --- Elemental_GetActiveCount (pure) ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalGetActiveCount;
        def.displayName = "Get Elemental Count";
        def.description = "Get the number of active elemental particles";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.2f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {};
        def.outputs = {Int("Count", PK::Output)};
        def.keywords = {"elemental", "count", "particles", "active"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            if (s_VisualScriptElemental)
                return static_cast<i32>(s_VisualScriptElemental->GetActiveCount());
            return 0;
        };
        RegisterNode(def);
    }

    // --- Elemental_SetEmitterActive ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalSetEmitterActive;
        def.displayName = "Set Emitter Active";
        def.description = "Enable or disable an elemental emitter on an entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.9f, 0.5f, 0.2f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Bool("Active", PK::Input, true)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"elemental", "emitter", "active", "toggle"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1]) && ctx.world) {
                bool active = (inputs.size() > 2 && std::holds_alternative<bool>(inputs[2]))
                    ? std::get<bool>(inputs[2]) : true;
                auto* emitter = ctx.world->GetComponent<ECS::ElementalEmitterComponent>(
                    std::get<ECS::Entity>(inputs[1]));
                if (emitter) emitter->active = active;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // --- Elemental_SetEmitterElement ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalSetEmitterElement;
        def.displayName = "Set Emitter Element";
        def.description = "Change the element signature of an emitter (fire, water, earth, air weights)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.9f, 0.5f, 0.2f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Fire", PK::Input, 1.0f),
            Float("Water", PK::Input, 0.0f),
            Float("Earth", PK::Input, 0.0f),
            Float("Air", PK::Input, 0.0f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"elemental", "emitter", "element", "type", "fire", "water", "earth", "air"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1]) && ctx.world) {
                f32 fire  = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ? std::get<f32>(inputs[2]) : 1.0f;
                f32 water = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3])) ? std::get<f32>(inputs[3]) : 0.0f;
                f32 earth = (inputs.size() > 4 && std::holds_alternative<f32>(inputs[4])) ? std::get<f32>(inputs[4]) : 0.0f;
                f32 air   = (inputs.size() > 5 && std::holds_alternative<f32>(inputs[5])) ? std::get<f32>(inputs[5]) : 0.0f;
                auto* emitter = ctx.world->GetComponent<ECS::ElementalEmitterComponent>(
                    std::get<ECS::Entity>(inputs[1]));
                if (emitter) emitter->element = Math::Vector4(fire, water, earth, air);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // --- Elemental_SetFlammability ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalSetFlammability;
        def.displayName = "Set Flammability";
        def.description = "Set how flammable an entity's elemental surface is (0=fireproof, 1=kindling)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.9f, 0.3f, 0.1f);
        def.inputs = {
            FlowIn(),
            EntityPin("Entity", PK::Input),
            Float("Flammability", PK::Input, 0.5f)
        };
        def.outputs = {FlowOut()};
        def.keywords = {"elemental", "surface", "flammability", "burn", "fire"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            if (inputs.size() > 1 && std::holds_alternative<ECS::Entity>(inputs[1]) && ctx.world) {
                f32 flam = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                    ? std::get<f32>(inputs[2]) : 0.5f;
                auto* surface = ctx.world->GetComponent<ECS::ElementalSurfaceComponent>(
                    std::get<ECS::Entity>(inputs[1]));
                if (surface) surface->flammability = Math::Clamp(flam, 0.0f, 1.0f);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // --- Elemental_GetSurfaceState (pure, multi-output) ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ElementalGetSurfaceState;
        def.displayName = "Get Surface State";
        def.description = "Get the elemental surface state (charring, wetness, snow, frost) of an entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.4f, 0.2f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {
            Float("Char", PK::Output),
            Float("Wetness", PK::Output),
            Float("Snow", PK::Output),
            Float("Frost", PK::Output)
        };
        def.keywords = {"elemental", "surface", "char", "wet", "snow", "frost", "query"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }
            if (ctx.world && target != ECS::INVALID_ENTITY) {
                auto* surface = ctx.world->GetComponent<ECS::ElementalSurfaceComponent>(target);
                if (surface) return surface->charAmount;
            }
            return 0.0f;
        };
        RegisterNode(def);
    }

    // =======================================================================
    // Tier-1 node-coverage additions (see docs/NODE_COVERAGE.md):
    // hierarchy, tags, save-meta (bool/int/string), sprite animation.
    // All operate at the ECS component level like their sibling nodes.
    // =======================================================================

    // Helper lambda pattern for entity input: default to ctx.entity, override
    // with a connected entity pin when non-zero.

    // --- Hierarchy ---
    // Set Parent
    {
        NodeDefinition def;
        def.typeId = NodeTypes::EntitySetParent;
        def.displayName = "Set Parent";
        def.description = "Re-parent Child under Parent (keeps hierarchy transforms)";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.5f);
        def.inputs = {FlowIn(), EntityPin("Child", PK::Input), EntityPin("Parent", PK::Input)};
        def.outputs = {FlowOut()};
        def.keywords = {"parent", "child", "hierarchy", "attach", "reparent"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity child = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                child = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            ECS::Entity parent = ECS::INVALID_ENTITY;
            if (inputs.size() > 2 && std::holds_alternative<u64>(inputs[2]))
                parent = static_cast<ECS::Entity>(std::get<u64>(inputs[2]));
            if (ctx.world && child != ECS::INVALID_ENTITY)
                ECS::SetParent(ctx.world, child, parent);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Remove Parent
    {
        NodeDefinition def;
        def.typeId = NodeTypes::EntityRemoveParent;
        def.displayName = "Remove Parent";
        def.description = "Detach an entity from its parent (becomes a root)";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.5f);
        def.inputs = {FlowIn(), EntityPin("Child", PK::Input)};
        def.outputs = {FlowOut()};
        def.keywords = {"parent", "detach", "unparent", "hierarchy", "root"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity child = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                child = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            if (ctx.world && child != ECS::INVALID_ENTITY)
                ECS::RemoveParent(ctx.world, child);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Get Parent
    {
        NodeDefinition def;
        def.typeId = NodeTypes::EntityGetParent;
        def.displayName = "Get Parent";
        def.description = "The parent of an entity (invalid if it is a root)";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.5f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Child", PK::Input)};
        def.outputs = {EntityPin("Parent", PK::Output)};
        def.keywords = {"parent", "hierarchy", "get"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity child = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                child = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            if (!ctx.world || child == ECS::INVALID_ENTITY) return ECS::INVALID_ENTITY;
            return ECS::GetParent(ctx.world, child);
        };
        RegisterNode(def);
    }
    // Get Child Count
    {
        NodeDefinition def;
        def.typeId = NodeTypes::EntityGetChildCount;
        def.displayName = "Get Child Count";
        def.description = "Number of direct children of an entity";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.5f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Int("Count", PK::Output)};
        def.keywords = {"child", "children", "count", "hierarchy"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            if (!ctx.world || e == ECS::INVALID_ENTITY) return static_cast<i32>(0);
            return static_cast<i32>(ECS::GetChildren(ctx.world, e).size());
        };
        RegisterNode(def);
    }
    // Get Child
    {
        NodeDefinition def;
        def.typeId = NodeTypes::EntityGetChild;
        def.displayName = "Get Child";
        def.description = "The child at Index (invalid if out of range)";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.5f, 0.4f, 0.5f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input), Int("Index", PK::Input, 0)};
        def.outputs = {EntityPin("Child", PK::Output)};
        def.keywords = {"child", "children", "index", "hierarchy"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            i32 index = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 0;
            if (!ctx.world || e == ECS::INVALID_ENTITY || index < 0) return ECS::INVALID_ENTITY;
            const auto& kids = ECS::GetChildren(ctx.world, e);
            if (static_cast<usize>(index) >= kids.size()) return ECS::INVALID_ENTITY;
            return kids[static_cast<usize>(index)];
        };
        RegisterNode(def);
    }

    // --- Tags ---
    // Add Tag
    {
        NodeDefinition def;
        def.typeId = NodeTypes::EntityAddTag;
        def.displayName = "Add Tag";
        def.description = "Add a string tag to an entity (creates TagComponent if needed)";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.45f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), String("Tag", PK::Input, "")};
        def.outputs = {FlowOut()};
        def.keywords = {"tag", "label", "add", "mark"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity e = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            std::string tag = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                ? std::get<std::string>(inputs[2]) : "";
            if (ctx.world && e != ECS::INVALID_ENTITY && !tag.empty()) {
                auto* tc = ctx.world->GetComponent<ECS::TagComponent>(e);
                if (!tc) tc = &ctx.world->AddComponent<ECS::TagComponent>(e);
                tc->AddTag(tag);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Remove Tag
    {
        NodeDefinition def;
        def.typeId = NodeTypes::EntityRemoveTag;
        def.displayName = "Remove Tag";
        def.description = "Remove a string tag from an entity";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.45f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), String("Tag", PK::Input, "")};
        def.outputs = {FlowOut()};
        def.keywords = {"tag", "label", "remove", "unmark"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity e = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            std::string tag = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                ? std::get<std::string>(inputs[2]) : "";
            if (ctx.world && e != ECS::INVALID_ENTITY && !tag.empty()) {
                auto* tc = ctx.world->GetComponent<ECS::TagComponent>(e);
                if (tc) tc->RemoveTag(tag);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Has Tag
    {
        NodeDefinition def;
        def.typeId = NodeTypes::EntityHasTag;
        def.displayName = "Has Tag";
        def.description = "True if the entity carries the given tag";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.45f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input), String("Tag", PK::Input, "")};
        def.outputs = {Bool("Has Tag", PK::Output)};
        def.keywords = {"tag", "label", "has", "check", "is"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            std::string tag = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            if (!ctx.world || e == ECS::INVALID_ENTITY || tag.empty()) return false;
            auto* tc = ctx.world->GetComponent<ECS::TagComponent>(e);
            return tc ? tc->HasTag(tag) : false;
        };
        RegisterNode(def);
    }
    // Find Entity By Tag
    {
        NodeDefinition def;
        def.typeId = NodeTypes::FindEntityByTag;
        def.displayName = "Find Entity By Tag";
        def.description = "First entity carrying the given tag (invalid if none)";
        def.category = NodeCategory::Entity;
        def.headerColor = Math::Vector3(0.4f, 0.5f, 0.45f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {String("Tag", PK::Input, "")};
        def.outputs = {EntityPin("Entity", PK::Output)};
        def.keywords = {"tag", "find", "search", "query", "byTag"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            std::string tag = (!inputs.empty() && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            if (!ctx.world || tag.empty()) return ECS::INVALID_ENTITY;
            for (ECS::Entity e : ctx.world->GetEntitiesWithComponent<ECS::TagComponent>()) {
                auto* tc = ctx.world->GetComponent<ECS::TagComponent>(e);
                if (tc && tc->HasTag(tag)) return e;
            }
            return ECS::INVALID_ENTITY;
        };
        RegisterNode(def);
    }

    // --- Save meta (bool / int / string) ---
    // Set Meta Bool
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MetaSetBool;
        def.displayName = "Set Meta Bool";
        def.description = "Set a permanent meta-progression bool value";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.inputs = {FlowIn(), String("Key", PK::Input, ""), Bool("Value", PK::Input, false)};
        def.outputs = {FlowOut()};
        def.keywords = {"meta", "progression", "set", "bool", "flag", "permanent"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string key = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            bool val = (inputs.size() > 2 && std::holds_alternative<bool>(inputs[2])) && std::get<bool>(inputs[2]);
            if (s_VisualScriptSaveSystem && !key.empty()) {
                s_VisualScriptSaveSystem->SetMetaBool(key, val);
                s_VisualScriptSaveSystem->SaveMeta();
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Get Meta Bool
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MetaGetBool;
        def.displayName = "Get Meta Bool";
        def.description = "Read a permanent meta-progression bool value";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {String("Key", PK::Input, "")};
        def.outputs = {Bool("Value", PK::Output)};
        def.keywords = {"meta", "progression", "get", "bool", "flag"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            std::string key = (!inputs.empty() && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            if (s_VisualScriptSaveSystem && !key.empty())
                return s_VisualScriptSaveSystem->GetMetaBool(key, false);
            return false;
        };
        RegisterNode(def);
    }
    // Set Meta Int
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MetaSetInt;
        def.displayName = "Set Meta Int";
        def.description = "Set a permanent meta-progression int value";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.inputs = {FlowIn(), String("Key", PK::Input, ""), Int("Value", PK::Input, 0)};
        def.outputs = {FlowOut()};
        def.keywords = {"meta", "progression", "set", "int", "permanent"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string key = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            i32 val = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? std::get<i32>(inputs[2]) : 0;
            if (s_VisualScriptSaveSystem && !key.empty()) {
                s_VisualScriptSaveSystem->SetMetaInt(key, val);
                s_VisualScriptSaveSystem->SaveMeta();
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Get Meta Int
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MetaGetInt;
        def.displayName = "Get Meta Int";
        def.description = "Read a permanent meta-progression int value";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {String("Key", PK::Input, "")};
        def.outputs = {Int("Value", PK::Output)};
        def.keywords = {"meta", "progression", "get", "int"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            std::string key = (!inputs.empty() && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            if (s_VisualScriptSaveSystem && !key.empty())
                return s_VisualScriptSaveSystem->GetMetaInt(key, 0);
            return static_cast<i32>(0);
        };
        RegisterNode(def);
    }
    // Set Meta String
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MetaSetString;
        def.displayName = "Set Meta String";
        def.description = "Set a permanent meta-progression string value";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.inputs = {FlowIn(), String("Key", PK::Input, ""), String("Value", PK::Input, "")};
        def.outputs = {FlowOut()};
        def.keywords = {"meta", "progression", "set", "string", "text", "permanent"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            std::string key = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            std::string val = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                ? std::get<std::string>(inputs[2]) : "";
            if (s_VisualScriptSaveSystem && !key.empty()) {
                s_VisualScriptSaveSystem->SetMetaString(key, val);
                s_VisualScriptSaveSystem->SaveMeta();
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Get Meta String
    {
        NodeDefinition def;
        def.typeId = NodeTypes::MetaGetString;
        def.displayName = "Get Meta String";
        def.description = "Read a permanent meta-progression string value";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.5f, 0.3f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {String("Key", PK::Input, "")};
        def.outputs = {String("Value", PK::Output)};
        def.keywords = {"meta", "progression", "get", "string", "text"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            std::string key = (!inputs.empty() && std::holds_alternative<std::string>(inputs[0]))
                ? std::get<std::string>(inputs[0]) : "";
            if (s_VisualScriptSaveSystem && !key.empty())
                return s_VisualScriptSaveSystem->GetMetaString(key, "");
            return std::string("");
        };
        RegisterNode(def);
    }

    // --- Sprite animation (AnimatedSprite2DComponent) ---
    // Play
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SpriteAnimPlay;
        def.displayName = "Sprite Anim Play";
        def.description = "Start/restart the sprite's frame animation from frame 0";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.5f, 0.3f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input)};
        def.outputs = {FlowOut()};
        def.keywords = {"sprite", "animation", "play", "start", "frame"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity e = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* ac = ctx.world ? ctx.world->GetComponent<ECS::AnimatedSprite2DComponent>(e) : nullptr;
            if (ac) {
                ac->playing = true;
                ac->animationComplete = false;
                ac->currentFrame = 0;
                ac->frameTimer = 0.0f;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Stop
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SpriteAnimStop;
        def.displayName = "Sprite Anim Stop";
        def.description = "Pause the sprite's frame animation";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.5f, 0.3f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input)};
        def.outputs = {FlowOut()};
        def.keywords = {"sprite", "animation", "stop", "pause", "frame"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity e = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* ac = ctx.world ? ctx.world->GetComponent<ECS::AnimatedSprite2DComponent>(e) : nullptr;
            if (ac) ac->playing = false;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Set Speed
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SpriteAnimSetSpeed;
        def.displayName = "Sprite Anim Set Speed";
        def.description = "Set the sprite animation playback speed multiplier";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.5f, 0.3f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), Float("Speed", PK::Input, 1.0f)};
        def.outputs = {FlowOut()};
        def.keywords = {"sprite", "animation", "speed", "rate", "frame"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity e = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            f32 speed = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ? std::get<f32>(inputs[2]) : 1.0f;
            auto* ac = ctx.world ? ctx.world->GetComponent<ECS::AnimatedSprite2DComponent>(e) : nullptr;
            if (ac) ac->playbackSpeed = speed;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Is Playing
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SpriteAnimIsPlaying;
        def.displayName = "Sprite Anim Is Playing";
        def.description = "True while the sprite's frame animation is running";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Bool("Playing", PK::Output)};
        def.keywords = {"sprite", "animation", "playing", "is", "frame"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* ac = ctx.world ? ctx.world->GetComponent<ECS::AnimatedSprite2DComponent>(e) : nullptr;
            return ac ? ac->playing : false;
        };
        RegisterNode(def);
    }
    // Get Current Frame
    {
        NodeDefinition def;
        def.typeId = NodeTypes::SpriteAnimGetFrame;
        def.displayName = "Sprite Anim Get Frame";
        def.description = "The current frame index of the sprite animation";
        def.category = NodeCategory::Components;
        def.headerColor = Math::Vector3(0.6f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Int("Frame", PK::Output)};
        def.keywords = {"sprite", "animation", "frame", "current", "index"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* ac = ctx.world ? ctx.world->GetComponent<ECS::AnimatedSprite2DComponent>(e) : nullptr;
            return ac ? static_cast<i32>(ac->currentFrame) : static_cast<i32>(0);
        };
        RegisterNode(def);
    }

    // =======================================================================
    // Branching dialogue (docs/NODE_COVERAGE.md tier-1 #1). Reads are pure
    // component lookups; Choose / Get-Set Variable need the DialogueSystem
    // runtime tree player (ctx.dialogue), wired in VisualScriptExecutor.
    // =======================================================================

    // Choose (select a branching choice)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DialogueChoose;
        def.displayName = "Dialogue Choose";
        def.description = "Select a branching dialogue choice by index";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), Int("Choice", PK::Input, 0)};
        def.outputs = {FlowOut()};
        def.keywords = {"dialogue", "choice", "choose", "select", "branch", "option"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            i32 choice = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? std::get<i32>(inputs[2]) : 0;
            if (ctx.dialogue && ctx.world && target != ECS::INVALID_ENTITY && choice >= 0)
                ctx.dialogue->SelectChoice(ctx.world, target, static_cast<u32>(choice));
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Get Choice Count
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DialogueGetChoiceCount;
        def.displayName = "Dialogue Choice Count";
        def.description = "Number of choices currently offered by the dialogue";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Int("Count", PK::Output)};
        def.keywords = {"dialogue", "choice", "count", "options"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* dlg = ctx.world ? ctx.world->GetComponent<ECS::DialogueComponent>(target) : nullptr;
            if (!dlg) return static_cast<i32>(0);
            return static_cast<i32>(dlg->IsTreeMode() ? dlg->currentChoices.size() : dlg->choices.size());
        };
        RegisterNode(def);
    }
    // Get Choice Text
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DialogueGetChoiceText;
        def.displayName = "Dialogue Choice Text";
        def.description = "Text of the choice at the given index";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input), Int("Index", PK::Input, 0)};
        def.outputs = {String("Text", PK::Output)};
        def.keywords = {"dialogue", "choice", "text", "option", "label"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            i32 index = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 0;
            auto* dlg = ctx.world ? ctx.world->GetComponent<ECS::DialogueComponent>(target) : nullptr;
            if (!dlg || index < 0) return std::string("");
            if (dlg->IsTreeMode()) {
                if (static_cast<usize>(index) < dlg->currentChoices.size())
                    return dlg->currentChoices[static_cast<usize>(index)].text;
            } else {
                if (static_cast<usize>(index) < dlg->choices.size())
                    return dlg->choices[static_cast<usize>(index)].text;
            }
            return std::string("");
        };
        RegisterNode(def);
    }
    // Get Current Speaker
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DialogueGetSpeaker;
        def.displayName = "Dialogue Speaker";
        def.description = "Name of the current dialogue speaker";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {String("Speaker", PK::Output)};
        def.keywords = {"dialogue", "speaker", "name", "who"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* dlg = ctx.world ? ctx.world->GetComponent<ECS::DialogueComponent>(target) : nullptr;
            if (!dlg) return std::string("");
            return dlg->IsTreeMode() ? dlg->currentSpeaker : dlg->speakerName;
        };
        RegisterNode(def);
    }
    // Get Current Text
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DialogueGetText;
        def.displayName = "Dialogue Text";
        def.description = "Currently visible dialogue line (respects typewriter)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {String("Text", PK::Output)};
        def.keywords = {"dialogue", "text", "line", "say"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* dlg = ctx.world ? ctx.world->GetComponent<ECS::DialogueComponent>(target) : nullptr;
            if (!dlg) return std::string("");
            return dlg->IsTreeMode() ? dlg->GetTreeVisibleText() : dlg->GetVisibleText();
        };
        RegisterNode(def);
    }
    // Set Variable
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DialogueSetVariable;
        def.displayName = "Dialogue Set Variable";
        def.description = "Set a dialogue variable (drives branching conditions)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), String("Name", PK::Input, ""), String("Value", PK::Input, "")};
        def.outputs = {FlowOut()};
        def.keywords = {"dialogue", "variable", "set", "flag", "condition"};
        def.execute = [](ExecutionContext& ctx,
                         const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            std::string name = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2]))
                ? std::get<std::string>(inputs[2]) : "";
            std::string value = (inputs.size() > 3 && std::holds_alternative<std::string>(inputs[3]))
                ? std::get<std::string>(inputs[3]) : "";
            if (ctx.dialogue && ctx.world && target != ECS::INVALID_ENTITY && !name.empty())
                ctx.dialogue->SetVariable(ctx.world, target, name, value);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Get Variable
    {
        NodeDefinition def;
        def.typeId = NodeTypes::DialogueGetVariable;
        def.displayName = "Dialogue Get Variable";
        def.description = "Read a dialogue variable value";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input), String("Name", PK::Input, "")};
        def.outputs = {String("Value", PK::Output)};
        def.keywords = {"dialogue", "variable", "get", "flag", "condition"};
        def.evaluate = [](const ExecutionContext& ctx,
                          const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            std::string name = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1]))
                ? std::get<std::string>(inputs[1]) : "";
            if (ctx.dialogue && ctx.world && target != ECS::INVALID_ENTITY && !name.empty())
                return ctx.dialogue->GetVariable(ctx.world, target, name);
            return std::string("");
        };
        RegisterNode(def);
    }

    // =======================================================================
    // Accessibility settings (docs/NODE_COVERAGE.md tier-2). Get/Set over the
    // runtime accessibility settings so a node-only game can drive an in-game
    // accessibility menu. Forwarders reuse the AngelScript wiring (settings
    // pointer + apply/save callbacks) so Set takes effect immediately.
    // =======================================================================

    // Small helpers to cut the boilerplate for uniform Set/Get nodes.
    auto addA11ySetFloat = [this](const char* id, const char* name, const char* desc,
                                  const char* kw, void(*fn)(f32), f32 def_) {
        NodeDefinition def;
        def.typeId = id; def.displayName = name; def.description = desc;
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.35f, 0.6f);
        def.inputs = {FlowIn(), Float("Value", PK::Input, def_)};
        def.outputs = {FlowOut()};
        def.keywords = {"accessibility", "a11y", kw, "set"};
        def.execute = [fn](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                           std::vector<ECS::VariableValue>&) {
            f32 v = (inputs.size() > 1 && std::holds_alternative<f32>(inputs[1])) ? std::get<f32>(inputs[1]) : 0.0f;
            fn(v);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    };
    auto addA11yGetFloat = [this](const char* id, const char* name, const char* desc,
                                  const char* kw, f32(*fn)()) {
        NodeDefinition def;
        def.typeId = id; def.displayName = name; def.description = desc;
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.35f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"accessibility", "a11y", kw, "get"};
        def.evaluate = [fn](const ExecutionContext&, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return fn();
        };
        RegisterNode(def);
    };
    auto addA11ySetBool = [this](const char* id, const char* name, const char* desc,
                                 const char* kw, void(*fn)(bool)) {
        NodeDefinition def;
        def.typeId = id; def.displayName = name; def.description = desc;
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.35f, 0.6f);
        def.inputs = {FlowIn(), Bool("Enabled", PK::Input, false)};
        def.outputs = {FlowOut()};
        def.keywords = {"accessibility", "a11y", kw, "set", "toggle"};
        def.execute = [fn](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                           std::vector<ECS::VariableValue>&) {
            bool v = (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) && std::get<bool>(inputs[1]);
            fn(v);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    };
    auto addA11yGetBool = [this](const char* id, const char* name, const char* desc,
                                 const char* kw, bool(*fn)()) {
        NodeDefinition def;
        def.typeId = id; def.displayName = name; def.description = desc;
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.35f, 0.6f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Bool("Enabled", PK::Output)};
        def.keywords = {"accessibility", "a11y", kw, "get"};
        def.evaluate = [fn](const ExecutionContext&, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return fn();
        };
        RegisterNode(def);
    };

    using namespace Enjin::Scripting;
    addA11ySetFloat(NodeTypes::A11ySetFontScale, "A11y Set Font Scale",
        "Set the UI font scale (0.5-3.0)", "font", &VSAccessSetFontScale, 1.0f);
    addA11yGetFloat(NodeTypes::A11yGetFontScale, "A11y Get Font Scale",
        "Read the UI font scale", "font", &VSAccessGetFontScale);
    addA11ySetBool(NodeTypes::A11ySetReducedMotion, "A11y Set Reduced Motion",
        "Toggle reduced-motion mode", "motion", &VSAccessSetReducedMotion);
    addA11yGetBool(NodeTypes::A11yGetReducedMotion, "A11y Get Reduced Motion",
        "Read reduced-motion mode", "motion", &VSAccessGetReducedMotion);
    addA11ySetBool(NodeTypes::A11ySetScreenShake, "A11y Set Screen Shake Disabled",
        "Disable/enable screen shake", "shake", &VSAccessSetDisableScreenShake);
    addA11yGetBool(NodeTypes::A11yGetScreenShake, "A11y Get Screen Shake Disabled",
        "Read whether screen shake is disabled", "shake", &VSAccessGetDisableScreenShake);
    addA11ySetFloat(NodeTypes::A11ySetContrast, "A11y Set Contrast",
        "Set the screen contrast multiplier", "contrast", &VSAccessSetContrast, 1.0f);
    addA11yGetFloat(NodeTypes::A11yGetContrast, "A11y Get Contrast",
        "Read the screen contrast multiplier", "contrast", &VSAccessGetContrast);
    addA11ySetFloat(NodeTypes::A11ySetColorblindStrength, "A11y Set Colorblind Strength",
        "Set colorblind-filter strength (0-1)", "colorblind", &VSAccessSetColorblindStrength, 1.0f);
    addA11yGetFloat(NodeTypes::A11yGetColorblindStrength, "A11y Get Colorblind Strength",
        "Read colorblind-filter strength", "colorblind", &VSAccessGetColorblindStrength);
    addA11ySetBool(NodeTypes::A11ySetSubtitles, "A11y Set Subtitles",
        "Toggle subtitles", "subtitles", &VSAccessSetSubtitles);
    addA11yGetBool(NodeTypes::A11yGetSubtitles, "A11y Get Subtitles",
        "Read whether subtitles are on", "subtitles", &VSAccessGetSubtitles);
    addA11ySetBool(NodeTypes::A11ySetDyslexiaFont, "A11y Set Dyslexia Font",
        "Toggle the dyslexia-friendly font", "dyslexia", &VSAccessSetDyslexiaFont);
    addA11yGetBool(NodeTypes::A11yGetDyslexiaFont, "A11y Get Dyslexia Font",
        "Read whether the dyslexia font is on", "dyslexia", &VSAccessGetDyslexiaFont);
    addA11ySetBool(NodeTypes::A11ySetScreenReader, "A11y Set Screen Reader",
        "Toggle the screen reader", "screenreader", &VSAccessSetScreenReader);
    addA11yGetBool(NodeTypes::A11yGetScreenReader, "A11y Get Screen Reader",
        "Read whether the screen reader is on", "screenreader", &VSAccessGetScreenReader);
    // Save (persist current settings)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::A11ySave;
        def.displayName = "A11y Save Settings";
        def.description = "Persist the current accessibility settings to disk";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.35f, 0.6f);
        def.inputs = {FlowIn()};
        def.outputs = {FlowOut()};
        def.keywords = {"accessibility", "a11y", "save", "persist"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>&,
                         std::vector<ECS::VariableValue>&) {
            Enjin::Scripting::VSAccessSave();
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // =======================================================================
    // Entity direction vectors + AI tuning (docs/NODE_COVERAGE.md tier-2).
    // Pure ECS component access.
    // =======================================================================

    // Direction vectors from TransformComponent rotation.
    auto addDirection = [this](const char* id, const char* name, const char* desc,
                               const char* kw, Math::Vector3 axis) {
        NodeDefinition def;
        def.typeId = id; def.displayName = name; def.description = desc;
        def.category = NodeCategory::Transform;
        def.headerColor = Math::Vector3(0.4f, 0.55f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Vec3("Direction", PK::Output)};
        def.keywords = {"direction", "vector", kw, "transform"};
        def.evaluate = [axis](const ExecutionContext& ctx,
                              const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* t = ctx.world ? ctx.world->GetComponent<ECS::TransformComponent>(e) : nullptr;
            return t ? t->rotation.Rotate(axis) : axis;
        };
        RegisterNode(def);
    };
    addDirection(NodeTypes::EntityGetForward, "Get Forward",
        "The entity's forward (-Z) direction in world space", "forward", Math::Vector3(0, 0, -1));
    addDirection(NodeTypes::EntityGetRight, "Get Right",
        "The entity's right (+X) direction in world space", "right", Math::Vector3(1, 0, 0));
    addDirection(NodeTypes::EntityGetUp, "Get Up",
        "The entity's up (+Y) direction in world space", "up", Math::Vector3(0, 1, 0));

    // AI tuning — set/get scalar fields on AIControllerComponent.
    auto addAISetFloat = [this](const char* id, const char* name, const char* desc,
                                const char* kw, f32 ECS::AIControllerComponent::* field, f32 def_) {
        NodeDefinition def;
        def.typeId = id; def.displayName = name; def.description = desc;
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.7f, 0.45f, 0.35f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), Float("Value", PK::Input, def_)};
        def.outputs = {FlowOut()};
        def.keywords = {"ai", "agent", kw, "set"};
        def.execute = [field](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                              std::vector<ECS::VariableValue>&) {
            ECS::Entity e = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            f32 v = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ? std::get<f32>(inputs[2]) : 0.0f;
            auto* ai = ctx.world ? ctx.world->GetComponent<ECS::AIControllerComponent>(e) : nullptr;
            if (ai) ai->*field = v;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    };
    auto addAIGetFloat = [this](const char* id, const char* name, const char* desc,
                                const char* kw, f32 ECS::AIControllerComponent::* field, f32 fallback) {
        NodeDefinition def;
        def.typeId = id; def.displayName = name; def.description = desc;
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.7f, 0.45f, 0.35f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input)};
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"ai", "agent", kw, "get"};
        def.evaluate = [field, fallback](const ExecutionContext& ctx,
                                         const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            auto* ai = ctx.world ? ctx.world->GetComponent<ECS::AIControllerComponent>(e) : nullptr;
            return ai ? ai->*field : fallback;
        };
        RegisterNode(def);
    };
    addAISetFloat(NodeTypes::AISetMoveSpeed, "AI Set Move Speed", "Set the agent's move speed", "speed", &ECS::AIControllerComponent::moveSpeed, 3.0f);
    addAIGetFloat(NodeTypes::AIGetMoveSpeed, "AI Get Move Speed", "Read the agent's move speed", "speed", &ECS::AIControllerComponent::moveSpeed, 0.0f);
    addAISetFloat(NodeTypes::AISetDetectionRange, "AI Set Detection Range", "Set how far the agent can sense targets", "detection", &ECS::AIControllerComponent::detectionRange, 10.0f);
    addAIGetFloat(NodeTypes::AIGetDetectionRange, "AI Get Detection Range", "Read the agent's detection range", "detection", &ECS::AIControllerComponent::detectionRange, 0.0f);
    addAISetFloat(NodeTypes::AISetAttackRange, "AI Set Attack Range", "Set the agent's attack range", "attack", &ECS::AIControllerComponent::attackRange, 2.0f);
    addAIGetFloat(NodeTypes::AIGetAttackRange, "AI Get Attack Range", "Read the agent's attack range", "attack", &ECS::AIControllerComponent::attackRange, 0.0f);
    addAISetFloat(NodeTypes::AISetChaseSpeed, "AI Set Chase Speed", "Set the agent's speed while chasing", "chase", &ECS::AIControllerComponent::chaseSpeed, 5.0f);
    addAISetFloat(NodeTypes::AISetFleeSpeed, "AI Set Flee Speed", "Set the agent's speed while fleeing", "flee", &ECS::AIControllerComponent::fleeSpeed, 6.0f);
    addAISetFloat(NodeTypes::AISetFieldOfView, "AI Set Field Of View", "Set the agent's vision cone in degrees", "fov", &ECS::AIControllerComponent::fieldOfView, 120.0f);

    // AI Set Use Navmesh (bool)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AISetUseNavmesh;
        def.displayName = "AI Set Use Navmesh";
        def.description = "Toggle whether the agent uses navmesh pathfinding";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.7f, 0.45f, 0.35f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), Bool("Use Navmesh", PK::Input, true)};
        def.outputs = {FlowOut()};
        def.keywords = {"ai", "agent", "navmesh", "pathfinding", "set"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>&) {
            ECS::Entity e = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            bool v = (inputs.size() > 2 && std::holds_alternative<bool>(inputs[2])) && std::get<bool>(inputs[2]);
            auto* ai = ctx.world ? ctx.world->GetComponent<ECS::AIControllerComponent>(e) : nullptr;
            if (ai) ai->useNavmesh = v;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // AI Set Target Position (Vector3)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::AISetTargetPosition;
        def.displayName = "AI Set Target Position";
        def.description = "Set the world position the agent moves toward";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.7f, 0.45f, 0.35f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), Vec3("Position", PK::Input)};
        def.outputs = {FlowOut()};
        def.keywords = {"ai", "agent", "target", "position", "move", "set"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>&) {
            ECS::Entity e = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                e = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            Math::Vector3 pos = (inputs.size() > 2 && std::holds_alternative<Math::Vector3>(inputs[2]))
                ? std::get<Math::Vector3>(inputs[2]) : Math::Vector3(0, 0, 0);
            auto* ai = ctx.world ? ctx.world->GetComponent<ECS::AIControllerComponent>(e) : nullptr;
            if (ai) ai->targetPosition = pos;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // =======================================================================
    // BehaviorTree blackboard (docs/NODE_COVERAGE.md tier-2). Pure component
    // access: bt->blackboard.Set/Get on ECS::BehaviorTreeComponent. The
    // blackboard is how BTs carry data, so this is what makes BT nodes usable.
    // =======================================================================

    // Shared entity-input resolver for the BB nodes.
    auto bbEntity = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, usize idx) -> ECS::Entity {
        ECS::Entity e = ctx.entity;
        if (inputs.size() > idx && std::holds_alternative<u64>(inputs[idx]) && std::get<u64>(inputs[idx]) != 0)
            e = static_cast<ECS::Entity>(std::get<u64>(inputs[idx]));
        return e;
    };

    // --- Set Blackboard {Bool,Float,Int,String} (flow) ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTSetBBBool;
        def.displayName = "BT Set Blackboard Bool";
        def.description = "Write a bool into the entity's behavior-tree blackboard";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.55f, 0.5f, 0.3f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), String("Key", PK::Input, ""), Bool("Value", PK::Input, false)};
        def.outputs = {FlowOut()};
        def.keywords = {"behaviortree", "bt", "blackboard", "bool", "set"};
        def.execute = [bbEntity](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            ECS::Entity e = bbEntity(ctx, inputs, 1);
            std::string key = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2])) ? std::get<std::string>(inputs[2]) : "";
            bool v = (inputs.size() > 3 && std::holds_alternative<bool>(inputs[3])) && std::get<bool>(inputs[3]);
            auto* bt = ctx.world ? ctx.world->GetComponent<ECS::BehaviorTreeComponent>(e) : nullptr;
            if (bt && !key.empty()) bt->blackboard.Set(key, AI::BlackboardValue(v));
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTSetBBFloat;
        def.displayName = "BT Set Blackboard Float";
        def.description = "Write a float into the entity's behavior-tree blackboard";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.55f, 0.5f, 0.3f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), String("Key", PK::Input, ""), Float("Value", PK::Input, 0.0f)};
        def.outputs = {FlowOut()};
        def.keywords = {"behaviortree", "bt", "blackboard", "float", "set"};
        def.execute = [bbEntity](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            ECS::Entity e = bbEntity(ctx, inputs, 1);
            std::string key = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2])) ? std::get<std::string>(inputs[2]) : "";
            f32 v = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3])) ? std::get<f32>(inputs[3]) : 0.0f;
            auto* bt = ctx.world ? ctx.world->GetComponent<ECS::BehaviorTreeComponent>(e) : nullptr;
            if (bt && !key.empty()) bt->blackboard.Set(key, AI::BlackboardValue(v));
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTSetBBInt;
        def.displayName = "BT Set Blackboard Int";
        def.description = "Write an int into the entity's behavior-tree blackboard";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.55f, 0.5f, 0.3f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), String("Key", PK::Input, ""), Int("Value", PK::Input, 0)};
        def.outputs = {FlowOut()};
        def.keywords = {"behaviortree", "bt", "blackboard", "int", "set"};
        def.execute = [bbEntity](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            ECS::Entity e = bbEntity(ctx, inputs, 1);
            std::string key = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2])) ? std::get<std::string>(inputs[2]) : "";
            i32 v = (inputs.size() > 3 && std::holds_alternative<i32>(inputs[3])) ? std::get<i32>(inputs[3]) : 0;
            auto* bt = ctx.world ? ctx.world->GetComponent<ECS::BehaviorTreeComponent>(e) : nullptr;
            if (bt && !key.empty()) bt->blackboard.Set(key, AI::BlackboardValue(v));
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTSetBBString;
        def.displayName = "BT Set Blackboard String";
        def.description = "Write a string into the entity's behavior-tree blackboard";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.55f, 0.5f, 0.3f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), String("Key", PK::Input, ""), String("Value", PK::Input, "")};
        def.outputs = {FlowOut()};
        def.keywords = {"behaviortree", "bt", "blackboard", "string", "set"};
        def.execute = [bbEntity](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            ECS::Entity e = bbEntity(ctx, inputs, 1);
            std::string key = (inputs.size() > 2 && std::holds_alternative<std::string>(inputs[2])) ? std::get<std::string>(inputs[2]) : "";
            std::string v = (inputs.size() > 3 && std::holds_alternative<std::string>(inputs[3])) ? std::get<std::string>(inputs[3]) : "";
            auto* bt = ctx.world ? ctx.world->GetComponent<ECS::BehaviorTreeComponent>(e) : nullptr;
            if (bt && !key.empty()) bt->blackboard.Set(key, AI::BlackboardValue(v));
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // --- Get Blackboard {Bool,Float,Int,String} (pure) ---
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTGetBBBool;
        def.displayName = "BT Get Blackboard Bool";
        def.description = "Read a bool from the entity's behavior-tree blackboard";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.55f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input), String("Key", PK::Input, "")};
        def.outputs = {Bool("Value", PK::Output)};
        def.keywords = {"behaviortree", "bt", "blackboard", "bool", "get"};
        def.evaluate = [bbEntity](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = bbEntity(ctx, inputs, 0);
            std::string key = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) ? std::get<std::string>(inputs[1]) : "";
            auto* bt = ctx.world ? ctx.world->GetComponent<ECS::BehaviorTreeComponent>(e) : nullptr;
            if (bt && !key.empty()) { auto* v = bt->blackboard.Get(key); if (v && std::holds_alternative<bool>(*v)) return std::get<bool>(*v); }
            return false;
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTGetBBFloat;
        def.displayName = "BT Get Blackboard Float";
        def.description = "Read a float from the entity's behavior-tree blackboard";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.55f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input), String("Key", PK::Input, "")};
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"behaviortree", "bt", "blackboard", "float", "get"};
        def.evaluate = [bbEntity](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = bbEntity(ctx, inputs, 0);
            std::string key = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) ? std::get<std::string>(inputs[1]) : "";
            auto* bt = ctx.world ? ctx.world->GetComponent<ECS::BehaviorTreeComponent>(e) : nullptr;
            if (bt && !key.empty()) { auto* v = bt->blackboard.Get(key); if (v && std::holds_alternative<f32>(*v)) return std::get<f32>(*v); }
            return 0.0f;
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTGetBBInt;
        def.displayName = "BT Get Blackboard Int";
        def.description = "Read an int from the entity's behavior-tree blackboard";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.55f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input), String("Key", PK::Input, "")};
        def.outputs = {Int("Value", PK::Output)};
        def.keywords = {"behaviortree", "bt", "blackboard", "int", "get"};
        def.evaluate = [bbEntity](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = bbEntity(ctx, inputs, 0);
            std::string key = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) ? std::get<std::string>(inputs[1]) : "";
            auto* bt = ctx.world ? ctx.world->GetComponent<ECS::BehaviorTreeComponent>(e) : nullptr;
            if (bt && !key.empty()) { auto* v = bt->blackboard.Get(key); if (v && std::holds_alternative<i32>(*v)) return std::get<i32>(*v); }
            return static_cast<i32>(0);
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::BTGetBBString;
        def.displayName = "BT Get Blackboard String";
        def.description = "Read a string from the entity's behavior-tree blackboard";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.55f, 0.5f, 0.3f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input), String("Key", PK::Input, "")};
        def.outputs = {String("Value", PK::Output)};
        def.keywords = {"behaviortree", "bt", "blackboard", "string", "get"};
        def.evaluate = [bbEntity](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity e = bbEntity(ctx, inputs, 0);
            std::string key = (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) ? std::get<std::string>(inputs[1]) : "";
            auto* bt = ctx.world ? ctx.world->GetComponent<ECS::BehaviorTreeComponent>(e) : nullptr;
            if (bt && !key.empty()) { auto* v = bt->blackboard.Get(key); if (v && std::holds_alternative<std::string>(*v)) return std::get<std::string>(*v); }
            return std::string("");
        };
        RegisterNode(def);
    }

    // =======================================================================
    // Navmesh queries (docs/NODE_COVERAGE.md tier-2). Forward through the AI
    // bindings' navmesh/pathfinder pointers + last-path buffer (VSNavmesh*).
    // =======================================================================

    // Has Navmesh (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NavmeshHasNavmesh;
        def.displayName = "Has Navmesh";
        def.description = "True if a navmesh + pathfinder are available";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.5f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Bool("Has Navmesh", PK::Output)};
        def.keywords = {"navmesh", "navigation", "pathfinding", "has"};
        def.evaluate = [](const ExecutionContext&, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return Enjin::Scripting::VSNavmeshHasNavmesh();
        };
        RegisterNode(def);
    }
    // Find Path (flow: Start,End -> waypoint Count; stores the path for Get Waypoint)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NavmeshFindPath;
        def.displayName = "Navmesh Find Path";
        def.description = "Find a path from Start to End; outputs the waypoint count (read them with Get Waypoint)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.5f);
        def.inputs = {FlowIn(), Vec3("Start", PK::Input), Vec3("End", PK::Input)};
        def.outputs = {FlowOut(), Int("Waypoint Count", PK::Output)};
        def.keywords = {"navmesh", "navigation", "pathfinding", "path", "find", "route"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            Math::Vector3 s = (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) ? std::get<Math::Vector3>(inputs[1]) : Math::Vector3(0,0,0);
            Math::Vector3 e = (inputs.size() > 2 && std::holds_alternative<Math::Vector3>(inputs[2])) ? std::get<Math::Vector3>(inputs[2]) : Math::Vector3(0,0,0);
            i32 count = Enjin::Scripting::VSNavmeshFindPath(s.x, s.y, s.z, e.x, e.y, e.z);
            outputs.resize(1);
            outputs[0] = count;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Get Waypoint (pure: Index -> Position from the last Find Path)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NavmeshGetWaypoint;
        def.displayName = "Navmesh Get Waypoint";
        def.description = "World position of the waypoint at Index from the last Find Path";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.5f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {Int("Index", PK::Input, 0)};
        def.outputs = {Vec3("Position", PK::Output)};
        def.keywords = {"navmesh", "navigation", "pathfinding", "waypoint", "path"};
        def.evaluate = [](const ExecutionContext&, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            i32 index = (!inputs.empty() && std::holds_alternative<i32>(inputs[0])) ? std::get<i32>(inputs[0]) : 0;
            f32 x, y, z;
            Enjin::Scripting::VSNavmeshGetWaypoint(index, x, y, z);
            return Math::Vector3(x, y, z);
        };
        RegisterNode(def);
    }
    // Path Exists (pure: Start,End -> bool)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NavmeshPathExists;
        def.displayName = "Navmesh Path Exists";
        def.description = "True if a path exists from Start to End (no allocation of the path)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.3f, 0.6f, 0.5f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {Vec3("Start", PK::Input), Vec3("End", PK::Input)};
        def.outputs = {Bool("Exists", PK::Output)};
        def.keywords = {"navmesh", "navigation", "pathfinding", "path", "exists", "reachable"};
        def.evaluate = [](const ExecutionContext&, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            Math::Vector3 s = (!inputs.empty() && std::holds_alternative<Math::Vector3>(inputs[0])) ? std::get<Math::Vector3>(inputs[0]) : Math::Vector3(0,0,0);
            Math::Vector3 e = (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1])) ? std::get<Math::Vector3>(inputs[1]) : Math::Vector3(0,0,0);
            return Enjin::Scripting::VSNavmeshPathExists(s.x, s.y, s.z, e.x, e.y, e.z);
        };
        RegisterNode(def);
    }

    // =======================================================================
    // Tween extras (docs/NODE_COVERAGE.md tier-2): opacity, read value, stop all.
    // Mirrors the existing Tween Float/Scale nodes (operate on TweenComponent).
    // =======================================================================

    // Tween Opacity (flow) — animate MaterialComponent.opacity
    {
        NodeDefinition def;
        def.typeId = NodeTypes::TweenOpacity;
        def.displayName = "Tween Opacity";
        def.description = "Fade an entity's material opacity from Start to End over Duration";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.8f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input), Float("Start", PK::Input, 1.0f),
                      Float("End", PK::Input, 0.0f), Float("Duration", PK::Input, 1.0f)};
        def.outputs = {FlowOut()};
        def.keywords = {"tween", "opacity", "fade", "alpha", "animate"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>&) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            if (!ctx.world) { ctx.nextFlowIndex = 0; return; }
            auto* tc = ctx.world->GetComponent<ECS::TweenComponent>(target);
            if (!tc) { ctx.nextFlowIndex = 0; return; }
            if (tc->tweens.empty()) tc->tweens.push_back(ECS::TweenEntry{});
            auto& entry = tc->tweens[0];
            f32 startVal = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ? std::get<f32>(inputs[2]) : 1.0f;
            f32 endVal   = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3])) ? std::get<f32>(inputs[3]) : 0.0f;
            entry.property = ECS::TweenProperty::Opacity;
            entry.startValue = Math::Vector3(startVal, 0, 0);
            entry.endValue = Math::Vector3(endVal, 0, 0);
            entry.useCurrentAsStart = false;
            if (inputs.size() > 4 && std::holds_alternative<f32>(inputs[4])) entry.duration = std::get<f32>(inputs[4]);
            entry.elapsed = 0.0f;
            entry.isPlaying = true;
            entry.isComplete = false;
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Tween Get Value (pure) — current interpolated value of a tween
    {
        NodeDefinition def;
        def.typeId = NodeTypes::TweenGetValue;
        def.displayName = "Tween Get Value";
        def.description = "Current interpolated value (x) of the tween at Index";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.8f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Entity", PK::Input), Int("Index", PK::Input, 0)};
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"tween", "value", "get", "current", "progress"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity target = ctx.entity;
            if (!inputs.empty() && std::holds_alternative<u64>(inputs[0]) && std::get<u64>(inputs[0]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[0]));
            i32 index = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 0;
            auto* tc = ctx.world ? ctx.world->GetComponent<ECS::TweenComponent>(target) : nullptr;
            if (tc && index >= 0 && static_cast<usize>(index) < tc->tweens.size())
                return tc->tweens[static_cast<usize>(index)].currentValue.x;
            return 0.0f;
        };
        RegisterNode(def);
    }
    // Tween Stop All (flow) — halt every tween on the entity
    {
        NodeDefinition def;
        def.typeId = NodeTypes::TweenStopAll;
        def.displayName = "Tween Stop All";
        def.description = "Stop all tweens currently running on an entity";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.6f, 0.3f, 0.8f);
        def.inputs = {FlowIn(), EntityPin("Entity", PK::Input)};
        def.outputs = {FlowOut()};
        def.keywords = {"tween", "stop", "halt", "cancel", "all"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>&) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1]) && std::get<u64>(inputs[1]) != 0)
                target = static_cast<ECS::Entity>(std::get<u64>(inputs[1]));
            auto* tc = ctx.world ? ctx.world->GetComponent<ECS::TweenComponent>(target) : nullptr;
            if (tc) for (auto& tw : tc->tweens) { tw.isPlaying = false; tw.isComplete = true; }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // =======================================================================
    // Networking lobby/session queries (docs/NODE_COVERAGE.md tier-3).
    // Read straight off ctx.networking (like the existing Get Ping node).
    // =======================================================================
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetGetPlayerCount;
        def.displayName = "Net Player Count";
        def.description = "Number of connected players";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Int("Count", PK::Output)};
        def.keywords = {"net", "network", "players", "count", "multiplayer"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return ctx.networking ? static_cast<i32>(ctx.networking->GetConnectedPlayerCount()) : static_cast<i32>(0);
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetIsHost;
        def.displayName = "Net Is Host";
        def.description = "True if this instance is the host";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Bool("Is Host", PK::Output)};
        def.keywords = {"net", "network", "host", "server", "multiplayer"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return ctx.networking ? ctx.networking->IsHost() : false;
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetGetLocalPlayerId;
        def.displayName = "Net Local Player Id";
        def.description = "This client's player id";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Int("Player Id", PK::Output)};
        def.keywords = {"net", "network", "player", "id", "local"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return ctx.networking ? static_cast<i32>(ctx.networking->GetLocalPlayerId()) : static_cast<i32>(-1);
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetGetPacketLoss;
        def.displayName = "Net Packet Loss";
        def.description = "Current packet loss fraction (0-1)";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Float("Packet Loss", PK::Output)};
        def.keywords = {"net", "network", "packet", "loss", "quality"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return ctx.networking ? ctx.networking->GetPacketLoss() : 0.0f;
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetSetReady;
        def.displayName = "Net Set Ready";
        def.description = "Set this player's lobby ready state";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.inputs = {FlowIn(), Bool("Ready", PK::Input, true)};
        def.outputs = {FlowOut()};
        def.keywords = {"net", "network", "ready", "lobby", "set"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            bool ready = (inputs.size() > 1 && std::holds_alternative<bool>(inputs[1])) ? std::get<bool>(inputs[1]) : true;
            if (ctx.networking) ctx.networking->SetReady(ready);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetLobbyCount;
        def.displayName = "Net Lobby Player Count";
        def.description = "Number of players in the lobby";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Int("Count", PK::Output)};
        def.keywords = {"net", "network", "lobby", "players", "count"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return ctx.networking ? static_cast<i32>(ctx.networking->GetLobbyPlayers().size()) : static_cast<i32>(0);
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetLobbyName;
        def.displayName = "Net Lobby Player Name";
        def.description = "Name of the lobby player at Index";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {Int("Index", PK::Input, 0)};
        def.outputs = {String("Name", PK::Output)};
        def.keywords = {"net", "network", "lobby", "player", "name"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            i32 index = (!inputs.empty() && std::holds_alternative<i32>(inputs[0])) ? std::get<i32>(inputs[0]) : 0;
            if (ctx.networking) {
                const auto& players = ctx.networking->GetLobbyPlayers();
                if (index >= 0 && static_cast<usize>(index) < players.size()) return players[static_cast<usize>(index)].name;
            }
            return std::string("");
        };
        RegisterNode(def);
    }
    {
        NodeDefinition def;
        def.typeId = NodeTypes::NetLobbyReady;
        def.displayName = "Net Lobby Player Ready";
        def.description = "Whether the lobby player at Index is ready";
        def.category = NodeCategory::Networking;
        def.headerColor = Math::Vector3(0.3f, 0.5f, 0.7f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {Int("Index", PK::Input, 0)};
        def.outputs = {Bool("Ready", PK::Output)};
        def.keywords = {"net", "network", "lobby", "player", "ready"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            i32 index = (!inputs.empty() && std::holds_alternative<i32>(inputs[0])) ? std::get<i32>(inputs[0]) : 0;
            if (ctx.networking) {
                const auto& players = ctx.networking->GetLobbyPlayers();
                if (index >= 0 && static_cast<usize>(index) < players.size()) return players[static_cast<usize>(index)].ready;
            }
            return false;
        };
        RegisterNode(def);
    }

    // =======================================================================
    // Weather reads + wind (docs/NODE_COVERAGE.md tier-3). Same access as the
    // existing Set Weather / Set Fog nodes: the s_VisualScriptWeather global.
    // =======================================================================

    // Compact helper for pure float/bool weather getters.
    auto addWeatherGetFloat = [this](const char* id, const char* name, const char* desc,
                                     const char* kw, f32(*read)(Effects::WeatherSystem*)) {
        NodeDefinition def;
        def.typeId = id; def.displayName = name; def.description = desc;
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.75f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Float("Value", PK::Output)};
        def.keywords = {"weather", kw, "get"};
        def.evaluate = [read](const ExecutionContext&, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return s_VisualScriptWeather ? read(s_VisualScriptWeather) : 0.0f;
        };
        RegisterNode(def);
    };
    addWeatherGetFloat(NodeTypes::WeatherGetRain, "Get Rain Intensity",
        "Current rain intensity (0-1)", "rain", [](Effects::WeatherSystem* w) { return w->GetRainIntensity(); });
    addWeatherGetFloat(NodeTypes::WeatherGetSnow, "Get Snow Intensity",
        "Current snow intensity (0-1)", "snow", [](Effects::WeatherSystem* w) { return w->GetSnowIntensity(); });
    addWeatherGetFloat(NodeTypes::WeatherGetFogDensity, "Get Fog Density",
        "Current fog density (0-1)", "fog", [](Effects::WeatherSystem* w) { return w->GetFogDensity(); });
    addWeatherGetFloat(NodeTypes::WeatherGetWindStrength, "Get Wind Strength",
        "Current wind strength", "wind", [](Effects::WeatherSystem* w) { return w->GetWindStrength(); });

    // Get Weather Type (pure, int = WeatherType enum value)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::WeatherGetType;
        def.displayName = "Get Weather";
        def.description = "Current weather type (0=Clear 1=Cloudy 2=Rain 3=HeavyRain 4=Snow 5=Fog 6=Storm)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.75f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Int("Type", PK::Output)};
        def.keywords = {"weather", "type", "get", "current"};
        def.evaluate = [](const ExecutionContext&, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return s_VisualScriptWeather ? static_cast<i32>(s_VisualScriptWeather->GetWeather()) : static_cast<i32>(0);
        };
        RegisterNode(def);
    }
    // Is Lightning (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::WeatherIsLightning;
        def.displayName = "Is Lightning";
        def.description = "True while a lightning flash is active";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.75f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Bool("Lightning", PK::Output)};
        def.keywords = {"weather", "lightning", "storm", "flash"};
        def.evaluate = [](const ExecutionContext&, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return s_VisualScriptWeather ? s_VisualScriptWeather->IsLightningActive() : false;
        };
        RegisterNode(def);
    }
    // Get Wind Direction (pure, Vec3)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::WeatherGetWindDirection;
        def.displayName = "Get Wind Direction";
        def.description = "Current wind direction vector";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.75f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Vec3("Direction", PK::Output)};
        def.keywords = {"weather", "wind", "direction", "get"};
        def.evaluate = [](const ExecutionContext&, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return s_VisualScriptWeather ? s_VisualScriptWeather->GetWindDirection() : Math::Vector3(0, 0, 0);
        };
        RegisterNode(def);
    }
    // Set Wind (flow: direction + strength)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::WeatherSetWind;
        def.displayName = "Set Wind";
        def.description = "Set the wind direction and strength";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.4f, 0.6f, 0.75f);
        def.inputs = {FlowIn(), Vec3("Direction", PK::Input), Float("Strength", PK::Input, 1.0f)};
        def.outputs = {FlowOut()};
        def.keywords = {"weather", "wind", "set", "direction", "strength"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            if (s_VisualScriptWeather) {
                Math::Vector3 dir = (inputs.size() > 1 && std::holds_alternative<Math::Vector3>(inputs[1]))
                    ? std::get<Math::Vector3>(inputs[1]) : Math::Vector3(1, 0, 0);
                f32 strength = (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) ? std::get<f32>(inputs[2]) : 1.0f;
                s_VisualScriptWeather->SetWindDirection(dir);
                s_VisualScriptWeather->SetWindStrength(strength);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // =======================================================================
    // UI read-state + styling (docs/NODE_COVERAGE.md tier-3). Same component
    // path as the existing UI Set nodes: UICanvasComponent -> GetElement(id).
    // Inputs: Canvas Entity + Element Id.
    // =======================================================================

    // Resolve canvas element from (entity, elementId) node inputs starting at idx.
    auto uiElement = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                        usize idx) -> GUI::UIElement* {
        if (!ctx.world || inputs.size() <= idx + 1) return nullptr;
        ECS::Entity entity = std::holds_alternative<u64>(inputs[idx])
            ? static_cast<ECS::Entity>(std::get<u64>(inputs[idx])) : ECS::INVALID_ENTITY;
        i32 elemId = std::holds_alternative<i32>(inputs[idx + 1]) ? std::get<i32>(inputs[idx + 1]) : 0;
        auto* canvas = ctx.world->GetComponent<GUI::UICanvasComponent>(entity);
        return canvas ? canvas->GetElement(static_cast<u32>(elemId)) : nullptr;
    };

    // Pure read nodes.
    struct UIReadDef { const char* id; const char* name; const char* desc; const char* kw; int kind; };
    // kind: 0=checked(bool) 1=slider(f32) 2=hovered(bool) 3=pressed(bool) 4=text(string) 5=progress(f32)
    const UIReadDef uiReads[] = {
        {NodeTypes::UIIsChecked,      "UI Is Checked",       "Whether a checkbox element is checked",       "checkbox", 0},
        {NodeTypes::UIGetSliderValue, "UI Get Slider Value", "Current value of a slider element",           "slider",   1},
        {NodeTypes::UIIsHovered,      "UI Is Hovered",       "Whether the pointer is over the element",     "hover",    2},
        {NodeTypes::UIIsPressed,      "UI Is Pressed",       "Whether the element is being pressed",        "press",    3},
        {NodeTypes::UIGetText,        "UI Get Text",         "Current text of a text/button element",       "text",     4},
        {NodeTypes::UIGetProgress,    "UI Get Progress",     "Current fill of a progress bar (0-1)",        "progress", 5},
    };
    for (const auto& rd : uiReads) {
        NodeDefinition def;
        def.typeId = rd.id; def.displayName = rd.name; def.description = rd.desc;
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.35f, 0.55f, 0.65f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Canvas", PK::Input), Int("Element Id", PK::Input, 0)};
        switch (rd.kind) {
            case 1: case 5: def.outputs = {Float("Value", PK::Output)}; break;
            case 4:         def.outputs = {String("Text", PK::Output)}; break;
            default:        def.outputs = {Bool("Value", PK::Output)}; break;
        }
        def.keywords = {"ui", "canvas", rd.kw, "get"};
        const int kind = rd.kind;
        def.evaluate = [uiElement, kind](const ExecutionContext& ctx,
                                         const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            GUI::UIElement* el = uiElement(ctx, inputs, 0);
            switch (kind) {
                case 0: return el ? el->data.checked : false;
                case 1: return el ? el->data.sliderValue : 0.0f;
                case 2: return el ? el->interaction.hovered : false;
                case 3: return el ? el->interaction.pressed : false;
                case 4: return el ? el->data.text : std::string("");
                default: return el ? el->data.progressValue : 0.0f;
            }
        };
        RegisterNode(def);
    }

    // UI Set Text Color (flow)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::UISetTextColor;
        def.displayName = "UI Set Text Color";
        def.description = "Set an element's text color (RGB 0-1)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.35f, 0.55f, 0.65f);
        def.inputs = {FlowIn(), EntityPin("Canvas", PK::Input), Int("Element Id", PK::Input, 0), Vec3("Color", PK::Input, Math::Vector3(1, 1, 1))};
        def.outputs = {FlowOut()};
        def.keywords = {"ui", "canvas", "text", "color", "style", "set"};
        def.execute = [uiElement](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            if (GUI::UIElement* el = uiElement(ctx, inputs, 1)) {
                if (inputs.size() > 3 && std::holds_alternative<Math::Vector3>(inputs[3]))
                    el->style.textColor = std::get<Math::Vector3>(inputs[3]);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // UI Set Bg Color (flow)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::UISetBgColor;
        def.displayName = "UI Set Bg Color";
        def.description = "Set an element's background color (RGB 0-1) and alpha";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.35f, 0.55f, 0.65f);
        def.inputs = {FlowIn(), EntityPin("Canvas", PK::Input), Int("Element Id", PK::Input, 0), Vec3("Color", PK::Input, Math::Vector3(0, 0, 0)), Float("Alpha", PK::Input, 1.0f)};
        def.outputs = {FlowOut()};
        def.keywords = {"ui", "canvas", "background", "color", "style", "set"};
        def.execute = [uiElement](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            if (GUI::UIElement* el = uiElement(ctx, inputs, 1)) {
                if (inputs.size() > 3 && std::holds_alternative<Math::Vector3>(inputs[3]))
                    el->style.bgColor = std::get<Math::Vector3>(inputs[3]);
                if (inputs.size() > 4 && std::holds_alternative<f32>(inputs[4]))
                    el->style.bgAlpha = std::get<f32>(inputs[4]);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // UI Set Image Alpha (flow)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::UISetImageAlpha;
        def.displayName = "UI Set Image Alpha";
        def.description = "Set an image element's opacity (0-1)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.35f, 0.55f, 0.65f);
        def.inputs = {FlowIn(), EntityPin("Canvas", PK::Input), Int("Element Id", PK::Input, 0), Float("Alpha", PK::Input, 1.0f)};
        def.outputs = {FlowOut()};
        def.keywords = {"ui", "canvas", "image", "alpha", "opacity", "style", "set"};
        def.execute = [uiElement](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            if (GUI::UIElement* el = uiElement(ctx, inputs, 1)) {
                if (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3]))
                    el->data.imageAlpha = Math::Clamp(std::get<f32>(inputs[3]), 0.0f, 1.0f);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // =======================================================================
    // Physics joints (docs/NODE_COVERAGE.md tier-3). Joints are entities
    // carrying Hinge/DistanceJointComponent — mirror the AngelScript bindings.
    // Nodes run on the main thread, so CreateEntity here is safe (adr-0004).
    // =======================================================================

    // Create Hinge Joint (flow: A, B, Axis -> Joint)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::JointCreateHinge;
        def.displayName = "Create Hinge Joint";
        def.description = "Create a hinge joint entity connecting A and B around Axis";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.55f, 0.45f, 0.35f);
        def.inputs = {FlowIn(), EntityPin("Entity A", PK::Input), EntityPin("Entity B", PK::Input), Vec3("Axis", PK::Input, Math::Vector3(0, 1, 0))};
        def.outputs = {FlowOut(), EntityPin("Joint", PK::Output)};
        def.keywords = {"joint", "hinge", "physics", "create", "door", "constraint"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            outputs[0] = static_cast<ECS::Entity>(ECS::INVALID_ENTITY);
            ECS::Entity a = (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1])) ? static_cast<ECS::Entity>(std::get<u64>(inputs[1])) : ECS::INVALID_ENTITY;
            ECS::Entity b = (inputs.size() > 2 && std::holds_alternative<u64>(inputs[2])) ? static_cast<ECS::Entity>(std::get<u64>(inputs[2])) : ECS::INVALID_ENTITY;
            Math::Vector3 axis = (inputs.size() > 3 && std::holds_alternative<Math::Vector3>(inputs[3])) ? std::get<Math::Vector3>(inputs[3]) : Math::Vector3(0, 1, 0);
            if (ctx.world && ctx.world->IsValid(a) && ctx.world->IsValid(b)) {
                ECS::Entity joint = ctx.world->CreateEntity();
                ctx.world->AddComponent<ECS::NameComponent>(joint, ECS::NameComponent{"HingeJoint"});
                auto& jc = ctx.world->AddComponent<ECS::HingeJointComponent>(joint);
                jc.entityA = a;
                jc.entityB = b;
                jc.axis = axis;
                outputs[0] = joint;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Create Distance Joint (flow: A, B, Rest Distance -> Joint)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::JointCreateDistance;
        def.displayName = "Create Distance Joint";
        def.description = "Create a distance joint entity holding A and B at Rest Distance";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.55f, 0.45f, 0.35f);
        def.inputs = {FlowIn(), EntityPin("Entity A", PK::Input), EntityPin("Entity B", PK::Input), Float("Rest Distance", PK::Input, 2.0f)};
        def.outputs = {FlowOut(), EntityPin("Joint", PK::Output)};
        def.keywords = {"joint", "distance", "physics", "create", "rope", "constraint"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                         std::vector<ECS::VariableValue>& outputs) {
            outputs.resize(1);
            outputs[0] = static_cast<ECS::Entity>(ECS::INVALID_ENTITY);
            ECS::Entity a = (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1])) ? static_cast<ECS::Entity>(std::get<u64>(inputs[1])) : ECS::INVALID_ENTITY;
            ECS::Entity b = (inputs.size() > 2 && std::holds_alternative<u64>(inputs[2])) ? static_cast<ECS::Entity>(std::get<u64>(inputs[2])) : ECS::INVALID_ENTITY;
            f32 rest = (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3])) ? std::get<f32>(inputs[3]) : 2.0f;
            if (ctx.world && ctx.world->IsValid(a) && ctx.world->IsValid(b)) {
                ECS::Entity joint = ctx.world->CreateEntity();
                ctx.world->AddComponent<ECS::NameComponent>(joint, ECS::NameComponent{"DistanceJoint"});
                auto& jc = ctx.world->AddComponent<ECS::DistanceJointComponent>(joint);
                jc.entityA = a;
                jc.entityB = b;
                jc.restDistance = rest;
                outputs[0] = joint;
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Destroy Joint (flow)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::JointDestroy;
        def.displayName = "Destroy Joint";
        def.description = "Destroy a joint entity (deferred to end of frame)";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.55f, 0.45f, 0.35f);
        def.inputs = {FlowIn(), EntityPin("Joint", PK::Input)};
        def.outputs = {FlowOut()};
        def.keywords = {"joint", "destroy", "remove", "physics"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            ECS::Entity joint = (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1])) ? static_cast<ECS::Entity>(std::get<u64>(inputs[1])) : ECS::INVALID_ENTITY;
            if (ctx.world && ctx.world->IsValid(joint)) ctx.world->DestroyEntity(joint);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Hinge Set Limits (flow)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::JointHingeSetLimits;
        def.displayName = "Hinge Set Limits";
        def.description = "Enable and set a hinge joint's angle limits (degrees)";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.55f, 0.45f, 0.35f);
        def.inputs = {FlowIn(), EntityPin("Joint", PK::Input), Float("Lower", PK::Input, -45.0f), Float("Upper", PK::Input, 45.0f)};
        def.outputs = {FlowOut()};
        def.keywords = {"joint", "hinge", "limits", "angle", "set"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            ECS::Entity joint = (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1])) ? static_cast<ECS::Entity>(std::get<u64>(inputs[1])) : ECS::INVALID_ENTITY;
            auto* jc = ctx.world ? ctx.world->GetComponent<ECS::HingeJointComponent>(joint) : nullptr;
            if (jc) {
                jc->useLimits = true;
                if (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) jc->lowerLimit = std::get<f32>(inputs[2]);
                if (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3])) jc->upperLimit = std::get<f32>(inputs[3]);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Hinge Set Motor (flow)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::JointHingeSetMotor;
        def.displayName = "Hinge Set Motor";
        def.description = "Enable and set a hinge joint's motor speed and max force";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.55f, 0.45f, 0.35f);
        def.inputs = {FlowIn(), EntityPin("Joint", PK::Input), Float("Speed", PK::Input, 1.0f), Float("Max Force", PK::Input, 100.0f)};
        def.outputs = {FlowOut()};
        def.keywords = {"joint", "hinge", "motor", "spin", "set"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            ECS::Entity joint = (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1])) ? static_cast<ECS::Entity>(std::get<u64>(inputs[1])) : ECS::INVALID_ENTITY;
            auto* jc = ctx.world ? ctx.world->GetComponent<ECS::HingeJointComponent>(joint) : nullptr;
            if (jc) {
                jc->useMotor = true;
                if (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) jc->motorSpeed = std::get<f32>(inputs[2]);
                if (inputs.size() > 3 && std::holds_alternative<f32>(inputs[3])) jc->motorMaxForce = std::get<f32>(inputs[3]);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Hinge Get Angle (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::JointHingeGetAngle;
        def.displayName = "Hinge Get Angle";
        def.description = "Current angle of a hinge joint (degrees)";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.55f, 0.45f, 0.35f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Joint", PK::Input)};
        def.outputs = {Float("Angle", PK::Output)};
        def.keywords = {"joint", "hinge", "angle", "get"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity joint = (!inputs.empty() && std::holds_alternative<u64>(inputs[0])) ? static_cast<ECS::Entity>(std::get<u64>(inputs[0])) : ECS::INVALID_ENTITY;
            auto* jc = ctx.world ? ctx.world->GetComponent<ECS::HingeJointComponent>(joint) : nullptr;
            return jc ? jc->currentAngle : 0.0f;
        };
        RegisterNode(def);
    }
    // Distance Set Rest (flow)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::JointDistanceSetRest;
        def.displayName = "Distance Set Rest";
        def.description = "Set a distance joint's rest distance";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.55f, 0.45f, 0.35f);
        def.inputs = {FlowIn(), EntityPin("Joint", PK::Input), Float("Distance", PK::Input, 2.0f)};
        def.outputs = {FlowOut()};
        def.keywords = {"joint", "distance", "rest", "length", "set"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            ECS::Entity joint = (inputs.size() > 1 && std::holds_alternative<u64>(inputs[1])) ? static_cast<ECS::Entity>(std::get<u64>(inputs[1])) : ECS::INVALID_ENTITY;
            auto* jc = ctx.world ? ctx.world->GetComponent<ECS::DistanceJointComponent>(joint) : nullptr;
            if (jc && inputs.size() > 2 && std::holds_alternative<f32>(inputs[2])) jc->restDistance = std::get<f32>(inputs[2]);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Distance Get Stress (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::JointDistanceGetStress;
        def.displayName = "Distance Get Stress";
        def.description = "Current stress on a distance joint (for breakable ropes)";
        def.category = NodeCategory::Physics;
        def.headerColor = Math::Vector3(0.55f, 0.45f, 0.35f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {EntityPin("Joint", PK::Input)};
        def.outputs = {Float("Stress", PK::Output)};
        def.keywords = {"joint", "distance", "stress", "break", "get"};
        def.evaluate = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            ECS::Entity joint = (!inputs.empty() && std::holds_alternative<u64>(inputs[0])) ? static_cast<ECS::Entity>(std::get<u64>(inputs[0])) : ECS::INVALID_ENTITY;
            auto* jc = ctx.world ? ctx.world->GetComponent<ECS::DistanceJointComponent>(joint) : nullptr;
            return jc ? jc->currentStress : 0.0f;
        };
        RegisterNode(def);
    }

    // =======================================================================
    // HUD styling (docs/NODE_COVERAGE.md tier-3). HUD widgets are UICanvas
    // entities; these style the first element of the relevant widget type,
    // mirroring the HUD_* bindings' legacy semantics.
    // =======================================================================

    auto hudFirstOfType = [](const ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs,
                             GUI::UIWidgetType t) -> GUI::UIElement* {
        if (!ctx.world || inputs.size() < 2 || !std::holds_alternative<u64>(inputs[1])) return nullptr;
        auto* c = ctx.world->GetComponent<GUI::UICanvasComponent>(static_cast<ECS::Entity>(std::get<u64>(inputs[1])));
        if (!c) return nullptr;
        for (auto& e : c->elements) if (e.type == t) return &e;
        return nullptr;
    };

    // HUD Set Fill Color (progress bar)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::HUDSetFillColor;
        def.displayName = "HUD Set Fill Color";
        def.description = "Set a HUD bar's fill color (RGB 0-1)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.45f, 0.6f, 0.4f);
        def.inputs = {FlowIn(), EntityPin("HUD Entity", PK::Input), Vec3("Color", PK::Input, Math::Vector3(0.2f, 0.8f, 0.2f))};
        def.outputs = {FlowOut()};
        def.keywords = {"hud", "bar", "fill", "color", "style"};
        def.execute = [hudFirstOfType](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            if (auto* el = hudFirstOfType(ctx, inputs, GUI::UIWidgetType::ProgressBar)) {
                if (inputs.size() > 2 && std::holds_alternative<Math::Vector3>(inputs[2]))
                    el->data.progressFillColor = std::get<Math::Vector3>(inputs[2]);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // HUD Set Text Color (label)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::HUDSetTextColor;
        def.displayName = "HUD Set Text Color";
        def.description = "Set a HUD label's text color (RGB 0-1)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.45f, 0.6f, 0.4f);
        def.inputs = {FlowIn(), EntityPin("HUD Entity", PK::Input), Vec3("Color", PK::Input, Math::Vector3(1, 1, 1))};
        def.outputs = {FlowOut()};
        def.keywords = {"hud", "text", "color", "style"};
        def.execute = [hudFirstOfType](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            if (auto* el = hudFirstOfType(ctx, inputs, GUI::UIWidgetType::Label)) {
                if (inputs.size() > 2 && std::holds_alternative<Math::Vector3>(inputs[2]))
                    el->style.textColor = std::get<Math::Vector3>(inputs[2]);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // HUD Set Font Size (label)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::HUDSetFontSize;
        def.displayName = "HUD Set Font Size";
        def.description = "Set a HUD label's font size";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.45f, 0.6f, 0.4f);
        def.inputs = {FlowIn(), EntityPin("HUD Entity", PK::Input), Float("Size", PK::Input, 24.0f)};
        def.outputs = {FlowOut()};
        def.keywords = {"hud", "font", "size", "text", "style"};
        def.execute = [hudFirstOfType](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            if (auto* el = hudFirstOfType(ctx, inputs, GUI::UIWidgetType::Label)) {
                if (inputs.size() > 2 && std::holds_alternative<f32>(inputs[2]))
                    el->style.fontSize = std::get<f32>(inputs[2]);
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // HUD Set Position (anchor 0-1 on root elements)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::HUDSetPosition;
        def.displayName = "HUD Set Position";
        def.description = "Anchor a HUD widget at a screen position (0-1 both axes)";
        def.category = NodeCategory::Gameplay;
        def.headerColor = Math::Vector3(0.45f, 0.6f, 0.4f);
        def.inputs = {FlowIn(), EntityPin("HUD Entity", PK::Input), Vec2("Anchor", PK::Input, Math::Vector2(0.5f, 0.5f))};
        def.outputs = {FlowOut()};
        def.keywords = {"hud", "position", "anchor", "move", "style"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            if (ctx.world && inputs.size() > 1 && std::holds_alternative<u64>(inputs[1])) {
                auto* c = ctx.world->GetComponent<GUI::UICanvasComponent>(static_cast<ECS::Entity>(std::get<u64>(inputs[1])));
                Math::Vector2 anchor = (inputs.size() > 2 && std::holds_alternative<Math::Vector2>(inputs[2]))
                    ? std::get<Math::Vector2>(inputs[2]) : Math::Vector2(0.5f, 0.5f);
                if (c) {
                    for (auto& el : c->elements) {
                        if (el.parentId != 0) continue;  // roots carry the widget position
                        el.anchor.anchorMin = anchor;
                        el.anchor.anchorMax = anchor;
                    }
                }
            }
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }

    // =======================================================================
    // Input actions (docs/NODE_COVERAGE.md tier-4, Marty call 2026-08-15):
    // the rebindable action layer, so node-built games can offer rebinding.
    // VSInput* forwarders reuse the AngelScript action-map wiring.
    // =======================================================================

    // Pure action-index readers.
    struct ActionReadDef { const char* id; const char* name; const char* desc; const char* kw; int kind; };
    // kind: 0=isDown(bool) 1=isPressed(bool) 2=value(f32) 3=name(string) 4=binding(string)
    const ActionReadDef actionReads[] = {
        {NodeTypes::ActionIsDown,     "Action Is Down",    "True while the action's key/button is held",       "held",    0},
        {NodeTypes::ActionIsPressed,  "Action Is Pressed", "True on the frame the action is pressed",          "pressed", 1},
        {NodeTypes::ActionGetValue,   "Action Get Value",  "Analog value of the action (0-1)",                 "value",   2},
        {NodeTypes::ActionGetName,    "Action Get Name",   "Display name of the action at Index",              "name",    3},
        {NodeTypes::ActionGetBinding, "Action Get Binding","Display name of the key currently bound at Index", "binding", 4},
    };
    for (const auto& ad : actionReads) {
        NodeDefinition def;
        def.typeId = ad.id; def.displayName = ad.name; def.description = ad.desc;
        def.category = NodeCategory::Input;
        def.headerColor = Math::Vector3(0.65f, 0.55f, 0.25f);
        def.flags = NodeDefFlags::Pure;
        def.inputs = {Int(ad.kind >= 3 ? "Index" : "Action", PK::Input, 0)};
        switch (ad.kind) {
            case 2:  def.outputs = {Float("Value", PK::Output)}; break;
            case 3: case 4: def.outputs = {String("Name", PK::Output)}; break;
            default: def.outputs = {Bool("Value", PK::Output)}; break;
        }
        def.keywords = {"input", "action", "rebind", ad.kw};
        const int kind = ad.kind;
        def.evaluate = [kind](const ExecutionContext&, const std::vector<ECS::VariableValue>& inputs) -> ECS::VariableValue {
            i32 idx = (!inputs.empty() && std::holds_alternative<i32>(inputs[0])) ? std::get<i32>(inputs[0]) : 0;
            switch (kind) {
                case 0: return Enjin::Scripting::VSInputActionIsDown(idx);
                case 1: return Enjin::Scripting::VSInputActionIsPressed(idx);
                case 2: return Enjin::Scripting::VSInputActionGetValue(idx);
                case 3: return Enjin::Scripting::VSInputActionName(idx);
                default: return Enjin::Scripting::VSInputBindingName(idx);
            }
        };
        RegisterNode(def);
    }
    // Action Count (pure)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ActionCount;
        def.displayName = "Action Count";
        def.description = "Number of rebindable actions";
        def.category = NodeCategory::Input;
        def.headerColor = Math::Vector3(0.65f, 0.55f, 0.25f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Int("Count", PK::Output)};
        def.keywords = {"input", "action", "count", "rebind"};
        def.evaluate = [](const ExecutionContext&, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return Enjin::Scripting::VSInputActionCount();
        };
        RegisterNode(def);
    }
    // Rebind Action (flow: action index + key code)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ActionRebind;
        def.displayName = "Rebind Action";
        def.description = "Bind the action at Index to Key Code (use Poll Key to capture one)";
        def.category = NodeCategory::Input;
        def.headerColor = Math::Vector3(0.65f, 0.55f, 0.25f);
        def.inputs = {FlowIn(), Int("Action", PK::Input, 0), Int("Key Code", PK::Input, 0)};
        def.outputs = {FlowOut()};
        def.keywords = {"input", "action", "rebind", "bind", "key", "remap"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>& inputs, std::vector<ECS::VariableValue>&) {
            i32 action = (inputs.size() > 1 && std::holds_alternative<i32>(inputs[1])) ? std::get<i32>(inputs[1]) : 0;
            i32 key = (inputs.size() > 2 && std::holds_alternative<i32>(inputs[2])) ? std::get<i32>(inputs[2]) : 0;
            Enjin::Scripting::VSInputRebind(action, key);
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
    // Poll Key (pure: key code of the next fresh key press, -1 if none — drives
    // "press a key to rebind" UI)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ActionPollKey;
        def.displayName = "Poll Key Press";
        def.description = "Key code of a key pressed this frame (-1 if none); feed into Rebind Action";
        def.category = NodeCategory::Input;
        def.headerColor = Math::Vector3(0.65f, 0.55f, 0.25f);
        def.flags = NodeDefFlags::Pure;
        def.outputs = {Int("Key Code", PK::Output)};
        def.keywords = {"input", "key", "poll", "capture", "rebind", "listen"};
        def.evaluate = [](const ExecutionContext&, const std::vector<ECS::VariableValue>&) -> ECS::VariableValue {
            return Enjin::Scripting::VSInputPollKey();
        };
        RegisterNode(def);
    }
    // Reset Bindings (flow)
    {
        NodeDefinition def;
        def.typeId = NodeTypes::ActionResetBindings;
        def.displayName = "Reset Bindings";
        def.description = "Restore all action bindings to their defaults";
        def.category = NodeCategory::Input;
        def.headerColor = Math::Vector3(0.65f, 0.55f, 0.25f);
        def.inputs = {FlowIn()};
        def.outputs = {FlowOut()};
        def.keywords = {"input", "action", "reset", "defaults", "rebind"};
        def.execute = [](ExecutionContext& ctx, const std::vector<ECS::VariableValue>&, std::vector<ECS::VariableValue>&) {
            Enjin::Scripting::VSInputResetBindings();
            ctx.nextFlowIndex = 0;
        };
        RegisterNode(def);
    }
}

} // namespace VisualScript
} // namespace Enjin
