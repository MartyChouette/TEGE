#include "Enjin/Editor/ShaderGraph.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Enjin {
namespace Editor {

// Category colors
static const ImU32 COLOR_INPUT   = IM_COL32(60, 140, 60, 255);   // Green
static const ImU32 COLOR_MATH    = IM_COL32(80, 120, 180, 255);  // Blue
static const ImU32 COLOR_TEXTURE = IM_COL32(160, 100, 60, 255);  // Orange
static const ImU32 COLOR_COLOR   = IM_COL32(160, 60, 100, 255);  // Pink
static const ImU32 COLOR_VECTOR  = IM_COL32(100, 80, 160, 255);  // Purple
static const ImU32 COLOR_OUTPUT  = IM_COL32(160, 50, 50, 255);   // Red
static const ImU32 COLOR_UTILITY = IM_COL32(100, 100, 100, 255); // Grey

static const f32 NODE_WIDTH  = 160.0f;
static const f32 NODE_HEADER = 28.0f;
static const f32 NODE_BODY   = 50.0f;
static const f32 PIN_RADIUS  = 5.0f;
static const f32 PIN_SPACING = 20.0f;

static ImU32 GetCategoryColor(ShaderNodeCategory cat) {
    switch (cat) {
        case ShaderNodeCategory::Input:   return COLOR_INPUT;
        case ShaderNodeCategory::Math:    return COLOR_MATH;
        case ShaderNodeCategory::Texture: return COLOR_TEXTURE;
        case ShaderNodeCategory::Color:   return COLOR_COLOR;
        case ShaderNodeCategory::Vector:  return COLOR_VECTOR;
        case ShaderNodeCategory::Output:  return COLOR_OUTPUT;
        case ShaderNodeCategory::Utility: return COLOR_UTILITY;
        default:                          return COLOR_UTILITY;
    }
}

// ============================================================================
// Category Mapping
// ============================================================================

ShaderNodeCategory ShaderGraphEditor::GetCategory(ShaderNodeType type) {
    switch (type) {
        case ShaderNodeType::VertexPosition:
        case ShaderNodeType::VertexNormal:
        case ShaderNodeType::VertexUV:
        case ShaderNodeType::VertexColor:
        case ShaderNodeType::Time:
        case ShaderNodeType::CameraPosition:
        case ShaderNodeType::ViewDirection:
        case ShaderNodeType::FloatConstant:
        case ShaderNodeType::Vec2Constant:
        case ShaderNodeType::Vec3Constant:
        case ShaderNodeType::Vec4Constant:
        case ShaderNodeType::ColorConstant:
        case ShaderNodeType::TextureParameter:
        case ShaderNodeType::FloatParameter:
            return ShaderNodeCategory::Input;

        case ShaderNodeType::Add:
        case ShaderNodeType::Subtract:
        case ShaderNodeType::Multiply:
        case ShaderNodeType::Divide:
        case ShaderNodeType::Lerp:
        case ShaderNodeType::Clamp:
        case ShaderNodeType::Saturate:
        case ShaderNodeType::Abs:
        case ShaderNodeType::Negate:
        case ShaderNodeType::Sin:
        case ShaderNodeType::Cos:
        case ShaderNodeType::Pow:
        case ShaderNodeType::Sqrt:
        case ShaderNodeType::Floor:
        case ShaderNodeType::Ceil:
        case ShaderNodeType::Fract:
        case ShaderNodeType::Min:
        case ShaderNodeType::Max:
        case ShaderNodeType::Step:
        case ShaderNodeType::SmoothStep:
            return ShaderNodeCategory::Math;

        case ShaderNodeType::SampleTexture2D:
        case ShaderNodeType::SampleCubemap:
        case ShaderNodeType::UVTransform:
        case ShaderNodeType::Parallax:
        case ShaderNodeType::Flipbook:
            return ShaderNodeCategory::Texture;

        case ShaderNodeType::HSVToRGB:
        case ShaderNodeType::RGBToHSV:
        case ShaderNodeType::Brightness:
        case ShaderNodeType::Contrast:
        case ShaderNodeType::Blend:
            return ShaderNodeCategory::Color;

        case ShaderNodeType::SplitVec2:
        case ShaderNodeType::SplitVec3:
        case ShaderNodeType::SplitVec4:
        case ShaderNodeType::CombineVec2:
        case ShaderNodeType::CombineVec3:
        case ShaderNodeType::CombineVec4:
        case ShaderNodeType::Normalize:
        case ShaderNodeType::DotProduct:
        case ShaderNodeType::CrossProduct:
        case ShaderNodeType::Reflect:
            return ShaderNodeCategory::Vector;

        case ShaderNodeType::FragmentOutput:
        case ShaderNodeType::VertexOutput:
            return ShaderNodeCategory::Output;

        case ShaderNodeType::Comment:
        case ShaderNodeType::Reroute:
            return ShaderNodeCategory::Utility;

        default:
            return ShaderNodeCategory::Utility;
    }
}

// ============================================================================
// Node Names
// ============================================================================

const char* ShaderGraphEditor::GetNodeName(ShaderNodeType type) {
    switch (type) {
        // Inputs
        case ShaderNodeType::VertexPosition:   return "Vertex Position";
        case ShaderNodeType::VertexNormal:     return "Vertex Normal";
        case ShaderNodeType::VertexUV:         return "Vertex UV";
        case ShaderNodeType::VertexColor:      return "Vertex Color";
        case ShaderNodeType::Time:             return "Time";
        case ShaderNodeType::CameraPosition:   return "Camera Position";
        case ShaderNodeType::ViewDirection:     return "View Direction";
        case ShaderNodeType::FloatConstant:    return "Float";
        case ShaderNodeType::Vec2Constant:     return "Vector2";
        case ShaderNodeType::Vec3Constant:     return "Vector3";
        case ShaderNodeType::Vec4Constant:     return "Vector4";
        case ShaderNodeType::ColorConstant:    return "Color";
        case ShaderNodeType::TextureParameter: return "Texture Param";
        case ShaderNodeType::FloatParameter:   return "Float Param";

        // Math
        case ShaderNodeType::Add:        return "Add";
        case ShaderNodeType::Subtract:   return "Subtract";
        case ShaderNodeType::Multiply:   return "Multiply";
        case ShaderNodeType::Divide:     return "Divide";
        case ShaderNodeType::Lerp:       return "Lerp";
        case ShaderNodeType::Clamp:      return "Clamp";
        case ShaderNodeType::Saturate:   return "Saturate";
        case ShaderNodeType::Abs:        return "Abs";
        case ShaderNodeType::Negate:     return "Negate";
        case ShaderNodeType::Sin:        return "Sin";
        case ShaderNodeType::Cos:        return "Cos";
        case ShaderNodeType::Pow:        return "Pow";
        case ShaderNodeType::Sqrt:       return "Sqrt";
        case ShaderNodeType::Floor:      return "Floor";
        case ShaderNodeType::Ceil:       return "Ceil";
        case ShaderNodeType::Fract:      return "Fract";
        case ShaderNodeType::Min:        return "Min";
        case ShaderNodeType::Max:        return "Max";
        case ShaderNodeType::Step:       return "Step";
        case ShaderNodeType::SmoothStep: return "Smooth Step";

        // Texture
        case ShaderNodeType::SampleTexture2D: return "Sample Texture 2D";
        case ShaderNodeType::SampleCubemap:   return "Sample Cubemap";
        case ShaderNodeType::UVTransform:     return "UV Transform";
        case ShaderNodeType::Parallax:        return "Parallax";
        case ShaderNodeType::Flipbook:        return "Flipbook";

        // Color
        case ShaderNodeType::HSVToRGB:    return "HSV to RGB";
        case ShaderNodeType::RGBToHSV:    return "RGB to HSV";
        case ShaderNodeType::Brightness:  return "Brightness";
        case ShaderNodeType::Contrast:    return "Contrast";
        case ShaderNodeType::Blend:       return "Blend";

        // Vector
        case ShaderNodeType::SplitVec2:    return "Split Vec2";
        case ShaderNodeType::SplitVec3:    return "Split Vec3";
        case ShaderNodeType::SplitVec4:    return "Split Vec4";
        case ShaderNodeType::CombineVec2:  return "Combine Vec2";
        case ShaderNodeType::CombineVec3:  return "Combine Vec3";
        case ShaderNodeType::CombineVec4:  return "Combine Vec4";
        case ShaderNodeType::Normalize:    return "Normalize";
        case ShaderNodeType::DotProduct:   return "Dot Product";
        case ShaderNodeType::CrossProduct: return "Cross Product";
        case ShaderNodeType::Reflect:      return "Reflect";

        // Output
        case ShaderNodeType::FragmentOutput: return "Fragment Output";
        case ShaderNodeType::VertexOutput:   return "Vertex Output";

        // Utility
        case ShaderNodeType::Comment: return "Comment";
        case ShaderNodeType::Reroute: return "Reroute";

        default: return "Unknown";
    }
}

// ============================================================================
// Graph Management
// ============================================================================

void ShaderGraphEditor::SetGraph(ShaderGraphData* graph) {
    m_Graph = graph;
    m_SelectedNodeId = 0;
}

// ============================================================================
// Render
// ============================================================================

void ShaderGraphEditor::Render() {
    if (!m_Open) return;

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Shader Graph", &m_Open, ImGuiWindowFlags_MenuBar)) {
        ImGui::End();
        return;
    }

    if (!m_Graph) {
        ImGui::TextDisabled("No shader graph loaded");
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
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Generate GLSL", "Ctrl+G")) {
                GenerateGLSL();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Toolbar
    ImGui::Text("Shader: %s", m_Graph->name.c_str());
    ImGui::SameLine();
    const char* stageNames[] = { "Vertex", "Fragment", "Both" };
    i32 stageIdx = static_cast<i32>(m_Graph->stage);
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::Combo("##Stage", &stageIdx, stageNames, 3)) {
        m_Graph->stage = static_cast<ShaderGraphData::Stage>(stageIdx);
    }
    ImGui::SameLine();
    if (ImGui::Button("Generate GLSL")) {
        GenerateGLSL();
    }
    ImGui::Separator();

    // Layout: canvas on left, inspector on right
    f32 inspectorWidth = 250.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    f32 canvasWidth = avail.x - inspectorWidth - 8.0f;
    if (canvasWidth < 300.0f) canvasWidth = avail.x;

    // Canvas area
    ImGui::BeginChild("##SGCanvas", ImVec2(canvasWidth, avail.y), true,
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
            ImGui::OpenPopup("##SGContextMenu");
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
        ImGui::BeginChild("##SGInspector", ImVec2(inspectorWidth, avail.y), true);
        DrawInspector();
        ImGui::EndChild();
    }

    ImGui::End();
}

// ============================================================================
// Draw Node
// ============================================================================

void ShaderGraphEditor::DrawNode(ShaderGraphNode& node) {
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

    ShaderNodeCategory cat = GetCategory(node.type);
    ImU32 headerColor = GetCategoryColor(cat);
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
    drawList->AddCircleFilled(inputPinPos, pinR, IM_COL32(180, 180, 180, 255));

    // Output pin (right side)
    ImVec2 outputPinPos(nodeEnd.x, nodePos.y + headerH + bodyH * 0.5f);
    drawList->AddCircleFilled(outputPinPos, pinR, IM_COL32(180, 180, 180, 255));

    // Invisible button for selection and dragging
    ImGui::SetCursorScreenPos(nodePos);
    char btnId[32];
    snprintf(btnId, sizeof(btnId), "##sgnode_%u", node.id);
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

void ShaderGraphEditor::DrawConnections() {
    if (!m_Graph) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    for (const auto& link : m_Graph->links) {
        // Find source and destination nodes
        const ShaderGraphNode* fromNode = nullptr;
        const ShaderGraphNode* toNode = nullptr;
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

        drawList->AddBezierCubic(p1, cp1, cp2, p2, IM_COL32(200, 200, 200, 200), 2.0f * m_Zoom);
    }
}

// ============================================================================
// Draw Inspector
// ============================================================================

void ShaderGraphEditor::DrawInspector() {
    ImGui::Text("Inspector");
    ImGui::Separator();

    if (!m_Graph || m_SelectedNodeId == 0) {
        ImGui::TextDisabled("Select a node to inspect");
        return;
    }

    // Find selected node
    ShaderGraphNode* selected = nullptr;
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

    ShaderNodeCategory cat = GetCategory(selected->type);
    const char* catNames[] = { "Input", "Math", "Texture", "Color", "Vector", "Output", "Utility" };

    ImGui::Text("Type: %s", GetNodeName(selected->type));
    ImGui::Text("Category: %s", catNames[static_cast<u8>(cat)]);
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
        case ShaderNodeType::FloatConstant:
        case ShaderNodeType::FloatParameter:
            ImGui::DragFloat("Value", &selected->floatValue, 0.01f);
            break;

        case ShaderNodeType::Vec2Constant:
            ImGui::DragFloat2("Value", &selected->vec2Value.x, 0.01f);
            break;

        case ShaderNodeType::Vec3Constant:
            ImGui::DragFloat3("Value", &selected->vec3Value.x, 0.01f);
            break;

        case ShaderNodeType::Vec4Constant:
            ImGui::DragFloat4("Value", &selected->vec4Value.x, 0.01f);
            break;

        case ShaderNodeType::ColorConstant:
            ImGui::ColorEdit4("Color", &selected->vec4Value.x);
            break;

        case ShaderNodeType::TextureParameter:
        case ShaderNodeType::SampleTexture2D:
        case ShaderNodeType::SampleCubemap: {
            char texBuf[256];
            strncpy(texBuf, selected->texturePath.c_str(), sizeof(texBuf) - 1);
            texBuf[sizeof(texBuf) - 1] = '\0';
            if (ImGui::InputText("Texture", texBuf, sizeof(texBuf))) {
                selected->texturePath = texBuf;
            }
            break;
        }

        case ShaderNodeType::Lerp:
        case ShaderNodeType::Clamp:
        case ShaderNodeType::Step:
        case ShaderNodeType::SmoothStep:
        case ShaderNodeType::Pow:
            ImGui::DragFloat("Param", &selected->floatValue, 0.01f);
            break;

        case ShaderNodeType::Brightness:
        case ShaderNodeType::Contrast:
            ImGui::DragFloat("Amount", &selected->floatValue, 0.01f, 0.0f, 2.0f);
            break;

        default:
            ImGui::TextDisabled("No editable properties");
            break;
    }

    // Parameter name for parameterized nodes
    if (selected->type == ShaderNodeType::FloatParameter ||
        selected->type == ShaderNodeType::TextureParameter) {
        char paramBuf[128];
        strncpy(paramBuf, selected->parameterName.c_str(), sizeof(paramBuf) - 1);
        paramBuf[sizeof(paramBuf) - 1] = '\0';
        if (ImGui::InputText("Param Name", paramBuf, sizeof(paramBuf))) {
            selected->parameterName = paramBuf;
        }
    }

    ImGui::Separator();

    // Position display
    ImGui::Text("Position: (%.0f, %.0f)", selected->position.x, selected->position.y);

    // Delete button
    ImGui::Spacing();
    if (selected->type != ShaderNodeType::FragmentOutput &&
        selected->type != ShaderNodeType::VertexOutput) {
        if (ImGui::Button("Delete Node")) {
            // Remove links connected to this node
            auto& links = m_Graph->links;
            links.erase(
                std::remove_if(links.begin(), links.end(),
                    [this](const ShaderGraphLink& l) {
                        return l.fromNode == m_SelectedNodeId || l.toNode == m_SelectedNodeId;
                    }),
                links.end());

            // Remove node
            auto& nodes = m_Graph->nodes;
            nodes.erase(
                std::remove_if(nodes.begin(), nodes.end(),
                    [this](const ShaderGraphNode& n) { return n.id == m_SelectedNodeId; }),
                nodes.end());

            m_SelectedNodeId = 0;
        }
    }
}

// ============================================================================
// Draw Context Menu
// ============================================================================

void ShaderGraphEditor::DrawContextMenu() {
    if (!ImGui::BeginPopup("##SGContextMenu")) return;

    ImVec2 mousePos = ImGui::GetMousePosOnOpeningCurrentPopup();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    // Convert screen position to graph position
    Math::Vector2 spawnPos(
        (mousePos.x - canvasPos.x) / m_Zoom - m_ScrollOffset.x,
        (mousePos.y - canvasPos.y) / m_Zoom - m_ScrollOffset.y
    );

    auto addNode = [&](ShaderNodeType type) {
        ShaderGraphNode node;
        node.id = m_Graph->nextNodeId++;
        node.type = type;
        node.position = spawnPos;
        node.label = GetNodeName(type);
        m_Graph->nodes.push_back(node);
        m_SelectedNodeId = node.id;
        ImGui::CloseCurrentPopup();
    };

    if (ImGui::BeginMenu("Input")) {
        if (ImGui::MenuItem("Vertex Position"))   addNode(ShaderNodeType::VertexPosition);
        if (ImGui::MenuItem("Vertex Normal"))     addNode(ShaderNodeType::VertexNormal);
        if (ImGui::MenuItem("Vertex UV"))         addNode(ShaderNodeType::VertexUV);
        if (ImGui::MenuItem("Vertex Color"))      addNode(ShaderNodeType::VertexColor);
        ImGui::Separator();
        if (ImGui::MenuItem("Time"))              addNode(ShaderNodeType::Time);
        if (ImGui::MenuItem("Camera Position"))   addNode(ShaderNodeType::CameraPosition);
        if (ImGui::MenuItem("View Direction"))     addNode(ShaderNodeType::ViewDirection);
        ImGui::Separator();
        if (ImGui::MenuItem("Float"))             addNode(ShaderNodeType::FloatConstant);
        if (ImGui::MenuItem("Vector2"))           addNode(ShaderNodeType::Vec2Constant);
        if (ImGui::MenuItem("Vector3"))           addNode(ShaderNodeType::Vec3Constant);
        if (ImGui::MenuItem("Vector4"))           addNode(ShaderNodeType::Vec4Constant);
        if (ImGui::MenuItem("Color"))             addNode(ShaderNodeType::ColorConstant);
        ImGui::Separator();
        if (ImGui::MenuItem("Texture Parameter")) addNode(ShaderNodeType::TextureParameter);
        if (ImGui::MenuItem("Float Parameter"))   addNode(ShaderNodeType::FloatParameter);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Math")) {
        if (ImGui::MenuItem("Add"))          addNode(ShaderNodeType::Add);
        if (ImGui::MenuItem("Subtract"))     addNode(ShaderNodeType::Subtract);
        if (ImGui::MenuItem("Multiply"))     addNode(ShaderNodeType::Multiply);
        if (ImGui::MenuItem("Divide"))       addNode(ShaderNodeType::Divide);
        ImGui::Separator();
        if (ImGui::MenuItem("Lerp"))         addNode(ShaderNodeType::Lerp);
        if (ImGui::MenuItem("Clamp"))        addNode(ShaderNodeType::Clamp);
        if (ImGui::MenuItem("Saturate"))     addNode(ShaderNodeType::Saturate);
        if (ImGui::MenuItem("Abs"))          addNode(ShaderNodeType::Abs);
        if (ImGui::MenuItem("Negate"))       addNode(ShaderNodeType::Negate);
        ImGui::Separator();
        if (ImGui::MenuItem("Sin"))          addNode(ShaderNodeType::Sin);
        if (ImGui::MenuItem("Cos"))          addNode(ShaderNodeType::Cos);
        if (ImGui::MenuItem("Pow"))          addNode(ShaderNodeType::Pow);
        if (ImGui::MenuItem("Sqrt"))         addNode(ShaderNodeType::Sqrt);
        if (ImGui::MenuItem("Floor"))        addNode(ShaderNodeType::Floor);
        if (ImGui::MenuItem("Ceil"))         addNode(ShaderNodeType::Ceil);
        if (ImGui::MenuItem("Fract"))        addNode(ShaderNodeType::Fract);
        ImGui::Separator();
        if (ImGui::MenuItem("Min"))          addNode(ShaderNodeType::Min);
        if (ImGui::MenuItem("Max"))          addNode(ShaderNodeType::Max);
        if (ImGui::MenuItem("Step"))         addNode(ShaderNodeType::Step);
        if (ImGui::MenuItem("Smooth Step"))  addNode(ShaderNodeType::SmoothStep);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Texture")) {
        if (ImGui::MenuItem("Sample Texture 2D")) addNode(ShaderNodeType::SampleTexture2D);
        if (ImGui::MenuItem("Sample Cubemap"))    addNode(ShaderNodeType::SampleCubemap);
        if (ImGui::MenuItem("UV Transform"))      addNode(ShaderNodeType::UVTransform);
        if (ImGui::MenuItem("Parallax"))          addNode(ShaderNodeType::Parallax);
        if (ImGui::MenuItem("Flipbook"))          addNode(ShaderNodeType::Flipbook);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Color")) {
        if (ImGui::MenuItem("HSV to RGB"))  addNode(ShaderNodeType::HSVToRGB);
        if (ImGui::MenuItem("RGB to HSV"))  addNode(ShaderNodeType::RGBToHSV);
        if (ImGui::MenuItem("Brightness"))  addNode(ShaderNodeType::Brightness);
        if (ImGui::MenuItem("Contrast"))    addNode(ShaderNodeType::Contrast);
        if (ImGui::MenuItem("Blend"))       addNode(ShaderNodeType::Blend);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Vector")) {
        if (ImGui::MenuItem("Split Vec2"))    addNode(ShaderNodeType::SplitVec2);
        if (ImGui::MenuItem("Split Vec3"))    addNode(ShaderNodeType::SplitVec3);
        if (ImGui::MenuItem("Split Vec4"))    addNode(ShaderNodeType::SplitVec4);
        if (ImGui::MenuItem("Combine Vec2"))  addNode(ShaderNodeType::CombineVec2);
        if (ImGui::MenuItem("Combine Vec3"))  addNode(ShaderNodeType::CombineVec3);
        if (ImGui::MenuItem("Combine Vec4"))  addNode(ShaderNodeType::CombineVec4);
        ImGui::Separator();
        if (ImGui::MenuItem("Normalize"))     addNode(ShaderNodeType::Normalize);
        if (ImGui::MenuItem("Dot Product"))   addNode(ShaderNodeType::DotProduct);
        if (ImGui::MenuItem("Cross Product")) addNode(ShaderNodeType::CrossProduct);
        if (ImGui::MenuItem("Reflect"))       addNode(ShaderNodeType::Reflect);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Output")) {
        if (ImGui::MenuItem("Fragment Output")) addNode(ShaderNodeType::FragmentOutput);
        if (ImGui::MenuItem("Vertex Output"))   addNode(ShaderNodeType::VertexOutput);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Utility")) {
        if (ImGui::MenuItem("Comment"))  addNode(ShaderNodeType::Comment);
        if (ImGui::MenuItem("Reroute"))  addNode(ShaderNodeType::Reroute);
        ImGui::EndMenu();
    }

    ImGui::EndPopup();
}

// ============================================================================
// Generate GLSL
// ============================================================================

ShaderCodeResult ShaderGraphEditor::GenerateGLSL() const {
    ShaderCodeResult result;

    // TODO: Implement full graph traversal and GLSL code generation.
    // For now, return passthrough shader templates.

    result.vertexCode =
        "// Generated by Enjin Shader Graph (stub)\n"
        "#version 450\n"
        "\n"
        "layout(location = 0) in vec3 inPosition;\n"
        "layout(location = 1) in vec3 inNormal;\n"
        "layout(location = 2) in vec2 inUV;\n"
        "layout(location = 3) in vec4 inColor;\n"
        "\n"
        "layout(set = 0, binding = 0) uniform ViewProjectionUBO {\n"
        "    mat4 view;\n"
        "    mat4 proj;\n"
        "} vp;\n"
        "\n"
        "layout(push_constant) uniform PushConstants {\n"
        "    mat4 model;\n"
        "} pc;\n"
        "\n"
        "layout(location = 0) out vec3 fragNormal;\n"
        "layout(location = 1) out vec2 fragUV;\n"
        "layout(location = 2) out vec4 fragColor;\n"
        "\n"
        "void main() {\n"
        "    gl_Position = vp.proj * vp.view * pc.model * vec4(inPosition, 1.0);\n"
        "    fragNormal = mat3(pc.model) * inNormal;\n"
        "    fragUV = inUV;\n"
        "    fragColor = inColor;\n"
        "}\n";

    result.fragmentCode =
        "// Generated by Enjin Shader Graph (stub)\n"
        "#version 450\n"
        "\n"
        "layout(location = 0) in vec3 fragNormal;\n"
        "layout(location = 1) in vec2 fragUV;\n"
        "layout(location = 2) in vec4 fragColor;\n"
        "\n"
        "layout(location = 0) out vec4 outColor;\n"
        "\n"
        "void main() {\n"
        "    // TODO: Replace with shader graph output\n"
        "    vec3 normal = normalize(fragNormal);\n"
        "    float diffuse = max(dot(normal, vec3(0.0, 1.0, 0.0)), 0.2);\n"
        "    outColor = vec4(fragColor.rgb * diffuse, fragColor.a);\n"
        "}\n";

    result.success = true;
    return result;
}

} // namespace Editor
} // namespace Enjin
