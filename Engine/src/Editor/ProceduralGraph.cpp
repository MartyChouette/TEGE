#include "Enjin/Editor/ProceduralGraph.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <chrono>

namespace Enjin {
namespace Editor {

// Category colors
static const ImU32 COLOR_GENERATOR = IM_COL32(60, 140, 60, 255);    // Green
static const ImU32 COLOR_TRANSFORM = IM_COL32(80, 120, 180, 255);   // Blue
static const ImU32 COLOR_FILTER    = IM_COL32(140, 100, 160, 255);  // Purple
static const ImU32 COLOR_COMBINE   = IM_COL32(160, 130, 50, 255);   // Gold
static const ImU32 COLOR_OUTPUT    = IM_COL32(160, 50, 50, 255);    // Red
static const ImU32 COLOR_PARAMETER = IM_COL32(100, 100, 100, 255);  // Grey

static const f32 NODE_WIDTH  = 180.0f;
static const f32 NODE_HEADER = 28.0f;
static const f32 NODE_BODY   = 60.0f;
static const f32 PIN_RADIUS  = 5.0f;
static const f32 PIN_SPACING = 20.0f;

static ImU32 GetCategoryColor(ProcNodeCategory cat) {
    switch (cat) {
        case ProcNodeCategory::Generator: return COLOR_GENERATOR;
        case ProcNodeCategory::Transform: return COLOR_TRANSFORM;
        case ProcNodeCategory::Filter:    return COLOR_FILTER;
        case ProcNodeCategory::Combine:   return COLOR_COMBINE;
        case ProcNodeCategory::Output:    return COLOR_OUTPUT;
        case ProcNodeCategory::Parameter: return COLOR_PARAMETER;
        default:                          return COLOR_PARAMETER;
    }
}

// ============================================================================
// Category Mapping
// ============================================================================

ProcNodeCategory ProceduralGraphEditor::GetCategory(ProcNodeType type) {
    switch (type) {
        case ProcNodeType::Gen_CellularAutomata:
        case ProcNodeType::Gen_RandomWalker:
        case ProcNodeType::Gen_BSP:
        case ProcNodeType::Gen_DiamondSquare:
        case ProcNodeType::Gen_LSystem:
        case ProcNodeType::Gen_WFC:
        case ProcNodeType::Gen_Voronoi:
        case ProcNodeType::Gen_Grammar:
        case ProcNodeType::Gen_PrefabAssembler:
        case ProcNodeType::Gen_Noise:
            return ProcNodeCategory::Generator;

        case ProcNodeType::Xform_Resize:
        case ProcNodeType::Xform_Rotate90:
        case ProcNodeType::Xform_Mirror:
        case ProcNodeType::Xform_Offset:
        case ProcNodeType::Xform_Invert:
            return ProcNodeCategory::Transform;

        case ProcNodeType::Filt_Threshold:
        case ProcNodeType::Filt_Erode:
        case ProcNodeType::Filt_Dilate:
        case ProcNodeType::Filt_Smooth:
        case ProcNodeType::Filt_EdgeDetect:
            return ProcNodeCategory::Filter;

        case ProcNodeType::Comb_Blend:
        case ProcNodeType::Comb_Overlay:
        case ProcNodeType::Comb_Mask:
        case ProcNodeType::Comb_Add:
        case ProcNodeType::Comb_Subtract:
        case ProcNodeType::Comb_Min:
        case ProcNodeType::Comb_Max:
            return ProcNodeCategory::Combine;

        case ProcNodeType::Out_Tilemap:
        case ProcNodeType::Out_Heightmap:
        case ProcNodeType::Out_EntitySpawner:
            return ProcNodeCategory::Output;

        case ProcNodeType::Param_Seed:
        case ProcNodeType::Param_Size:
        case ProcNodeType::Param_Float:
        case ProcNodeType::Param_Int:
            return ProcNodeCategory::Parameter;

        default: return ProcNodeCategory::Parameter;
    }
}

const char* ProceduralGraphEditor::GetNodeName(ProcNodeType type) {
    switch (type) {
        case ProcNodeType::Gen_CellularAutomata: return "Cellular Automata";
        case ProcNodeType::Gen_RandomWalker:     return "Random Walker";
        case ProcNodeType::Gen_BSP:              return "BSP Dungeon";
        case ProcNodeType::Gen_DiamondSquare:    return "Diamond-Square";
        case ProcNodeType::Gen_LSystem:          return "L-System";
        case ProcNodeType::Gen_WFC:              return "Wave Function Collapse";
        case ProcNodeType::Gen_Voronoi:          return "Voronoi";
        case ProcNodeType::Gen_Grammar:          return "Grammar";
        case ProcNodeType::Gen_PrefabAssembler:  return "Prefab Assembler";
        case ProcNodeType::Gen_Noise:            return "Noise";

        case ProcNodeType::Xform_Resize:    return "Resize";
        case ProcNodeType::Xform_Rotate90:  return "Rotate 90";
        case ProcNodeType::Xform_Mirror:    return "Mirror";
        case ProcNodeType::Xform_Offset:    return "Offset";
        case ProcNodeType::Xform_Invert:    return "Invert";

        case ProcNodeType::Filt_Threshold:  return "Threshold";
        case ProcNodeType::Filt_Erode:      return "Erode";
        case ProcNodeType::Filt_Dilate:     return "Dilate";
        case ProcNodeType::Filt_Smooth:     return "Smooth";
        case ProcNodeType::Filt_EdgeDetect: return "Edge Detect";

        case ProcNodeType::Comb_Blend:    return "Blend";
        case ProcNodeType::Comb_Overlay:  return "Overlay";
        case ProcNodeType::Comb_Mask:     return "Mask";
        case ProcNodeType::Comb_Add:      return "Add";
        case ProcNodeType::Comb_Subtract: return "Subtract";
        case ProcNodeType::Comb_Min:      return "Min";
        case ProcNodeType::Comb_Max:      return "Max";

        case ProcNodeType::Out_Tilemap:       return "Tilemap Output";
        case ProcNodeType::Out_Heightmap:     return "Heightmap Output";
        case ProcNodeType::Out_EntitySpawner: return "Entity Spawner";

        case ProcNodeType::Param_Seed:  return "Seed";
        case ProcNodeType::Param_Size:  return "Size";
        case ProcNodeType::Param_Float: return "Float";
        case ProcNodeType::Param_Int:   return "Int";

        default: return "Unknown";
    }
}

u32 ProceduralGraphEditor::GetInputPinCount(ProcNodeType type) {
    auto cat = GetCategory(type);
    switch (cat) {
        case ProcNodeCategory::Generator: return 0;  // Generators have no grid input
        case ProcNodeCategory::Parameter: return 0;   // Parameters are sources
        case ProcNodeCategory::Transform:
        case ProcNodeCategory::Filter:
            return 1;   // Single grid input
        case ProcNodeCategory::Combine:
            return 2;   // Two grid inputs (A and B)
        case ProcNodeCategory::Output:
            return 1;   // Single grid input
        default: return 0;
    }
}

u32 ProceduralGraphEditor::GetOutputPinCount(ProcNodeType type) {
    auto cat = GetCategory(type);
    switch (cat) {
        case ProcNodeCategory::Output: return 0;  // Outputs are sinks
        default: return 1;                          // Everything else has one grid output
    }
}

// ============================================================================
// Editor
// ============================================================================

void ProceduralGraphEditor::SetGraph(ProcGraphData* graph) {
    m_Graph = graph;
    m_PreviewDirty = true;
}

void ProceduralGraphEditor::Render() {
    if (!m_Open || !m_Graph) return;

    ImGui::Begin("Procedural Graph", &m_Open);

    // Toolbar
    if (ImGui::Button("Execute")) {
        m_PreviewDirty = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    i32 seed = static_cast<i32>(m_Graph->globalSeed);
    if (ImGui::InputInt("Seed", &seed)) {
        m_Graph->globalSeed = static_cast<u32>(std::max(0, seed));
        m_PreviewDirty = true;
    }
    ImGui::SameLine();
    ImGui::Text("Nodes: %zu  Links: %zu", m_Graph->nodes.size(), m_Graph->links.size());

    // Split: graph canvas (left) and inspector+preview (right)
    f32 avail = ImGui::GetContentRegionAvail().x;
    f32 inspectorW = 250.0f;
    f32 canvasW = avail - inspectorW - 8.0f;
    if (canvasW < 200.0f) canvasW = avail;

    // Canvas
    ImGui::BeginChild("ProcGraphCanvas", ImVec2(canvasW, 0), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Background grid
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                            IM_COL32(22, 22, 28, 255));
    f32 gridSize = 32.0f * m_Zoom;
    for (f32 x = std::fmod(m_ScrollOffset.x, gridSize); x < canvasSize.x; x += gridSize) {
        drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
                          ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                          IM_COL32(40, 40, 50, 255));
    }
    for (f32 y = std::fmod(m_ScrollOffset.y, gridSize); y < canvasSize.y; y += gridSize) {
        drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
                          ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                          IM_COL32(40, 40, 50, 255));
    }

    // Draw links
    DrawConnections();

    // Draw nodes
    for (auto& node : m_Graph->nodes) {
        DrawNode(node);
    }

    // Canvas interaction
    if (ImGui::IsWindowHovered()) {
        // Pan with middle mouse
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_ScrollOffset.x += delta.x;
            m_ScrollOffset.y += delta.y;
        }
        // Zoom with scroll
        f32 scroll = ImGui::GetIO().MouseWheel;
        if (scroll != 0.0f) {
            m_Zoom = std::clamp(m_Zoom + scroll * 0.1f, 0.25f, 3.0f);
        }
        // Right-click context menu
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsAnyItemHovered()) {
            ImGui::OpenPopup("ProcGraphContextMenu");
        }
        // Click canvas to deselect
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
            m_SelectedNodeId = 0;
        }
    }

    DrawContextMenu();

    ImGui::EndChild();

    // Inspector + Preview (right side)
    if (canvasW < avail) {
        ImGui::SameLine();
        ImGui::BeginChild("ProcGraphInspector", ImVec2(inspectorW, 0), true);
        DrawInspector();
        ImGui::Separator();
        DrawPreview();
        ImGui::EndChild();
    }

    ImGui::End();

    // Execute if dirty
    if (m_PreviewDirty && m_Graph) {
        auto result = Execute();
        if (result.success) {
            m_PreviewGrid = result.grid;
            m_PreviewW = result.width;
            m_PreviewH = result.height;
        }
        m_PreviewDirty = false;
    }
}

void ProceduralGraphEditor::DrawNode(ProcGraphNode& node) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    f32 x = canvasPos.x + m_ScrollOffset.x + node.position.x * m_Zoom;
    f32 y = canvasPos.y + m_ScrollOffset.y + node.position.y * m_Zoom;
    f32 w = NODE_WIDTH * m_Zoom;
    f32 h = (NODE_HEADER + NODE_BODY) * m_Zoom;

    ProcNodeCategory cat = GetCategory(node.type);
    ImU32 headerColor = GetCategoryColor(cat);
    bool selected = (m_SelectedNodeId == node.id);

    // Node body
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h),
                            IM_COL32(35, 35, 42, 240), 4.0f * m_Zoom);
    // Header
    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + NODE_HEADER * m_Zoom),
                            headerColor, 4.0f * m_Zoom, ImDrawFlags_RoundCornersTop);
    // Border
    ImU32 borderColor = selected ? IM_COL32(255, 200, 50, 255) : IM_COL32(60, 60, 70, 255);
    drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h),
                      borderColor, 4.0f * m_Zoom, 0, selected ? 2.0f : 1.0f);

    // Title
    const char* name = node.label.empty() ? GetNodeName(node.type) : node.label.c_str();
    drawList->AddText(ImVec2(x + 6.0f * m_Zoom, y + 6.0f * m_Zoom),
                      IM_COL32(220, 220, 220, 255), name);

    // Input pins
    u32 inCount = GetInputPinCount(node.type);
    for (u32 i = 0; i < inCount; ++i) {
        f32 py = y + NODE_HEADER * m_Zoom + (10.0f + i * PIN_SPACING) * m_Zoom;
        drawList->AddCircleFilled(ImVec2(x, py), PIN_RADIUS * m_Zoom,
                                  IM_COL32(180, 180, 200, 255), 12);
        const char* pinLabel = (i == 0) ? "In" : "In B";
        drawList->AddText(ImVec2(x + 8.0f * m_Zoom, py - 6.0f * m_Zoom),
                          IM_COL32(160, 160, 170, 200), pinLabel);
    }

    // Output pins
    u32 outCount = GetOutputPinCount(node.type);
    for (u32 i = 0; i < outCount; ++i) {
        f32 py = y + NODE_HEADER * m_Zoom + (10.0f + i * PIN_SPACING) * m_Zoom;
        drawList->AddCircleFilled(ImVec2(x + w, py), PIN_RADIUS * m_Zoom,
                                  IM_COL32(180, 200, 180, 255), 12);
        drawList->AddText(ImVec2(x + w - 26.0f * m_Zoom, py - 6.0f * m_Zoom),
                          IM_COL32(160, 170, 160, 200), "Out");
    }

    // Invisible button for interaction
    ImGui::SetCursorScreenPos(ImVec2(x, y));
    ImGui::InvisibleButton(("procnode_" + std::to_string(node.id)).c_str(),
                           ImVec2(w, h));
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        m_SelectedNodeId = node.id;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        node.position.x += delta.x / m_Zoom;
        node.position.y += delta.y / m_Zoom;
        m_PreviewDirty = true;
    }
}

void ProceduralGraphEditor::DrawConnections() {
    if (!m_Graph) return;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    for (const auto& link : m_Graph->links) {
        // Find source and target nodes
        const ProcGraphNode* fromNode = nullptr;
        const ProcGraphNode* toNode = nullptr;
        for (const auto& n : m_Graph->nodes) {
            if (n.id == link.fromNode) fromNode = &n;
            if (n.id == link.toNode) toNode = &n;
        }
        if (!fromNode || !toNode) continue;

        f32 fromX = canvasPos.x + m_ScrollOffset.x + (fromNode->position.x + NODE_WIDTH) * m_Zoom;
        f32 fromY = canvasPos.y + m_ScrollOffset.y +
                    (fromNode->position.y + NODE_HEADER + 10.0f + link.fromPin * PIN_SPACING) * m_Zoom;
        f32 toX = canvasPos.x + m_ScrollOffset.x + toNode->position.x * m_Zoom;
        f32 toY = canvasPos.y + m_ScrollOffset.y +
                  (toNode->position.y + NODE_HEADER + 10.0f + link.toPin * PIN_SPACING) * m_Zoom;

        // Bezier curve
        f32 dx = std::abs(toX - fromX) * 0.5f;
        drawList->AddBezierCubic(
            ImVec2(fromX, fromY), ImVec2(fromX + dx, fromY),
            ImVec2(toX - dx, toY), ImVec2(toX, toY),
            IM_COL32(200, 200, 200, 180), 2.0f);
    }
}

void ProceduralGraphEditor::DrawInspector() {
    ImGui::Text("Inspector");
    ImGui::Separator();

    if (m_SelectedNodeId == 0 || !m_Graph) {
        ImGui::TextDisabled("Select a node to inspect");
        return;
    }

    ProcGraphNode* node = nullptr;
    for (auto& n : m_Graph->nodes) {
        if (n.id == m_SelectedNodeId) { node = &n; break; }
    }
    if (!node) { ImGui::TextDisabled("Node not found"); return; }

    ImGui::Text("%s", GetNodeName(node->type));
    ProcNodeCategory cat = GetCategory(node->type);
    ImU32 catColor = GetCategoryColor(cat);
    ImGui::SameLine();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, ImVec2(p.x + 8, p.y + 8), catColor, 2.0f);
    ImGui::Dummy(ImVec2(10, 10));

    ImGui::Separator();

    // Node-specific parameters
    bool changed = false;
    switch (node->type) {
        case ProcNodeType::Gen_CellularAutomata:
            changed |= ImGui::SliderInt("Iterations", &node->intParams[0], 1, 20);
            changed |= ImGui::SliderInt("Birth Thresh", &node->intParams[1], 0, 8);
            changed |= ImGui::SliderInt("Death Thresh", &node->intParams[2], 0, 8);
            changed |= ImGui::SliderFloat("Fill %", &node->floatParams[0], 0.1f, 0.9f);
            break;

        case ProcNodeType::Gen_RandomWalker:
            changed |= ImGui::SliderInt("Steps", &node->intParams[0], 100, 10000);
            changed |= ImGui::SliderFloat("Turn Chance", &node->floatParams[0], 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Dir Bias", &node->floatParams[1], 0.0f, 1.0f);
            break;

        case ProcNodeType::Gen_BSP:
            changed |= ImGui::SliderInt("Min Room", &node->intParams[0], 3, 20);
            changed |= ImGui::SliderInt("Max Room", &node->intParams[1], 5, 30);
            changed |= ImGui::SliderInt("Iterations", &node->intParams[2], 1, 10);
            changed |= ImGui::SliderInt("Corridor W", &node->intParams[3], 1, 5);
            break;

        case ProcNodeType::Gen_DiamondSquare:
            changed |= ImGui::SliderFloat("Roughness", &node->floatParams[0], 0.0f, 2.0f);
            changed |= ImGui::SliderFloat("Scale", &node->floatParams[1], 0.1f, 10.0f);
            break;

        case ProcNodeType::Gen_LSystem: {
            char axiomBuf[256] = {};
            std::strncpy(axiomBuf, node->stringParam.c_str(), sizeof(axiomBuf) - 1);
            if (ImGui::InputText("Axiom", axiomBuf, sizeof(axiomBuf))) {
                node->stringParam = axiomBuf;
                changed = true;
            }
            changed |= ImGui::SliderInt("Iterations", &node->intParams[0], 1, 8);
            changed |= ImGui::SliderFloat("Angle", &node->floatParams[0], 10.0f, 90.0f);
            break;
        }

        case ProcNodeType::Gen_WFC:
            changed |= ImGui::SliderInt("Tile Types", &node->intParams[0], 2, 16);
            changed |= ImGui::SliderInt("Max Retries", &node->intParams[1], 1, 100);
            break;

        case ProcNodeType::Gen_Voronoi:
            changed |= ImGui::SliderInt("Seeds", &node->intParams[0], 2, 100);
            changed |= ImGui::SliderInt("Metric", &node->intParams[1], 0, 2);
            if (node->intParams[1] >= 0 && node->intParams[1] <= 2) {
                const char* metrics[] = { "Euclidean", "Manhattan", "Chebyshev" };
                ImGui::TextDisabled("(%s)", metrics[node->intParams[1]]);
            }
            break;

        case ProcNodeType::Gen_Noise:
            changed |= ImGui::SliderFloat("Frequency", &node->floatParams[0], 0.01f, 0.5f);
            changed |= ImGui::SliderInt("Octaves", &node->intParams[0], 1, 8);
            changed |= ImGui::SliderFloat("Persistence", &node->floatParams[1], 0.1f, 1.0f);
            break;

        case ProcNodeType::Xform_Resize:
            changed |= ImGui::InputInt("Width", &node->intParams[0]);
            changed |= ImGui::InputInt("Height", &node->intParams[1]);
            break;

        case ProcNodeType::Xform_Rotate90:
            changed |= ImGui::SliderInt("Rotations", &node->intParams[0], 1, 3);
            break;

        case ProcNodeType::Xform_Mirror: {
            bool hFlip = node->intParams[0] != 0;
            bool vFlip = node->intParams[1] != 0;
            if (ImGui::Checkbox("Horizontal", &hFlip)) { node->intParams[0] = hFlip ? 1 : 0; changed = true; }
            if (ImGui::Checkbox("Vertical", &vFlip)) { node->intParams[1] = vFlip ? 1 : 0; changed = true; }
            break;
        }

        case ProcNodeType::Xform_Offset:
            changed |= ImGui::InputInt("X Offset", &node->intParams[0]);
            changed |= ImGui::InputInt("Y Offset", &node->intParams[1]);
            break;

        case ProcNodeType::Filt_Threshold:
            changed |= ImGui::SliderFloat("Threshold", &node->floatParams[0], 0.0f, 1.0f);
            break;

        case ProcNodeType::Filt_Erode:
        case ProcNodeType::Filt_Dilate:
            changed |= ImGui::SliderInt("Iterations", &node->intParams[0], 1, 10);
            break;

        case ProcNodeType::Filt_Smooth:
            changed |= ImGui::SliderInt("Radius", &node->intParams[0], 1, 10);
            break;

        case ProcNodeType::Comb_Blend:
            changed |= ImGui::SliderFloat("Weight", &node->floatParams[0], 0.0f, 1.0f);
            break;

        case ProcNodeType::Out_EntitySpawner:
            changed |= ImGui::SliderFloat("Spawn Thresh", &node->floatParams[0], 0.0f, 1.0f);
            changed |= ImGui::SliderFloat("Spacing", &node->floatParams[1], 0.5f, 10.0f);
            break;

        case ProcNodeType::Param_Seed:
            changed |= ImGui::InputInt("Seed", &node->intParams[0]);
            break;

        case ProcNodeType::Param_Size:
            changed |= ImGui::InputInt("Width", &node->intParams[0]);
            changed |= ImGui::InputInt("Height", &node->intParams[1]);
            break;

        case ProcNodeType::Param_Float:
            changed |= ImGui::InputFloat("Value", &node->floatParams[0]);
            break;

        case ProcNodeType::Param_Int:
            changed |= ImGui::InputInt("Value", &node->intParams[0]);
            break;

        default:
            ImGui::TextDisabled("No parameters");
            break;
    }

    if (changed) m_PreviewDirty = true;

    ImGui::Separator();
    if (ImGui::Button("Delete Node")) {
        // Remove links connected to this node
        m_Graph->links.erase(
            std::remove_if(m_Graph->links.begin(), m_Graph->links.end(),
                [this](const ProcGraphLink& l) {
                    return l.fromNode == m_SelectedNodeId || l.toNode == m_SelectedNodeId;
                }),
            m_Graph->links.end());
        // Remove node
        m_Graph->nodes.erase(
            std::remove_if(m_Graph->nodes.begin(), m_Graph->nodes.end(),
                [this](const ProcGraphNode& n) { return n.id == m_SelectedNodeId; }),
            m_Graph->nodes.end());
        m_SelectedNodeId = 0;
        m_PreviewDirty = true;
    }
}

void ProceduralGraphEditor::DrawContextMenu() {
    if (!ImGui::BeginPopup("ProcGraphContextMenu")) return;

    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 mousePos = ImGui::GetMousePosOnOpeningCurrentPopup();
    f32 spawnX = (mousePos.x - canvasPos.x - m_ScrollOffset.x) / m_Zoom;
    f32 spawnY = (mousePos.y - canvasPos.y - m_ScrollOffset.y) / m_Zoom;

    auto AddNodeMenuItem = [&](const char* label, ProcNodeType type) {
        if (ImGui::MenuItem(label)) {
            ProcGraphNode node;
            node.id = m_Graph->nextNodeId++;
            node.type = type;
            node.position = { spawnX, spawnY };

            // Set defaults based on type
            switch (type) {
                case ProcNodeType::Gen_CellularAutomata:
                    node.intParams[0] = 5;   // iterations
                    node.intParams[1] = 4;   // birth limit
                    node.intParams[2] = 3;   // death limit
                    node.floatParams[0] = 0.45f; // fill (0-1, converted to 0-100)
                    break;
                case ProcNodeType::Gen_RandomWalker:
                    node.intParams[0] = 2000;
                    node.floatParams[0] = 0.3f;
                    break;
                case ProcNodeType::Gen_BSP:
                    node.intParams[0] = 5; node.intParams[1] = 12;
                    node.intParams[2] = 5; node.intParams[3] = 2;
                    break;
                case ProcNodeType::Gen_DiamondSquare:
                    node.floatParams[0] = 0.7f; node.floatParams[1] = 1.0f;
                    break;
                case ProcNodeType::Gen_LSystem:
                    node.stringParam = "F";
                    node.intParams[0] = 4;
                    node.floatParams[0] = 25.0f;
                    break;
                case ProcNodeType::Gen_WFC:
                    node.intParams[0] = 4; node.intParams[1] = 10;
                    break;
                case ProcNodeType::Gen_Voronoi:
                    node.intParams[0] = 20; node.intParams[1] = 0;
                    break;
                case ProcNodeType::Gen_Noise:
                    node.floatParams[0] = 0.05f; node.intParams[0] = 4;
                    node.floatParams[1] = 0.5f;
                    break;
                case ProcNodeType::Filt_Threshold:
                    node.floatParams[0] = 0.5f;
                    break;
                case ProcNodeType::Filt_Erode:
                case ProcNodeType::Filt_Dilate:
                    node.intParams[0] = 1;
                    break;
                case ProcNodeType::Filt_Smooth:
                    node.intParams[0] = 2;
                    break;
                case ProcNodeType::Comb_Blend:
                    node.floatParams[0] = 0.5f;
                    break;
                case ProcNodeType::Xform_Resize:
                    node.intParams[0] = 128; node.intParams[1] = 128;
                    break;
                case ProcNodeType::Xform_Rotate90:
                    node.intParams[0] = 1;
                    break;
                case ProcNodeType::Out_EntitySpawner:
                    node.floatParams[0] = 0.5f; node.floatParams[1] = 2.0f;
                    break;
                case ProcNodeType::Param_Size:
                    node.intParams[0] = 64; node.intParams[1] = 64;
                    break;
                default: break;
            }

            m_Graph->nodes.push_back(node);
            m_SelectedNodeId = node.id;
            m_PreviewDirty = true;
        }
    };

    if (ImGui::BeginMenu("Generators")) {
        AddNodeMenuItem("Cellular Automata", ProcNodeType::Gen_CellularAutomata);
        AddNodeMenuItem("Random Walker", ProcNodeType::Gen_RandomWalker);
        AddNodeMenuItem("BSP Dungeon", ProcNodeType::Gen_BSP);
        AddNodeMenuItem("Diamond-Square", ProcNodeType::Gen_DiamondSquare);
        AddNodeMenuItem("L-System", ProcNodeType::Gen_LSystem);
        AddNodeMenuItem("Wave Function Collapse", ProcNodeType::Gen_WFC);
        AddNodeMenuItem("Voronoi", ProcNodeType::Gen_Voronoi);
        AddNodeMenuItem("Noise", ProcNodeType::Gen_Noise);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Transforms")) {
        AddNodeMenuItem("Resize", ProcNodeType::Xform_Resize);
        AddNodeMenuItem("Rotate 90", ProcNodeType::Xform_Rotate90);
        AddNodeMenuItem("Mirror", ProcNodeType::Xform_Mirror);
        AddNodeMenuItem("Offset", ProcNodeType::Xform_Offset);
        AddNodeMenuItem("Invert", ProcNodeType::Xform_Invert);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Filters")) {
        AddNodeMenuItem("Threshold", ProcNodeType::Filt_Threshold);
        AddNodeMenuItem("Erode", ProcNodeType::Filt_Erode);
        AddNodeMenuItem("Dilate", ProcNodeType::Filt_Dilate);
        AddNodeMenuItem("Smooth", ProcNodeType::Filt_Smooth);
        AddNodeMenuItem("Edge Detect", ProcNodeType::Filt_EdgeDetect);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Combine")) {
        AddNodeMenuItem("Blend", ProcNodeType::Comb_Blend);
        AddNodeMenuItem("Overlay", ProcNodeType::Comb_Overlay);
        AddNodeMenuItem("Mask", ProcNodeType::Comb_Mask);
        AddNodeMenuItem("Add", ProcNodeType::Comb_Add);
        AddNodeMenuItem("Subtract", ProcNodeType::Comb_Subtract);
        AddNodeMenuItem("Min", ProcNodeType::Comb_Min);
        AddNodeMenuItem("Max", ProcNodeType::Comb_Max);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Output")) {
        AddNodeMenuItem("Tilemap Output", ProcNodeType::Out_Tilemap);
        AddNodeMenuItem("Heightmap Output", ProcNodeType::Out_Heightmap);
        AddNodeMenuItem("Entity Spawner", ProcNodeType::Out_EntitySpawner);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Parameters")) {
        AddNodeMenuItem("Seed", ProcNodeType::Param_Seed);
        AddNodeMenuItem("Size", ProcNodeType::Param_Size);
        AddNodeMenuItem("Float", ProcNodeType::Param_Float);
        AddNodeMenuItem("Int", ProcNodeType::Param_Int);
        ImGui::EndMenu();
    }

    ImGui::EndPopup();
}

void ProceduralGraphEditor::DrawPreview() {
    ImGui::Text("Preview");
    if (m_PreviewGrid.empty() || m_PreviewW == 0 || m_PreviewH == 0) {
        ImGui::TextDisabled("No preview available");
        return;
    }

    f32 avail = ImGui::GetContentRegionAvail().x;
    f32 previewSize = std::min(avail, 240.0f);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    f32 cellW = previewSize / static_cast<f32>(m_PreviewW);
    f32 cellH = previewSize / static_cast<f32>(m_PreviewH);

    for (u32 y = 0; y < m_PreviewH && y < m_PreviewGrid.size(); ++y) {
        for (u32 x = 0; x < m_PreviewW && x < m_PreviewGrid[y].size(); ++x) {
            f32 val = std::clamp(m_PreviewGrid[y][x], 0.0f, 1.0f);
            u8 gray = static_cast<u8>(val * 255.0f);
            ImU32 col = IM_COL32(gray, gray, gray, 255);
            dl->AddRectFilled(
                ImVec2(p.x + x * cellW, p.y + y * cellH),
                ImVec2(p.x + (x + 1) * cellW, p.y + (y + 1) * cellH),
                col);
        }
    }

    ImGui::Dummy(ImVec2(previewSize, previewSize));
    ImGui::Text("%ux%u", m_PreviewW, m_PreviewH);
}

// ============================================================================
// Graph Execution
// ============================================================================

ProcGraphResult ProceduralGraphEditor::Execute() const {
    if (!m_Graph || m_Graph->nodes.empty()) {
        return { {}, 0, 0, false, "No graph data" };
    }

    u32 width = m_Graph->previewWidth;
    u32 height = m_Graph->previewHeight;
    u32 seed = m_Graph->globalSeed;
    if (seed == 0) {
        seed = static_cast<u32>(std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFF);
    }

    std::unordered_map<u32, ProcGraphResult> cache;

    // Find output nodes, or fall back to last node
    u32 evalNodeId = 0;
    for (const auto& node : m_Graph->nodes) {
        auto cat = GetCategory(node.type);
        if (cat == ProcNodeCategory::Output) {
            evalNodeId = node.id;
            break;
        }
    }
    if (evalNodeId == 0 && !m_Graph->nodes.empty()) {
        evalNodeId = m_Graph->nodes.back().id;
    }

    return EvaluateNode(evalNodeId, width, height, seed, cache);
}

ProcGraphResult ProceduralGraphEditor::EvaluateNode(
    u32 nodeId, u32 width, u32 height, u32 seed,
    std::unordered_map<u32, ProcGraphResult>& cache) const
{
    // Check cache
    auto cacheIt = cache.find(nodeId);
    if (cacheIt != cache.end()) return cacheIt->second;

    // Find node
    const ProcGraphNode* node = nullptr;
    for (const auto& n : m_Graph->nodes) {
        if (n.id == nodeId) { node = &n; break; }
    }
    if (!node) return { {}, 0, 0, false, "Node not found" };

    // Get input grids by following links
    std::vector<ProcGraphResult> inputs;
    u32 inCount = GetInputPinCount(node->type);
    for (u32 pin = 0; pin < inCount; ++pin) {
        bool found = false;
        for (const auto& link : m_Graph->links) {
            if (link.toNode == nodeId && link.toPin == pin) {
                auto inputResult = EvaluateNode(link.fromNode, width, height, seed, cache);
                inputs.push_back(inputResult);
                found = true;
                break;
            }
        }
        if (!found) {
            // No input connected — use empty grid
            ProcGraphResult empty;
            empty.width = width;
            empty.height = height;
            empty.grid.resize(height, std::vector<f32>(width, 0.0f));
            empty.success = true;
            inputs.push_back(empty);
        }
    }

    ProcGraphResult result;
    result.width = width;
    result.height = height;
    result.success = true;

    // Execute node based on type
    switch (node->type) {
        // ---- Generators ----
        case ProcNodeType::Gen_CellularAutomata: {
            Procedural::CellularAutomata::Params params;
            params.width = width;
            params.height = height;
            params.seed = seed;
            params.iterations = static_cast<u32>(std::max(1, node->intParams[0]));
            params.birthLimit = static_cast<u32>(std::max(0, node->intParams[1]));
            params.deathLimit = static_cast<u32>(std::max(0, node->intParams[2]));
            params.fillPercent = static_cast<u32>(std::clamp(node->floatParams[0] * 100.0f, 0.0f, 100.0f));
            auto r = Procedural::CellularAutomata::Generate(params);
            result.grid.resize(r.height);
            for (u32 y = 0; y < r.height; ++y) {
                result.grid[y].resize(r.width);
                for (u32 x = 0; x < r.width; ++x) {
                    result.grid[y][x] = static_cast<f32>(r.grid[y][x]);
                }
            }
            result.width = r.width;
            result.height = r.height;
            break;
        }
        case ProcNodeType::Gen_RandomWalker: {
            Procedural::RandomWalker::Params params;
            params.width = width;
            params.height = height;
            params.seed = seed;
            params.steps = static_cast<u32>(std::max(100, node->intParams[0]));
            params.turnChance = node->floatParams[0];
            params.directionalBias = { node->floatParams[1], 0.0f };
            auto r = Procedural::RandomWalker::Generate(params);
            result.grid.resize(r.grid.size());
            for (u32 y = 0; y < r.grid.size(); ++y) {
                result.grid[y].resize(r.grid[y].size());
                for (u32 x = 0; x < r.grid[y].size(); ++x) {
                    result.grid[y][x] = static_cast<f32>(r.grid[y][x]);
                }
            }
            break;
        }
        case ProcNodeType::Gen_BSP: {
            Procedural::BSPGenerator::Params params;
            params.width = width;
            params.height = height;
            params.seed = seed;
            params.minRoomSize = static_cast<u32>(std::max(3, node->intParams[0]));
            params.maxRoomSize = static_cast<u32>(std::max(static_cast<i32>(params.minRoomSize), node->intParams[1]));
            params.splitDepth = static_cast<u32>(std::max(1, node->intParams[2]));
            params.corridorWidth = static_cast<u32>(std::max(1, node->intParams[3]));
            auto r = Procedural::BSPGenerator::Generate(params);
            result.grid.resize(r.grid.size());
            for (u32 y = 0; y < r.grid.size(); ++y) {
                result.grid[y].resize(r.grid[y].size());
                for (u32 x = 0; x < r.grid[y].size(); ++x) {
                    result.grid[y][x] = static_cast<f32>(r.grid[y][x]);
                }
            }
            break;
        }
        case ProcNodeType::Gen_DiamondSquare: {
            Procedural::DiamondSquare::Params params;
            params.seed = seed;
            params.roughness = node->floatParams[0];
            // DS requires power-of-2+1 size
            u32 dsSize = 1;
            while (dsSize + 1 < std::max(width, height)) dsSize *= 2;
            params.size = dsSize + 1;
            auto r = Procedural::DiamondSquare::Generate(params);
            result.width = static_cast<u32>(r.size);
            result.height = static_cast<u32>(r.size);
            result.grid.resize(r.size);
            for (u32 y = 0; y < static_cast<u32>(r.size); ++y) {
                result.grid[y].resize(r.size);
                for (u32 x = 0; x < static_cast<u32>(r.size); ++x) {
                    result.grid[y][x] = r.heightmap[y][x];
                }
            }
            break;
        }
        case ProcNodeType::Gen_Voronoi: {
            Procedural::VoronoiGenerator::Params params;
            params.width = width;
            params.height = height;
            params.seed = seed;
            params.numSeeds = static_cast<u32>(std::max(2, node->intParams[0]));
            params.distanceType = static_cast<Procedural::VoronoiGenerator::DistanceType>(
                std::clamp(node->intParams[1], 0, 2));
            auto r = Procedural::VoronoiGenerator::Generate(params);
            result.grid.resize(r.grid.size());
            f32 maxRegion = static_cast<f32>(std::max(1u, params.numSeeds));
            for (u32 y = 0; y < r.grid.size(); ++y) {
                result.grid[y].resize(r.grid[y].size());
                for (u32 x = 0; x < r.grid[y].size(); ++x) {
                    result.grid[y][x] = static_cast<f32>(r.grid[y][x]) / maxRegion;
                }
            }
            break;
        }
        case ProcNodeType::Gen_Noise: {
            // Simple Perlin-like noise generation using the grid
            result.grid.resize(height, std::vector<f32>(width, 0.0f));
            f32 freq = std::max(0.001f, node->floatParams[0]);
            i32 octaves = std::clamp(node->intParams[0], 1, 8);
            f32 persistence = std::clamp(node->floatParams[1], 0.1f, 1.0f);
            for (u32 y = 0; y < height; ++y) {
                for (u32 x = 0; x < width; ++x) {
                    f32 val = 0.0f;
                    f32 amp = 1.0f;
                    f32 f = freq;
                    f32 maxAmp = 0.0f;
                    for (i32 o = 0; o < octaves; ++o) {
                        // Simple hash-based pseudo noise
                        u32 ix = static_cast<u32>(x * f + seed * 37) ^ (seed >> 3);
                        u32 iy = static_cast<u32>(y * f + seed * 53) ^ (seed >> 7);
                        u32 hash = (ix * 2654435761u) ^ (iy * 2246822519u);
                        hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
                        hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
                        hash = (hash >> 16) ^ hash;
                        f32 noise = static_cast<f32>(hash & 0xFFFF) / 65535.0f;
                        val += noise * amp;
                        maxAmp += amp;
                        amp *= persistence;
                        f *= 2.0f;
                    }
                    result.grid[y][x] = val / maxAmp;
                }
            }
            break;
        }
        // Fall through for generators without direct grid output
        case ProcNodeType::Gen_LSystem:
        case ProcNodeType::Gen_WFC:
        case ProcNodeType::Gen_Grammar:
        case ProcNodeType::Gen_PrefabAssembler: {
            // These produce non-grid output; generate a placeholder
            result.grid.resize(height, std::vector<f32>(width, 0.5f));
            break;
        }

        // ---- Transforms ----
        case ProcNodeType::Xform_Invert: {
            if (!inputs.empty() && inputs[0].success) {
                result = inputs[0];
                for (auto& row : result.grid) {
                    for (auto& v : row) v = 1.0f - v;
                }
            }
            break;
        }
        case ProcNodeType::Xform_Mirror: {
            if (!inputs.empty() && inputs[0].success) {
                result = inputs[0];
                bool hFlip = node->intParams[0] != 0;
                bool vFlip = node->intParams[1] != 0;
                if (hFlip) {
                    for (auto& row : result.grid) {
                        std::reverse(row.begin(), row.end());
                    }
                }
                if (vFlip) {
                    std::reverse(result.grid.begin(), result.grid.end());
                }
            }
            break;
        }
        case ProcNodeType::Xform_Rotate90: {
            if (!inputs.empty() && inputs[0].success) {
                auto& src = inputs[0];
                i32 rotations = std::clamp(node->intParams[0], 1, 3);
                result = src;
                for (i32 r = 0; r < rotations; ++r) {
                    u32 oldW = result.width;
                    u32 oldH = result.height;
                    std::vector<std::vector<f32>> rotated(oldW, std::vector<f32>(oldH, 0.0f));
                    for (u32 y = 0; y < oldH; ++y) {
                        for (u32 x = 0; x < oldW; ++x) {
                            rotated[x][oldH - 1 - y] = result.grid[y][x];
                        }
                    }
                    result.grid = rotated;
                    result.width = oldH;
                    result.height = oldW;
                }
            }
            break;
        }
        case ProcNodeType::Xform_Offset: {
            if (!inputs.empty() && inputs[0].success) {
                result = inputs[0];
                i32 ox = node->intParams[0];
                i32 oy = node->intParams[1];
                auto copy = result.grid;
                for (u32 y = 0; y < result.height; ++y) {
                    for (u32 x = 0; x < result.width; ++x) {
                        i32 sx = (static_cast<i32>(x) - ox % static_cast<i32>(result.width) + result.width) % result.width;
                        i32 sy = (static_cast<i32>(y) - oy % static_cast<i32>(result.height) + result.height) % result.height;
                        result.grid[y][x] = copy[sy][sx];
                    }
                }
            }
            break;
        }
        case ProcNodeType::Xform_Resize: {
            if (!inputs.empty() && inputs[0].success) {
                auto& src = inputs[0];
                u32 newW = std::clamp(static_cast<u32>(node->intParams[0]), 4u, 1024u);
                u32 newH = std::clamp(static_cast<u32>(node->intParams[1]), 4u, 1024u);
                result.width = newW;
                result.height = newH;
                result.grid.resize(newH, std::vector<f32>(newW, 0.0f));
                for (u32 y = 0; y < newH; ++y) {
                    for (u32 x = 0; x < newW; ++x) {
                        u32 srcX = x * src.width / newW;
                        u32 srcY = y * src.height / newH;
                        srcX = std::min(srcX, src.width - 1);
                        srcY = std::min(srcY, src.height - 1);
                        result.grid[y][x] = src.grid[srcY][srcX];
                    }
                }
            }
            break;
        }

        // ---- Filters ----
        case ProcNodeType::Filt_Threshold: {
            if (!inputs.empty() && inputs[0].success) {
                result = inputs[0];
                f32 thresh = node->floatParams[0];
                for (auto& row : result.grid) {
                    for (auto& v : row) v = (v >= thresh) ? 1.0f : 0.0f;
                }
            }
            break;
        }
        case ProcNodeType::Filt_Erode: {
            if (!inputs.empty() && inputs[0].success) {
                result = inputs[0];
                i32 iters = std::max(1, node->intParams[0]);
                for (i32 it = 0; it < iters; ++it) {
                    auto copy = result.grid;
                    for (u32 y = 1; y + 1 < result.height; ++y) {
                        for (u32 x = 1; x + 1 < result.width; ++x) {
                            f32 minVal = copy[y][x];
                            minVal = std::min(minVal, copy[y-1][x]);
                            minVal = std::min(minVal, copy[y+1][x]);
                            minVal = std::min(minVal, copy[y][x-1]);
                            minVal = std::min(minVal, copy[y][x+1]);
                            result.grid[y][x] = minVal;
                        }
                    }
                }
            }
            break;
        }
        case ProcNodeType::Filt_Dilate: {
            if (!inputs.empty() && inputs[0].success) {
                result = inputs[0];
                i32 iters = std::max(1, node->intParams[0]);
                for (i32 it = 0; it < iters; ++it) {
                    auto copy = result.grid;
                    for (u32 y = 1; y + 1 < result.height; ++y) {
                        for (u32 x = 1; x + 1 < result.width; ++x) {
                            f32 maxVal = copy[y][x];
                            maxVal = std::max(maxVal, copy[y-1][x]);
                            maxVal = std::max(maxVal, copy[y+1][x]);
                            maxVal = std::max(maxVal, copy[y][x-1]);
                            maxVal = std::max(maxVal, copy[y][x+1]);
                            result.grid[y][x] = maxVal;
                        }
                    }
                }
            }
            break;
        }
        case ProcNodeType::Filt_Smooth: {
            if (!inputs.empty() && inputs[0].success) {
                result = inputs[0];
                i32 radius = std::clamp(node->intParams[0], 1, 10);
                auto copy = result.grid;
                for (u32 y = 0; y < result.height; ++y) {
                    for (u32 x = 0; x < result.width; ++x) {
                        f32 sum = 0.0f;
                        i32 count = 0;
                        for (i32 dy = -radius; dy <= radius; ++dy) {
                            for (i32 dx = -radius; dx <= radius; ++dx) {
                                i32 nx = static_cast<i32>(x) + dx;
                                i32 ny = static_cast<i32>(y) + dy;
                                if (nx >= 0 && nx < static_cast<i32>(result.width) &&
                                    ny >= 0 && ny < static_cast<i32>(result.height)) {
                                    sum += copy[ny][nx];
                                    ++count;
                                }
                            }
                        }
                        result.grid[y][x] = sum / static_cast<f32>(count);
                    }
                }
            }
            break;
        }
        case ProcNodeType::Filt_EdgeDetect: {
            if (!inputs.empty() && inputs[0].success) {
                result = inputs[0];
                auto copy = result.grid;
                for (u32 y = 1; y + 1 < result.height; ++y) {
                    for (u32 x = 1; x + 1 < result.width; ++x) {
                        f32 gx = -copy[y-1][x-1] + copy[y-1][x+1]
                                -2.0f*copy[y][x-1] + 2.0f*copy[y][x+1]
                                -copy[y+1][x-1] + copy[y+1][x+1];
                        f32 gy = -copy[y-1][x-1] - 2.0f*copy[y-1][x] - copy[y-1][x+1]
                                +copy[y+1][x-1] + 2.0f*copy[y+1][x] + copy[y+1][x+1];
                        result.grid[y][x] = std::clamp(std::sqrt(gx*gx + gy*gy), 0.0f, 1.0f);
                    }
                }
            }
            break;
        }

        // ---- Combine ----
        case ProcNodeType::Comb_Blend: {
            if (inputs.size() >= 2 && inputs[0].success && inputs[1].success) {
                result = inputs[0];
                f32 w = node->floatParams[0];
                for (u32 y = 0; y < result.height; ++y) {
                    for (u32 x = 0; x < result.width; ++x) {
                        f32 a = result.grid[y][x];
                        f32 b = (y < inputs[1].height && x < inputs[1].width) ? inputs[1].grid[y][x] : 0.0f;
                        result.grid[y][x] = a * (1.0f - w) + b * w;
                    }
                }
            }
            break;
        }
        case ProcNodeType::Comb_Overlay: {
            if (inputs.size() >= 2 && inputs[0].success && inputs[1].success) {
                result = inputs[0];
                for (u32 y = 0; y < result.height; ++y) {
                    for (u32 x = 0; x < result.width; ++x) {
                        f32 b = (y < inputs[1].height && x < inputs[1].width) ? inputs[1].grid[y][x] : 0.0f;
                        if (b > 0.0f) result.grid[y][x] = b;
                    }
                }
            }
            break;
        }
        case ProcNodeType::Comb_Mask: {
            if (inputs.size() >= 2 && inputs[0].success && inputs[1].success) {
                result = inputs[0];
                for (u32 y = 0; y < result.height; ++y) {
                    for (u32 x = 0; x < result.width; ++x) {
                        f32 mask = (y < inputs[1].height && x < inputs[1].width) ? inputs[1].grid[y][x] : 0.0f;
                        result.grid[y][x] *= mask;
                    }
                }
            }
            break;
        }
        case ProcNodeType::Comb_Add: {
            if (inputs.size() >= 2 && inputs[0].success && inputs[1].success) {
                result = inputs[0];
                for (u32 y = 0; y < result.height; ++y) {
                    for (u32 x = 0; x < result.width; ++x) {
                        f32 b = (y < inputs[1].height && x < inputs[1].width) ? inputs[1].grid[y][x] : 0.0f;
                        result.grid[y][x] = std::clamp(result.grid[y][x] + b, 0.0f, 1.0f);
                    }
                }
            }
            break;
        }
        case ProcNodeType::Comb_Subtract: {
            if (inputs.size() >= 2 && inputs[0].success && inputs[1].success) {
                result = inputs[0];
                for (u32 y = 0; y < result.height; ++y) {
                    for (u32 x = 0; x < result.width; ++x) {
                        f32 b = (y < inputs[1].height && x < inputs[1].width) ? inputs[1].grid[y][x] : 0.0f;
                        result.grid[y][x] = std::clamp(result.grid[y][x] - b, 0.0f, 1.0f);
                    }
                }
            }
            break;
        }
        case ProcNodeType::Comb_Min: {
            if (inputs.size() >= 2 && inputs[0].success && inputs[1].success) {
                result = inputs[0];
                for (u32 y = 0; y < result.height; ++y) {
                    for (u32 x = 0; x < result.width; ++x) {
                        f32 b = (y < inputs[1].height && x < inputs[1].width) ? inputs[1].grid[y][x] : 0.0f;
                        result.grid[y][x] = std::min(result.grid[y][x], b);
                    }
                }
            }
            break;
        }
        case ProcNodeType::Comb_Max: {
            if (inputs.size() >= 2 && inputs[0].success && inputs[1].success) {
                result = inputs[0];
                for (u32 y = 0; y < result.height; ++y) {
                    for (u32 x = 0; x < result.width; ++x) {
                        f32 b = (y < inputs[1].height && x < inputs[1].width) ? inputs[1].grid[y][x] : 0.0f;
                        result.grid[y][x] = std::max(result.grid[y][x], b);
                    }
                }
            }
            break;
        }

        // ---- Output (pass-through) ----
        case ProcNodeType::Out_Tilemap:
        case ProcNodeType::Out_Heightmap:
        case ProcNodeType::Out_EntitySpawner: {
            if (!inputs.empty() && inputs[0].success) {
                result = inputs[0];
            }
            break;
        }

        // ---- Parameters (produce constant grids) ----
        case ProcNodeType::Param_Float: {
            result.grid.resize(height, std::vector<f32>(width, node->floatParams[0]));
            break;
        }
        case ProcNodeType::Param_Int: {
            f32 val = static_cast<f32>(node->intParams[0]);
            result.grid.resize(height, std::vector<f32>(width, val));
            break;
        }
        case ProcNodeType::Param_Seed:
        case ProcNodeType::Param_Size: {
            result.grid.resize(height, std::vector<f32>(width, 0.5f));
            break;
        }

        default:
            result.success = false;
            result.error = "Unknown node type";
            break;
    }

    cache[nodeId] = result;
    return result;
}

} // namespace Editor
} // namespace Enjin
