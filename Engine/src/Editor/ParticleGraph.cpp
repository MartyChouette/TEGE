#include "Enjin/Editor/ParticleGraph.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Enjin {
namespace Editor {

// Category colors for particle node types
static const ImU32 COLOR_EMITTER     = IM_COL32(60, 140, 180, 255);  // Cyan
static const ImU32 COLOR_MODIFIER    = IM_COL32(180, 120, 50, 255);  // Orange
static const ImU32 COLOR_SUBEMITTER  = IM_COL32(140, 80, 160, 255);  // Purple
static const ImU32 COLOR_COLLISION   = IM_COL32(160, 60, 60, 255);   // Red
static const ImU32 COLOR_RENDERER    = IM_COL32(60, 160, 60, 255);   // Green
static const ImU32 COLOR_CONTROL     = IM_COL32(120, 120, 60, 255);  // Yellow-brown

static const f32 NODE_WIDTH  = 160.0f;
static const f32 NODE_HEADER = 28.0f;
static const f32 NODE_BODY   = 50.0f;
static const f32 PIN_RADIUS  = 5.0f;

static ImU32 GetParticleNodeColor(ParticleNodeType type) {
    switch (type) {
        case ParticleNodeType::PointEmitter:
        case ParticleNodeType::SphereEmitter:
        case ParticleNodeType::BoxEmitter:
        case ParticleNodeType::ConeEmitter:
        case ParticleNodeType::MeshEmitter:
            return COLOR_EMITTER;

        case ParticleNodeType::Gravity:
        case ParticleNodeType::Wind:
        case ParticleNodeType::Turbulence:
        case ParticleNodeType::Drag:
        case ParticleNodeType::Vortex:
        case ParticleNodeType::ColorOverLife:
        case ParticleNodeType::SizeOverLife:
        case ParticleNodeType::SpeedOverLife:
        case ParticleNodeType::RotationOverLife:
            return COLOR_MODIFIER;

        case ParticleNodeType::SubEmitterOnBirth:
        case ParticleNodeType::SubEmitterOnDeath:
        case ParticleNodeType::SubEmitterOnCollision:
            return COLOR_SUBEMITTER;

        case ParticleNodeType::PlaneCollider:
        case ParticleNodeType::WorldCollider:
            return COLOR_COLLISION;

        case ParticleNodeType::BillboardRenderer:
        case ParticleNodeType::MeshRenderer:
        case ParticleNodeType::TrailRenderer:
            return COLOR_RENDERER;

        case ParticleNodeType::Burst:
        case ParticleNodeType::Loop:
        case ParticleNodeType::Delay:
            return COLOR_CONTROL;

        default:
            return IM_COL32(100, 100, 100, 255);
    }
}

// ============================================================================
// Node Names
// ============================================================================

const char* ParticleGraphEditor::GetNodeName(ParticleNodeType type) {
    switch (type) {
        case ParticleNodeType::PointEmitter:          return "Point Emitter";
        case ParticleNodeType::SphereEmitter:         return "Sphere Emitter";
        case ParticleNodeType::BoxEmitter:            return "Box Emitter";
        case ParticleNodeType::ConeEmitter:           return "Cone Emitter";
        case ParticleNodeType::MeshEmitter:           return "Mesh Emitter";
        case ParticleNodeType::Gravity:               return "Gravity";
        case ParticleNodeType::Wind:                  return "Wind";
        case ParticleNodeType::Turbulence:            return "Turbulence";
        case ParticleNodeType::Drag:                  return "Drag";
        case ParticleNodeType::Vortex:                return "Vortex";
        case ParticleNodeType::ColorOverLife:         return "Color Over Life";
        case ParticleNodeType::SizeOverLife:          return "Size Over Life";
        case ParticleNodeType::SpeedOverLife:         return "Speed Over Life";
        case ParticleNodeType::RotationOverLife:      return "Rotation Over Life";
        case ParticleNodeType::SubEmitterOnBirth:     return "Sub-Emitter (Birth)";
        case ParticleNodeType::SubEmitterOnDeath:     return "Sub-Emitter (Death)";
        case ParticleNodeType::SubEmitterOnCollision: return "Sub-Emitter (Collision)";
        case ParticleNodeType::PlaneCollider:         return "Plane Collider";
        case ParticleNodeType::WorldCollider:         return "World Collider";
        case ParticleNodeType::BillboardRenderer:     return "Billboard Renderer";
        case ParticleNodeType::MeshRenderer:          return "Mesh Renderer";
        case ParticleNodeType::TrailRenderer:         return "Trail Renderer";
        case ParticleNodeType::Burst:                 return "Burst";
        case ParticleNodeType::Loop:                  return "Loop";
        case ParticleNodeType::Delay:                 return "Delay";
        default:                                      return "Unknown";
    }
}

// ============================================================================
// Graph Management
// ============================================================================

void ParticleGraphEditor::SetGraph(ParticleGraphData* graph) {
    m_Graph = graph;
    m_SelectedNodeId = 0;
}

// ============================================================================
// Render
// ============================================================================

void ParticleGraphEditor::Render() {
    if (!m_Open) return;

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Particle Graph", &m_Open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (!m_Graph) {
        ImGui::TextDisabled("No particle graph loaded");
        ImGui::End();
        return;
    }

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                m_Graph->nodes.clear();
                m_Graph->links.clear();
                m_Graph->nextNodeId = 1;
                m_Graph->nextLinkId = 1;
                m_SelectedNodeId = 0;
            }
            ImGui::MenuItem("Save", "Ctrl+S");  // TODO: implement
            ImGui::MenuItem("Load");             // TODO: implement
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Toolbar
    ImGui::Text("System: %s", m_Graph->name.c_str());
    ImGui::Separator();

    // Layout: canvas on left, inspector on right
    f32 inspectorWidth = 260.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    f32 canvasWidth = avail.x - inspectorWidth - 8.0f;
    if (canvasWidth < 300.0f) canvasWidth = avail.x;

    // Canvas area
    ImGui::BeginChild("##PGCanvas", ImVec2(canvasWidth, avail.y), true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Background grid
    f32 gridStep = 32.0f * m_Zoom;
    for (f32 x = fmodf(m_ScrollOffset.x, gridStep); x < canvasSize.x; x += gridStep) {
        drawList->AddLine(
            ImVec2(canvasPos.x + x, canvasPos.y),
            ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
            IM_COL32(50, 50, 50, 80));
    }
    for (f32 y = fmodf(m_ScrollOffset.y, gridStep); y < canvasSize.y; y += gridStep) {
        drawList->AddLine(
            ImVec2(canvasPos.x, canvasPos.y + y),
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
            IM_COL32(50, 50, 50, 80));
    }

    // Draw connections first (behind nodes)
    DrawConnections();

    // Draw nodes
    for (auto& node : m_Graph->nodes) {
        DrawNode(node);
    }

    // Handle canvas interaction
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive()) {
        // Pan with middle mouse button
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_ScrollOffset.x += delta.x;
            m_ScrollOffset.y += delta.y;
        }

        // Zoom with scroll wheel
        f32 scroll = ImGui::GetIO().MouseWheel;
        if (scroll != 0.0f) {
            m_Zoom += scroll * 0.1f;
            if (m_Zoom < 0.25f) m_Zoom = 0.25f;
            if (m_Zoom > 3.0f) m_Zoom = 3.0f;
        }

        // Right-click context menu
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("##PGContextMenu");
        }

        // Click to deselect
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_SelectedNodeId = 0;
        }
    }

    // Context menu popup
    DrawContextMenu();

    ImGui::EndChild();

    // Inspector sidebar
    if (canvasWidth < avail.x) {
        ImGui::SameLine();
        ImGui::BeginChild("##PGInspector", ImVec2(inspectorWidth, avail.y), true);
        DrawInspector();
        ImGui::EndChild();
    }

    ImGui::End();
}

// ============================================================================
// Draw Node
// ============================================================================

void ParticleGraphEditor::DrawNode(ParticleGraphNode& node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    f32 nodeW = NODE_WIDTH * m_Zoom;
    f32 headerH = NODE_HEADER * m_Zoom;
    f32 bodyH = NODE_BODY * m_Zoom;
    f32 pinR = PIN_RADIUS * m_Zoom;

    ImVec2 nodePos(
        canvasPos.x + (node.position.x + m_ScrollOffset.x) * m_Zoom,
        canvasPos.y + (node.position.y + m_ScrollOffset.y) * m_Zoom
    );

    ImVec2 nodeEnd(nodePos.x + nodeW, nodePos.y + headerH + bodyH);

    ImU32 headerColor = GetParticleNodeColor(node.type);
    ImU32 bodyColor = IM_COL32(40, 40, 40, 230);
    ImU32 borderColor = (node.id == m_SelectedNodeId)
        ? IM_COL32(255, 200, 50, 255)
        : IM_COL32(80, 80, 80, 255);

    // Node body
    drawList->AddRectFilled(nodePos, nodeEnd, bodyColor, 6.0f * m_Zoom);

    // Node header
    drawList->AddRectFilled(
        nodePos,
        ImVec2(nodeEnd.x, nodePos.y + headerH),
        headerColor, 6.0f * m_Zoom, ImDrawFlags_RoundCornersTop);

    // Border
    drawList->AddRect(nodePos, nodeEnd, borderColor, 6.0f * m_Zoom, 0, 2.0f * m_Zoom);

    // Title text
    const char* name = node.label.empty() ? GetNodeName(node.type) : node.label.c_str();
    ImVec2 textPos(nodePos.x + 8.0f * m_Zoom, nodePos.y + 5.0f * m_Zoom);
    drawList->AddText(nullptr, 13.0f * m_Zoom, textPos, IM_COL32(255, 255, 255, 255), name);

    // Input pin (left side)
    ImVec2 inputPinPos(nodePos.x, nodePos.y + headerH + bodyH * 0.5f);
    drawList->AddCircleFilled(inputPinPos, pinR, IM_COL32(180, 220, 220, 255));

    // Output pin (right side)
    ImVec2 outputPinPos(nodeEnd.x, nodePos.y + headerH + bodyH * 0.5f);
    drawList->AddCircleFilled(outputPinPos, pinR, IM_COL32(220, 180, 180, 255));

    // Invisible button for selection and dragging
    ImGui::SetCursorScreenPos(nodePos);
    char btnId[32];
    snprintf(btnId, sizeof(btnId), "##pgnode_%u", node.id);
    ImGui::InvisibleButton(btnId, ImVec2(nodeW, headerH + bodyH));

    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_SelectedNodeId = node.id;
    }

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        node.position.x += delta.x / m_Zoom;
        node.position.y += delta.y / m_Zoom;
    }
}

// ============================================================================
// Draw Connections
// ============================================================================

void ParticleGraphEditor::DrawConnections() {
    if (!m_Graph) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    for (const auto& link : m_Graph->links) {
        const ParticleGraphNode* fromNode = nullptr;
        const ParticleGraphNode* toNode = nullptr;
        for (const auto& n : m_Graph->nodes) {
            if (n.id == link.fromNode) fromNode = &n;
            if (n.id == link.toNode) toNode = &n;
        }
        if (!fromNode || !toNode) continue;

        f32 nodeW = NODE_WIDTH * m_Zoom;
        f32 headerH = NODE_HEADER * m_Zoom;
        f32 bodyH = NODE_BODY * m_Zoom;

        // Output pin position (right side of from node)
        ImVec2 p1(
            canvasPos.x + (fromNode->position.x + m_ScrollOffset.x) * m_Zoom + nodeW,
            canvasPos.y + (fromNode->position.y + m_ScrollOffset.y) * m_Zoom + headerH + bodyH * 0.5f
        );

        // Input pin position (left side of to node)
        ImVec2 p2(
            canvasPos.x + (toNode->position.x + m_ScrollOffset.x) * m_Zoom,
            canvasPos.y + (toNode->position.y + m_ScrollOffset.y) * m_Zoom + headerH + bodyH * 0.5f
        );

        // Bezier control points
        f32 tangentLen = (p2.x - p1.x) * 0.5f;
        if (tangentLen < 50.0f * m_Zoom) tangentLen = 50.0f * m_Zoom;

        ImVec2 cp1(p1.x + tangentLen, p1.y);
        ImVec2 cp2(p2.x - tangentLen, p2.y);

        // Particle links drawn in a warm cyan
        drawList->AddBezierCubic(p1, cp1, cp2, p2, IM_COL32(120, 200, 220, 200), 2.0f * m_Zoom);
    }
}

// ============================================================================
// Draw Inspector
// ============================================================================

void ParticleGraphEditor::DrawInspector() {
    ImGui::Text("Inspector");
    ImGui::Separator();

    if (!m_Graph || m_SelectedNodeId == 0) {
        ImGui::TextDisabled("Select a node to inspect");
        return;
    }

    // Find selected node
    ParticleGraphNode* selected = nullptr;
    for (auto& node : m_Graph->nodes) {
        if (node.id == m_SelectedNodeId) {
            selected = &node;
            break;
        }
    }

    if (!selected) {
        ImGui::TextDisabled("Node not found");
        return;
    }

    ImGui::Text("Type: %s", GetNodeName(selected->type));
    ImGui::Text("ID: %u", selected->id);
    ImGui::Separator();

    // Label editing
    char labelBuf[128];
    strncpy(labelBuf, selected->label.c_str(), sizeof(labelBuf) - 1);
    labelBuf[sizeof(labelBuf) - 1] = '\0';
    if (ImGui::InputText("Label", labelBuf, sizeof(labelBuf))) {
        selected->label = labelBuf;
    }

    // Type-specific properties
    switch (selected->type) {
        // Emitters
        case ParticleNodeType::PointEmitter:
        case ParticleNodeType::SphereEmitter:
        case ParticleNodeType::BoxEmitter:
        case ParticleNodeType::ConeEmitter:
        case ParticleNodeType::MeshEmitter:
            ImGui::DragFloat("Rate", &selected->rate, 0.5f, 0.0f, 10000.0f);
            ImGui::DragFloat("Lifetime", &selected->lifetime, 0.1f, 0.01f, 60.0f);
            ImGui::DragFloat("Start Speed", &selected->startSpeed, 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat3("Direction", &selected->direction.x, 0.01f);
            if (selected->type == ParticleNodeType::ConeEmitter) {
                ImGui::DragFloat("Spread", &selected->spread, 0.01f, 0.0f, 1.0f);
            }
            if (selected->type == ParticleNodeType::SphereEmitter ||
                selected->type == ParticleNodeType::BoxEmitter) {
                ImGui::DragFloat("Radius/Size", &selected->spread, 0.1f, 0.0f, 100.0f);
            }
            break;

        // Force modifiers
        case ParticleNodeType::Gravity:
        case ParticleNodeType::Wind:
        case ParticleNodeType::Vortex:
            ImGui::DragFloat("Strength", &selected->strength, 0.1f, -100.0f, 100.0f);
            ImGui::DragFloat3("Direction", &selected->direction.x, 0.01f);
            break;

        case ParticleNodeType::Turbulence:
            ImGui::DragFloat("Strength", &selected->strength, 0.1f, 0.0f, 50.0f);
            ImGui::DragFloat("Frequency", &selected->spread, 0.01f, 0.01f, 10.0f);
            break;

        case ParticleNodeType::Drag:
            ImGui::DragFloat("Drag Coefficient", &selected->strength, 0.01f, 0.0f, 10.0f);
            break;

        // Over-lifetime modifiers
        case ParticleNodeType::ColorOverLife:
        case ParticleNodeType::SizeOverLife:
        case ParticleNodeType::SpeedOverLife:
        case ParticleNodeType::RotationOverLife: {
            ImGui::Text("Curve Points: %u", static_cast<u32>(selected->curve.size()));
            for (usize i = 0; i < selected->curve.size(); i++) {
                ImGui::PushID(static_cast<int>(i));
                f32 vals[2] = { selected->curve[i].x, selected->curve[i].y };
                char ptLabel[32];
                snprintf(ptLabel, sizeof(ptLabel), "Point %u", static_cast<u32>(i));
                if (ImGui::DragFloat2(ptLabel, vals, 0.01f, 0.0f, 1.0f)) {
                    selected->curve[i].x = vals[0];
                    selected->curve[i].y = vals[1];
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    selected->curve.erase(selected->curve.begin() + i);
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add Point")) {
                f32 t = selected->curve.empty() ? 0.0f :
                    selected->curve.back().x + 0.1f;
                if (t > 1.0f) t = 1.0f;
                selected->curve.push_back(Math::Vector2(t, 1.0f));
            }
            break;
        }

        // Sub-emitters
        case ParticleNodeType::SubEmitterOnBirth:
        case ParticleNodeType::SubEmitterOnDeath:
        case ParticleNodeType::SubEmitterOnCollision:
            ImGui::DragFloat("Rate", &selected->rate, 0.5f, 0.0f, 1000.0f);
            ImGui::DragFloat("Inherit Speed", &selected->strength, 0.01f, 0.0f, 1.0f);
            break;

        // Collision
        case ParticleNodeType::PlaneCollider:
            ImGui::DragFloat3("Normal", &selected->direction.x, 0.01f);
            ImGui::DragFloat("Bounciness", &selected->strength, 0.01f, 0.0f, 1.0f);
            break;

        case ParticleNodeType::WorldCollider:
            ImGui::DragFloat("Bounciness", &selected->strength, 0.01f, 0.0f, 1.0f);
            break;

        // Renderers
        case ParticleNodeType::BillboardRenderer:
        case ParticleNodeType::MeshRenderer:
        case ParticleNodeType::TrailRenderer:
            ImGui::TextDisabled("Renderer properties (TODO)");
            break;

        // Control
        case ParticleNodeType::Burst:
            ImGui::DragFloat("Count", &selected->rate, 1.0f, 1.0f, 10000.0f);
            ImGui::DragFloat("Time", &selected->lifetime, 0.01f, 0.0f, 60.0f);
            break;

        case ParticleNodeType::Loop:
            ImGui::DragFloat("Duration", &selected->lifetime, 0.1f, 0.0f, 60.0f);
            break;

        case ParticleNodeType::Delay:
            ImGui::DragFloat("Delay (s)", &selected->lifetime, 0.01f, 0.0f, 30.0f);
            break;

        default:
            ImGui::TextDisabled("No editable properties");
            break;
    }

    ImGui::Separator();
    ImGui::Text("Position: (%.0f, %.0f)", selected->position.x, selected->position.y);

    // Delete button
    ImGui::Spacing();
    if (ImGui::Button("Delete Node")) {
        auto& links = m_Graph->links;
        links.erase(
            std::remove_if(links.begin(), links.end(),
                [this](const ParticleGraphLink& l) {
                    return l.fromNode == m_SelectedNodeId || l.toNode == m_SelectedNodeId;
                }),
            links.end());

        auto& nodes = m_Graph->nodes;
        nodes.erase(
            std::remove_if(nodes.begin(), nodes.end(),
                [this](const ParticleGraphNode& n) { return n.id == m_SelectedNodeId; }),
            nodes.end());

        m_SelectedNodeId = 0;
    }
}

// ============================================================================
// Draw Context Menu
// ============================================================================

void ParticleGraphEditor::DrawContextMenu() {
    if (!ImGui::BeginPopup("##PGContextMenu")) return;

    ImVec2 mousePos = ImGui::GetMousePosOnOpeningCurrentPopup();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    Math::Vector2 spawnPos(
        (mousePos.x - canvasPos.x) / m_Zoom - m_ScrollOffset.x,
        (mousePos.y - canvasPos.y) / m_Zoom - m_ScrollOffset.y
    );

    auto addNode = [&](ParticleNodeType type) {
        ParticleGraphNode node;
        node.id = m_Graph->nextNodeId++;
        node.type = type;
        node.position = spawnPos;
        node.label = GetNodeName(type);
        m_Graph->nodes.push_back(node);
        m_SelectedNodeId = node.id;
        ImGui::CloseCurrentPopup();
    };

    if (ImGui::BeginMenu("Emitters")) {
        if (ImGui::MenuItem("Point Emitter"))  addNode(ParticleNodeType::PointEmitter);
        if (ImGui::MenuItem("Sphere Emitter")) addNode(ParticleNodeType::SphereEmitter);
        if (ImGui::MenuItem("Box Emitter"))    addNode(ParticleNodeType::BoxEmitter);
        if (ImGui::MenuItem("Cone Emitter"))   addNode(ParticleNodeType::ConeEmitter);
        if (ImGui::MenuItem("Mesh Emitter"))   addNode(ParticleNodeType::MeshEmitter);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Modifiers")) {
        if (ImGui::MenuItem("Gravity"))           addNode(ParticleNodeType::Gravity);
        if (ImGui::MenuItem("Wind"))              addNode(ParticleNodeType::Wind);
        if (ImGui::MenuItem("Turbulence"))        addNode(ParticleNodeType::Turbulence);
        if (ImGui::MenuItem("Drag"))              addNode(ParticleNodeType::Drag);
        if (ImGui::MenuItem("Vortex"))            addNode(ParticleNodeType::Vortex);
        ImGui::Separator();
        if (ImGui::MenuItem("Color Over Life"))    addNode(ParticleNodeType::ColorOverLife);
        if (ImGui::MenuItem("Size Over Life"))     addNode(ParticleNodeType::SizeOverLife);
        if (ImGui::MenuItem("Speed Over Life"))    addNode(ParticleNodeType::SpeedOverLife);
        if (ImGui::MenuItem("Rotation Over Life")) addNode(ParticleNodeType::RotationOverLife);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Sub-Emitters")) {
        if (ImGui::MenuItem("On Birth"))     addNode(ParticleNodeType::SubEmitterOnBirth);
        if (ImGui::MenuItem("On Death"))     addNode(ParticleNodeType::SubEmitterOnDeath);
        if (ImGui::MenuItem("On Collision")) addNode(ParticleNodeType::SubEmitterOnCollision);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Collision")) {
        if (ImGui::MenuItem("Plane Collider")) addNode(ParticleNodeType::PlaneCollider);
        if (ImGui::MenuItem("World Collider")) addNode(ParticleNodeType::WorldCollider);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Renderers")) {
        if (ImGui::MenuItem("Billboard Renderer")) addNode(ParticleNodeType::BillboardRenderer);
        if (ImGui::MenuItem("Mesh Renderer"))      addNode(ParticleNodeType::MeshRenderer);
        if (ImGui::MenuItem("Trail Renderer"))      addNode(ParticleNodeType::TrailRenderer);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Control")) {
        if (ImGui::MenuItem("Burst")) addNode(ParticleNodeType::Burst);
        if (ImGui::MenuItem("Loop"))  addNode(ParticleNodeType::Loop);
        if (ImGui::MenuItem("Delay")) addNode(ParticleNodeType::Delay);
        ImGui::EndMenu();
    }

    ImGui::EndPopup();
}

} // namespace Editor
} // namespace Enjin
