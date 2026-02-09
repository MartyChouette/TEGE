#include "Enjin/Editor/BehaviorTreeEditor.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/ECS/Components/Name.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Enjin {
namespace Editor {

// Header colors by category
static const Math::Vector3 COLOR_ROOT      = Math::Vector3(0.4f, 0.4f, 0.4f);
static const Math::Vector3 COLOR_COMPOSITE = Math::Vector3(0.2f, 0.5f, 0.7f);
static const Math::Vector3 COLOR_DECORATOR = Math::Vector3(0.6f, 0.4f, 0.7f);
static const Math::Vector3 COLOR_ACTION    = Math::Vector3(0.3f, 0.6f, 0.3f);
static const Math::Vector3 COLOR_CONDITION = Math::Vector3(0.7f, 0.5f, 0.2f);

static Math::Vector3 GetColorForCategory(AI::BTNodeCategory cat) {
    switch (cat) {
        case AI::BTNodeCategory::Root:       return COLOR_ROOT;
        case AI::BTNodeCategory::Composite:  return COLOR_COMPOSITE;
        case AI::BTNodeCategory::Decorator:  return COLOR_DECORATOR;
        case AI::BTNodeCategory::Action:     return COLOR_ACTION;
        case AI::BTNodeCategory::Condition:  return COLOR_CONDITION;
        default:                             return COLOR_ROOT;
    }
}

// ============================================================================
// Target Management
// ============================================================================

void BehaviorTreeEditor::SetTarget(ECS::World* world, ECS::Entity entity) {
    if (m_World == world && m_TargetEntity == entity) return;
    m_World = world;
    m_TargetEntity = entity;
    m_NeedsSync = true;
}

void BehaviorTreeEditor::ClearTarget() {
    m_World = nullptr;
    m_TargetEntity = ECS::INVALID_ENTITY;
    m_GraphData.Clear();
    m_NeedsSync = true;
}

// ============================================================================
// Node Creation
// ============================================================================

NodeId BehaviorTreeEditor::CreateBTNode(AI::BTNodeType type, Math::Vector2 position) {
    AI::BTNodeCategory category = AI::GetNodeCategory(type);
    Math::Vector3 color = GetColorForCategory(category);
    const char* title = AI::BTNodeTypeToString(type);

    NodeId nodeId = m_GraphData.AddNode(title, position, color);
    auto* node = m_GraphData.FindNode(nodeId);
    if (!node) return 0;

    // Set flags for Root
    if (type == AI::BTNodeType::Root) {
        node->flags = NodeFlags::NoDelete;
    }

    // Create pins based on category
    switch (category) {
        case AI::BTNodeCategory::Root:
            // Only output (1 child)
            m_GraphData.AddPin(nodeId, "Child", PinType::Flow, PinKind::Output);
            break;

        case AI::BTNodeCategory::Composite:
            m_GraphData.AddPin(nodeId, "Parent", PinType::Flow, PinKind::Input);
            m_GraphData.AddPin(nodeId, "Children", PinType::Flow, PinKind::Output);
            break;

        case AI::BTNodeCategory::Decorator:
            m_GraphData.AddPin(nodeId, "Parent", PinType::Flow, PinKind::Input);
            m_GraphData.AddPin(nodeId, "Child", PinType::Flow, PinKind::Output);
            break;

        case AI::BTNodeCategory::Action:
        case AI::BTNodeCategory::Condition:
            m_GraphData.AddPin(nodeId, "Parent", PinType::Flow, PinKind::Input);
            break;
    }

    // Create meta
    AI::BTNodeMeta meta;
    meta.nodeType = type;

    // Set default properties for specific types
    switch (type) {
        case AI::BTNodeType::Repeater:   meta.properties["count"] = "3"; break;
        case AI::BTNodeType::Timeout:    meta.properties["duration"] = "5.0"; break;
        case AI::BTNodeType::Wait:       meta.properties["duration"] = "1.0"; break;
        case AI::BTNodeType::IsInRange:  meta.properties["range"] = "5.0"; break;
        case AI::BTNodeType::Parallel:   meta.properties["policy"] = "RequireOne"; break;
        case AI::BTNodeType::CheckVariable:
            meta.properties["key"] = "";
            meta.properties["operator"] = "==";
            meta.properties["value"] = "";
            break;
        case AI::BTNodeType::SetVariable:
            meta.properties["key"] = "";
            meta.properties["value"] = "";
            meta.properties["type"] = "string";
            break;
        case AI::BTNodeType::PlayAnimation:
            meta.properties["animation"] = "";
            break;
        default: break;
    }

    // Store meta in component
    if (m_World && m_TargetEntity != ECS::INVALID_ENTITY) {
        auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(m_TargetEntity);
        if (bt) {
            bt->nodeMeta[nodeId] = meta;
        }
    }

    return nodeId;
}

// ============================================================================
// Sync From Component
// ============================================================================

void BehaviorTreeEditor::SyncFromComponent() {
    m_GraphData.Clear();

    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(m_TargetEntity);
    if (!bt) return;

    // If the component has an existing graph, load it
    if (!bt->graph.GetNodes().empty()) {
        // Copy graph data from component
        auto json = bt->graph.ToJson();
        m_GraphData.FromJson(json);
    } else {
        // Create default Root node
        NodeId rootId = CreateBTNode(AI::BTNodeType::Root, Math::Vector2(300, 50));
        bt->rootNodeId = rootId;
        bt->graph = m_GraphData;
    }

    m_NeedsSync = false;
}

// ============================================================================
// Sync To Component
// ============================================================================

void BehaviorTreeEditor::SyncToComponent() {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(m_TargetEntity);
    if (!bt) return;

    // Write graph data back
    auto json = m_GraphData.ToJson();
    bt->graph.FromJson(json);
}

// ============================================================================
// Callbacks
// ============================================================================

void BehaviorTreeEditor::SetupCallbacks() {
    m_Callbacks = {};

    // Validation
    m_Callbacks.CanCreateLink = [this](const Pin& from, const Pin& to) -> bool {
        // Flow->Flow only
        if (from.type != PinType::Flow || to.type != PinType::Flow) return false;
        // Output->Input only
        if (from.kind != PinKind::Output || to.kind != PinKind::Input) return false;
        // No self-links
        if (from.nodeId == to.nodeId) return false;

        // Each node can have at most 1 parent (1 incoming link)
        auto existingLinks = m_GraphData.GetLinksForPin(to.id);
        if (!existingLinks.empty()) return false;

        // Root and Decorators can have at most 1 child (1 outgoing link)
        if (m_World && m_TargetEntity != ECS::INVALID_ENTITY) {
            auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(m_TargetEntity);
            if (bt) {
                auto metaIt = bt->nodeMeta.find(from.nodeId);
                if (metaIt != bt->nodeMeta.end()) {
                    auto cat = AI::GetNodeCategory(metaIt->second.nodeType);
                    if (cat == AI::BTNodeCategory::Root || cat == AI::BTNodeCategory::Decorator) {
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

    // Composites
    {
        ContextMenuCategory cat;
        cat.name = "Composites";
        cat.items.push_back({"Selector", [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::Selector, pos); SyncToComponent(); }});
        cat.items.push_back({"Sequence", [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::Sequence, pos); SyncToComponent(); }});
        cat.items.push_back({"Parallel", [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::Parallel, pos); SyncToComponent(); }});
        m_Callbacks.contextMenuCategories.push_back(cat);
    }

    // Decorators
    {
        ContextMenuCategory cat;
        cat.name = "Decorators";
        cat.items.push_back({"Inverter",      [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::Inverter, pos); SyncToComponent(); }});
        cat.items.push_back({"Repeater",      [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::Repeater, pos); SyncToComponent(); }});
        cat.items.push_back({"Timeout",       [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::Timeout, pos); SyncToComponent(); }});
        cat.items.push_back({"Force Success", [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::ForceSuccess, pos); SyncToComponent(); }});
        cat.items.push_back({"Force Failure", [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::ForceFailure, pos); SyncToComponent(); }});
        m_Callbacks.contextMenuCategories.push_back(cat);
    }

    // Actions
    {
        ContextMenuCategory cat;
        cat.name = "Actions";
        cat.items.push_back({"Move To",        [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::MoveTo, pos); SyncToComponent(); }});
        cat.items.push_back({"Attack",         [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::Attack, pos); SyncToComponent(); }});
        cat.items.push_back({"Patrol",         [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::Patrol, pos); SyncToComponent(); }});
        cat.items.push_back({"Wait",           [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::Wait, pos); SyncToComponent(); }});
        cat.items.push_back({"Play Animation", [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::PlayAnimation, pos); SyncToComponent(); }});
        cat.items.push_back({"Set Variable",   [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::SetVariable, pos); SyncToComponent(); }});
        m_Callbacks.contextMenuCategories.push_back(cat);
    }

    // Conditions
    {
        ContextMenuCategory cat;
        cat.name = "Conditions";
        cat.items.push_back({"Can See Target", [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::CanSeeTarget, pos); SyncToComponent(); }});
        cat.items.push_back({"Is In Range",    [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::IsInRange, pos); SyncToComponent(); }});
        cat.items.push_back({"Has Target",     [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::HasTarget, pos); SyncToComponent(); }});
        cat.items.push_back({"Check Variable", [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::CheckVariable, pos); SyncToComponent(); }});
        cat.items.push_back({"Is Alive",       [this](Math::Vector2 pos) { CreateBTNode(AI::BTNodeType::IsAlive, pos); SyncToComponent(); }});
        m_Callbacks.contextMenuCategories.push_back(cat);
    }

    // Custom node body rendering
    m_Callbacks.DrawNodeContent = [this](const GraphNode& node, ImDrawList* dl,
                                         ImVec2 bodyMin, ImVec2 bodyMax, f32 zoom) {
        if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

        auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(m_TargetEntity);
        if (!bt) return;

        auto metaIt = bt->nodeMeta.find(node.id);
        if (metaIt == bt->nodeMeta.end()) return;

        const auto& meta = metaIt->second;
        AI::BTNodeCategory category = AI::GetNodeCategory(meta.nodeType);

        // Category sublabel
        const char* catLabel = AI::BTNodeCategoryToString(category);
        char label[32];
        snprintf(label, sizeof(label), "[%s]", catLabel);

        f32 fontSize = 11.0f * zoom;
        ImVec2 textSize = ImGui::CalcTextSize(label);
        f32 x = bodyMin.x + (bodyMax.x - bodyMin.x - textSize.x * zoom) * 0.5f;
        f32 y = bodyMin.y + 2.0f * zoom;
        dl->AddText(nullptr, fontSize, ImVec2(x, y), 0x80FFFFFF, label);

        // Status dot during play mode
        if (meta.lastStatus != AI::BTStatus::Invalid) {
            u32 statusColor = 0xFF808080;  // grey = invalid
            switch (meta.lastStatus) {
                case AI::BTStatus::Running: statusColor = 0xFF00CCFF; break;  // Yellow
                case AI::BTStatus::Success: statusColor = 0xFF00CC00; break;  // Green
                case AI::BTStatus::Failure: statusColor = 0xFF0000CC; break;  // Red
                default: break;
            }
            f32 dotRadius = 4.0f * zoom;
            ImVec2 dotCenter(bodyMax.x - 8.0f * zoom, bodyMin.y + 8.0f * zoom);
            dl->AddCircleFilled(dotCenter, dotRadius, statusColor);
        }
    };

    // On node deleted — clean up meta
    m_Callbacks.OnNodeDeleted = [this](NodeId nodeId) {
        if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

        auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(m_TargetEntity);
        if (bt) {
            bt->nodeMeta.erase(nodeId);
            if (bt->rootNodeId == nodeId) {
                bt->rootNodeId = 0;
            }
        }
        SyncToComponent();
    };

    // On link created — sync back
    m_Callbacks.OnLinkCreated = [this](LinkId) { SyncToComponent(); };
    m_Callbacks.OnLinkDeleted = [this](LinkId) { SyncToComponent(); };
}

// ============================================================================
// Render
// ============================================================================

void BehaviorTreeEditor::Render(const EditorSettings& settings, bool isPlaying) {
    if (!HasTarget()) {
        ImGui::TextDisabled("Select an entity with a Behavior Tree component");
        return;
    }

    auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(m_TargetEntity);
    if (!bt) {
        ImGui::TextDisabled("Selected entity has no Behavior Tree component");
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
    f32 inspectorWidth = 260.0f * settings.uiScale;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    f32 canvasWidth = avail.x - inspectorWidth - 8.0f;
    if (canvasWidth < 200.0f) canvasWidth = avail.x;

    // Canvas
    ImGui::BeginChild("##BTCanvas", ImVec2(canvasWidth, avail.y), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_GraphEditor.Render(m_GraphData, m_Callbacks, m_Colors, settings.uiScale);
    ImGui::EndChild();

    SyncToComponent();

    // Inspector sidebar
    if (canvasWidth < avail.x) {
        ImGui::SameLine();
        ImGui::BeginChild("##BTInspector", ImVec2(inspectorWidth, avail.y), true);
        DrawInspector(isPlaying);
        ImGui::EndChild();
    }
}

// ============================================================================
// Toolbar
// ============================================================================

void BehaviorTreeEditor::DrawToolbar() {
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

void BehaviorTreeEditor::DrawInspector(bool isPlaying) {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(m_TargetEntity);
    if (!bt) return;

    // Selected node properties
    NodeId selectedId = m_GraphEditor.GetSelectedNodeId();
    if (selectedId != 0) {
        auto metaIt = bt->nodeMeta.find(selectedId);
        if (metaIt != bt->nodeMeta.end()) {
            auto& meta = metaIt->second;
            ImGui::Text("Node: %s", AI::BTNodeTypeToString(meta.nodeType));
            ImGui::Text("Category: %s", AI::BTNodeCategoryToString(AI::GetNodeCategory(meta.nodeType)));

            if (isPlaying) {
                ImGui::Text("Status: %s", AI::BTStatusToString(meta.lastStatus));
            }

            ImGui::Separator();
            ImGui::Text("Properties");

            // Edit properties
            for (auto& [key, value] : meta.properties) {
                char buf[256];
                strncpy(buf, value.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                char label[128];
                snprintf(label, sizeof(label), "##prop_%s", key.c_str());
                ImGui::Text("%s", key.c_str());
                ImGui::SameLine(100);
                if (ImGui::InputText(label, buf, sizeof(buf))) {
                    meta.properties[key] = buf;
                }
            }
        }
    }

    ImGui::Separator();

    // Blackboard editor
    if (ImGui::CollapsingHeader("Blackboard", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Show default entries
        for (usize i = 0; i < bt->blackboardDefaults.size(); ) {
            auto& entry = bt->blackboardDefaults[i];
            ImGui::PushID(static_cast<int>(i));

            ImGui::Text("%s", entry.key.c_str());
            ImGui::SameLine(120);

            // Edit value based on type
            if (auto* boolVal = std::get_if<bool>(&entry.value)) {
                bool v = *boolVal;
                if (ImGui::Checkbox("##val", &v)) entry.value = v;
            } else if (auto* intVal = std::get_if<i32>(&entry.value)) {
                i32 v = *intVal;
                if (ImGui::DragInt("##val", &v)) entry.value = v;
            } else if (auto* floatVal = std::get_if<f32>(&entry.value)) {
                f32 v = *floatVal;
                if (ImGui::DragFloat("##val", &v, 0.1f)) entry.value = v;
            } else if (auto* strVal = std::get_if<std::string>(&entry.value)) {
                char buf[256];
                strncpy(buf, strVal->c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText("##val", buf, sizeof(buf))) {
                    entry.value = std::string(buf);
                }
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                bt->blackboardDefaults.erase(bt->blackboardDefaults.begin() + i);
                ImGui::PopID();
                continue;
            }

            ImGui::PopID();
            i++;
        }

        // Add new blackboard entry
        ImGui::Separator();
        ImGui::InputText("Key##newbb", m_NewBBKey, sizeof(m_NewBBKey));
        ImGui::Combo("Type##newbb", &m_NewBBType, "Bool\0Int\0Float\0String\0");
        if (ImGui::Button("Add Variable") && m_NewBBKey[0] != '\0') {
            AI::BlackboardEntry entry;
            entry.key = m_NewBBKey;
            switch (m_NewBBType) {
                case 0: entry.value = false; break;
                case 1: entry.value = static_cast<i32>(0); break;
                case 2: entry.value = 0.0f; break;
                case 3: entry.value = std::string(""); break;
            }
            bt->blackboardDefaults.push_back(entry);
            m_NewBBKey[0] = '\0';
        }
    }

    // Runtime blackboard (during play)
    if (isPlaying && ImGui::CollapsingHeader("Runtime Blackboard")) {
        for (const auto& [key, val] : bt->blackboard.GetAll()) {
            ImGui::Text("%s: ", key.c_str());
            ImGui::SameLine();
            if (auto* b = std::get_if<bool>(&val)) ImGui::Text("%s", *b ? "true" : "false");
            else if (auto* i = std::get_if<i32>(&val)) ImGui::Text("%d", *i);
            else if (auto* f = std::get_if<f32>(&val)) ImGui::Text("%.2f", *f);
            else if (auto* s = std::get_if<std::string>(&val)) ImGui::Text("\"%s\"", s->c_str());
            else if (auto* v = std::get_if<Math::Vector3>(&val)) ImGui::Text("(%.1f, %.1f, %.1f)", v->x, v->y, v->z);
            else ImGui::Text("(entity)");
        }
    }
}

// ============================================================================
// Auto Layout
// ============================================================================

void BehaviorTreeEditor::AutoLayout() {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(m_TargetEntity);
    if (!bt) return;

    // Simple tree layout: BFS from root, position by depth
    auto& nodes = m_GraphData.GetNodes();
    if (nodes.empty()) return;

    // Find root
    NodeId rootId = bt->rootNodeId;
    if (rootId == 0 && !nodes.empty()) rootId = nodes[0].id;

    struct LayoutInfo {
        NodeId id;
        i32 depth;
        i32 index;
    };

    std::vector<LayoutInfo> queue;
    std::unordered_map<NodeId, bool> visited;
    queue.push_back({rootId, 0, 0});
    visited[rootId] = true;

    std::unordered_map<i32, i32> depthCount;

    usize front = 0;
    while (front < queue.size()) {
        auto current = queue[front++];

        // Get children via output links
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

    // Place any unvisited nodes below
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

void BehaviorTreeEditor::UpdatePlayModeHighlight() {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* bt = m_World->GetComponent<ECS::BehaviorTreeComponent>(m_TargetEntity);
    if (!bt) return;

    // Clear all highlights
    for (auto& node : m_GraphData.GetNodes()) {
        node.flags = static_cast<NodeFlags>(
            static_cast<u32>(node.flags) & ~static_cast<u32>(NodeFlags::Highlighted)
        );
        // Re-apply NoDelete for root
        if (node.id == bt->rootNodeId) {
            node.flags = node.flags | NodeFlags::NoDelete;
        }
    }

    // Highlight active node
    if (bt->activeNode != 0) {
        auto* node = m_GraphData.FindNode(bt->activeNode);
        if (node) {
            node->flags = node->flags | NodeFlags::Highlighted;
        }
    }
}

} // namespace Editor
} // namespace Enjin
