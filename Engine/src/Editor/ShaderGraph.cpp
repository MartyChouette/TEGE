#include "Enjin/Editor/ShaderGraph.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <queue>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

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
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                Save("shader_graph.enjshader");
            }
            if (ImGui::MenuItem("Load")) {
                Load("shader_graph.enjshader");
            }
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
        m_LastResult = GenerateGLSL();
        m_ShowCodeWindow = true;
    }
    if (m_LastResult.success) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "OK");
    } else if (!m_LastResult.errors.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%zu error(s)",
                           m_LastResult.errors.size());
    }
    ImGui::Separator();

    // Code output window
    if (m_ShowCodeWindow) {
        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Generated GLSL", &m_ShowCodeWindow)) {
            if (!m_LastResult.errors.empty()) {
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Errors:");
                for (const auto& err : m_LastResult.errors) {
                    ImGui::BulletText("%s", err.c_str());
                }
                ImGui::Separator();
            }
            if (!m_LastResult.vertexCode.empty()) {
                if (ImGui::CollapsingHeader("Vertex Shader", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::InputTextMultiline("##vert", const_cast<char*>(m_LastResult.vertexCode.c_str()),
                        m_LastResult.vertexCode.size() + 1, ImVec2(-1, 200),
                        ImGuiInputTextFlags_ReadOnly);
                }
            }
            if (!m_LastResult.fragmentCode.empty()) {
                if (ImGui::CollapsingHeader("Fragment Shader", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::InputTextMultiline("##frag", const_cast<char*>(m_LastResult.fragmentCode.c_str()),
                        m_LastResult.fragmentCode.size() + 1, ImVec2(-1, 200),
                        ImGuiInputTextFlags_ReadOnly);
                }
            }
        }
        ImGui::End();
    }

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
// GLSL Code Generation — topological sort + per-node emission
// ============================================================================

// Helper: get the GLSL variable name for a node output
static std::string NodeVar(u32 nodeId, u32 pinIdx = 0) {
    return "n" + std::to_string(nodeId) + "_" + std::to_string(pinIdx);
}

// Helper: get the GLSL type string for a node's output
static const char* NodeOutType(ShaderNodeType type) {
    switch (type) {
        case ShaderNodeType::FloatConstant:
        case ShaderNodeType::FloatParameter:
        case ShaderNodeType::Time:
        case ShaderNodeType::DotProduct:
        case ShaderNodeType::Abs:
        case ShaderNodeType::Negate:
        case ShaderNodeType::Sin:
        case ShaderNodeType::Cos:
        case ShaderNodeType::Pow:
        case ShaderNodeType::Sqrt:
        case ShaderNodeType::Floor:
        case ShaderNodeType::Ceil:
        case ShaderNodeType::Fract:
        case ShaderNodeType::Step:
        case ShaderNodeType::SmoothStep:
        case ShaderNodeType::Saturate:
        case ShaderNodeType::Min:
        case ShaderNodeType::Max:
        case ShaderNodeType::Brightness:
        case ShaderNodeType::Contrast:
        case ShaderNodeType::SplitVec2:
        case ShaderNodeType::SplitVec3:
        case ShaderNodeType::SplitVec4:
            return "float";
        case ShaderNodeType::Vec2Constant:
        case ShaderNodeType::VertexUV:
        case ShaderNodeType::CombineVec2:
        case ShaderNodeType::UVTransform:
        case ShaderNodeType::Flipbook:
            return "vec2";
        case ShaderNodeType::Vec3Constant:
        case ShaderNodeType::VertexPosition:
        case ShaderNodeType::VertexNormal:
        case ShaderNodeType::CameraPosition:
        case ShaderNodeType::ViewDirection:
        case ShaderNodeType::CombineVec3:
        case ShaderNodeType::Normalize:
        case ShaderNodeType::CrossProduct:
        case ShaderNodeType::Reflect:
        case ShaderNodeType::HSVToRGB:
        case ShaderNodeType::RGBToHSV:
            return "vec3";
        case ShaderNodeType::Vec4Constant:
        case ShaderNodeType::ColorConstant:
        case ShaderNodeType::VertexColor:
        case ShaderNodeType::CombineVec4:
        case ShaderNodeType::SampleTexture2D:
        case ShaderNodeType::SampleCubemap:
        case ShaderNodeType::Blend:
        case ShaderNodeType::Parallax:
            return "vec4";
        // Arithmetic nodes: type depends on inputs (default float)
        case ShaderNodeType::Add:
        case ShaderNodeType::Subtract:
        case ShaderNodeType::Multiply:
        case ShaderNodeType::Divide:
        case ShaderNodeType::Lerp:
        case ShaderNodeType::Clamp:
            return "float";  // conservative default
        default:
            return "float";
    }
}

// Get the input variable name feeding into a node's pin
static std::string GetInputExpr(const ShaderGraphData& graph, u32 nodeId, u32 pinIdx,
                                 const char* defaultVal = "0.0") {
    for (const auto& link : graph.links) {
        if (link.toNode == nodeId && link.toPin == pinIdx) {
            return NodeVar(link.fromNode, link.fromPin);
        }
    }
    return defaultVal;
}

ShaderCodeResult ShaderGraphEditor::GenerateGLSL() const {
    ShaderCodeResult result;
    if (!m_Graph || m_Graph->nodes.empty()) {
        result.errors.push_back("No graph data or empty graph");
        return result;
    }

    const auto& graph = *m_Graph;

    // Build node lookup
    std::unordered_map<u32, const ShaderGraphNode*> nodeMap;
    for (const auto& node : graph.nodes) {
        nodeMap[node.id] = &node;
    }

    // --- Topological sort (Kahn's algorithm) ---
    std::unordered_map<u32, u32> inDegree;
    std::unordered_map<u32, std::vector<u32>> adj; // from -> [to]
    for (const auto& node : graph.nodes) {
        inDegree[node.id] = 0;
    }
    for (const auto& link : graph.links) {
        adj[link.fromNode].push_back(link.toNode);
        inDegree[link.toNode]++;
    }

    std::vector<u32> sorted;
    std::queue<u32> ready;
    for (const auto& node : graph.nodes) {
        if (inDegree[node.id] == 0) ready.push(node.id);
    }
    while (!ready.empty()) {
        u32 cur = ready.front();
        ready.pop();
        sorted.push_back(cur);
        for (u32 next : adj[cur]) {
            if (--inDegree[next] == 0) ready.push(next);
        }
    }
    if (sorted.size() != graph.nodes.size()) {
        result.errors.push_back("Cycle detected in shader graph");
        return result;
    }

    // --- Collect texture samplers ---
    u32 samplerBinding = 3; // Start after existing bindings 0-2
    std::unordered_map<u32, u32> nodeSamplerBinding;
    for (u32 nid : sorted) {
        auto* node = nodeMap[nid];
        if (!node) continue;
        if (node->type == ShaderNodeType::SampleTexture2D ||
            node->type == ShaderNodeType::SampleCubemap ||
            node->type == ShaderNodeType::TextureParameter) {
            nodeSamplerBinding[nid] = samplerBinding++;
        }
    }

    // --- Generate fragment shader body ---
    std::string body;
    for (u32 nid : sorted) {
        auto* node = nodeMap[nid];
        if (!node) continue;

        std::string var = NodeVar(nid);
        const char* outType = NodeOutType(node->type);

        switch (node->type) {
            // --- Inputs ---
            case ShaderNodeType::VertexPosition:
                body += "    vec3 " + var + " = fragWorldPos;\n";
                break;
            case ShaderNodeType::VertexNormal:
                body += "    vec3 " + var + " = fragNormal;\n";
                break;
            case ShaderNodeType::VertexUV:
                body += "    vec2 " + var + " = fragUV;\n";
                break;
            case ShaderNodeType::VertexColor:
                body += "    vec4 " + var + " = fragColor;\n";
                break;
            case ShaderNodeType::Time:
                body += "    float " + var + " = pc.emissiveStrength;\n"; // Reuse push constant
                break;
            case ShaderNodeType::CameraPosition:
                body += "    vec3 " + var + " = lighting.cameraPos;\n";
                break;
            case ShaderNodeType::ViewDirection:
                body += "    vec3 " + var + " = normalize(lighting.cameraPos - fragWorldPos);\n";
                break;
            case ShaderNodeType::FloatConstant:
                body += "    float " + var + " = " + std::to_string(node->floatValue) + ";\n";
                break;
            case ShaderNodeType::Vec2Constant:
                body += "    vec2 " + var + " = vec2(" +
                    std::to_string(node->vec2Value.x) + ", " +
                    std::to_string(node->vec2Value.y) + ");\n";
                break;
            case ShaderNodeType::Vec3Constant:
                body += "    vec3 " + var + " = vec3(" +
                    std::to_string(node->vec3Value.x) + ", " +
                    std::to_string(node->vec3Value.y) + ", " +
                    std::to_string(node->vec3Value.z) + ");\n";
                break;
            case ShaderNodeType::Vec4Constant:
                body += "    vec4 " + var + " = vec4(" +
                    std::to_string(node->vec4Value.x) + ", " +
                    std::to_string(node->vec4Value.y) + ", " +
                    std::to_string(node->vec4Value.z) + ", " +
                    std::to_string(node->vec4Value.w) + ");\n";
                break;
            case ShaderNodeType::ColorConstant:
                body += "    vec4 " + var + " = vec4(" +
                    std::to_string(node->vec4Value.x) + ", " +
                    std::to_string(node->vec4Value.y) + ", " +
                    std::to_string(node->vec4Value.z) + ", " +
                    std::to_string(node->vec4Value.w) + ");\n";
                break;
            case ShaderNodeType::FloatParameter:
                body += "    float " + var + " = " + std::to_string(node->floatValue) + "; // param: " + node->parameterName + "\n";
                break;
            case ShaderNodeType::TextureParameter:
                // Handled as sampler below
                break;

            // --- Math ---
            case ShaderNodeType::Add: {
                auto a = GetInputExpr(graph, nid, 0, "0.0");
                auto b = GetInputExpr(graph, nid, 1, "0.0");
                body += "    " + std::string(outType) + " " + var + " = " + a + " + " + b + ";\n";
                break;
            }
            case ShaderNodeType::Subtract: {
                auto a = GetInputExpr(graph, nid, 0, "0.0");
                auto b = GetInputExpr(graph, nid, 1, "0.0");
                body += "    " + std::string(outType) + " " + var + " = " + a + " - " + b + ";\n";
                break;
            }
            case ShaderNodeType::Multiply: {
                auto a = GetInputExpr(graph, nid, 0, "1.0");
                auto b = GetInputExpr(graph, nid, 1, "1.0");
                body += "    " + std::string(outType) + " " + var + " = " + a + " * " + b + ";\n";
                break;
            }
            case ShaderNodeType::Divide: {
                auto a = GetInputExpr(graph, nid, 0, "1.0");
                auto b = GetInputExpr(graph, nid, 1, "1.0");
                body += "    " + std::string(outType) + " " + var + " = " + a + " / max(" + b + ", 0.0001);\n";
                break;
            }
            case ShaderNodeType::Lerp: {
                auto a = GetInputExpr(graph, nid, 0, "0.0");
                auto b = GetInputExpr(graph, nid, 1, "1.0");
                auto t = GetInputExpr(graph, nid, 2, "0.5");
                body += "    " + std::string(outType) + " " + var + " = mix(" + a + ", " + b + ", " + t + ");\n";
                break;
            }
            case ShaderNodeType::Clamp: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                auto lo = GetInputExpr(graph, nid, 1, "0.0");
                auto hi = GetInputExpr(graph, nid, 2, "1.0");
                body += "    " + std::string(outType) + " " + var + " = clamp(" + x + ", " + lo + ", " + hi + ");\n";
                break;
            }
            case ShaderNodeType::Saturate: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                body += "    float " + var + " = clamp(" + x + ", 0.0, 1.0);\n";
                break;
            }
            case ShaderNodeType::Abs: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                body += "    float " + var + " = abs(" + x + ");\n";
                break;
            }
            case ShaderNodeType::Negate: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                body += "    float " + var + " = -(" + x + ");\n";
                break;
            }
            case ShaderNodeType::Sin: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                body += "    float " + var + " = sin(" + x + ");\n";
                break;
            }
            case ShaderNodeType::Cos: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                body += "    float " + var + " = cos(" + x + ");\n";
                break;
            }
            case ShaderNodeType::Pow: {
                auto a = GetInputExpr(graph, nid, 0, "1.0");
                auto b = GetInputExpr(graph, nid, 1, "2.0");
                body += "    float " + var + " = pow(max(" + a + ", 0.0), " + b + ");\n";
                break;
            }
            case ShaderNodeType::Sqrt: {
                auto x = GetInputExpr(graph, nid, 0, "1.0");
                body += "    float " + var + " = sqrt(max(" + x + ", 0.0));\n";
                break;
            }
            case ShaderNodeType::Floor: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                body += "    float " + var + " = floor(" + x + ");\n";
                break;
            }
            case ShaderNodeType::Ceil: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                body += "    float " + var + " = ceil(" + x + ");\n";
                break;
            }
            case ShaderNodeType::Fract: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                body += "    float " + var + " = fract(" + x + ");\n";
                break;
            }
            case ShaderNodeType::Min: {
                auto a = GetInputExpr(graph, nid, 0, "0.0");
                auto b = GetInputExpr(graph, nid, 1, "1.0");
                body += "    float " + var + " = min(" + a + ", " + b + ");\n";
                break;
            }
            case ShaderNodeType::Max: {
                auto a = GetInputExpr(graph, nid, 0, "0.0");
                auto b = GetInputExpr(graph, nid, 1, "1.0");
                body += "    float " + var + " = max(" + a + ", " + b + ");\n";
                break;
            }
            case ShaderNodeType::Step: {
                auto edge = GetInputExpr(graph, nid, 0, "0.5");
                auto x = GetInputExpr(graph, nid, 1, "0.0");
                body += "    float " + var + " = step(" + edge + ", " + x + ");\n";
                break;
            }
            case ShaderNodeType::SmoothStep: {
                auto lo = GetInputExpr(graph, nid, 0, "0.0");
                auto hi = GetInputExpr(graph, nid, 1, "1.0");
                auto x = GetInputExpr(graph, nid, 2, "0.5");
                body += "    float " + var + " = smoothstep(" + lo + ", " + hi + ", " + x + ");\n";
                break;
            }

            // --- Texture ---
            case ShaderNodeType::SampleTexture2D: {
                auto uv = GetInputExpr(graph, nid, 0, "fragUV");
                auto it = nodeSamplerBinding.find(nid);
                u32 bind = it != nodeSamplerBinding.end() ? it->second : 3;
                body += "    vec4 " + var + " = texture(uSampler" + std::to_string(bind) + ", " + uv + ");\n";
                break;
            }
            case ShaderNodeType::SampleCubemap: {
                auto dir = GetInputExpr(graph, nid, 0, "fragNormal");
                auto it = nodeSamplerBinding.find(nid);
                u32 bind = it != nodeSamplerBinding.end() ? it->second : 3;
                body += "    vec4 " + var + " = texture(uSamplerCube" + std::to_string(bind) + ", " + dir + ");\n";
                break;
            }
            case ShaderNodeType::UVTransform: {
                auto uv = GetInputExpr(graph, nid, 0, "fragUV");
                auto scale = GetInputExpr(graph, nid, 1, "1.0");
                auto offset = GetInputExpr(graph, nid, 2, "0.0");
                body += "    vec2 " + var + " = " + uv + " * " + scale + " + " + offset + ";\n";
                break;
            }
            case ShaderNodeType::Parallax: {
                auto uv = GetInputExpr(graph, nid, 0, "fragUV");
                body += "    vec4 " + var + " = vec4(" + uv + ", 0.0, 1.0); // Parallax placeholder\n";
                break;
            }
            case ShaderNodeType::Flipbook: {
                auto uv = GetInputExpr(graph, nid, 0, "fragUV");
                body += "    vec2 " + var + " = " + uv + "; // Flipbook placeholder\n";
                break;
            }

            // --- Color ---
            case ShaderNodeType::HSVToRGB: {
                auto hsv = GetInputExpr(graph, nid, 0, "vec3(0.0, 1.0, 1.0)");
                body += "    vec3 " + var + ";\n";
                body += "    { vec3 c = " + hsv + "; vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0/3.0, 1.0/3.0)) * 6.0 - 3.0);\n";
                body += "      " + var + " = c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y); }\n";
                break;
            }
            case ShaderNodeType::RGBToHSV: {
                auto rgb = GetInputExpr(graph, nid, 0, "vec3(1.0)");
                body += "    vec3 " + var + ";\n";
                body += "    { vec3 c = " + rgb + "; float mx = max(c.r, max(c.g, c.b)), mn = min(c.r, min(c.g, c.b));\n";
                body += "      float d = mx - mn; float h = (d == 0.0) ? 0.0 : (mx == c.r) ? mod((c.g - c.b)/d, 6.0)/6.0 : (mx == c.g) ? ((c.b-c.r)/d+2.0)/6.0 : ((c.r-c.g)/d+4.0)/6.0;\n";
                body += "      " + var + " = vec3(h, (mx == 0.0) ? 0.0 : d/mx, mx); }\n";
                break;
            }
            case ShaderNodeType::Brightness: {
                auto col = GetInputExpr(graph, nid, 0, "vec3(1.0)");
                auto amt = GetInputExpr(graph, nid, 1, "1.0");
                body += "    float " + var + " = dot(" + col + ", vec3(0.299, 0.587, 0.114)) * " + amt + ";\n";
                break;
            }
            case ShaderNodeType::Contrast: {
                auto col = GetInputExpr(graph, nid, 0, "0.5");
                auto amt = GetInputExpr(graph, nid, 1, "1.0");
                body += "    float " + var + " = (" + col + " - 0.5) * " + amt + " + 0.5;\n";
                break;
            }
            case ShaderNodeType::Blend: {
                auto a = GetInputExpr(graph, nid, 0, "vec4(0.0)");
                auto b = GetInputExpr(graph, nid, 1, "vec4(1.0)");
                auto t = GetInputExpr(graph, nid, 2, "0.5");
                body += "    vec4 " + var + " = mix(" + a + ", " + b + ", " + t + ");\n";
                break;
            }

            // --- Vector ---
            case ShaderNodeType::SplitVec2: {
                auto v = GetInputExpr(graph, nid, 0, "vec2(0.0)");
                body += "    float " + NodeVar(nid, 0) + " = " + v + ".x;\n";
                body += "    float " + NodeVar(nid, 1) + " = " + v + ".y;\n";
                break;
            }
            case ShaderNodeType::SplitVec3: {
                auto v = GetInputExpr(graph, nid, 0, "vec3(0.0)");
                body += "    float " + NodeVar(nid, 0) + " = " + v + ".x;\n";
                body += "    float " + NodeVar(nid, 1) + " = " + v + ".y;\n";
                body += "    float " + NodeVar(nid, 2) + " = " + v + ".z;\n";
                break;
            }
            case ShaderNodeType::SplitVec4: {
                auto v = GetInputExpr(graph, nid, 0, "vec4(0.0)");
                body += "    float " + NodeVar(nid, 0) + " = " + v + ".x;\n";
                body += "    float " + NodeVar(nid, 1) + " = " + v + ".y;\n";
                body += "    float " + NodeVar(nid, 2) + " = " + v + ".z;\n";
                body += "    float " + NodeVar(nid, 3) + " = " + v + ".w;\n";
                break;
            }
            case ShaderNodeType::CombineVec2: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                auto y = GetInputExpr(graph, nid, 1, "0.0");
                body += "    vec2 " + var + " = vec2(" + x + ", " + y + ");\n";
                break;
            }
            case ShaderNodeType::CombineVec3: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                auto y = GetInputExpr(graph, nid, 1, "0.0");
                auto z = GetInputExpr(graph, nid, 2, "0.0");
                body += "    vec3 " + var + " = vec3(" + x + ", " + y + ", " + z + ");\n";
                break;
            }
            case ShaderNodeType::CombineVec4: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                auto y = GetInputExpr(graph, nid, 1, "0.0");
                auto z = GetInputExpr(graph, nid, 2, "0.0");
                auto w = GetInputExpr(graph, nid, 3, "1.0");
                body += "    vec4 " + var + " = vec4(" + x + ", " + y + ", " + z + ", " + w + ");\n";
                break;
            }
            case ShaderNodeType::Normalize: {
                auto v = GetInputExpr(graph, nid, 0, "vec3(0.0, 1.0, 0.0)");
                body += "    vec3 " + var + " = normalize(" + v + ");\n";
                break;
            }
            case ShaderNodeType::DotProduct: {
                auto a = GetInputExpr(graph, nid, 0, "vec3(0.0)");
                auto b = GetInputExpr(graph, nid, 1, "vec3(0.0)");
                body += "    float " + var + " = dot(" + a + ", " + b + ");\n";
                break;
            }
            case ShaderNodeType::CrossProduct: {
                auto a = GetInputExpr(graph, nid, 0, "vec3(1.0, 0.0, 0.0)");
                auto b = GetInputExpr(graph, nid, 1, "vec3(0.0, 1.0, 0.0)");
                body += "    vec3 " + var + " = cross(" + a + ", " + b + ");\n";
                break;
            }
            case ShaderNodeType::Reflect: {
                auto incident = GetInputExpr(graph, nid, 0, "vec3(0.0, -1.0, 0.0)");
                auto normal = GetInputExpr(graph, nid, 1, "vec3(0.0, 1.0, 0.0)");
                body += "    vec3 " + var + " = reflect(" + incident + ", " + normal + ");\n";
                break;
            }

            // --- Output ---
            case ShaderNodeType::FragmentOutput: {
                auto col = GetInputExpr(graph, nid, 0, "vec4(1.0)");
                auto alpha = GetInputExpr(graph, nid, 1, "1.0");
                body += "    outColor = vec4(" + col + ".rgb, " + alpha + ");\n";
                break;
            }
            case ShaderNodeType::VertexOutput:
                // Vertex displacement handled separately
                break;

            // --- Utility ---
            case ShaderNodeType::Comment:
                if (!node->label.empty()) {
                    body += "    // " + node->label + "\n";
                }
                break;
            case ShaderNodeType::Reroute: {
                auto x = GetInputExpr(graph, nid, 0, "0.0");
                body += "    " + std::string(outType) + " " + var + " = " + x + ";\n";
                break;
            }
        }
    }

    // --- Build sampler declarations ---
    std::string samplerDecls;
    for (const auto& [nid, binding] : nodeSamplerBinding) {
        auto nodeIt = nodeMap.find(nid);
        if (nodeIt == nodeMap.end()) {
            ENJIN_LOG_WARN(Editor, "ShaderGraph: Node %u referenced in sampler bindings not found in nodeMap, skipping", nid);
            continue;
        }
        auto* node = nodeIt->second;
        if (node->type == ShaderNodeType::SampleCubemap) {
            samplerDecls += "layout(set = 0, binding = " + std::to_string(binding) +
                ") uniform samplerCube uSamplerCube" + std::to_string(binding) + ";\n";
        } else {
            samplerDecls += "layout(set = 0, binding = " + std::to_string(binding) +
                ") uniform sampler2D uSampler" + std::to_string(binding) + ";\n";
        }
    }

    // --- Assemble vertex shader ---
    result.vertexCode =
        "// Generated by Enjin Shader Graph\n"
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
        "    vec3 baseColor; float metallic;\n"
        "    vec3 emissiveColor; float roughness;\n"
        "    float emissiveStrength, opacity, alphaCutoff;\n"
        "    int flags;\n"
        "    float parallaxScale;\n"
        "} pc;\n"
        "\n"
        "layout(location = 0) out vec3 fragNormal;\n"
        "layout(location = 1) out vec2 fragUV;\n"
        "layout(location = 2) out vec4 fragColor;\n"
        "layout(location = 3) out vec3 fragWorldPos;\n"
        "\n"
        "void main() {\n"
        "    vec4 worldPos = pc.model * vec4(inPosition, 1.0);\n"
        "    fragWorldPos = worldPos.xyz;\n"
        "    gl_Position = vp.proj * vp.view * worldPos;\n"
        "    fragNormal = mat3(pc.model) * inNormal;\n"
        "    fragUV = inUV;\n"
        "    fragColor = inColor;\n"
        "}\n";

    // --- Assemble fragment shader ---
    result.fragmentCode =
        "// Generated by Enjin Shader Graph\n"
        "#version 450\n"
        "\n"
        "layout(location = 0) in vec3 fragNormal;\n"
        "layout(location = 1) in vec2 fragUV;\n"
        "layout(location = 2) in vec4 fragColor;\n"
        "layout(location = 3) in vec3 fragWorldPos;\n"
        "\n"
        "layout(location = 0) out vec4 outColor;\n"
        "\n"
        "layout(set = 0, binding = 1) uniform LightingUBO {\n"
        "    vec3 cameraPos;\n"
        "    // ... (simplified)\n"
        "} lighting;\n"
        "\n"
        "layout(push_constant) uniform PushConstants {\n"
        "    mat4 model;\n"
        "    vec3 baseColor; float metallic;\n"
        "    vec3 emissiveColor; float roughness;\n"
        "    float emissiveStrength, opacity, alphaCutoff;\n"
        "    int flags;\n"
        "    float parallaxScale;\n"
        "} pc;\n"
        "\n" + samplerDecls +
        "\n"
        "void main() {\n" +
        body +
        "}\n";

    result.success = true;
    return result;
}

// ============================================================================
// Save/Load (.enjshader JSON format)
// ============================================================================

bool ShaderGraphEditor::Save(const std::string& path) const {
    if (!m_Graph) return false;

    std::string json = "{\n";
    json += "  \"name\": \"" + m_Graph->name + "\",\n";
    json += "  \"stage\": " + std::to_string(static_cast<int>(m_Graph->stage)) + ",\n";
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
                ", \"float\": " + std::to_string(n.floatValue) +
                ", \"v2\": [" + std::to_string(n.vec2Value.x) + "," + std::to_string(n.vec2Value.y) + "]" +
                ", \"v3\": [" + std::to_string(n.vec3Value.x) + "," + std::to_string(n.vec3Value.y) + "," + std::to_string(n.vec3Value.z) + "]" +
                ", \"v4\": [" + std::to_string(n.vec4Value.x) + "," + std::to_string(n.vec4Value.y) + "," + std::to_string(n.vec4Value.z) + "," + std::to_string(n.vec4Value.w) + "]" +
                ", \"tex\": \"" + n.texturePath + "\"" +
                ", \"param\": \"" + n.parameterName + "\"" +
                " }" + (i + 1 < m_Graph->nodes.size() ? ",\n" : "\n");
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
                " }" + (i + 1 < m_Graph->links.size() ? ",\n" : "\n");
    }
    json += "  ]\n";
    json += "}\n";

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << json;
    return true;
}

bool ShaderGraphEditor::Load(const std::string& path) {
    if (!m_Graph) return false;

    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::string json((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

    // Simple JSON parsing (reuse nlohmann if available, otherwise manual)
    // For robustness, use the same approach as other serializers in the engine
    try {
        nlohmann::json j = nlohmann::json::parse(json);
        m_Graph->name = j.value("name", "Untitled");
        m_Graph->stage = static_cast<ShaderGraphData::Stage>(j.value("stage", 1));
        m_Graph->nextNodeId = j.value("nextNodeId", 1u);
        m_Graph->nextLinkId = j.value("nextLinkId", 1u);

        m_Graph->nodes.clear();
        if (j.contains("nodes")) {
            for (const auto& nj : j["nodes"]) {
                ShaderGraphNode n;
                n.id = nj.value("id", 0u);
                n.type = static_cast<ShaderNodeType>(nj.value("type", 0));
                n.position.x = nj.value("x", 0.0f);
                n.position.y = nj.value("y", 0.0f);
                n.label = nj.value("label", "");
                n.floatValue = nj.value("float", 0.0f);
                if (nj.contains("v2")) { n.vec2Value.x = nj["v2"][0]; n.vec2Value.y = nj["v2"][1]; }
                if (nj.contains("v3")) { n.vec3Value.x = nj["v3"][0]; n.vec3Value.y = nj["v3"][1]; n.vec3Value.z = nj["v3"][2]; }
                if (nj.contains("v4")) { n.vec4Value.x = nj["v4"][0]; n.vec4Value.y = nj["v4"][1]; n.vec4Value.z = nj["v4"][2]; n.vec4Value.w = nj["v4"][3]; }
                n.texturePath = nj.value("tex", "");
                n.parameterName = nj.value("param", "");
                m_Graph->nodes.push_back(n);
            }
        }

        m_Graph->links.clear();
        if (j.contains("links")) {
            for (const auto& lj : j["links"]) {
                ShaderGraphLink l;
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

} // namespace Editor
} // namespace Enjin
