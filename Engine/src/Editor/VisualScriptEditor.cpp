#include "Enjin/Editor/VisualScriptEditor.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/VisualScript/NodeDefinition.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <nlohmann/json.hpp>

namespace Enjin {
namespace Editor {

// Header colors for different node categories
static const Math::Vector3 COLOR_EVENT      = Math::Vector3(0.2f, 0.6f, 0.3f);  // Green
static const Math::Vector3 COLOR_FLOW       = Math::Vector3(0.6f, 0.4f, 0.2f);  // Orange
static const Math::Vector3 COLOR_VARIABLE   = Math::Vector3(0.4f, 0.5f, 0.4f);  // Muted green
static const Math::Vector3 COLOR_MATH       = Math::Vector3(0.3f, 0.5f, 0.3f);  // Green
static const Math::Vector3 COLOR_TRANSFORM  = Math::Vector3(0.4f, 0.4f, 0.6f);  // Blue
static const Math::Vector3 COLOR_DEBUG      = Math::Vector3(0.5f, 0.3f, 0.5f);  // Purple

// Forward declare helper for variable serialization (used by undo commands)
static std::string SerializeVariableValue(const ECS::VisualScriptVariable& var);

// ============================================================================
// UNDO COMMANDS FOR VISUAL SCRIPT GRAPH
// ============================================================================

// Command for adding a node
class AddVisualScriptNodeCommand : public ICommand {
public:
    AddVisualScriptNodeCommand(VisualScriptEditor* editor, NodeId nodeId, const std::string& nodeJson)
        : m_Editor(editor), m_NodeId(nodeId), m_NodeJson(nodeJson), m_FirstExecute(true) {}

    void Execute() override {
        if (m_FirstExecute) {
            m_FirstExecute = false;
            return;  // Node was already added
        }
        // Redo: restore the node from JSON
        m_Editor->RestoreNodeFromJson(m_NodeId, m_NodeJson);
    }

    void Undo() override {
        m_Editor->RemoveNodeById(m_NodeId);
    }

    const char* GetDescription() const override { return "Add Node"; }

private:
    VisualScriptEditor* m_Editor;
    NodeId m_NodeId;
    std::string m_NodeJson;
    bool m_FirstExecute;
};

// Command for deleting a node
class DeleteVisualScriptNodeCommand : public ICommand {
public:
    DeleteVisualScriptNodeCommand(VisualScriptEditor* editor, NodeId nodeId, const std::string& nodeJson)
        : m_Editor(editor), m_NodeId(nodeId), m_NodeJson(nodeJson), m_FirstExecute(true) {}

    void Execute() override {
        if (m_FirstExecute) {
            m_FirstExecute = false;
            m_Editor->RemoveNodeById(m_NodeId);
            return;
        }
        m_Editor->RemoveNodeById(m_NodeId);
    }

    void Undo() override {
        m_Editor->RestoreNodeFromJson(m_NodeId, m_NodeJson);
    }

    const char* GetDescription() const override { return "Delete Node"; }

private:
    VisualScriptEditor* m_Editor;
    NodeId m_NodeId;
    std::string m_NodeJson;
    bool m_FirstExecute;
};

// Command for adding a link
class AddVisualScriptLinkCommand : public ICommand {
public:
    AddVisualScriptLinkCommand(VisualScriptEditor* editor, LinkId linkId, PinId startPin, PinId endPin)
        : m_Editor(editor), m_LinkId(linkId), m_StartPin(startPin), m_EndPin(endPin), m_FirstExecute(true) {}

    void Execute() override {
        if (m_FirstExecute) {
            m_FirstExecute = false;
            return;  // Link was already added
        }
        m_Editor->AddLinkById(m_StartPin, m_EndPin, m_LinkId);
    }

    void Undo() override {
        m_Editor->RemoveLinkById(m_LinkId);
    }

    const char* GetDescription() const override { return "Add Link"; }

private:
    VisualScriptEditor* m_Editor;
    LinkId m_LinkId;
    PinId m_StartPin;
    PinId m_EndPin;
    bool m_FirstExecute;
};

// Command for deleting a link
class DeleteVisualScriptLinkCommand : public ICommand {
public:
    DeleteVisualScriptLinkCommand(VisualScriptEditor* editor, LinkId linkId, PinId startPin, PinId endPin)
        : m_Editor(editor), m_LinkId(linkId), m_StartPin(startPin), m_EndPin(endPin), m_FirstExecute(true) {}

    void Execute() override {
        if (m_FirstExecute) {
            m_FirstExecute = false;
            m_Editor->RemoveLinkById(m_LinkId);
            return;
        }
        m_Editor->RemoveLinkById(m_LinkId);
    }

    void Undo() override {
        m_Editor->AddLinkById(m_StartPin, m_EndPin, m_LinkId);
    }

    const char* GetDescription() const override { return "Delete Link"; }

private:
    VisualScriptEditor* m_Editor;
    LinkId m_LinkId;
    PinId m_StartPin;
    PinId m_EndPin;
    bool m_FirstExecute;
};

// Command for editing a node property value
class EditNodePropertyCommand : public ICommand {
public:
    EditNodePropertyCommand(VisualScriptEditor* editor, NodeId nodeId,
                            const std::string& propertyName,
                            const std::string& oldValue, const std::string& newValue)
        : m_Editor(editor), m_NodeId(nodeId), m_PropertyName(propertyName),
          m_OldValue(oldValue), m_NewValue(newValue) {}

    void Execute() override {
        m_Editor->SetNodePropertyValue(m_NodeId, m_PropertyName, m_NewValue);
    }

    void Undo() override {
        m_Editor->SetNodePropertyValue(m_NodeId, m_PropertyName, m_OldValue);
    }

    const char* GetDescription() const override { return "Edit Node Property"; }

    // Support merging consecutive property edits on the same node/property
    bool CanMergeWith(const ICommand* other) const override {
        auto* otherEdit = dynamic_cast<const EditNodePropertyCommand*>(other);
        if (!otherEdit) return false;
        return m_NodeId == otherEdit->m_NodeId && m_PropertyName == otherEdit->m_PropertyName;
    }

    void MergeWith(const ICommand* other) override {
        auto* otherEdit = dynamic_cast<const EditNodePropertyCommand*>(other);
        if (otherEdit) {
            m_NewValue = otherEdit->m_NewValue;
        }
    }

private:
    VisualScriptEditor* m_Editor;
    NodeId m_NodeId;
    std::string m_PropertyName;
    std::string m_OldValue;
    std::string m_NewValue;
};

// Command for editing a variable value
class EditVariableCommand : public ICommand {
public:
    EditVariableCommand(VisualScriptEditor* editor, const std::string& varName,
                        const std::string& oldValue, const std::string& newValue)
        : m_Editor(editor), m_VarName(varName), m_OldValue(oldValue), m_NewValue(newValue) {}

    void Execute() override {
        m_Editor->SetVariableValue(m_VarName, m_NewValue);
    }

    void Undo() override {
        m_Editor->SetVariableValue(m_VarName, m_OldValue);
    }

    const char* GetDescription() const override { return "Edit Variable"; }

    // Support merging consecutive variable edits on the same variable
    bool CanMergeWith(const ICommand* other) const override {
        auto* otherEdit = dynamic_cast<const EditVariableCommand*>(other);
        if (!otherEdit) return false;
        return m_VarName == otherEdit->m_VarName;
    }

    void MergeWith(const ICommand* other) override {
        auto* otherEdit = dynamic_cast<const EditVariableCommand*>(other);
        if (otherEdit) {
            m_NewValue = otherEdit->m_NewValue;
        }
    }

private:
    VisualScriptEditor* m_Editor;
    std::string m_VarName;
    std::string m_OldValue;
    std::string m_NewValue;
};

// ============================================================================
// CONSTRUCTOR
// ============================================================================

VisualScriptEditor::VisualScriptEditor() {
    SetupCallbacks();
}

// ============================================================================
// TARGET MANAGEMENT
// ============================================================================

void VisualScriptEditor::SetTarget(ECS::World* world, ECS::Entity entity) {
    if (m_World == world && m_TargetEntity == entity) return;
    m_World = world;
    m_TargetEntity = entity;
    m_NeedsSync = true;
    m_SelectedNode = 0;
}

void VisualScriptEditor::ClearTarget() {
    m_World = nullptr;
    m_TargetEntity = ECS::INVALID_ENTITY;
    m_GraphData.Clear();
    m_NodeTypeMap.clear();
    m_NeedsSync = true;
    m_SelectedNode = 0;
}

// ============================================================================
// SYNC FROM COMPONENT
// ============================================================================

void VisualScriptEditor::SyncFromComponent() {
    m_GraphData.Clear();
    m_NodeTypeMap.clear();

    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* script = m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity);
    if (!script) return;

    // Copy the graph data from the component
    m_GraphData = script->graph;

    // Build node type map from metadata
    for (const auto& [nodeId, meta] : script->nodeMeta) {
        m_NodeTypeMap[nodeId] = meta.nodeType;
    }

    m_NeedsSync = false;
}

// ============================================================================
// SYNC TO COMPONENT
// ============================================================================

void VisualScriptEditor::SyncToComponent() {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* script = m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity);
    if (!script) return;

    // Copy graph data back to component
    script->graph = m_GraphData;

    // The metadata is updated directly when nodes are created/deleted
}

// ============================================================================
// CALLBACKS
// ============================================================================

void VisualScriptEditor::SetupCallbacks() {
    m_Callbacks = {};

    // Pin compatibility check
    m_Callbacks.CanCreateLink = [](const Pin& from, const Pin& to) -> bool {
        // Flow only connects to flow
        if (from.type == PinType::Flow || to.type == PinType::Flow) {
            return from.type == PinType::Flow && to.type == PinType::Flow;
        }

        // Any can connect to anything (except flow)
        if (from.type == PinType::Any || to.type == PinType::Any) {
            return true;
        }

        // Otherwise types must match
        return from.type == to.type;
    };

    // Node selection
    m_Callbacks.OnNodeSelected = [this](NodeId id) {
        m_SelectedNode = id;
    };

    m_Callbacks.OnLinkSelected = [this](LinkId id) {
        m_SelectedNode = 0;
    };

    m_Callbacks.OnSelectionCleared = [this]() {
        m_SelectedNode = 0;
    };

    // Node deleted - clean up metadata
    m_Callbacks.OnNodeDeleted = [this](NodeId id) {
        auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
        if (!script) return;

        // Remove from metadata
        script->nodeMeta.erase(id);
        m_NodeTypeMap.erase(id);

        // Remove from event mappings
        for (auto it = script->eventNodes.begin(); it != script->eventNodes.end(); ) {
            if (it->second == id) {
                it = script->eventNodes.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = script->customEventNodes.begin(); it != script->customEventNodes.end(); ) {
            if (it->second == id) {
                it = script->customEventNodes.erase(it);
            } else {
                ++it;
            }
        }

        if (m_SelectedNode == id) m_SelectedNode = 0;
    };

    // Populate context menu
    PopulateContextMenu();
}

void VisualScriptEditor::PopulateContextMenu() {
    // Clear the categories - we'll use custom search popup instead
    m_Callbacks.contextMenuCategories.clear();

    // Set up the custom context menu callback to open our search popup
    m_Callbacks.OnContextMenu = [this](Math::Vector2 pos) {
        m_ContextMenuPos = pos;
        m_NodeSearchBuf[0] = '\0';
        m_NodeSearchSelectedIndex = 0;
        ImGui::OpenPopup("NodeSearchPopup");
    };
}

// ============================================================================
// ADD NODE FROM DEFINITION
// ============================================================================

NodeId VisualScriptEditor::AddNodeFromDefinition(const VisualScript::NodeDefinition* def, Math::Vector2 position) {
    if (!def) return 0;

    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return 0;

    // Create the graph node
    NodeId nodeId = m_GraphData.AddNode(def->displayName, position, def->headerColor);
    auto* node = m_GraphData.FindNode(nodeId);
    if (!node) return 0;

    // Add input pins
    for (const auto& pinDef : def->inputs) {
        m_GraphData.AddPin(nodeId, pinDef.name, pinDef.type, PinKind::Input);
    }

    // Add output pins
    for (const auto& pinDef : def->outputs) {
        m_GraphData.AddPin(nodeId, pinDef.name, pinDef.type, PinKind::Output);
    }

    // Apply node flags
    if (def->IsEvent()) {
        node->flags = node->flags | NodeFlags::NoDelete;
    }

    // Create metadata
    ECS::VisualScriptNodeMeta meta;
    meta.nodeType = def->typeId;
    script->nodeMeta[nodeId] = meta;
    m_NodeTypeMap[nodeId] = def->typeId;

    // If this is an event node, register it
    if (def->typeId == VisualScript::NodeTypes::OnStart) {
        script->SetEventNode(ECS::VisualScriptEvent::OnStart, nodeId);
    } else if (def->typeId == VisualScript::NodeTypes::OnUpdate) {
        script->SetEventNode(ECS::VisualScriptEvent::OnUpdate, nodeId);
    } else if (def->typeId == VisualScript::NodeTypes::OnCollision) {
        script->SetEventNode(ECS::VisualScriptEvent::OnCollisionEnter, nodeId);
    }

    // Sync back to component
    script->graph = m_GraphData;

    // Add to undo stack
    if (m_UndoManager) {
        std::string nodeJson = SerializeNodeToJson(nodeId);
        m_UndoManager->Execute(std::make_unique<AddVisualScriptNodeCommand>(
            this, nodeId, nodeJson));
    }

    return nodeId;
}

// ============================================================================
// RENDER
// ============================================================================

void VisualScriptEditor::Render(const EditorSettings& settings, bool isPlaying) {
    // Store settings reference for recently-used tracking
    m_EditorSettings = const_cast<EditorSettings*>(&settings);

    // Apply theme colors
    m_Colors = NodeGraphColors::FromTheme(settings);

    // Sync from component if needed
    if (m_NeedsSync && HasTarget()) {
        SyncFromComponent();
    }

    // Get panel size
    ImVec2 panelSize = ImGui::GetContentRegionAvail();
    float sidebarWidth = 200.0f;
    float inspectorWidth = 250.0f;

    // Entity sidebar (left)
    ImGui::BeginChild("##vs_entity_sidebar", ImVec2(sidebarWidth, 0), true);
    DrawEntitySidebar();
    ImGui::EndChild();

    ImGui::SameLine();

    // Main graph area
    float graphWidth = panelSize.x - sidebarWidth - inspectorWidth - 16.0f;
    ImGui::BeginChild("##vs_graph_area", ImVec2(graphWidth, 0), true);

    if (HasTarget()) {
        DrawToolbar();
        DrawDebugControls(isPlaying);

        // Update node highlights for play mode visualization
        UpdatePlayModeHighlight(isPlaying);

        // Graph canvas
        ImVec2 graphSize = ImGui::GetContentRegionAvail();
        ImGui::BeginChild("##vs_canvas", graphSize, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        m_GraphEditor.Render(m_GraphData, m_Callbacks, m_Colors, settings.uiScale);

        // Update selected node from graph editor
        NodeId selected = m_GraphEditor.GetSelectedNodeId();
        if (selected != 0) {
            m_SelectedNode = selected;
        }

        ImGui::EndChild();
    } else {
        ImGui::TextDisabled("Select an entity with Visual Script component");
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Inspector sidebar (right)
    ImGui::BeginChild("##vs_inspector", ImVec2(inspectorWidth, 0), true);
    DrawInspector(isPlaying);
    ImGui::EndChild();

    // Handle keyboard shortcuts (copy/paste/delete)
    HandleKeyboardShortcuts();

    // Draw the node search popup (must be at window scope, not inside children)
    DrawNodeSearchPopup();

    // Breakpoint condition edit popup
    if (ImGui::BeginPopup("EditBreakpointCondition")) {
        auto* script = GetTargetScript();
        if (script && m_SelectedNode != 0) {
            auto bpIt = script->breakpoints.find(m_SelectedNode);
            if (bpIt != script->breakpoints.end()) {
                auto& bp = bpIt->second;

                ImGui::Text("Breakpoint Condition");
                ImGui::Separator();

                ImGui::Checkbox("Enabled", &bp.enabled);

                static char condBuf[128] = "";
                // Copy current condition to buffer on first frame
                if (ImGui::IsWindowAppearing()) {
                    strncpy(condBuf, bp.condition.c_str(), sizeof(condBuf) - 1);
                    condBuf[sizeof(condBuf) - 1] = '\0';
                }
                ImGui::Text("Condition (e.g. health < 50):");
                if (ImGui::InputText("##bpcond", condBuf, sizeof(condBuf))) {
                    bp.condition = condBuf;
                }

                i32 hitTarget = static_cast<i32>(bp.hitCountTarget);
                if (ImGui::InputInt("Hit Count Target", &hitTarget)) {
                    bp.hitCountTarget = static_cast<u32>(std::max(0, hitTarget));
                }
                ImGui::Text("Current hit count: %u", bp.hitCount);

                if (ImGui::Button("Reset Hit Count")) {
                    bp.hitCount = 0;
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove Breakpoint")) {
                    script->breakpoints.erase(m_SelectedNode);
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndPopup();
    }
}

// ============================================================================
// ENTITY SIDEBAR
// ============================================================================

void VisualScriptEditor::DrawEntitySidebar() {
    ImGui::Text("Entities");
    ImGui::Separator();

    if (!m_World) {
        ImGui::TextDisabled("No world");
        return;
    }

    // List all entities with VisualScriptComponent
    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::VisualScriptComponent>()) {
        // Get name
        std::string name = "Entity " + std::to_string(entity);
        auto* nameComp = m_World->GetComponent<ECS::NameComponent>(entity);
        if (nameComp && !nameComp->name.empty()) {
            name = nameComp->name;
        }

        bool isSelected = (entity == m_TargetEntity);
        if (ImGui::Selectable(name.c_str(), isSelected)) {
            SetTarget(m_World, entity);
        }
    }
}

// ============================================================================
// TOOLBAR
// ============================================================================

void VisualScriptEditor::DrawToolbar() {
    if (ImGui::Button("+ Event")) {
        ImGui::OpenPopup("AddEventPopup");
    }

    if (ImGui::BeginPopup("AddEventPopup")) {
        if (ImGui::MenuItem("On Start")) {
            auto* def = VisualScript::NodeRegistry::Instance().FindNode(VisualScript::NodeTypes::OnStart);
            AddNodeFromDefinition(def, Math::Vector2(100, 100));
        }
        if (ImGui::MenuItem("On Update")) {
            auto* def = VisualScript::NodeRegistry::Instance().FindNode(VisualScript::NodeTypes::OnUpdate);
            AddNodeFromDefinition(def, Math::Vector2(100, 200));
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("+ Variable")) {
        ImGui::OpenPopup("AddVariablePopup");
    }

    if (ImGui::BeginPopup("AddVariablePopup")) {
        ImGui::InputText("Name", m_NewVarName, sizeof(m_NewVarName));

        const char* typeNames[] = {"Bool", "Int", "Float", "String", "Vector3", "Entity"};
        ImGui::Combo("Type", &m_NewVarType, typeNames, IM_ARRAYSIZE(typeNames));

        if (ImGui::Button("Add")) {
            auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
            if (script) {
                ECS::VisualScriptVariable var;
                var.name = m_NewVarName;

                switch (m_NewVarType) {
                    case 0:
                        var = ECS::VisualScriptVariable::Bool(m_NewVarName);
                        break;
                    case 1:
                        var = ECS::VisualScriptVariable::Int(m_NewVarName);
                        break;
                    case 2:
                        var = ECS::VisualScriptVariable::Float(m_NewVarName);
                        break;
                    case 3:
                        var = ECS::VisualScriptVariable::String(m_NewVarName);
                        break;
                    case 4:
                        var = ECS::VisualScriptVariable::Vec3(m_NewVarName);
                        break;
                    case 5:
                        var = ECS::VisualScriptVariable::EntityRef(m_NewVarName);
                        break;
                }

                script->variables.push_back(var);
                std::strcpy(m_NewVarName, "NewVariable");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Fit All")) {
        m_GraphEditor.FitAllNodes(m_GraphData);
    }

    ImGui::Separator();
}

// ============================================================================
// DEBUG CONTROLS
// ============================================================================

void VisualScriptEditor::DrawDebugControls(bool isPlaying) {
    if (!isPlaying || !HasTarget()) return;

    auto* script = GetTargetScript();
    if (!script) return;

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.2f, 1.0f));

    // Continue button (F5)
    ImGui::BeginDisabled(!script->isPaused);
    if (ImGui::Button("Continue (F5)") || (script->isPaused && ImGui::IsKeyPressed(ImGuiKey_F5))) {
        ContinueExecution();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    // Step Over button (F10)
    ImGui::BeginDisabled(!script->isPaused);
    if (ImGui::Button("Step (F10)") || (script->isPaused && ImGui::IsKeyPressed(ImGuiKey_F10))) {
        StepOver();
    }
    ImGui::EndDisabled();

    ImGui::PopStyleColor();

    // Paused indicator
    if (script->isPaused) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "PAUSED at node %u", script->pausedAtNode);
    }

    // Recording toggle
    ImGui::SameLine();
    ImGui::Checkbox("Record", &script->recordingEnabled);

    ImGui::Separator();
}

void VisualScriptEditor::ToggleBreakpoint(NodeId nodeId) {
    auto* script = GetTargetScript();
    if (!script || nodeId == 0) return;

    if (script->breakpoints.count(nodeId) > 0) {
        script->breakpoints.erase(nodeId);
    } else {
        script->breakpoints[nodeId] = ECS::VisualScriptComponent::BreakpointInfo{};
    }
}

void VisualScriptEditor::ContinueExecution() {
    auto* script = GetTargetScript();
    if (!script) return;

    script->isPaused = false;
    script->pausedAtNode = 0;
    script->stepRequested = false;
}

void VisualScriptEditor::StepOver() {
    auto* script = GetTargetScript();
    if (!script) return;

    script->stepRequested = true;
    // isPaused remains true, will pause again after one step
}

ECS::VisualScriptComponent* VisualScriptEditor::GetTargetScript() {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return nullptr;
    return m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity);
}

// ============================================================================
// EXECUTION TIMELINE
// ============================================================================

void VisualScriptEditor::DrawExecutionTimeline() {
    auto* script = GetTargetScript();
    if (!script || script->executionHistory.empty()) return;

    if (ImGui::CollapsingHeader("Execution Timeline")) {
        // Timeline visualization
        ImVec2 canvasSize = ImVec2(ImGui::GetContentRegionAvail().x, 80);
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(canvasPos,
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
            IM_COL32(30, 30, 30, 255));

        // Draw execution bars
        f32 barHeight = 12.0f;
        usize maxRows = 6;
        usize startIdx = script->executionHistory.size() > 50 ?
                         script->executionHistory.size() - 50 : 0;

        for (usize i = startIdx; i < script->executionHistory.size(); i++) {
            const auto& rec = script->executionHistory[i];

            f32 x = canvasPos.x + ((i - startIdx) * 8.0f);
            if (x > canvasPos.x + canvasSize.x - 8.0f) break;

            f32 row = static_cast<f32>((i - startIdx) % maxRows);
            f32 y = canvasPos.y + row * barHeight + 4.0f;

            // Color by node category (simplified)
            u32 color = IM_COL32(100, 150, 200, 255);  // Default blue
            if (rec.nodeType.find("Event_") == 0) {
                color = IM_COL32(80, 180, 80, 255);  // Green for events
            } else if (rec.nodeType.find("Flow_") == 0) {
                color = IM_COL32(200, 140, 60, 255);  // Orange for flow
            } else if (rec.nodeType.find("Debug_") == 0) {
                color = IM_COL32(160, 100, 160, 255);  // Purple for debug
            }

            f32 width = std::max(4.0f, std::min(20.0f, rec.duration * 10.0f));
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + width, y + barHeight - 2), color);

            // Tooltip on hover
            if (ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(x + width, y + barHeight))) {
                ImGui::BeginTooltip();
                ImGui::Text("Node: %s", rec.nodeType.c_str());
                ImGui::Text("Duration: %.3f ms", rec.duration);
                ImGui::EndTooltip();
            }
        }

        ImGui::Dummy(canvasSize);

        // Stats
        ImGui::Text("Recorded: %zu events", script->executionHistory.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            script->executionHistory.clear();
        }
    }
}

// ============================================================================
// INSPECTOR
// ============================================================================

void VisualScriptEditor::DrawInspector(bool isPlaying) {
    if (!HasTarget()) {
        ImGui::TextDisabled("No entity selected");
        return;
    }

    auto* script = m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity);
    if (!script) {
        ImGui::TextDisabled("No Visual Script");
        return;
    }

    // Show enabled checkbox
    ImGui::Checkbox("Enabled", &script->enabled);
    ImGui::Separator();

    // Selected node properties
    if (m_SelectedNode != 0) {
        auto* node = m_GraphData.FindNode(m_SelectedNode);
        if (node) {
            ImGui::Text("Node: %s", node->title.c_str());

            auto typeIt = m_NodeTypeMap.find(m_SelectedNode);
            if (typeIt != m_NodeTypeMap.end()) {
                ImGui::TextDisabled("Type: %s", typeIt->second.c_str());
            }

            ImGui::Separator();
            DrawNodeProperties();
        }
    }

    ImGui::Separator();

    // Variables section
    if (ImGui::CollapsingHeader("Variables", ImGuiTreeNodeFlags_DefaultOpen)) {
        DrawVariableEditor();
    }

    // Functions section
    if (ImGui::CollapsingHeader("Functions")) {
        DrawFunctionsPanel();
    }

    // Watch Window & Call Stack (play mode)
    if (isPlaying && script->isPaused) {
        if (ImGui::CollapsingHeader("Watch", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Add watch input
            static char watchBuf[64] = "";
            ImGui::SetNextItemWidth(-40);
            ImGui::InputText("##addwatch", watchBuf, sizeof(watchBuf));
            ImGui::SameLine();
            if (ImGui::SmallButton("+##aw")) {
                if (watchBuf[0] != '\0') {
                    script->watchVariables.push_back(watchBuf);
                    watchBuf[0] = '\0';
                }
            }

            // Watch table
            if (!script->watchVariables.empty() && ImGui::BeginTable("WatchTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 50);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableHeadersRow();

                for (usize wi = 0; wi < script->watchVariables.size(); wi++) {
                    const auto& wname = script->watchVariables[wi];
                    const auto* var = script->FindVariable(wname);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", wname.c_str());

                    ImGui::TableNextColumn();
                    if (var) {
                        if (std::holds_alternative<f32>(var->value)) ImGui::Text("Float");
                        else if (std::holds_alternative<i32>(var->value)) ImGui::Text("Int");
                        else if (std::holds_alternative<bool>(var->value)) ImGui::Text("Bool");
                        else if (std::holds_alternative<std::string>(var->value)) ImGui::Text("String");
                        else if (std::holds_alternative<Math::Vector3>(var->value)) ImGui::Text("Vec3");
                        else ImGui::Text("?");
                    } else {
                        ImGui::TextDisabled("--");
                    }

                    ImGui::TableNextColumn();
                    if (var) {
                        if (std::holds_alternative<f32>(var->value))
                            ImGui::Text("%.3f", std::get<f32>(var->value));
                        else if (std::holds_alternative<i32>(var->value))
                            ImGui::Text("%d", std::get<i32>(var->value));
                        else if (std::holds_alternative<bool>(var->value))
                            ImGui::Text("%s", std::get<bool>(var->value) ? "true" : "false");
                        else if (std::holds_alternative<std::string>(var->value))
                            ImGui::Text("%s", std::get<std::string>(var->value).c_str());
                        else if (std::holds_alternative<Math::Vector3>(var->value)) {
                            auto v = std::get<Math::Vector3>(var->value);
                            ImGui::Text("(%.1f, %.1f, %.1f)", v.x, v.y, v.z);
                        }
                    } else {
                        ImGui::TextDisabled("not found");
                    }
                }
                ImGui::EndTable();
            }
        }

        if (ImGui::CollapsingHeader("Call Stack", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (script->callStack.empty()) {
                ImGui::TextDisabled("Empty");
            } else {
                for (i32 si = static_cast<i32>(script->callStack.size()) - 1; si >= 0; si--) {
                    const auto& entry = script->callStack[si];
                    char label[128];
                    snprintf(label, sizeof(label), "#%d %s (%s)", si, entry.displayName.c_str(), entry.nodeType.c_str());
                    if (ImGui::Selectable(label)) {
                        // Navigate to this node
                        m_SelectedNode = entry.nodeId;
                    }
                }
            }
        }
    }

    // Execution Timeline (play mode)
    if (isPlaying) {
        DrawExecutionTimeline();
    }
}

// ============================================================================
// VARIABLE EDITOR
// ============================================================================

void VisualScriptEditor::DrawVariableEditor() {
    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return;

    for (usize i = 0; i < script->variables.size(); i++) {
        auto& var = script->variables[i];

        ImGui::PushID(static_cast<int>(i));

        // Variable name and type
        ImGui::Text("%s", var.name.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", var.type == PinType::Bool ? "Bool" :
                                    var.type == PinType::Int ? "Int" :
                                    var.type == PinType::Float ? "Float" :
                                    var.type == PinType::String ? "String" :
                                    var.type == PinType::Vector3 ? "Vector3" :
                                    var.type == PinType::Entity ? "Entity" : "Any");

        // Exposed toggle
        ImGui::SameLine();
        ImGui::Checkbox("Exposed", &var.exposed);

        // Capture before value for undo
        std::string beforeValue = SerializeVariableValue(var);

        // Value editor with undo support
        switch (var.type) {
            case PinType::Bool: {
                bool val = std::holds_alternative<bool>(var.value) ? std::get<bool>(var.value) : false;
                if (ImGui::Checkbox("Value##val", &val)) {
                    std::string afterValue = val ? "true" : "false";
                    if (m_UndoManager && beforeValue != afterValue) {
                        m_UndoManager->Execute(std::make_unique<EditVariableCommand>(
                            this, var.name, beforeValue, afterValue));
                    } else {
                        var.value = val;
                    }
                }
                break;
            }
            case PinType::Int: {
                i32 val = std::holds_alternative<i32>(var.value) ? std::get<i32>(var.value) : 0;
                if (ImGui::InputInt("Value##val", &val)) {
                    // Continuously update while editing
                    var.value = val;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string afterValue = std::to_string(val);
                    if (m_UndoManager && beforeValue != afterValue) {
                        m_UndoManager->Execute(std::make_unique<EditVariableCommand>(
                            this, var.name, beforeValue, afterValue));
                    }
                }
                break;
            }
            case PinType::Float: {
                f32 val = std::holds_alternative<f32>(var.value) ? std::get<f32>(var.value) : 0.0f;
                if (ImGui::InputFloat("Value##val", &val)) {
                    var.value = val;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string afterValue = std::to_string(val);
                    if (m_UndoManager && beforeValue != afterValue) {
                        m_UndoManager->Execute(std::make_unique<EditVariableCommand>(
                            this, var.name, beforeValue, afterValue));
                    }
                }
                break;
            }
            case PinType::String: {
                std::string val = std::holds_alternative<std::string>(var.value) ? std::get<std::string>(var.value) : "";
                char buf[256];
                std::strncpy(buf, val.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText("Value##val", buf, sizeof(buf))) {
                    var.value = std::string(buf);
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string afterValue = buf;
                    if (m_UndoManager && beforeValue != afterValue) {
                        m_UndoManager->Execute(std::make_unique<EditVariableCommand>(
                            this, var.name, beforeValue, afterValue));
                    }
                }
                break;
            }
            case PinType::Vector3: {
                Math::Vector3 val = std::holds_alternative<Math::Vector3>(var.value) ?
                    std::get<Math::Vector3>(var.value) : Math::Vector3(0, 0, 0);
                float v[3] = {val.x, val.y, val.z};
                if (ImGui::InputFloat3("Value##val", v)) {
                    var.value = Math::Vector3(v[0], v[1], v[2]);
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    char afterBuf[128];
                    std::snprintf(afterBuf, sizeof(afterBuf), "%f,%f,%f", v[0], v[1], v[2]);
                    std::string afterValue = afterBuf;
                    if (m_UndoManager && beforeValue != afterValue) {
                        m_UndoManager->Execute(std::make_unique<EditVariableCommand>(
                            this, var.name, beforeValue, afterValue));
                    }
                }
                break;
            }
            default:
                ImGui::TextDisabled("(no editor)");
                break;
        }

        // Delete button
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            script->variables.erase(script->variables.begin() + i);
            ImGui::PopID();
            break;  // Iterator invalidated
        }

        ImGui::PopID();
    }
}

// ============================================================================
// FUNCTIONS PANEL (Subgraph management)
// ============================================================================

void VisualScriptEditor::DrawFunctionsPanel() {
    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return;

    // Breadcrumb: show current editing context
    if (m_EditingFunctionIndex >= 0 && m_EditingFunctionIndex < static_cast<i32>(script->functions.size())) {
        ImGui::TextDisabled("Editing:");
        ImGui::SameLine();
        if (ImGui::SmallButton("Main Graph")) {
            // Save subgraph back and switch to main graph
            script->functions[m_EditingFunctionIndex].graph = m_GraphData;
            script->functions[m_EditingFunctionIndex].nodeMeta.clear();
            for (const auto& [nid, meta] : script->nodeMeta) {
                script->functions[m_EditingFunctionIndex].nodeMeta[nid] = meta;
            }
            // Restore main graph
            m_GraphData = script->graph;
            m_NeedsSync = true;
            m_EditingFunctionIndex = -1;
        }
        ImGui::SameLine();
        ImGui::Text("> %s", script->functions[m_EditingFunctionIndex].name.c_str());
    }

    // List existing functions
    for (i32 i = 0; i < static_cast<i32>(script->functions.size()); i++) {
        auto& func = script->functions[i];
        ImGui::PushID(i);

        bool isCurrent = (i == m_EditingFunctionIndex);
        if (isCurrent) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));

        if (ImGui::Selectable(func.name.c_str(), isCurrent)) {
            if (!isCurrent) {
                // Save current graph first
                if (m_EditingFunctionIndex >= 0) {
                    script->functions[m_EditingFunctionIndex].graph = m_GraphData;
                } else {
                    script->graph = m_GraphData;
                    SyncToComponent();
                }

                // Switch to function subgraph
                m_EditingFunctionIndex = i;
                m_GraphData = func.graph;
                m_NeedsSync = true;
            }
        }

        if (isCurrent) ImGui::PopStyleColor();

        ImGui::SameLine();
        if (ImGui::SmallButton("X##del")) {
            if (i == m_EditingFunctionIndex) {
                // Go back to main graph first
                m_GraphData = script->graph;
                m_EditingFunctionIndex = -1;
                m_NeedsSync = true;
            }
            script->functions.erase(script->functions.begin() + i);
            ImGui::PopID();
            break;
        }

        ImGui::PopID();
    }

    // Add new function
    ImGui::InputText("##funcname", m_NewFuncName, sizeof(m_NewFuncName));
    ImGui::SameLine();
    if (ImGui::SmallButton("Add Function")) {
        std::string funcName = m_NewFuncName;
        if (!funcName.empty()) {
            ECS::VisualScriptFunction newFunc;
            newFunc.name = funcName;

            // Create a Function_Entry node in the subgraph
            NodeId entryId = newFunc.graph.AddNode("Function Entry", Math::Vector2(100, 200),
                                                     Math::Vector3(0.2f, 0.6f, 0.3f));
            newFunc.graph.AddPin(entryId, "", PinType::Flow, PinKind::Output);

            ECS::VisualScriptNodeMeta meta;
            meta.nodeType = VisualScript::NodeTypes::FunctionEntry;
            newFunc.nodeMeta[entryId] = meta;

            script->functions.push_back(std::move(newFunc));
            std::strncpy(m_NewFuncName, "NewFunction", sizeof(m_NewFuncName));
        }
    }
}

// ============================================================================
// NODE SEARCH POPUP
// ============================================================================

// Scoring function for fuzzy matching (similar to EditorLayer's component search)
static int ScoreNodeMatch(const VisualScript::NodeDefinition* def, const char* filter) {
    if (!def || !filter || filter[0] == '\0') return 100; // No filter = show all

    std::string filterLower = filter;
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);

    std::string nameLower = def->displayName;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    std::string typeIdLower = def->typeId;
    std::transform(typeIdLower.begin(), typeIdLower.end(), typeIdLower.begin(), ::tolower);

    // Exact match
    if (nameLower == filterLower) return 100;

    // Prefix match
    if (nameLower.find(filterLower) == 0) return 90;

    // Substring match
    if (nameLower.find(filterLower) != std::string::npos) return 70;

    // Type ID contains filter
    if (typeIdLower.find(filterLower) != std::string::npos) return 60;

    // Category match
    const char* catName = "";
    switch (def->category) {
        case VisualScript::NodeCategory::Events: catName = "events"; break;
        case VisualScript::NodeCategory::FlowControl: catName = "flow"; break;
        case VisualScript::NodeCategory::Variables: catName = "variables"; break;
        case VisualScript::NodeCategory::Math: catName = "math"; break;
        case VisualScript::NodeCategory::Logic: catName = "logic"; break;
        case VisualScript::NodeCategory::Vector: catName = "vector"; break;
        case VisualScript::NodeCategory::Transform: catName = "transform"; break;
        case VisualScript::NodeCategory::Entity: catName = "entity"; break;
        case VisualScript::NodeCategory::Physics: catName = "physics"; break;
        case VisualScript::NodeCategory::Components: catName = "components"; break;
        case VisualScript::NodeCategory::Debug: catName = "debug"; break;
        default: break;
    }
    if (std::string(catName).find(filterLower) != std::string::npos) return 55;

    // Word boundary match (e.g., "gp" matches "Get Position")
    bool allMatch = true;
    usize filterIdx = 0;
    for (usize i = 0; i < nameLower.size() && filterIdx < filterLower.size(); i++) {
        // Check at word boundaries (start or after space/uppercase)
        bool isWordStart = (i == 0) ||
                           (def->displayName[i-1] == ' ') ||
                           (i > 0 && std::islower(def->displayName[i-1]) && std::isupper(def->displayName[i]));

        if (isWordStart && nameLower[i] == filterLower[filterIdx]) {
            filterIdx++;
        }
    }
    if (filterIdx == filterLower.size()) return 50;

    // No match
    return 0;
}

static const char* GetCategoryName(VisualScript::NodeCategory cat) {
    switch (cat) {
        case VisualScript::NodeCategory::Events: return "Events";
        case VisualScript::NodeCategory::FlowControl: return "Flow Control";
        case VisualScript::NodeCategory::Variables: return "Variables";
        case VisualScript::NodeCategory::Math: return "Math";
        case VisualScript::NodeCategory::Logic: return "Logic";
        case VisualScript::NodeCategory::Vector: return "Vector";
        case VisualScript::NodeCategory::Transform: return "Transform";
        case VisualScript::NodeCategory::Entity: return "Entity";
        case VisualScript::NodeCategory::Physics: return "Physics";
        case VisualScript::NodeCategory::Components: return "Components";
        case VisualScript::NodeCategory::Audio: return "Audio";
        case VisualScript::NodeCategory::Debug: return "Debug";
        case VisualScript::NodeCategory::Utility: return "Utility";
        case VisualScript::NodeCategory::Custom: return "Custom";
        default: return "Other";
    }
}

void VisualScriptEditor::DrawNodeSearchPopup() {
    ImVec2 popupSize(350, 400);
    ImGui::SetNextWindowSize(popupSize, ImGuiCond_Always);

    if (ImGui::BeginPopup("NodeSearchPopup")) {
        // Search input
        ImGui::SetNextItemWidth(-1);
        bool focusSearch = ImGui::IsWindowAppearing();
        if (focusSearch) {
            ImGui::SetKeyboardFocusHere();
        }

        bool searchChanged = ImGui::InputTextWithHint("##NodeSearch", "Search nodes...",
                                                       m_NodeSearchBuf, sizeof(m_NodeSearchBuf));
        if (searchChanged) {
            m_NodeSearchSelectedIndex = 0;
        }

        ImGui::Separator();

        // Get all nodes and score them
        struct ScoredNode {
            const VisualScript::NodeDefinition* def;
            int score;
        };
        std::vector<ScoredNode> scoredNodes;

        auto allNodes = VisualScript::NodeRegistry::Instance().GetAllNodes();
        for (auto* def : allNodes) {
            int score = ScoreNodeMatch(def, m_NodeSearchBuf);
            if (score > 0) {
                scoredNodes.push_back({def, score});
            }
        }

        // Sort by score descending, then by name
        std::sort(scoredNodes.begin(), scoredNodes.end(), [](const ScoredNode& a, const ScoredNode& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.def->displayName < b.def->displayName;
        });

        // Clamp selection index
        if (m_NodeSearchSelectedIndex >= static_cast<int>(scoredNodes.size())) {
            m_NodeSearchSelectedIndex = static_cast<int>(scoredNodes.size()) - 1;
        }
        if (m_NodeSearchSelectedIndex < 0) {
            m_NodeSearchSelectedIndex = 0;
        }

        // Recently used section (when no filter)
        bool showRecent = (m_NodeSearchBuf[0] == '\0');
        if (showRecent && m_EditorSettings) {
            const auto& recentNodes = m_EditorSettings->recentVisualScriptNodes;
            if (!recentNodes.empty()) {
                ImGui::TextDisabled("Recently Used");
                for (const auto& typeId : recentNodes) {
                    auto* def = VisualScript::NodeRegistry::Instance().FindNode(typeId);
                    if (def) {
                        if (ImGui::Selectable(def->displayName.c_str())) {
                            NodeId newId = AddNodeFromDefinition(def, m_ContextMenuPos);
                            if (newId != 0 && m_EditorSettings) {
                                m_EditorSettings->AddRecentVisualScriptNode(def->typeId);
                            }
                            ImGui::CloseCurrentPopup();
                        }
                    }
                }
                ImGui::Separator();
            }
        }

        // Scrollable node list
        ImGui::BeginChild("##NodeList", ImVec2(0, 0), false);

        int displayIndex = 0;
        VisualScript::NodeCategory lastCategory = VisualScript::NodeCategory::Custom;
        bool firstCategory = true;

        for (const auto& scored : scoredNodes) {
            auto* def = scored.def;

            // Category header (when not filtering)
            if (showRecent && def->category != lastCategory) {
                if (!firstCategory) {
                    ImGui::Spacing();
                }
                ImGui::TextDisabled("%s", GetCategoryName(def->category));
                lastCategory = def->category;
                firstCategory = false;
            }

            // Node entry
            bool isSelected = (displayIndex == m_NodeSearchSelectedIndex);
            ImGui::PushID(displayIndex);

            // Highlight selected item
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
            }

            if (ImGui::Selectable(def->displayName.c_str(), isSelected)) {
                NodeId newId = AddNodeFromDefinition(def, m_ContextMenuPos);
                if (newId != 0 && m_EditorSettings) {
                    m_EditorSettings->AddRecentVisualScriptNode(def->typeId);
                }
                ImGui::CloseCurrentPopup();
            }

            if (isSelected) {
                ImGui::PopStyleColor();
                // Scroll to selected
                if (ImGui::IsWindowAppearing() || searchChanged) {
                    ImGui::SetScrollHereY();
                }
            }

            // Tooltip with category info
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\nCategory: %s", def->typeId.c_str(), GetCategoryName(def->category));
            }

            ImGui::PopID();
            displayIndex++;
        }

        ImGui::EndChild();

        // Keyboard navigation
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            m_NodeSearchSelectedIndex++;
            if (m_NodeSearchSelectedIndex >= static_cast<int>(scoredNodes.size())) {
                m_NodeSearchSelectedIndex = 0;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            m_NodeSearchSelectedIndex--;
            if (m_NodeSearchSelectedIndex < 0) {
                m_NodeSearchSelectedIndex = static_cast<int>(scoredNodes.size()) - 1;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !scoredNodes.empty()) {
            auto* def = scoredNodes[m_NodeSearchSelectedIndex].def;
            NodeId newId = AddNodeFromDefinition(def, m_ContextMenuPos);
            if (newId != 0 && m_EditorSettings) {
                m_EditorSettings->AddRecentVisualScriptNode(def->typeId);
            }
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

// ============================================================================
// NODE PROPERTIES
// ============================================================================

void VisualScriptEditor::DrawNodeProperties() {
    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script || m_SelectedNode == 0) return;

    auto metaIt = script->nodeMeta.find(m_SelectedNode);
    if (metaIt == script->nodeMeta.end()) return;

    auto& meta = metaIt->second;

    // Get/Set Variable nodes need variable name selection
    if (meta.nodeType == VisualScript::NodeTypes::GetVariable ||
        meta.nodeType == VisualScript::NodeTypes::SetVariable) {

        std::string currentVar = meta.properties["variableName"];

        if (ImGui::BeginCombo("Variable", currentVar.empty() ? "(none)" : currentVar.c_str())) {
            for (const auto& var : script->variables) {
                bool isSelected = (var.name == currentVar);
                if (ImGui::Selectable(var.name.c_str(), isSelected)) {
                    std::string oldValue = currentVar;
                    std::string newValue = var.name;

                    if (m_UndoManager && oldValue != newValue) {
                        m_UndoManager->Execute(std::make_unique<EditNodePropertyCommand>(
                            this, m_SelectedNode, "variableName", oldValue, newValue));
                    } else {
                        meta.properties["variableName"] = var.name;
                    }
                }
            }
            ImGui::EndCombo();
        }
    }

    // Delay node duration property
    if (meta.nodeType == VisualScript::NodeTypes::Delay) {
        f32 duration = 1.0f;
        auto durationIt = meta.properties.find("duration");
        if (durationIt != meta.properties.end()) {
            try { duration = std::stof(durationIt->second); }
            catch (...) { duration = 1.0f; }
        }

        std::string beforeValue = std::to_string(duration);
        if (ImGui::InputFloat("Duration (s)", &duration, 0.1f, 1.0f, "%.2f")) {
            if (duration < 0.0f) duration = 0.0f;
            meta.properties["duration"] = std::to_string(duration);
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string afterValue = std::to_string(duration);
            if (m_UndoManager && beforeValue != afterValue) {
                m_UndoManager->Execute(std::make_unique<EditNodePropertyCommand>(
                    this, m_SelectedNode, "duration", beforeValue, afterValue));
            }
        }
    }

    // Print node message property
    if (meta.nodeType == VisualScript::NodeTypes::Print) {
        std::string message;
        auto msgIt = meta.properties.find("message");
        if (msgIt != meta.properties.end()) {
            message = msgIt->second;
        }

        char buf[512];
        std::strncpy(buf, message.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        std::string beforeValue = message;
        if (ImGui::InputText("Message", buf, sizeof(buf))) {
            meta.properties["message"] = buf;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string afterValue = buf;
            if (m_UndoManager && beforeValue != afterValue) {
                m_UndoManager->Execute(std::make_unique<EditNodePropertyCommand>(
                    this, m_SelectedNode, "message", beforeValue, afterValue));
            }
        }
    }
}

// ============================================================================
// CLIPBOARD OPERATIONS
// ============================================================================

void VisualScriptEditor::CopySelectedNodes() {
    const auto& selectedNodes = m_GraphEditor.GetSelectedNodeIds();
    if (selectedNodes.empty() && m_SelectedNode == 0) return;

    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return;

    m_ClipboardNodeIds.clear();

    // Determine which nodes to copy (multi-select or single)
    std::unordered_set<NodeId> nodesToCopy;
    if (!selectedNodes.empty()) {
        nodesToCopy = selectedNodes;
    } else if (m_SelectedNode != 0) {
        nodesToCopy.insert(m_SelectedNode);
    }

    if (nodesToCopy.empty()) return;

    nlohmann::json clipboard;
    clipboard["nodes"] = nlohmann::json::array();
    clipboard["links"] = nlohmann::json::array();

    // Build a set of all pin IDs belonging to copied nodes
    std::unordered_set<PinId> copiedPinIds;

    // Serialize all selected nodes
    for (NodeId nodeId : nodesToCopy) {
        const auto* srcNode = m_GraphData.FindNode(nodeId);
        if (!srcNode) continue;

        nlohmann::json nodeJson;
        nodeJson["id"] = srcNode->id;
        nodeJson["title"] = srcNode->title;
        nodeJson["position"] = {srcNode->position.x, srcNode->position.y};
        nodeJson["headerColor"] = {srcNode->headerColor.x, srcNode->headerColor.y, srcNode->headerColor.z};

        // Serialize pins
        nlohmann::json inputPins = nlohmann::json::array();
        for (const auto& pin : srcNode->inputs) {
            inputPins.push_back({
                {"id", pin.id},
                {"name", pin.name},
                {"type", static_cast<int>(pin.type)},
                {"kind", static_cast<int>(pin.kind)}
            });
            copiedPinIds.insert(pin.id);
        }
        nodeJson["inputs"] = inputPins;

        nlohmann::json outputPins = nlohmann::json::array();
        for (const auto& pin : srcNode->outputs) {
            outputPins.push_back({
                {"id", pin.id},
                {"name", pin.name},
                {"type", static_cast<int>(pin.type)},
                {"kind", static_cast<int>(pin.kind)}
            });
            copiedPinIds.insert(pin.id);
        }
        nodeJson["outputs"] = outputPins;

        // Include node metadata
        auto metaIt = script->nodeMeta.find(nodeId);
        if (metaIt != script->nodeMeta.end()) {
            nodeJson["nodeType"] = metaIt->second.nodeType;
            nodeJson["customEventName"] = metaIt->second.customEventName;
            nlohmann::json propsJson;
            for (const auto& [key, val] : metaIt->second.properties) {
                propsJson[key] = val;
            }
            nodeJson["properties"] = propsJson;
        }

        clipboard["nodes"].push_back(nodeJson);
        m_ClipboardNodeIds.push_back(nodeId);
    }

    // Copy internal links (links where both endpoints are in the selection)
    for (const auto& link : m_GraphData.GetLinks()) {
        if (copiedPinIds.count(link.startPinId) > 0 &&
            copiedPinIds.count(link.endPinId) > 0) {
            clipboard["links"].push_back({
                {"id", link.id},
                {"startPinId", link.startPinId},
                {"endPinId", link.endPinId}
            });
        }
    }

    m_ClipboardJson = clipboard.dump();
    m_ClipboardIsCut = false;
}

void VisualScriptEditor::CutSelectedNodes() {
    CopySelectedNodes();
    if (!m_ClipboardJson.empty()) {
        m_ClipboardIsCut = true;
        DeleteSelectedNodes();
    }
}

void VisualScriptEditor::PasteNodes() {
    if (m_ClipboardJson.empty()) return;

    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return;

    try {
        auto clipboard = nlohmann::json::parse(m_ClipboardJson);

        if (!clipboard.contains("nodes") || !clipboard["nodes"].is_array()) return;
        if (clipboard["nodes"].empty()) return;

        // Calculate center of copied nodes for relative positioning
        Math::Vector2 center(0, 0);
        usize nodeCount = 0;
        for (const auto& nodeJson : clipboard["nodes"]) {
            if (nodeJson.contains("position") && nodeJson["position"].is_array()) {
                center.x += nodeJson["position"][0].get<f32>();
                center.y += nodeJson["position"][1].get<f32>();
                nodeCount++;
            }
        }
        if (nodeCount > 0) {
            center.x /= static_cast<f32>(nodeCount);
            center.y /= static_cast<f32>(nodeCount);
        }

        // Offset for paste position
        Math::Vector2 offset(50.0f, 50.0f);

        // Maps from old IDs to new IDs
        std::unordered_map<NodeId, NodeId> nodeIdMap;
        std::unordered_map<PinId, PinId> pinIdMap;

        // Clear selection for new nodes
        m_GraphEditor.ClearSelection();

        // Paste each node with new IDs and relative positions
        for (const auto& nodeJson : clipboard["nodes"]) {
            std::string title = nodeJson.value("title", "Node");
            Math::Vector2 pos(100, 100);
            if (nodeJson.contains("position") && nodeJson["position"].is_array()) {
                pos.x = nodeJson["position"][0].get<f32>() + offset.x;
                pos.y = nodeJson["position"][1].get<f32>() + offset.y;
            }
            Math::Vector3 color(0.3f, 0.3f, 0.6f);
            if (nodeJson.contains("headerColor") && nodeJson["headerColor"].is_array()) {
                color.x = nodeJson["headerColor"][0].get<f32>();
                color.y = nodeJson["headerColor"][1].get<f32>();
                color.z = nodeJson["headerColor"][2].get<f32>();
            }

            NodeId oldId = nodeJson.value("id", 0u);

            // Create new node
            NodeId newId = m_GraphData.AddNode(title, pos, color);
            auto* newNode = m_GraphData.FindNode(newId);
            if (!newNode) continue;

            nodeIdMap[oldId] = newId;

            // Recreate pins and build pin ID mapping
            if (nodeJson.contains("inputs") && nodeJson["inputs"].is_array()) {
                for (const auto& pinJson : nodeJson["inputs"]) {
                    std::string name = pinJson.value("name", "");
                    PinType type = static_cast<PinType>(pinJson.value("type", 0));
                    PinId oldPinId = pinJson.value("id", 0u);
                    PinId newPinId = m_GraphData.AddPin(newId, name, type, PinKind::Input);
                    pinIdMap[oldPinId] = newPinId;
                }
            }
            if (nodeJson.contains("outputs") && nodeJson["outputs"].is_array()) {
                for (const auto& pinJson : nodeJson["outputs"]) {
                    std::string name = pinJson.value("name", "");
                    PinType type = static_cast<PinType>(pinJson.value("type", 0));
                    PinId oldPinId = pinJson.value("id", 0u);
                    PinId newPinId = m_GraphData.AddPin(newId, name, type, PinKind::Output);
                    pinIdMap[oldPinId] = newPinId;
                }
            }

            // Recreate metadata
            ECS::VisualScriptNodeMeta meta;
            meta.nodeType = nodeJson.value("nodeType", "");
            meta.customEventName = nodeJson.value("customEventName", "");
            if (nodeJson.contains("properties") && nodeJson["properties"].is_object()) {
                for (const auto& [key, val] : nodeJson["properties"].items()) {
                    meta.properties[key] = val.get<std::string>();
                }
            }
            script->nodeMeta[newId] = meta;
            m_NodeTypeMap[newId] = meta.nodeType;

            // Add to selection
            m_GraphEditor.SelectNode(newId, true);
        }

        // Recreate internal links with remapped pin IDs
        if (clipboard.contains("links") && clipboard["links"].is_array()) {
            for (const auto& linkJson : clipboard["links"]) {
                PinId oldStartPin = linkJson.value("startPinId", 0u);
                PinId oldEndPin = linkJson.value("endPinId", 0u);

                auto startIt = pinIdMap.find(oldStartPin);
                auto endIt = pinIdMap.find(oldEndPin);

                if (startIt != pinIdMap.end() && endIt != pinIdMap.end()) {
                    m_GraphData.AddLink(startIt->second, endIt->second);
                }
            }
        }

        // Set primary selection to first pasted node
        if (!nodeIdMap.empty()) {
            m_SelectedNode = nodeIdMap.begin()->second;
        }

        // Sync back to component
        SyncToComponent();

    } catch (const std::exception& e) {
        ENJIN_LOG_WARN(Editor, "Failed to paste nodes: %s", e.what());
    }
}

void VisualScriptEditor::DeleteSelectedNodes() {
    const auto& selectedNodes = m_GraphEditor.GetSelectedNodeIds();
    if (selectedNodes.empty() && m_SelectedNode == 0) return;

    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return;

    // Determine which nodes to delete
    std::vector<NodeId> nodesToDelete;
    if (!selectedNodes.empty()) {
        for (NodeId nodeId : selectedNodes) {
            auto* node = m_GraphData.FindNode(nodeId);
            if (node && !HasFlag(node->flags, NodeFlags::NoDelete)) {
                nodesToDelete.push_back(nodeId);
            }
        }
    } else if (m_SelectedNode != 0) {
        auto* node = m_GraphData.FindNode(m_SelectedNode);
        if (node && !HasFlag(node->flags, NodeFlags::NoDelete)) {
            nodesToDelete.push_back(m_SelectedNode);
        }
    }

    if (nodesToDelete.empty()) return;

    // Delete all selected nodes
    for (NodeId nodeToDelete : nodesToDelete) {
        // Serialize node before deleting for undo
        std::string nodeJson = SerializeNodeToJson(nodeToDelete);

        // Use undo command if manager is available
        if (m_UndoManager) {
            m_UndoManager->Execute(std::make_unique<DeleteVisualScriptNodeCommand>(
                this, nodeToDelete, nodeJson));
        } else {
            RemoveNodeById(nodeToDelete);
        }
    }

    m_SelectedNode = 0;
    m_GraphEditor.ClearSelection();
}

void VisualScriptEditor::HandleKeyboardShortcuts() {
    // Only handle shortcuts when the graph area is focused
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) return;

    // Ctrl+C - Copy
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        CopySelectedNodes();
    }

    // Ctrl+X - Cut
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
        CutSelectedNodes();
    }

    // Ctrl+V - Paste
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
        PasteNodes();
    }

    // Delete - Delete selected node
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        DeleteSelectedNodes();
    }

    // Ctrl+D - Duplicate (copy then paste)
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        CopySelectedNodes();
        PasteNodes();
    }

    // Ctrl+Z - Undo
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !ImGui::GetIO().KeyShift) {
        if (m_UndoManager && m_UndoManager->CanUndo()) {
            m_UndoManager->Undo();
            m_NeedsSync = true;
        }
    }

    // Ctrl+Y or Ctrl+Shift+Z - Redo
    if ((ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) ||
        (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))) {
        if (m_UndoManager && m_UndoManager->CanRedo()) {
            m_UndoManager->Redo();
            m_NeedsSync = true;
        }
    }

    // F9 - Toggle breakpoint on selected node
    if (ImGui::IsKeyPressed(ImGuiKey_F9) && m_SelectedNode != 0) {
        if (ImGui::GetIO().KeyShift) {
            // Shift+F9 - Edit breakpoint condition
            auto* script = GetTargetScript();
            if (script) {
                if (script->breakpoints.count(m_SelectedNode) == 0) {
                    script->breakpoints[m_SelectedNode] = ECS::VisualScriptComponent::BreakpointInfo{};
                }
                ImGui::OpenPopup("EditBreakpointCondition");
            }
        } else {
            ToggleBreakpoint(m_SelectedNode);
        }
    }
}

// ============================================================================
// UNDO/REDO HELPER METHODS
// ============================================================================

std::string VisualScriptEditor::SerializeNodeToJson(NodeId nodeId) {
    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return "{}";

    const auto* srcNode = m_GraphData.FindNode(nodeId);
    if (!srcNode) return "{}";

    nlohmann::json nodeJson;
    nodeJson["id"] = srcNode->id;
    nodeJson["title"] = srcNode->title;
    nodeJson["position"] = {srcNode->position.x, srcNode->position.y};
    nodeJson["headerColor"] = {srcNode->headerColor.x, srcNode->headerColor.y, srcNode->headerColor.z};
    nodeJson["flags"] = static_cast<u32>(srcNode->flags);

    // Serialize pins
    nlohmann::json inputPins = nlohmann::json::array();
    for (const auto& pin : srcNode->inputs) {
        inputPins.push_back({
            {"id", pin.id},
            {"name", pin.name},
            {"type", static_cast<int>(pin.type)},
            {"kind", static_cast<int>(pin.kind)}
        });
    }
    nodeJson["inputs"] = inputPins;

    nlohmann::json outputPins = nlohmann::json::array();
    for (const auto& pin : srcNode->outputs) {
        outputPins.push_back({
            {"id", pin.id},
            {"name", pin.name},
            {"type", static_cast<int>(pin.type)},
            {"kind", static_cast<int>(pin.kind)}
        });
    }
    nodeJson["outputs"] = outputPins;

    // Include node metadata
    auto metaIt = script->nodeMeta.find(nodeId);
    if (metaIt != script->nodeMeta.end()) {
        nodeJson["nodeType"] = metaIt->second.nodeType;
        nodeJson["customEventName"] = metaIt->second.customEventName;
        nlohmann::json propsJson;
        for (const auto& [key, val] : metaIt->second.properties) {
            propsJson[key] = val;
        }
        nodeJson["properties"] = propsJson;
    }

    // Serialize connected links
    nlohmann::json linksJson = nlohmann::json::array();
    auto allLinks = m_GraphData.GetLinksForNode(nodeId);
    for (LinkId linkId : allLinks) {
        const auto* link = m_GraphData.FindLink(linkId);
        if (link) {
            linksJson.push_back({
                {"id", link->id},
                {"startPinId", link->startPinId},
                {"endPinId", link->endPinId}
            });
        }
    }
    nodeJson["links"] = linksJson;

    return nodeJson.dump();
}

void VisualScriptEditor::RemoveNodeById(NodeId nodeId) {
    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return;

    // Remove the node
    m_GraphData.RemoveNode(nodeId);

    // Clean up metadata
    script->nodeMeta.erase(nodeId);
    m_NodeTypeMap.erase(nodeId);

    // Remove from event mappings
    for (auto it = script->eventNodes.begin(); it != script->eventNodes.end(); ) {
        if (it->second == nodeId) {
            it = script->eventNodes.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = script->customEventNodes.begin(); it != script->customEventNodes.end(); ) {
        if (it->second == nodeId) {
            it = script->customEventNodes.erase(it);
        } else {
            ++it;
        }
    }

    if (m_SelectedNode == nodeId) m_SelectedNode = 0;
    SyncToComponent();
}

void VisualScriptEditor::RestoreNodeFromJson(NodeId nodeId, const std::string& json) {
    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return;

    try {
        auto nodeJson = nlohmann::json::parse(json);

        std::string title = nodeJson.value("title", "Node");
        Math::Vector2 pos(100, 100);
        if (nodeJson.contains("position") && nodeJson["position"].is_array()) {
            pos.x = nodeJson["position"][0].get<f32>();
            pos.y = nodeJson["position"][1].get<f32>();
        }
        Math::Vector3 color(0.3f, 0.3f, 0.6f);
        if (nodeJson.contains("headerColor") && nodeJson["headerColor"].is_array()) {
            color.x = nodeJson["headerColor"][0].get<f32>();
            color.y = nodeJson["headerColor"][1].get<f32>();
            color.z = nodeJson["headerColor"][2].get<f32>();
        }

        // Create new node with specific ID (if the graph supports it, otherwise use AddNode)
        NodeId newId = m_GraphData.AddNode(title, pos, color);
        auto* newNode = m_GraphData.FindNode(newId);
        if (!newNode) return;

        // Restore flags
        if (nodeJson.contains("flags")) {
            newNode->flags = static_cast<NodeFlags>(nodeJson["flags"].get<u32>());
        }

        // Recreate pins
        if (nodeJson.contains("inputs") && nodeJson["inputs"].is_array()) {
            for (const auto& pinJson : nodeJson["inputs"]) {
                std::string name = pinJson.value("name", "");
                PinType type = static_cast<PinType>(pinJson.value("type", 0));
                m_GraphData.AddPin(newId, name, type, PinKind::Input);
            }
        }
        if (nodeJson.contains("outputs") && nodeJson["outputs"].is_array()) {
            for (const auto& pinJson : nodeJson["outputs"]) {
                std::string name = pinJson.value("name", "");
                PinType type = static_cast<PinType>(pinJson.value("type", 0));
                m_GraphData.AddPin(newId, name, type, PinKind::Output);
            }
        }

        // Recreate metadata
        ECS::VisualScriptNodeMeta meta;
        meta.nodeType = nodeJson.value("nodeType", "");
        meta.customEventName = nodeJson.value("customEventName", "");
        if (nodeJson.contains("properties") && nodeJson["properties"].is_object()) {
            for (const auto& [key, val] : nodeJson["properties"].items()) {
                meta.properties[key] = val.get<std::string>();
            }
        }
        script->nodeMeta[newId] = meta;
        m_NodeTypeMap[newId] = meta.nodeType;

        // Re-register event node if applicable
        if (meta.nodeType == VisualScript::NodeTypes::OnStart) {
            script->SetEventNode(ECS::VisualScriptEvent::OnStart, newId);
        } else if (meta.nodeType == VisualScript::NodeTypes::OnUpdate) {
            script->SetEventNode(ECS::VisualScriptEvent::OnUpdate, newId);
        }

        SyncToComponent();

    } catch (const std::exception& e) {
        ENJIN_LOG_WARN(Editor, "Failed to restore node from JSON: %s", e.what());
    }
}

void VisualScriptEditor::AddLinkById(PinId startPin, PinId endPin, LinkId linkId) {
    (void)linkId; // LinkId is auto-generated by NodeGraphData
    m_GraphData.AddLink(startPin, endPin, 0);
    SyncToComponent();
}

void VisualScriptEditor::RemoveLinkById(LinkId linkId) {
    m_GraphData.RemoveLink(linkId);
    SyncToComponent();
}

// ============================================================================
// PLAY MODE HIGHLIGHT
// ============================================================================

void VisualScriptEditor::UpdatePlayModeHighlight(bool isPlaying) {
    // Clear all highlights first
    for (auto& node : m_GraphData.GetNodes()) {
        node.flags = static_cast<NodeFlags>(
            static_cast<u32>(node.flags) & ~static_cast<u32>(NodeFlags::Highlighted));
    }

    if (!isPlaying || !HasTarget()) return;

    auto* script = m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity);
    if (!script) return;

    // Highlight paused node
    if (script->isPaused && script->pausedAtNode != 0) {
        auto* pausedNode = m_GraphData.FindNode(script->pausedAtNode);
        if (pausedNode) {
            pausedNode->flags = pausedNode->flags | NodeFlags::Highlighted;
        }
    }
    // Highlight currently executing node
    else if (script->currentlyExecutingNode != 0) {
        auto* node = m_GraphData.FindNode(script->currentlyExecutingNode);
        if (node) {
            node->flags = node->flags | NodeFlags::Highlighted;
        }
    }

    // Also highlight any active latent nodes (e.g., Delay waiting)
    for (const auto& [nodeId, state] : script->latentStates) {
        if (state.isActive) {
            auto* latentNode = m_GraphData.FindNode(nodeId);
            if (latentNode) {
                latentNode->flags = latentNode->flags | NodeFlags::Highlighted;
            }
        }
    }
}

// ============================================================================
// BREAKPOINT VISUALIZATION (called from node rendering callback)
// ============================================================================

// Helper to get breakpoint color for a node (red dot)
static void DrawBreakpointIndicators(ImDrawList* dl, const NodeGraphData& graphData,
                                     const ECS::VisualScriptComponent* script,
                                     ImVec2 canvasPos, f32 zoom, f32 uiScale,
                                     const Math::Vector2& scrollOffset) {
    if (!script) return;

    for (const auto& node : graphData.GetNodes()) {
        auto bpIt = script->breakpoints.find(node.id);
        if (bpIt != script->breakpoints.end()) {
            const auto& bp = bpIt->second;

            // Calculate node screen position
            f32 s = zoom * uiScale;
            f32 nodeX = canvasPos.x + node.position.x * zoom + scrollOffset.x;
            f32 nodeY = canvasPos.y + node.position.y * zoom + scrollOffset.y;

            ImVec2 dotPos(nodeX - 4.0f, nodeY + 10.0f * s);

            // Color: red=unconditional, yellow=conditional, gray=disabled
            ImU32 color;
            if (!bp.enabled)
                color = IM_COL32(128, 128, 128, 200);
            else if (!bp.condition.empty() || bp.hitCountTarget > 0)
                color = IM_COL32(255, 200, 50, 255);  // Yellow for conditional
            else
                color = IM_COL32(255, 50, 50, 255);    // Red for unconditional

            dl->AddCircleFilled(dotPos, 5.0f * s, color);
            dl->AddCircle(dotPos, 5.0f * s, IM_COL32(255, 255, 255, 180), 12, 1.0f);
        }
    }
}

// ============================================================================
// PROPERTY EDIT HELPERS (for undo support)
// ============================================================================

void VisualScriptEditor::SetNodePropertyValue(NodeId nodeId, const std::string& propertyName,
                                               const std::string& value) {
    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return;

    auto metaIt = script->nodeMeta.find(nodeId);
    if (metaIt == script->nodeMeta.end()) return;

    metaIt->second.properties[propertyName] = value;
}

void VisualScriptEditor::SetVariableValue(const std::string& varName, const std::string& value) {
    auto* script = m_World ? m_World->GetComponent<ECS::VisualScriptComponent>(m_TargetEntity) : nullptr;
    if (!script) return;

    for (auto& var : script->variables) {
        if (var.name == varName) {
            // Parse the value based on variable type
            switch (var.type) {
                case PinType::Bool:
                    var.value = (value == "true" || value == "1");
                    break;
                case PinType::Int:
                    try { var.value = std::stoi(value); }
                    catch (...) { var.value = static_cast<i32>(0); }
                    break;
                case PinType::Float:
                    try { var.value = std::stof(value); }
                    catch (...) { var.value = 0.0f; }
                    break;
                case PinType::String:
                    var.value = value;
                    break;
                case PinType::Vector3: {
                    // Parse "x,y,z" format
                    f32 x = 0, y = 0, z = 0;
                    std::sscanf(value.c_str(), "%f,%f,%f", &x, &y, &z);
                    var.value = Math::Vector3(x, y, z);
                    break;
                }
                default:
                    break;
            }
            return;
        }
    }
}

// Helper to serialize a variable value to string for undo
static std::string SerializeVariableValue(const ECS::VisualScriptVariable& var) {
    switch (var.type) {
        case PinType::Bool:
            return std::holds_alternative<bool>(var.value) && std::get<bool>(var.value) ? "true" : "false";
        case PinType::Int:
            return std::to_string(std::holds_alternative<i32>(var.value) ? std::get<i32>(var.value) : 0);
        case PinType::Float:
            return std::to_string(std::holds_alternative<f32>(var.value) ? std::get<f32>(var.value) : 0.0f);
        case PinType::String:
            return std::holds_alternative<std::string>(var.value) ? std::get<std::string>(var.value) : "";
        case PinType::Vector3: {
            if (std::holds_alternative<Math::Vector3>(var.value)) {
                const auto& v = std::get<Math::Vector3>(var.value);
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%f,%f,%f", v.x, v.y, v.z);
                return buf;
            }
            return "0,0,0";
        }
        default:
            return "";
    }
}

} // namespace Editor
} // namespace Enjin
