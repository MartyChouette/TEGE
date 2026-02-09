#include "Enjin/Editor/QuestFlowEditor.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/ECS/Components/Name.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Enjin {
namespace Editor {

// Header colors by node type
static const Math::Vector3 COLOR_START     = Math::Vector3(0.3f, 0.7f, 0.3f);   // Green
static const Math::Vector3 COLOR_OBJECTIVE = Math::Vector3(0.2f, 0.5f, 0.8f);   // Blue
static const Math::Vector3 COLOR_CONDITION = Math::Vector3(0.8f, 0.5f, 0.2f);   // Orange
static const Math::Vector3 COLOR_REWARD    = Math::Vector3(0.8f, 0.7f, 0.2f);   // Gold
static const Math::Vector3 COLOR_BRANCH    = Math::Vector3(0.6f, 0.3f, 0.7f);   // Purple
static const Math::Vector3 COLOR_END       = Math::Vector3(0.7f, 0.3f, 0.3f);   // Red
static const Math::Vector3 COLOR_DELAY     = Math::Vector3(0.5f, 0.5f, 0.5f);   // Grey
static const Math::Vector3 COLOR_EVENT     = Math::Vector3(0.2f, 0.6f, 0.6f);   // Teal

static Math::Vector3 GetColorForNodeType(Gameplay::QuestNodeType type) {
    switch (type) {
        case Gameplay::QuestNodeType::Start:     return COLOR_START;
        case Gameplay::QuestNodeType::Objective: return COLOR_OBJECTIVE;
        case Gameplay::QuestNodeType::Condition: return COLOR_CONDITION;
        case Gameplay::QuestNodeType::Reward:    return COLOR_REWARD;
        case Gameplay::QuestNodeType::Branch:    return COLOR_BRANCH;
        case Gameplay::QuestNodeType::End:       return COLOR_END;
        case Gameplay::QuestNodeType::Delay:     return COLOR_DELAY;
        case Gameplay::QuestNodeType::Event:     return COLOR_EVENT;
        default:                                 return COLOR_DELAY;
    }
}

// ============================================================================
// Target Management
// ============================================================================

void QuestFlowEditor::SetTarget(ECS::World* world, ECS::Entity entity) {
    if (m_World == world && m_TargetEntity == entity) return;
    m_World = world;
    m_TargetEntity = entity;
    m_NeedsSync = true;
}

void QuestFlowEditor::ClearTarget() {
    m_World = nullptr;
    m_TargetEntity = ECS::INVALID_ENTITY;
    m_GraphData.Clear();
    m_NeedsSync = true;
}

// ============================================================================
// Node Creation
// ============================================================================

NodeId QuestFlowEditor::CreateQFNode(Gameplay::QuestNodeType type, Math::Vector2 position) {
    Math::Vector3 color = GetColorForNodeType(type);
    const char* title = Gameplay::QuestNodeTypeToString(type);

    NodeId nodeId = m_GraphData.AddNode(title, position, color);
    auto* node = m_GraphData.FindNode(nodeId);
    if (!node) return 0;

    // Set flags for Start
    if (type == Gameplay::QuestNodeType::Start) {
        node->flags = NodeFlags::NoDelete;
    }

    // Create pins based on node type
    switch (type) {
        case Gameplay::QuestNodeType::Start:
            m_GraphData.AddPin(nodeId, "Next", PinType::Flow, PinKind::Output);
            break;

        case Gameplay::QuestNodeType::Objective:
            m_GraphData.AddPin(nodeId, "In", PinType::Flow, PinKind::Input);
            m_GraphData.AddPin(nodeId, "Done", PinType::Flow, PinKind::Output);
            break;

        case Gameplay::QuestNodeType::Condition:
            m_GraphData.AddPin(nodeId, "In", PinType::Flow, PinKind::Input);
            m_GraphData.AddPin(nodeId, "True", PinType::Flow, PinKind::Output);
            m_GraphData.AddPin(nodeId, "False", PinType::Flow, PinKind::Output);
            break;

        case Gameplay::QuestNodeType::Reward:
            m_GraphData.AddPin(nodeId, "In", PinType::Flow, PinKind::Input);
            m_GraphData.AddPin(nodeId, "Next", PinType::Flow, PinKind::Output);
            break;

        case Gameplay::QuestNodeType::Branch:
            m_GraphData.AddPin(nodeId, "In", PinType::Flow, PinKind::Input);
            m_GraphData.AddPin(nodeId, "True", PinType::Flow, PinKind::Output);
            m_GraphData.AddPin(nodeId, "False", PinType::Flow, PinKind::Output);
            break;

        case Gameplay::QuestNodeType::End:
            m_GraphData.AddPin(nodeId, "In", PinType::Flow, PinKind::Input);
            break;

        case Gameplay::QuestNodeType::Delay:
            m_GraphData.AddPin(nodeId, "In", PinType::Flow, PinKind::Input);
            m_GraphData.AddPin(nodeId, "Next", PinType::Flow, PinKind::Output);
            break;

        case Gameplay::QuestNodeType::Event:
            m_GraphData.AddPin(nodeId, "In", PinType::Flow, PinKind::Input);
            m_GraphData.AddPin(nodeId, "Next", PinType::Flow, PinKind::Output);
            break;

        default:
            break;
    }

    // Create meta with default properties
    Gameplay::QuestNodeMeta meta;
    meta.nodeType = type;

    switch (type) {
        case Gameplay::QuestNodeType::Objective:
            meta.properties["description"] = "";
            meta.properties["objectiveType"] = "custom";
            meta.properties["targetCount"] = "1";
            meta.properties["targetTag"] = "";
            break;
        case Gameplay::QuestNodeType::Condition:
            meta.properties["conditionType"] = "custom";
            meta.properties["key"] = "";
            meta.properties["operator"] = "==";
            meta.properties["value"] = "";
            break;
        case Gameplay::QuestNodeType::Reward:
            meta.properties["rewardType"] = "xp";
            meta.properties["amount"] = "100";
            meta.properties["itemId"] = "";
            break;
        case Gameplay::QuestNodeType::Branch:
            meta.properties["conditionType"] = "custom";
            meta.properties["key"] = "";
            meta.properties["operator"] = "==";
            meta.properties["value"] = "";
            break;
        case Gameplay::QuestNodeType::End:
            meta.properties["endStatus"] = "completed";
            break;
        case Gameplay::QuestNodeType::Delay:
            meta.properties["duration"] = "1.0";
            break;
        case Gameplay::QuestNodeType::Event:
            meta.properties["eventName"] = "";
            meta.properties["eventData"] = "";
            break;
        default:
            break;
    }

    // Store meta in component
    if (m_World && m_TargetEntity != ECS::INVALID_ENTITY) {
        auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(m_TargetEntity);
        if (qf) {
            qf->nodeMeta[nodeId] = meta;
        }
    }

    return nodeId;
}

// ============================================================================
// Sync From Component
// ============================================================================

void QuestFlowEditor::SyncFromComponent() {
    m_GraphData.Clear();

    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(m_TargetEntity);
    if (!qf) return;

    // If the component has an existing graph, load it
    if (!qf->graph.GetNodes().empty()) {
        auto json = qf->graph.ToJson();
        m_GraphData.FromJson(json);
    } else {
        // Create default Start node
        NodeId startId = CreateQFNode(Gameplay::QuestNodeType::Start, Math::Vector2(300, 50));
        qf->startNodeId = startId;
        qf->graph = m_GraphData;
    }

    // Sync quest info buffers
    strncpy(m_QuestIdBuf, qf->questId.c_str(), sizeof(m_QuestIdBuf) - 1);
    m_QuestIdBuf[sizeof(m_QuestIdBuf) - 1] = '\0';
    strncpy(m_QuestTitleBuf, qf->questTitle.c_str(), sizeof(m_QuestTitleBuf) - 1);
    m_QuestTitleBuf[sizeof(m_QuestTitleBuf) - 1] = '\0';
    strncpy(m_QuestDescBuf, qf->questDescription.c_str(), sizeof(m_QuestDescBuf) - 1);
    m_QuestDescBuf[sizeof(m_QuestDescBuf) - 1] = '\0';

    m_NeedsSync = false;
}

// ============================================================================
// Sync To Component
// ============================================================================

void QuestFlowEditor::SyncToComponent() {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(m_TargetEntity);
    if (!qf) return;

    auto json = m_GraphData.ToJson();
    qf->graph.FromJson(json);
}

// ============================================================================
// Callbacks
// ============================================================================

void QuestFlowEditor::SetupCallbacks() {
    m_Callbacks = {};

    // Validation
    m_Callbacks.CanCreateLink = [this](const Pin& from, const Pin& to) -> bool {
        // Flow->Flow only
        if (from.type != PinType::Flow || to.type != PinType::Flow) return false;
        // Output->Input only
        if (from.kind != PinKind::Output || to.kind != PinKind::Input) return false;
        // No self-links
        if (from.nodeId == to.nodeId) return false;

        // Each input pin: max 1 incoming link
        auto existingLinks = m_GraphData.GetLinksForPin(to.id);
        if (!existingLinks.empty()) return false;

        // Start node: max 1 outgoing link from its output
        if (m_World && m_TargetEntity != ECS::INVALID_ENTITY) {
            auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(m_TargetEntity);
            if (qf) {
                auto metaIt = qf->nodeMeta.find(from.nodeId);
                if (metaIt != qf->nodeMeta.end()) {
                    if (metaIt->second.nodeType == Gameplay::QuestNodeType::Start) {
                        auto outLinks = m_GraphData.GetLinksForPin(from.id);
                        if (!outLinks.empty()) return false;
                    }
                }
            }
        }

        return true;
    };

    // Context menu categories
    m_Callbacks.contextMenuCategories.clear();

    // Flow
    {
        ContextMenuCategory cat;
        cat.name = "Flow";
        cat.items.push_back({"Start", [this](Math::Vector2 pos) { CreateQFNode(Gameplay::QuestNodeType::Start, pos); SyncToComponent(); }});
        cat.items.push_back({"End", [this](Math::Vector2 pos) { CreateQFNode(Gameplay::QuestNodeType::End, pos); SyncToComponent(); }});
        cat.items.push_back({"Delay", [this](Math::Vector2 pos) { CreateQFNode(Gameplay::QuestNodeType::Delay, pos); SyncToComponent(); }});
        cat.items.push_back({"Event", [this](Math::Vector2 pos) { CreateQFNode(Gameplay::QuestNodeType::Event, pos); SyncToComponent(); }});
        m_Callbacks.contextMenuCategories.push_back(cat);
    }

    // Objectives
    {
        ContextMenuCategory cat;
        cat.name = "Objectives";
        cat.items.push_back({"Objective", [this](Math::Vector2 pos) { CreateQFNode(Gameplay::QuestNodeType::Objective, pos); SyncToComponent(); }});
        m_Callbacks.contextMenuCategories.push_back(cat);
    }

    // Logic
    {
        ContextMenuCategory cat;
        cat.name = "Logic";
        cat.items.push_back({"Condition", [this](Math::Vector2 pos) { CreateQFNode(Gameplay::QuestNodeType::Condition, pos); SyncToComponent(); }});
        cat.items.push_back({"Branch", [this](Math::Vector2 pos) { CreateQFNode(Gameplay::QuestNodeType::Branch, pos); SyncToComponent(); }});
        cat.items.push_back({"Reward", [this](Math::Vector2 pos) { CreateQFNode(Gameplay::QuestNodeType::Reward, pos); SyncToComponent(); }});
        m_Callbacks.contextMenuCategories.push_back(cat);
    }

    // Custom node body rendering
    m_Callbacks.DrawNodeContent = [this](const GraphNode& node, ImDrawList* dl,
                                         ImVec2 bodyMin, ImVec2 bodyMax, f32 zoom) {
        if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

        auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(m_TargetEntity);
        if (!qf) return;

        auto metaIt = qf->nodeMeta.find(node.id);
        if (metaIt == qf->nodeMeta.end()) return;

        const auto& meta = metaIt->second;

        // Node type sublabel
        const char* typeStr = Gameplay::QuestNodeTypeToString(meta.nodeType);
        char label[32];
        snprintf(label, sizeof(label), "[%s]", typeStr);

        f32 fontSize = 11.0f * zoom;
        ImVec2 textSize = ImGui::CalcTextSize(label);
        f32 x = bodyMin.x + (bodyMax.x - bodyMin.x - textSize.x * zoom) * 0.5f;
        f32 y = bodyMin.y + 2.0f * zoom;
        dl->AddText(nullptr, fontSize, ImVec2(x, y), 0x80FFFFFF, label);

        // Key property preview
        f32 propY = y + fontSize + 2.0f * zoom;
        const char* previewKey = nullptr;
        switch (meta.nodeType) {
            case Gameplay::QuestNodeType::Objective: previewKey = "description"; break;
            case Gameplay::QuestNodeType::Condition: previewKey = "key"; break;
            case Gameplay::QuestNodeType::Branch:    previewKey = "key"; break;
            case Gameplay::QuestNodeType::Reward:    previewKey = "amount"; break;
            case Gameplay::QuestNodeType::Delay:     previewKey = "duration"; break;
            case Gameplay::QuestNodeType::Event:     previewKey = "eventName"; break;
            case Gameplay::QuestNodeType::End:       previewKey = "endStatus"; break;
            default: break;
        }
        if (previewKey) {
            auto propIt = meta.properties.find(previewKey);
            if (propIt != meta.properties.end() && !propIt->second.empty()) {
                const std::string& val = propIt->second;
                // Truncate for display
                char propLabel[48];
                if (val.size() > 24) {
                    snprintf(propLabel, sizeof(propLabel), "%.21s...", val.c_str());
                } else {
                    snprintf(propLabel, sizeof(propLabel), "%s", val.c_str());
                }
                f32 propFontSize = 10.0f * zoom;
                dl->AddText(nullptr, propFontSize, ImVec2(bodyMin.x + 4.0f * zoom, propY),
                    0x60FFFFFF, propLabel);
            }
        }

        // Status indicator during play mode
        bool isActive = qf->activeNodes.find(node.id) != qf->activeNodes.end();
        bool isCompleted = qf->completedNodes.find(node.id) != qf->completedNodes.end();
        if (isActive || isCompleted || meta.reached) {
            u32 statusColor = 0xFF808080;  // grey = unreached
            if (isActive) statusColor = 0xFF00CCFF;       // Yellow (BGR)
            else if (isCompleted) statusColor = 0xFF00CC00; // Green
            f32 dotRadius = 4.0f * zoom;
            ImVec2 dotCenter(bodyMax.x - 8.0f * zoom, bodyMin.y + 8.0f * zoom);
            dl->AddCircleFilled(dotCenter, dotRadius, statusColor);
        }
    };

    // On node deleted -- clean up meta
    m_Callbacks.OnNodeDeleted = [this](NodeId nodeId) {
        if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

        auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(m_TargetEntity);
        if (qf) {
            qf->nodeMeta.erase(nodeId);
            if (qf->startNodeId == nodeId) {
                qf->startNodeId = 0;
            }
        }
        SyncToComponent();
    };

    // On link created/deleted -- sync back
    m_Callbacks.OnLinkCreated = [this](LinkId) { SyncToComponent(); };
    m_Callbacks.OnLinkDeleted = [this](LinkId) { SyncToComponent(); };
}

// ============================================================================
// Render
// ============================================================================

void QuestFlowEditor::Render(const EditorSettings& settings, bool isPlaying) {
    if (!HasTarget()) {
        ImGui::TextDisabled("Select an entity with a Quest Flow component");
        return;
    }

    auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(m_TargetEntity);
    if (!qf) {
        ImGui::TextDisabled("Selected entity has no Quest Flow component");
        return;
    }

    if (m_NeedsSync) {
        SyncFromComponent();
        SetupCallbacks();
        m_Colors = NodeGraphColors::FromTheme(settings);
    }

    if (isPlaying) {
        UpdatePlayModeHighlight();
    }

    DrawToolbar();

    // Layout: canvas on left, inspector on right
    f32 inspectorWidth = 280.0f * settings.uiScale;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    f32 canvasWidth = avail.x - inspectorWidth - 8.0f;
    if (canvasWidth < 200.0f) canvasWidth = avail.x;

    // Canvas
    ImGui::BeginChild("##QFCanvas", ImVec2(canvasWidth, avail.y), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_GraphEditor.Render(m_GraphData, m_Callbacks, m_Colors, settings.uiScale);
    ImGui::EndChild();

    SyncToComponent();

    // Inspector sidebar
    if (canvasWidth < avail.x) {
        ImGui::SameLine();
        ImGui::BeginChild("##QFInspector", ImVec2(inspectorWidth, avail.y), true);
        DrawInspector(isPlaying);
        ImGui::EndChild();
    }
}

// ============================================================================
// Toolbar
// ============================================================================

void QuestFlowEditor::DrawToolbar() {
    if (ImGui::Button("Auto Layout")) {
        AutoLayout();
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit All")) {
        m_GraphEditor.FitAllNodes(m_GraphData);
    }
    ImGui::Separator();
}

// ============================================================================
// Inspector
// ============================================================================

void QuestFlowEditor::DrawInspector(bool isPlaying) {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(m_TargetEntity);
    if (!qf) return;

    // Quest info
    ImGui::Text("Quest Info");
    ImGui::Separator();

    if (ImGui::InputText("Quest ID##qf", m_QuestIdBuf, sizeof(m_QuestIdBuf))) {
        qf->questId = m_QuestIdBuf;
    }
    if (ImGui::InputText("Title##qf", m_QuestTitleBuf, sizeof(m_QuestTitleBuf))) {
        qf->questTitle = m_QuestTitleBuf;
    }
    if (ImGui::InputTextMultiline("Description##qf", m_QuestDescBuf, sizeof(m_QuestDescBuf),
            ImVec2(-1, 60))) {
        qf->questDescription = m_QuestDescBuf;
    }

    if (isPlaying) {
        ImGui::Separator();
        ImGui::Text("Status: %s", Gameplay::QuestFlowStatusToString(qf->status));
        ImGui::Text("Active Nodes: %zu", qf->activeNodes.size());
        ImGui::Text("Completed: %zu", qf->completedNodes.size());
    }

    ImGui::Separator();

    // Selected node properties
    NodeId selectedId = m_GraphEditor.GetSelectedNodeId();
    if (selectedId != 0) {
        auto metaIt = qf->nodeMeta.find(selectedId);
        if (metaIt != qf->nodeMeta.end()) {
            auto& meta = metaIt->second;
            ImGui::Text("Node: %s", Gameplay::QuestNodeTypeToString(meta.nodeType));
            ImGui::Text("Category: %s",
                Gameplay::QuestNodeCategoryToString(Gameplay::GetQuestNodeCategory(meta.nodeType)));

            ImGui::Separator();
            ImGui::Text("Properties");

            // Edit properties
            for (auto& [key, value] : meta.properties) {
                char buf[256];
                strncpy(buf, value.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                char label[128];
                snprintf(label, sizeof(label), "##qfprop_%s", key.c_str());
                ImGui::Text("%s", key.c_str());
                ImGui::SameLine(100);
                if (ImGui::InputText(label, buf, sizeof(buf))) {
                    meta.properties[key] = buf;
                }
            }

            // Objective progress during play
            if (isPlaying && meta.nodeType == Gameplay::QuestNodeType::Objective) {
                ImGui::Separator();
                i32 count = qf->nodeCounters[selectedId];
                auto tcIt = meta.properties.find("targetCount");
                std::string target = (tcIt != meta.properties.end()) ? tcIt->second : "1";
                ImGui::Text("Progress: %d / %s", count, target.c_str());
            }
        }
    }
}

// ============================================================================
// Auto Layout
// ============================================================================

void QuestFlowEditor::AutoLayout() {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(m_TargetEntity);
    if (!qf) return;

    auto& nodes = m_GraphData.GetNodes();
    if (nodes.empty()) return;

    // Find start node
    NodeId startId = qf->startNodeId;
    if (startId == 0 && !nodes.empty()) startId = nodes[0].id;

    struct LayoutInfo {
        NodeId id;
        i32 depth;
        i32 index;
    };

    std::vector<LayoutInfo> queue;
    std::unordered_map<NodeId, bool> visited;
    queue.push_back({startId, 0, 0});
    visited[startId] = true;

    std::unordered_map<i32, i32> depthCount;

    usize front = 0;
    while (front < queue.size()) {
        auto current = queue[front++];

        auto* node = m_GraphData.FindNode(current.id);
        if (!node) continue;

        for (const auto& outPin : node->outputs) {
            auto links = m_GraphData.GetLinksForPin(outPin.id);
            for (auto linkId : links) {
                auto* link = m_GraphData.FindLink(linkId);
                if (!link) continue;

                PinId childPinId = (link->startPinId == outPin.id) ? link->endPinId : link->startPinId;
                NodeId childNodeId = m_GraphData.GetPinOwner(childPinId);
                if (childNodeId != 0 && !visited[childNodeId]) {
                    visited[childNodeId] = true;
                    i32 childDepth = current.depth + 1;
                    i32 childIdx = depthCount[childDepth]++;
                    queue.push_back({childNodeId, childDepth, childIdx});
                }
            }
        }
    }

    // Position nodes
    f32 xSpacing = 220.0f;
    f32 ySpacing = 120.0f;

    for (const auto& info : queue) {
        auto* node = m_GraphData.FindNode(info.id);
        if (node) {
            i32 countAtDepth = depthCount[info.depth];
            f32 totalWidth = static_cast<f32>(countAtDepth - 1) * xSpacing;
            f32 startX = 300.0f - totalWidth * 0.5f;
            node->position = Math::Vector2(
                startX + static_cast<f32>(info.index) * xSpacing,
                50.0f + static_cast<f32>(info.depth) * ySpacing
            );
        }
    }

    // Place unvisited nodes below
    f32 orphanY = 50.0f + static_cast<f32>(depthCount.size()) * ySpacing + 50.0f;
    i32 orphanIdx = 0;
    for (auto& node : nodes) {
        if (!visited[node.id]) {
            node.position = Math::Vector2(
                50.0f + static_cast<f32>(orphanIdx) * xSpacing,
                orphanY
            );
            orphanIdx++;
        }
    }

    SyncToComponent();
}

// ============================================================================
// Play Mode Highlight
// ============================================================================

void QuestFlowEditor::UpdatePlayModeHighlight() {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(m_TargetEntity);
    if (!qf) return;

    // Clear all highlights
    for (auto& node : m_GraphData.GetNodes()) {
        node.flags = static_cast<NodeFlags>(
            static_cast<u32>(node.flags) & ~static_cast<u32>(NodeFlags::Highlighted)
        );
        // Re-apply NoDelete for start
        if (node.id == qf->startNodeId) {
            node.flags = node.flags | NodeFlags::NoDelete;
        }
    }

    // Highlight active nodes
    for (auto activeId : qf->activeNodes) {
        auto* node = m_GraphData.FindNode(activeId);
        if (node) {
            node->flags = node->flags | NodeFlags::Highlighted;
        }
    }
}

} // namespace Editor
} // namespace Enjin
