#include "Enjin/Editor/ParticleGraph.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>
#include <fstream>

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
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                Save("particle_graph.enjparticle");
            }
            if (ImGui::MenuItem("Load")) {
                Load("particle_graph.enjparticle");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Toolbar
    ImGui::Text("System: %s", m_Graph->name.c_str());
    ImGui::Separator();

    // Compile + Apply button
    if (ImGui::Button("Apply to Selected Entity")) {
        m_CompileRequested = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu nodes, %zu links)", m_Graph->nodes.size(), m_Graph->links.size());
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

    m_CanvasOriginX = canvasPos.x;
    m_CanvasOriginY = canvasPos.y;
    m_FramePins.clear();
    m_PinInteracted = false;

    // Draw connections first (behind nodes)
    DrawConnections();

    // Draw nodes
    for (auto& node : m_Graph->nodes) {
        DrawNode(node);
    }

    HandleLinking();

    // Handle canvas interaction
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && !m_Linking && !m_PinInteracted) {
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

// Flow-chain pin model: emitters produce, renderers consume, the rest do both.
static bool PGHasInput(ParticleNodeType t) {
    switch (t) {
        case ParticleNodeType::PointEmitter: case ParticleNodeType::SphereEmitter:
        case ParticleNodeType::BoxEmitter: case ParticleNodeType::ConeEmitter:
        case ParticleNodeType::MeshEmitter:
            return false;
        default: return true;
    }
}
static bool PGHasOutput(ParticleNodeType t) {
    switch (t) {
        case ParticleNodeType::BillboardRenderer: case ParticleNodeType::MeshRenderer:
        case ParticleNodeType::TrailRenderer:
            return false;
        default: return true;
    }
}
static ImVec2 PGPinPos(const ParticleGraphNode& n, i32 pin,
                       f32 originX, f32 originY, const Math::Vector2& scroll, f32 zoom) {
    f32 x = originX + (n.position.x + scroll.x) * zoom;
    f32 y = originY + (n.position.y + scroll.y) * zoom + (NODE_HEADER + NODE_BODY * 0.5f) * zoom;
    if (pin < 0) x += NODE_WIDTH * zoom;
    return ImVec2(x, y);
}

void ParticleGraphEditor::DrawNode(ParticleGraphNode& node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    f32 nodeW = NODE_WIDTH * m_Zoom;
    f32 headerH = NODE_HEADER * m_Zoom;
    f32 bodyH = NODE_BODY * m_Zoom;
    f32 pinR = PIN_RADIUS * m_Zoom;

    ImVec2 nodePos(
        m_CanvasOriginX + (node.position.x + m_ScrollOffset.x) * m_Zoom,
        m_CanvasOriginY + (node.position.y + m_ScrollOffset.y) * m_Zoom
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

    // Pins: hoverable link points (grow on hover so they read as clickable)
    ImVec2 mouse = ImGui::GetIO().MousePos;
    auto drawPin = [&](i32 pinIdx, ImU32 col) {
        ImVec2 p = PGPinPos(node, pinIdx, m_CanvasOriginX, m_CanvasOriginY, m_ScrollOffset, m_Zoom);
        f32 dx = mouse.x - p.x, dy = mouse.y - p.y;
        bool hot = (dx * dx + dy * dy) <= (pinR * 3.0f) * (pinR * 3.0f);
        drawList->AddCircleFilled(p, hot ? pinR * 1.6f : pinR, col);
        if (hot) drawList->AddCircle(p, pinR * 2.0f, IM_COL32(255, 255, 255, 160), 0, 1.5f);
        m_FramePins.push_back({node.id, pinIdx, p.x, p.y});
    };
    if (PGHasInput(node.type)) drawPin(0, IM_COL32(180, 220, 220, 255));
    if (PGHasOutput(node.type)) drawPin(-1, IM_COL32(220, 180, 180, 255));

    // Invisible button for selection and dragging - inset from the pin strips
    ImGui::SetCursorScreenPos(ImVec2(nodePos.x + pinR * 2.5f, nodePos.y));
    char btnId[32];
    snprintf(btnId, sizeof(btnId), "##pgnode_%u", node.id);
    ImGui::InvisibleButton(btnId, ImVec2(nodeW - pinR * 5.0f, headerH + bodyH));

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

void ParticleGraphEditor::HandleLinking() {
    if (!m_Graph) return;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    f32 hitR = PIN_RADIUS * m_Zoom * 3.0f;

    const FramePin* hot = nullptr;
    f32 best = hitR * hitR;
    for (const auto& fp : m_FramePins) {
        f32 dx = mouse.x - fp.x, dy = mouse.y - fp.y;
        f32 d2 = dx * dx + dy * dy;
        if (d2 <= best) { best = d2; hot = &fp; }
    }

    if (hot && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        m_PinInteracted = true;
        auto& links = m_Graph->links;
        links.erase(std::remove_if(links.begin(), links.end(), [&](const ParticleGraphLink& l) {
            if (hot->pin < 0) return l.fromNode == hot->node;
            return l.toNode == hot->node;
        }), links.end());
        return;
    }

    if (hot && !m_Linking && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_PinInteracted = true;
        m_Linking = true;
        m_LinkNode = hot->node;
        m_LinkPin = hot->pin;
        if (hot->pin >= 0) {
            auto& links = m_Graph->links;
            for (auto it = links.begin(); it != links.end(); ++it) {
                if (it->toNode == hot->node) {
                    m_LinkNode = it->fromNode;   // detach and re-drag from the source
                    m_LinkPin = -1;
                    links.erase(it);
                    break;
                }
            }
        }
    }

    if (!m_Linking) return;
    m_PinInteracted = true;

    const ParticleGraphNode* fixedNode = nullptr;
    for (const auto& n : m_Graph->nodes) if (n.id == m_LinkNode) { fixedNode = &n; break; }
    if (!fixedNode) { m_Linking = false; return; }
    ImVec2 a = PGPinPos(*fixedNode, m_LinkPin, m_CanvasOriginX, m_CanvasOriginY, m_ScrollOffset, m_Zoom);
    bool snap = hot && hot->node != m_LinkNode && ((m_LinkPin < 0) != (hot->pin < 0));
    ImVec2 b = snap ? ImVec2(hot->x, hot->y) : mouse;
    ImVec2 o = (m_LinkPin < 0) ? a : b;
    ImVec2 iP = (m_LinkPin < 0) ? b : a;
    f32 tangent = (iP.x - o.x) * 0.5f;
    if (tangent < 50.0f * m_Zoom) tangent = 50.0f * m_Zoom;
    drawList->AddBezierCubic(o, ImVec2(o.x + tangent, o.y), ImVec2(iP.x - tangent, iP.y), iP,
                             snap ? IM_COL32(120, 255, 160, 255) : IM_COL32(255, 220, 120, 200),
                             2.5f * m_Zoom);

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (snap) {
            u32 outNode = (m_LinkPin < 0) ? m_LinkNode : hot->node;
            u32 inNode  = (m_LinkPin < 0) ? hot->node : m_LinkNode;
            auto& links = m_Graph->links;
            links.erase(std::remove_if(links.begin(), links.end(), [&](const ParticleGraphLink& l) {
                return l.toNode == inNode;
            }), links.end());
            ParticleGraphLink nl;
            nl.id = m_Graph->nextLinkId++;
            nl.fromNode = outNode;
            nl.fromPin = 0;
            nl.toNode = inNode;
            nl.toPin = 0;
            links.push_back(nl);
        }
        m_Linking = false;
    }
}

void ParticleGraphEditor::DrawConnections() {
    if (!m_Graph) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (const auto& link : m_Graph->links) {
        const ParticleGraphNode* fromNode = nullptr;
        const ParticleGraphNode* toNode = nullptr;
        for (const auto& n : m_Graph->nodes) {
            if (n.id == link.fromNode) fromNode = &n;
            if (n.id == link.toNode) toNode = &n;
        }
        if (!fromNode || !toNode) continue;

        ImVec2 p1 = PGPinPos(*fromNode, -1, m_CanvasOriginX, m_CanvasOriginY, m_ScrollOffset, m_Zoom);
        ImVec2 p2 = PGPinPos(*toNode, 0, m_CanvasOriginX, m_CanvasOriginY, m_ScrollOffset, m_Zoom);

        f32 tangentLen = (p2.x - p1.x) * 0.5f;
        if (tangentLen < 50.0f * m_Zoom) tangentLen = 50.0f * m_Zoom;
        ImVec2 cp1(p1.x + tangentLen, p1.y);
        ImVec2 cp2(p2.x - tangentLen, p2.y);
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
        case ParticleNodeType::BillboardRenderer: {
            char texBuf[256] = {};
            std::strncpy(texBuf, selected->texturePath.c_str(), sizeof(texBuf) - 1);
            if (ImGui::InputText("Texture", texBuf, sizeof(texBuf)))
                selected->texturePath = texBuf;
            const char* bbModes[] = { "Camera-Facing", "Velocity-Stretched" };
            ImGui::Combo("Billboard Mode", &selected->billboardMode, bbModes, 2);
            const char* sortModes[] = { "None", "Back-to-Front", "Front-to-Back" };
            ImGui::Combo("Sort Mode", &selected->sortMode, sortModes, 3);
            const char* blendModes[] = { "Alpha", "Additive", "Multiply" };
            ImGui::Combo("Blend Mode", &selected->blendMode, blendModes, 3);
            ImGui::DragFloat("Size Multiplier", &selected->sizeMultiplier, 0.01f, 0.01f, 100.0f);
            ImGui::ColorEdit3("Color Tint", &selected->colorTint.x);
            break;
        }
        case ParticleNodeType::MeshRenderer: {
            char meshBuf[256] = {};
            std::strncpy(meshBuf, selected->meshPath.c_str(), sizeof(meshBuf) - 1);
            if (ImGui::InputText("Mesh Path", meshBuf, sizeof(meshBuf)))
                selected->meshPath = meshBuf;
            char texBuf2[256] = {};
            std::strncpy(texBuf2, selected->texturePath.c_str(), sizeof(texBuf2) - 1);
            if (ImGui::InputText("Texture", texBuf2, sizeof(texBuf2)))
                selected->texturePath = texBuf2;
            ImGui::DragFloat3("Scale", &selected->meshScale.x, 0.01f, 0.01f, 100.0f);
            const char* rotAligns[] = { "None", "Velocity", "Custom Axis" };
            ImGui::Combo("Rotation Alignment", &selected->rotationAlignment, rotAligns, 3);
            ImGui::ColorEdit3("Color Tint", &selected->colorTint.x);
            const char* blendModes2[] = { "Alpha", "Additive", "Multiply" };
            ImGui::Combo("Blend Mode", &selected->blendMode, blendModes2, 3);
            break;
        }
        case ParticleNodeType::TrailRenderer: {
            ImGui::DragFloat("Start Width", &selected->trailWidth, 0.01f, 0.01f, 10.0f);
            ImGui::DragFloat("End Width", &selected->trailEndWidth, 0.01f, 0.0f, 10.0f);
            char texBuf3[256] = {};
            std::strncpy(texBuf3, selected->texturePath.c_str(), sizeof(texBuf3) - 1);
            if (ImGui::InputText("Texture", texBuf3, sizeof(texBuf3)))
                selected->texturePath = texBuf3;
            const char* texModes[] = { "Stretch", "Tile" };
            ImGui::Combo("Texture Mode", &selected->trailTextureMode, texModes, 2);
            ImGui::DragFloat("Min Vertex Distance", &selected->trailMinVertexDistance, 0.01f, 0.01f, 5.0f);
            ImGui::ColorEdit3("Start Color", &selected->trailStartColor.x);
            ImGui::ColorEdit3("End Color", &selected->trailEndColor.x);
            const char* blendModes3[] = { "Alpha", "Additive", "Multiply" };
            ImGui::Combo("Blend Mode", &selected->blendMode, blendModes3, 3);
            break;
        }

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

// ============================================================================
// Save/Load (.enjparticle JSON format)
// ============================================================================

bool ParticleGraphEditor::Save(const std::string& path) const {
    if (!m_Graph) return false;

    std::string json = "{\n";
    json += "  \"name\": \"" + m_Graph->name + "\",\n";
    json += "  \"nextNodeId\": " + std::to_string(m_Graph->nextNodeId) + ",\n";
    json += "  \"nextLinkId\": " + std::to_string(m_Graph->nextLinkId) + ",\n";

    json += "  \"nodes\": [\n";
    for (usize i = 0; i < m_Graph->nodes.size(); ++i) {
        const auto& n = m_Graph->nodes[i];
        json += "    { \"id\": " + std::to_string(n.id) +
                ", \"type\": " + std::to_string(static_cast<int>(n.type)) +
                ", \"x\": " + std::to_string(n.position.x) +
                ", \"y\": " + std::to_string(n.position.y) +
                ", \"label\": \"" + n.label + "\"" +
                ", \"rate\": " + std::to_string(n.rate) +
                ", \"lifetime\": " + std::to_string(n.lifetime) +
                ", \"startSpeed\": " + std::to_string(n.startSpeed) +
                ", \"dir\": [" + std::to_string(n.direction.x) + "," + std::to_string(n.direction.y) + "," + std::to_string(n.direction.z) + "]" +
                ", \"spread\": " + std::to_string(n.spread) +
                ", \"strength\": " + std::to_string(n.strength) +
                ", \"texturePath\": \"" + n.texturePath + "\"" +
                ", \"meshPath\": \"" + n.meshPath + "\"" +
                ", \"billboardMode\": " + std::to_string(n.billboardMode) +
                ", \"sortMode\": " + std::to_string(n.sortMode) +
                ", \"blendMode\": " + std::to_string(n.blendMode) +
                ", \"sizeMultiplier\": " + std::to_string(n.sizeMultiplier) +
                ", \"colorTint\": [" + std::to_string(n.colorTint.x) + "," + std::to_string(n.colorTint.y) + "," + std::to_string(n.colorTint.z) + "]" +
                ", \"meshScale\": [" + std::to_string(n.meshScale.x) + "," + std::to_string(n.meshScale.y) + "," + std::to_string(n.meshScale.z) + "]" +
                ", \"rotationAlignment\": " + std::to_string(n.rotationAlignment) +
                ", \"trailWidth\": " + std::to_string(n.trailWidth) +
                ", \"trailEndWidth\": " + std::to_string(n.trailEndWidth) +
                ", \"trailTextureMode\": " + std::to_string(n.trailTextureMode) +
                ", \"trailMinVertexDistance\": " + std::to_string(n.trailMinVertexDistance) +
                ", \"trailStartColor\": [" + std::to_string(n.trailStartColor.x) + "," + std::to_string(n.trailStartColor.y) + "," + std::to_string(n.trailStartColor.z) + "]" +
                ", \"trailEndColor\": [" + std::to_string(n.trailEndColor.x) + "," + std::to_string(n.trailEndColor.y) + "," + std::to_string(n.trailEndColor.z) + "]";

        if (!n.curve.empty()) {
            json += ", \"curve\": [";
            for (usize c = 0; c < n.curve.size(); ++c) {
                json += "[" + std::to_string(n.curve[c].x) + "," + std::to_string(n.curve[c].y) + "]";
                if (c + 1 < n.curve.size()) json += ",";
            }
            json += "]";
        }

        json += " }" + std::string(i + 1 < m_Graph->nodes.size() ? ",\n" : "\n");
    }
    json += "  ],\n";

    json += "  \"links\": [\n";
    for (usize i = 0; i < m_Graph->links.size(); ++i) {
        const auto& l = m_Graph->links[i];
        json += "    { \"id\": " + std::to_string(l.id) +
                ", \"from\": " + std::to_string(l.fromNode) +
                ", \"fromPin\": " + std::to_string(l.fromPin) +
                ", \"to\": " + std::to_string(l.toNode) +
                ", \"toPin\": " + std::to_string(l.toPin) +
                " }" + std::string(i + 1 < m_Graph->links.size() ? ",\n" : "\n");
    }
    json += "  ]\n";
    json += "}\n";

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << json;
    return true;
}

bool ParticleGraphEditor::Load(const std::string& path) {
    if (!m_Graph) return false;

    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string json((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

    try {
        nlohmann::json j = nlohmann::json::parse(json);
        m_Graph->name = j.value("name", "New Particle System");
        m_Graph->nextNodeId = j.value("nextNodeId", 1u);
        m_Graph->nextLinkId = j.value("nextLinkId", 1u);

        m_Graph->nodes.clear();
        if (j.contains("nodes")) {
            for (const auto& nj : j["nodes"]) {
                ParticleGraphNode n;
                n.id = nj.value("id", 0u);
                n.type = static_cast<ParticleNodeType>(nj.value("type", 0));
                n.position.x = nj.value("x", 0.0f);
                n.position.y = nj.value("y", 0.0f);
                n.label = nj.value("label", std::string(""));
                n.rate = nj.value("rate", 10.0f);
                n.lifetime = nj.value("lifetime", 2.0f);
                n.startSpeed = nj.value("startSpeed", 5.0f);
                if (nj.contains("dir")) {
                    n.direction.x = nj["dir"][0];
                    n.direction.y = nj["dir"][1];
                    n.direction.z = nj["dir"][2];
                }
                n.spread = nj.value("spread", 0.5f);
                n.strength = nj.value("strength", 1.0f);
                n.texturePath = nj.value("texturePath", std::string(""));
                n.meshPath = nj.value("meshPath", std::string(""));
                n.billboardMode = nj.value("billboardMode", 0);
                n.sortMode = nj.value("sortMode", 0);
                n.blendMode = nj.value("blendMode", 0);
                n.sizeMultiplier = nj.value("sizeMultiplier", 1.0f);
                if (nj.contains("colorTint") && nj["colorTint"].size() >= 3) {
                    n.colorTint.x = nj["colorTint"][0]; n.colorTint.y = nj["colorTint"][1]; n.colorTint.z = nj["colorTint"][2];
                }
                if (nj.contains("meshScale") && nj["meshScale"].size() >= 3) {
                    n.meshScale.x = nj["meshScale"][0]; n.meshScale.y = nj["meshScale"][1]; n.meshScale.z = nj["meshScale"][2];
                }
                n.rotationAlignment = nj.value("rotationAlignment", 0);
                n.trailWidth = nj.value("trailWidth", 0.5f);
                n.trailEndWidth = nj.value("trailEndWidth", 0.1f);
                n.trailTextureMode = nj.value("trailTextureMode", 0);
                n.trailMinVertexDistance = nj.value("trailMinVertexDistance", 0.1f);
                if (nj.contains("trailStartColor") && nj["trailStartColor"].size() >= 3) {
                    n.trailStartColor.x = nj["trailStartColor"][0]; n.trailStartColor.y = nj["trailStartColor"][1]; n.trailStartColor.z = nj["trailStartColor"][2];
                }
                if (nj.contains("trailEndColor") && nj["trailEndColor"].size() >= 3) {
                    n.trailEndColor.x = nj["trailEndColor"][0]; n.trailEndColor.y = nj["trailEndColor"][1]; n.trailEndColor.z = nj["trailEndColor"][2];
                }
                if (nj.contains("curve")) {
                    for (const auto& cp : nj["curve"]) {
                        n.curve.push_back(Math::Vector2(cp[0].get<f32>(), cp[1].get<f32>()));
                    }
                }
                m_Graph->nodes.push_back(n);
            }
        }

        m_Graph->links.clear();
        if (j.contains("links")) {
            for (const auto& lj : j["links"]) {
                ParticleGraphLink l;
                l.id = lj.value("id", 0u);
                l.fromNode = lj.value("from", 0u);
                l.fromPin = lj.value("fromPin", 0u);
                l.toNode = lj.value("to", 0u);
                l.toPin = lj.value("toPin", 0u);
                m_Graph->links.push_back(l);
            }
        }
    } catch (...) {
        return false;
    }
    return true;
}

// ============================================================================
// Particle Graph Compiler
// ============================================================================

static bool IsEmitterType(ParticleNodeType type) {
    return type == ParticleNodeType::PointEmitter ||
           type == ParticleNodeType::SphereEmitter ||
           type == ParticleNodeType::BoxEmitter ||
           type == ParticleNodeType::ConeEmitter ||
           type == ParticleNodeType::MeshEmitter;
}

ParticleCompileResult ParticleGraphCompiler::Compile(const ParticleGraphData& graph,
                                                      ECS::ParticleEmitterComponent& target) {
    ParticleCompileResult result;

    // ---- Find first emitter node ----
    const ParticleGraphNode* emitter = nullptr;
    for (const auto& node : graph.nodes) {
        if (IsEmitterType(node.type)) {
            emitter = &node;
            break;
        }
    }

    if (!emitter) {
        result.errors.push_back("No emitter node found in graph");
        return result;
    }

    // ---- Map emitter type to EmitterShape ----
    using Shape = ECS::ParticleEmitterComponent::EmitterShape;
    switch (emitter->type) {
        case ParticleNodeType::PointEmitter:
            target.shape = Shape::Point;
            break;
        case ParticleNodeType::SphereEmitter:
            target.shape = Shape::Sphere;
            target.shapeRadius = emitter->spread;
            break;
        case ParticleNodeType::BoxEmitter:
            target.shape = Shape::Box;
            target.shapeRadius = emitter->spread;
            break;
        case ParticleNodeType::ConeEmitter:
            target.shape = Shape::Cone;
            target.coneAngle = emitter->spread * 90.0f;
            break;
        case ParticleNodeType::MeshEmitter:
            target.shape = Shape::Point;  // Fallback
            result.warnings.push_back("MeshEmitter mapped to Point shape (mesh emission not supported on component)");
            break;
        default:
            break;
    }

    // ---- Set basic emitter properties ----
    target.emissionRate = emitter->rate;
    target.lifetime = emitter->lifetime;
    target.startSpeed = emitter->startSpeed;

    // ---- Walk all modifier nodes ----
    for (const auto& node : graph.nodes) {
        switch (node.type) {
            case ParticleNodeType::Gravity:
                target.gravity = node.direction * node.strength;
                break;

            case ParticleNodeType::Wind:
                target.gravity.x += node.direction.x * node.strength;
                target.gravity.y += node.direction.y * node.strength;
                target.gravity.z += node.direction.z * node.strength;
                result.warnings.push_back("Wind force added to gravity (no separate wind field on component)");
                break;

            case ParticleNodeType::Drag:
                target.drag = node.strength;
                break;

            case ParticleNodeType::Turbulence:
                result.warnings.push_back("Turbulence not directly supported on ParticleEmitterComponent");
                break;

            case ParticleNodeType::Vortex:
                result.warnings.push_back("Vortex not directly supported on ParticleEmitterComponent");
                break;

            case ParticleNodeType::ColorOverLife:
                if (node.curve.size() >= 2) {
                    f32 startIntensity = node.curve.front().y;
                    f32 endIntensity = node.curve.back().y;
                    target.startColor = Math::Vector3(startIntensity, startIntensity, startIntensity);
                    target.endColor = Math::Vector3(endIntensity, endIntensity, endIntensity);
                    // Also map to alpha if curve descends toward zero
                    target.startAlpha = startIntensity;
                    target.endAlpha = endIntensity;
                } else if (node.curve.size() == 1) {
                    f32 intensity = node.curve[0].y;
                    target.startColor = Math::Vector3(intensity, intensity, intensity);
                    target.endColor = Math::Vector3(intensity, intensity, intensity);
                }
                break;

            case ParticleNodeType::SizeOverLife:
                if (!node.curve.empty()) {
                    target.startSize = node.curve.front().y;
                    target.endSize = node.curve.back().y;
                    if (node.curve.size() >= 3) {
                        // Use the middle point for sizeMid
                        usize midIdx = node.curve.size() / 2;
                        target.sizeMid = node.curve[midIdx].y;
                    }
                }
                break;

            case ParticleNodeType::SpeedOverLife:
                if (node.curve.size() >= 2) {
                    target.speedMultiplierEnd = node.curve.back().y;
                    if (node.curve.size() >= 3) {
                        usize midIdx = node.curve.size() / 2;
                        target.speedMultiplierMid = node.curve[midIdx].y;
                    }
                } else if (node.curve.size() == 1) {
                    target.speedMultiplierEnd = node.curve[0].y;
                }
                break;

            case ParticleNodeType::RotationOverLife:
                if (node.curve.size() >= 2) {
                    // Estimate rotation speed from slope of curve
                    f32 dt = node.curve.back().x - node.curve.front().x;
                    f32 dv = node.curve.back().y - node.curve.front().y;
                    if (dt > 0.0f) {
                        target.rotationSpeed = dv / dt;
                    }
                } else if (node.curve.size() == 1) {
                    target.rotationSpeed = node.curve[0].y;
                }
                break;

            default:
                break;
        }
    }

    // ---- Walk control nodes ----
    for (const auto& node : graph.nodes) {
        switch (node.type) {
            case ParticleNodeType::Burst:
                target.burstCount = static_cast<i32>(node.rate);
                target.burstInterval = node.lifetime;
                break;

            case ParticleNodeType::Loop:
                target.loop = true;
                if (node.lifetime > 0.0f) {
                    result.warnings.push_back("Loop duration (" + std::to_string(node.lifetime) +
                        "s) noted but component uses continuous looping");
                }
                break;

            case ParticleNodeType::Delay:
                result.warnings.push_back("Delay (" + std::to_string(node.lifetime) +
                    "s) not supported on ParticleEmitterComponent — no startDelay field");
                break;

            default:
                break;
        }
    }

    // ---- Walk renderer nodes ----
    using RMode = ECS::ParticleEmitterComponent::RenderMode;
    for (const auto& node : graph.nodes) {
        switch (node.type) {
            case ParticleNodeType::BillboardRenderer:
                target.renderMode = (node.billboardMode == 1) ? RMode::VelocityStretch : RMode::Billboard;
                if (!node.texturePath.empty()) target.texturePath = node.texturePath;
                if (node.sizeMultiplier != 1.0f) target.startSize *= node.sizeMultiplier;
                target.startColor = node.colorTint;
                break;

            case ParticleNodeType::MeshRenderer:
                target.renderMode = RMode::Billboard;
                if (!node.texturePath.empty()) target.texturePath = node.texturePath;
                target.startColor = node.colorTint;
                result.warnings.push_back("MeshRenderer mapped to Billboard (mesh particle rendering not supported)");
                break;

            case ParticleNodeType::TrailRenderer:
                target.startColor = node.trailStartColor;
                target.endColor = node.trailEndColor;
                result.warnings.push_back("TrailRenderer not supported on ParticleEmitterComponent — colors mapped to start/end");
                break;

            default:
                break;
        }
    }

    result.success = true;
    return result;
}

} // namespace Editor
} // namespace Enjin
