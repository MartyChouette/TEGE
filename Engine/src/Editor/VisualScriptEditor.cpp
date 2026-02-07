#include "Enjin/Editor/VisualScriptEditor.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/VisualScript/NodeDefinition.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Enjin {
namespace Editor {

// Header colors for different node categories
static const Math::Vector3 COLOR_EVENT      = Math::Vector3(0.2f, 0.6f, 0.3f);  // Green
static const Math::Vector3 COLOR_FLOW       = Math::Vector3(0.6f, 0.4f, 0.2f);  // Orange
static const Math::Vector3 COLOR_VARIABLE   = Math::Vector3(0.4f, 0.5f, 0.4f);  // Muted green
static const Math::Vector3 COLOR_MATH       = Math::Vector3(0.3f, 0.5f, 0.3f);  // Green
static const Math::Vector3 COLOR_TRANSFORM  = Math::Vector3(0.4f, 0.4f, 0.6f);  // Blue
static const Math::Vector3 COLOR_DEBUG      = Math::Vector3(0.5f, 0.3f, 0.5f);  // Purple

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

    // Draw the node search popup (must be at window scope, not inside children)
    DrawNodeSearchPopup();
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
    for (ECS::Entity entity : m_World->GetAllEntities()) {
        if (!m_World->HasComponent<ECS::VisualScriptComponent>(entity)) continue;

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

        // Value editor
        switch (var.type) {
            case PinType::Bool: {
                bool val = std::holds_alternative<bool>(var.value) ? std::get<bool>(var.value) : false;
                if (ImGui::Checkbox("Value##val", &val)) {
                    var.value = val;
                }
                break;
            }
            case PinType::Int: {
                i32 val = std::holds_alternative<i32>(var.value) ? std::get<i32>(var.value) : 0;
                if (ImGui::InputInt("Value##val", &val)) {
                    var.value = val;
                }
                break;
            }
            case PinType::Float: {
                f32 val = std::holds_alternative<f32>(var.value) ? std::get<f32>(var.value) : 0.0f;
                if (ImGui::InputFloat("Value##val", &val)) {
                    var.value = val;
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
                break;
            }
            case PinType::Vector3: {
                Math::Vector3 val = std::holds_alternative<Math::Vector3>(var.value) ?
                    std::get<Math::Vector3>(var.value) : Math::Vector3(0, 0, 0);
                float v[3] = {val.x, val.y, val.z};
                if (ImGui::InputFloat3("Value##val", v)) {
                    var.value = Math::Vector3(v[0], v[1], v[2]);
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
                    meta.properties["variableName"] = var.name;
                }
            }
            ImGui::EndCombo();
        }
    }
}

} // namespace Editor
} // namespace Enjin
