#include "Enjin/Editor/NodeGraph.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

using json = nlohmann::json;

namespace Enjin {
namespace Editor {

// ============================================================================
// Layout Constants
// ============================================================================

static constexpr f32 NODE_WIDTH      = 180.0f;
static constexpr f32 NODE_HEADER_H   = 28.0f;
static constexpr f32 PIN_ROW_H       = 22.0f;
static constexpr f32 PIN_RADIUS      = 6.0f;
static constexpr f32 PIN_MARGIN      = 12.0f;
static constexpr f32 NODE_ROUNDING   = 6.0f;
static constexpr f32 GRID_MINOR      = 20.0f;
static constexpr f32 GRID_MAJOR      = 100.0f;
static constexpr f32 LINK_THICKNESS  = 2.5f;
static constexpr f32 MINIMAP_SIZE    = 160.0f;
static constexpr f32 MINIMAP_MARGIN  = 10.0f;

// ============================================================================
// NodeGraphData Implementation
// ============================================================================

NodeId NodeGraphData::AddNode(const std::string& title, Math::Vector2 position,
                               Math::Vector3 headerColor) {
    GraphNode node;
    node.id = m_NextNodeId++;
    node.title = title;
    node.position = position;
    node.headerColor = headerColor;
    m_Nodes.push_back(std::move(node));
    return m_Nodes.back().id;
}

void NodeGraphData::RemoveNode(NodeId id) {
    // Remove all links connected to this node's pins
    auto* node = FindNode(id);
    if (!node) return;

    std::vector<PinId> pinIds;
    for (auto& p : node->inputs) pinIds.push_back(p.id);
    for (auto& p : node->outputs) pinIds.push_back(p.id);

    m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(),
        [&](const GraphLink& l) {
            for (auto pid : pinIds) {
                if (l.startPinId == pid || l.endPinId == pid) return true;
            }
            return false;
        }), m_Links.end());

    m_Nodes.erase(std::remove_if(m_Nodes.begin(), m_Nodes.end(),
        [id](const GraphNode& n) { return n.id == id; }), m_Nodes.end());
}

GraphNode* NodeGraphData::FindNode(NodeId id) {
    for (auto& n : m_Nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

const GraphNode* NodeGraphData::FindNode(NodeId id) const {
    for (auto& n : m_Nodes) {
        if (n.id == id) return &n;
    }
    return nullptr;
}

PinId NodeGraphData::AddPin(NodeId nodeId, const std::string& name,
                             PinType type, PinKind kind) {
    auto* node = FindNode(nodeId);
    if (!node) return 0;

    Pin pin;
    pin.id = m_NextPinId++;
    pin.name = name;
    pin.type = type;
    pin.kind = kind;
    pin.nodeId = nodeId;

    if (kind == PinKind::Input)
        node->inputs.push_back(pin);
    else
        node->outputs.push_back(pin);

    return pin.id;
}

Pin* NodeGraphData::FindPin(PinId id) {
    for (auto& n : m_Nodes) {
        for (auto& p : n.inputs) if (p.id == id) return &p;
        for (auto& p : n.outputs) if (p.id == id) return &p;
    }
    return nullptr;
}

const Pin* NodeGraphData::FindPin(PinId id) const {
    for (auto& n : m_Nodes) {
        for (auto& p : n.inputs) if (p.id == id) return &p;
        for (auto& p : n.outputs) if (p.id == id) return &p;
    }
    return nullptr;
}

NodeId NodeGraphData::GetPinOwner(PinId id) const {
    auto* pin = FindPin(id);
    return pin ? pin->nodeId : 0;
}

LinkId NodeGraphData::AddLink(PinId startPin, PinId endPin, u64 userData) {
    GraphLink link;
    link.id = m_NextLinkId++;
    link.startPinId = startPin;
    link.endPinId = endPin;
    link.userData = userData;
    m_Links.push_back(link);
    return link.id;
}

void NodeGraphData::RemoveLink(LinkId id) {
    m_Links.erase(std::remove_if(m_Links.begin(), m_Links.end(),
        [id](const GraphLink& l) { return l.id == id; }), m_Links.end());
}

GraphLink* NodeGraphData::FindLink(LinkId id) {
    for (auto& l : m_Links) if (l.id == id) return &l;
    return nullptr;
}

const GraphLink* NodeGraphData::FindLink(LinkId id) const {
    for (auto& l : m_Links) if (l.id == id) return &l;
    return nullptr;
}

std::vector<LinkId> NodeGraphData::GetLinksForPin(PinId pinId) const {
    std::vector<LinkId> result;
    for (auto& l : m_Links) {
        if (l.startPinId == pinId || l.endPinId == pinId)
            result.push_back(l.id);
    }
    return result;
}

std::vector<LinkId> NodeGraphData::GetLinksForNode(NodeId nodeId) const {
    auto* node = FindNode(nodeId);
    if (!node) return {};

    std::vector<PinId> pinIds;
    for (auto& p : node->inputs) pinIds.push_back(p.id);
    for (auto& p : node->outputs) pinIds.push_back(p.id);

    std::vector<LinkId> result;
    for (auto& l : m_Links) {
        for (auto pid : pinIds) {
            if (l.startPinId == pid || l.endPinId == pid) {
                result.push_back(l.id);
                break;
            }
        }
    }
    return result;
}

bool NodeGraphData::HasLinkBetween(PinId a, PinId b) const {
    for (auto& l : m_Links) {
        if ((l.startPinId == a && l.endPinId == b) ||
            (l.startPinId == b && l.endPinId == a))
            return true;
    }
    return false;
}

json NodeGraphData::ToJson() const {
    json j;
    j["nextNodeId"] = m_NextNodeId;
    j["nextPinId"]  = m_NextPinId;
    j["nextLinkId"] = m_NextLinkId;

    json nodes = json::array();
    for (auto& n : m_Nodes) {
        json jn;
        jn["id"] = n.id;
        jn["title"] = n.title;
        jn["position"] = { n.position.x, n.position.y };
        jn["headerColor"] = { n.headerColor.x, n.headerColor.y, n.headerColor.z };
        jn["flags"] = static_cast<u32>(n.flags);
        jn["userData"] = n.userData;

        json inputs = json::array();
        for (auto& p : n.inputs) {
            inputs.push_back({
                {"id", p.id}, {"name", p.name},
                {"type", static_cast<i32>(p.type)}, {"kind", static_cast<i32>(p.kind)},
                {"nodeId", p.nodeId}
            });
        }
        jn["inputs"] = inputs;

        json outputs = json::array();
        for (auto& p : n.outputs) {
            outputs.push_back({
                {"id", p.id}, {"name", p.name},
                {"type", static_cast<i32>(p.type)}, {"kind", static_cast<i32>(p.kind)},
                {"nodeId", p.nodeId}
            });
        }
        jn["outputs"] = outputs;
        nodes.push_back(jn);
    }
    j["nodes"] = nodes;

    json links = json::array();
    for (auto& l : m_Links) {
        links.push_back({
            {"id", l.id}, {"startPinId", l.startPinId},
            {"endPinId", l.endPinId}, {"userData", l.userData}
        });
    }
    j["links"] = links;
    return j;
}

void NodeGraphData::FromJson(const json& j) {
    Clear();
    if (j.contains("nextNodeId")) m_NextNodeId = j["nextNodeId"].get<u32>();
    if (j.contains("nextPinId"))  m_NextPinId  = j["nextPinId"].get<u32>();
    if (j.contains("nextLinkId")) m_NextLinkId = j["nextLinkId"].get<u32>();

    if (j.contains("nodes") && j["nodes"].is_array()) {
        for (auto& jn : j["nodes"]) {
            GraphNode node;
            node.id = jn.value("id", 0u);
            node.title = jn.value("title", std::string());
            if (jn.contains("position") && jn["position"].is_array() && jn["position"].size() >= 2) {
                node.position.x = jn["position"][0].get<f32>();
                node.position.y = jn["position"][1].get<f32>();
            }
            if (jn.contains("headerColor") && jn["headerColor"].is_array() && jn["headerColor"].size() >= 3) {
                node.headerColor.x = jn["headerColor"][0].get<f32>();
                node.headerColor.y = jn["headerColor"][1].get<f32>();
                node.headerColor.z = jn["headerColor"][2].get<f32>();
            }
            node.flags = static_cast<NodeFlags>(jn.value("flags", 0u));
            node.userData = jn.value("userData", (u64)0);

            auto deserializePins = [](const json& arr, std::vector<Pin>& pins) {
                if (!arr.is_array()) return;
                for (auto& jp : arr) {
                    Pin p;
                    p.id = jp.value("id", 0u);
                    p.name = jp.value("name", std::string());
                    p.type = static_cast<PinType>(jp.value("type", 0));
                    p.kind = static_cast<PinKind>(jp.value("kind", 0));
                    p.nodeId = jp.value("nodeId", 0u);
                    pins.push_back(p);
                }
            };
            if (jn.contains("inputs")) deserializePins(jn["inputs"], node.inputs);
            if (jn.contains("outputs")) deserializePins(jn["outputs"], node.outputs);
            m_Nodes.push_back(std::move(node));
        }
    }

    if (j.contains("links") && j["links"].is_array()) {
        for (auto& jl : j["links"]) {
            GraphLink link;
            link.id = jl.value("id", 0u);
            link.startPinId = jl.value("startPinId", 0u);
            link.endPinId = jl.value("endPinId", 0u);
            link.userData = jl.value("userData", (u64)0);
            m_Links.push_back(link);
        }
    }
}

void NodeGraphData::Clear() {
    m_Nodes.clear();
    m_Links.clear();
    m_NextNodeId = 1;
    m_NextPinId = 1;
    m_NextLinkId = 1;
}

// ============================================================================
// NodeGraphColors
// ============================================================================

NodeGraphColors NodeGraphColors::FromTheme(const EditorSettings& settings) {
    NodeGraphColors c;
    bool highContrast = (settings.theme == EditorTheme::HighContrastDark ||
                         settings.theme == EditorTheme::HighContrastLight);
    bool lightTheme = (settings.theme == EditorTheme::Light ||
                       settings.theme == EditorTheme::HighContrastLight);

    if (lightTheme) {
        c.canvasBg     = 0xFFF0F0F0;
        c.gridMinor    = 0xFFE0E0E0;
        c.gridMajor    = 0xFFC8C8C8;
        c.nodeBody     = 0xFFFFFFFF;
        c.nodeBorder   = 0xFFB0B0B0;
        c.nodeText     = 0xFF202020;
        c.linkDefault  = 0xFF606060;
        c.minimapBg    = 0xC0E0E0E0;
        c.minimapNode  = 0x80A0A0A0;
    }

    if (highContrast) {
        c.nodeBorder   = lightTheme ? 0xFF808080 : 0xFF808080;
        c.nodeSelected = 0xFFFFCC00;
    }

    return c;
}

// ============================================================================
// NodeGraphEditor — Coordinate Transforms
// ============================================================================

ImVec2 NodeGraphEditor::CanvasToScreen(Math::Vector2 pos, ImVec2 origin) const {
    return ImVec2(origin.x + pos.x * m_Zoom + m_ScrollOffset.x,
                  origin.y + pos.y * m_Zoom + m_ScrollOffset.y);
}

Math::Vector2 NodeGraphEditor::ScreenToCanvas(ImVec2 screenPos, ImVec2 origin) const {
    return Math::Vector2(
        (screenPos.x - origin.x - m_ScrollOffset.x) / m_Zoom,
        (screenPos.y - origin.y - m_ScrollOffset.y) / m_Zoom
    );
}

ImVec2 NodeGraphEditor::GetNodeSize(const GraphNode& node, f32 uiScale) const {
    f32 s = m_Zoom * uiScale;
    usize pinRows = std::max(node.inputs.size(), node.outputs.size());
    f32 bodyH = static_cast<f32>(pinRows) * PIN_ROW_H;
    if (pinRows == 0) bodyH = PIN_ROW_H;  // Minimum body height
    return ImVec2(NODE_WIDTH * s, (NODE_HEADER_H + bodyH) * s);
}

ImVec2 NodeGraphEditor::GetPinScreenPos(const GraphNode& node, const Pin& pin,
                                          ImVec2 canvasOrigin, f32 uiScale) const {
    f32 s = m_Zoom * uiScale;
    ImVec2 nodeScreen = CanvasToScreen(node.position, canvasOrigin);

    // Find pin index in its list
    usize idx = 0;
    if (pin.kind == PinKind::Input) {
        for (usize i = 0; i < node.inputs.size(); i++) {
            if (node.inputs[i].id == pin.id) { idx = i; break; }
        }
    } else {
        for (usize i = 0; i < node.outputs.size(); i++) {
            if (node.outputs[i].id == pin.id) { idx = i; break; }
        }
    }

    f32 y = nodeScreen.y + (NODE_HEADER_H + PIN_ROW_H * 0.5f + PIN_ROW_H * static_cast<f32>(idx)) * s;
    f32 x = (pin.kind == PinKind::Input)
        ? nodeScreen.x
        : nodeScreen.x + NODE_WIDTH * s;

    return ImVec2(x, y);
}

// ============================================================================
// NodeGraphEditor — Rendering
// ============================================================================

void NodeGraphEditor::Render(NodeGraphData& data, NodeGraphCallbacks& callbacks,
                              const NodeGraphColors& colors, f32 uiScale) {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    if (canvasSize.x < 50 || canvasSize.y < 50) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Canvas background
    dl->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        colors.canvasBg);

    // Clip to canvas
    dl->PushClipRect(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

    // Invisible button to capture input over canvas area
    ImGui::InvisibleButton("##NodeGraphCanvas", canvasSize,
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
        ImGuiButtonFlags_MouseButtonMiddle);
    bool canvasHovered = ImGui::IsItemHovered();

    DrawGrid(dl, canvasPos, canvasSize, colors, uiScale);
    DrawLinks(dl, canvasPos, data, colors, uiScale);
    DrawNodes(dl, canvasPos, data, callbacks, colors, uiScale);

    // Drag-to-connect preview line
    if (m_IsDraggingPin) {
        auto* startPin = data.FindPin(m_DragStartPin);
        if (startPin) {
            auto* ownerNode = data.FindNode(startPin->nodeId);
            if (ownerNode) {
                ImVec2 startPos = GetPinScreenPos(*ownerNode, *startPin, canvasPos, uiScale);
                ImVec2 endPos = m_DragEndPos;

                f32 dist = std::abs(endPos.x - startPos.x) * 0.5f;
                f32 cpDist = std::max(dist, 40.0f * m_Zoom * uiScale);

                ImVec2 cp1, cp2;
                if (startPin->kind == PinKind::Output) {
                    cp1 = ImVec2(startPos.x + cpDist, startPos.y);
                    cp2 = ImVec2(endPos.x - cpDist, endPos.y);
                } else {
                    cp1 = ImVec2(startPos.x - cpDist, startPos.y);
                    cp2 = ImVec2(endPos.x + cpDist, endPos.y);
                }
                dl->AddBezierCubic(startPos, cp1, cp2, endPos,
                    colors.linkPreview, LINK_THICKNESS * m_Zoom * uiScale);
            }
        }
    }

    // Box selection rectangle
    if (m_IsBoxSelecting) {
        ImVec2 bMin(std::min(m_BoxSelectStart.x, ImGui::GetIO().MousePos.x),
                    std::min(m_BoxSelectStart.y, ImGui::GetIO().MousePos.y));
        ImVec2 bMax(std::max(m_BoxSelectStart.x, ImGui::GetIO().MousePos.x),
                    std::max(m_BoxSelectStart.y, ImGui::GetIO().MousePos.y));
        dl->AddRectFilled(bMin, bMax, colors.selectionFill);
        dl->AddRect(bMin, bMax, colors.selectionBorder, 0.0f, 0, 1.5f);
    }

    if (m_ShowMinimap)
        DrawMinimap(dl, canvasPos, canvasSize, data, colors, uiScale);

    dl->PopClipRect();

    // Handle interactions after rendering
    if (canvasHovered) {
        HandleCanvasInteraction(canvasPos, canvasSize, data, callbacks, uiScale);
    }

    // Context menu (rendered outside clip rect)
    DrawContextMenu(data, callbacks, canvasPos, uiScale);

    // Keyboard
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        HandleKeyboard(data, callbacks);
}

void NodeGraphEditor::DrawGrid(ImDrawList* dl, ImVec2 canvasPos, ImVec2 canvasSize,
                                 const NodeGraphColors& colors, f32 uiScale) {
    f32 gridMinor = GRID_MINOR * m_Zoom * uiScale;
    f32 gridMajor = GRID_MAJOR * m_Zoom * uiScale;

    // Minor grid lines
    if (gridMinor > 4.0f) {
        f32 offsetX = std::fmod(m_ScrollOffset.x, gridMinor);
        f32 offsetY = std::fmod(m_ScrollOffset.y, gridMinor);
        for (f32 x = offsetX; x < canvasSize.x; x += gridMinor) {
            dl->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
                        ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                        colors.gridMinor);
        }
        for (f32 y = offsetY; y < canvasSize.y; y += gridMinor) {
            dl->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
                        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                        colors.gridMinor);
        }
    }

    // Major grid lines
    if (gridMajor > 8.0f) {
        f32 offsetX = std::fmod(m_ScrollOffset.x, gridMajor);
        f32 offsetY = std::fmod(m_ScrollOffset.y, gridMajor);
        for (f32 x = offsetX; x < canvasSize.x; x += gridMajor) {
            dl->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
                        ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                        colors.gridMajor);
        }
        for (f32 y = offsetY; y < canvasSize.y; y += gridMajor) {
            dl->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
                        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                        colors.gridMajor);
        }
    }
}

void NodeGraphEditor::DrawNodes(ImDrawList* dl, ImVec2 canvasPos, NodeGraphData& data,
                                  NodeGraphCallbacks& callbacks,
                                  const NodeGraphColors& colors, f32 uiScale) {
    f32 s = m_Zoom * uiScale;

    for (auto& node : data.GetNodes()) {
        ImVec2 nodePos = CanvasToScreen(node.position, canvasPos);
        ImVec2 nodeSize = GetNodeSize(node, uiScale);
        ImVec2 nodeEnd(nodePos.x + nodeSize.x, nodePos.y + nodeSize.y);

        // Node body
        dl->AddRectFilled(nodePos, nodeEnd, colors.nodeBody, NODE_ROUNDING * s);

        // Header background
        ImVec2 headerEnd(nodePos.x + nodeSize.x, nodePos.y + NODE_HEADER_H * s);
        u32 headerCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
            node.headerColor.x, node.headerColor.y, node.headerColor.z, 1.0f));
        dl->AddRectFilled(nodePos, headerEnd, headerCol, NODE_ROUNDING * s,
            ImDrawFlags_RoundCornersTop);

        // Header title
        f32 fontSize = 13.0f * s;
        ImVec2 textPos(nodePos.x + 8.0f * s, nodePos.y + 6.0f * s);
        dl->AddText(nullptr, fontSize, textPos, colors.nodeText, node.title.c_str());

        // Draw pins
        auto drawPin = [&](const Pin& pin, usize idx) {
            ImVec2 pinPos = GetPinScreenPos(node, pin, canvasPos, uiScale);
            f32 r = PIN_RADIUS * s;
            u32 pinCol = colors.pinColors[static_cast<usize>(pin.type)];

            // Pin circle
            bool isConnected = !data.GetLinksForPin(pin.id).empty();
            if (isConnected)
                dl->AddCircleFilled(pinPos, r, pinCol);
            else
                dl->AddCircle(pinPos, r, pinCol, 12, 1.5f * s);

            // Pin label
            f32 labelFontSize = 11.0f * s;
            if (pin.kind == PinKind::Input) {
                ImVec2 labelPos(pinPos.x + r + 4.0f * s, pinPos.y - labelFontSize * 0.5f);
                dl->AddText(nullptr, labelFontSize, labelPos, colors.nodeText, pin.name.c_str());
            } else {
                ImVec2 textSize = ImGui::CalcTextSize(pin.name.c_str());
                f32 scaledW = textSize.x * (labelFontSize / ImGui::GetFontSize());
                ImVec2 labelPos(pinPos.x - r - 4.0f * s - scaledW, pinPos.y - labelFontSize * 0.5f);
                dl->AddText(nullptr, labelFontSize, labelPos, colors.nodeText, pin.name.c_str());
            }

            // Tooltip
            ImVec2 mousePos = ImGui::GetIO().MousePos;
            f32 dx = mousePos.x - pinPos.x;
            f32 dy = mousePos.y - pinPos.y;
            if (dx * dx + dy * dy < (r + 4.0f) * (r + 4.0f)) {
                ImGui::BeginTooltip();
                const char* typeNames[] = {
                    "Flow", "Bool", "Float", "Int", "String",
                    "Vector3", "Color", "Entity", "Any"
                };
                ImGui::Text("%s (%s)", pin.name.c_str(),
                    typeNames[static_cast<usize>(pin.type)]);
                ImGui::TextDisabled("Drag to connect");
                ImGui::EndTooltip();
            }
        };

        for (usize i = 0; i < node.inputs.size(); i++)
            drawPin(node.inputs[i], i);
        for (usize i = 0; i < node.outputs.size(); i++)
            drawPin(node.outputs[i], i);

        // Custom body content
        if (callbacks.DrawNodeContent) {
            ImVec2 bodyMin(nodePos.x, nodePos.y + NODE_HEADER_H * s);
            callbacks.DrawNodeContent(node, dl, bodyMin, nodeEnd, m_Zoom);
        }

        // Border
        f32 borderThickness = 1.5f * s;
        u32 borderCol = colors.nodeBorder;

        if (m_SelectedNodeId == node.id) {
            borderCol = colors.nodeSelected;
            borderThickness = 2.5f * s;
        }
        if (HasFlag(node.flags, NodeFlags::Highlighted)) {
            borderCol = colors.nodeHighlighted;
            borderThickness = 3.0f * s;
        }
        if (HasFlag(node.flags, NodeFlags::Error)) {
            borderCol = colors.nodeError;
            borderThickness = 3.0f * s;
        }
        dl->AddRect(nodePos, nodeEnd, borderCol, NODE_ROUNDING * s, 0, borderThickness);
    }
}

void NodeGraphEditor::DrawLinks(ImDrawList* dl, ImVec2 canvasPos, NodeGraphData& data,
                                  const NodeGraphColors& colors, f32 uiScale) {
    f32 s = m_Zoom * uiScale;
    f32 thickness = LINK_THICKNESS * s;

    for (auto& link : data.GetLinks()) {
        auto* startPin = data.FindPin(link.startPinId);
        auto* endPin = data.FindPin(link.endPinId);
        if (!startPin || !endPin) continue;

        auto* startNode = data.FindNode(startPin->nodeId);
        auto* endNode = data.FindNode(endPin->nodeId);
        if (!startNode || !endNode) continue;

        ImVec2 p1 = GetPinScreenPos(*startNode, *startPin, canvasPos, uiScale);
        ImVec2 p2 = GetPinScreenPos(*endNode, *endPin, canvasPos, uiScale);

        // Bezier control points
        f32 dist = std::abs(p2.x - p1.x) * 0.5f;
        f32 cpDist = std::max(dist, 40.0f * s);

        ImVec2 cp1(p1.x + cpDist, p1.y);
        ImVec2 cp2(p2.x - cpDist, p2.y);

        // Use start pin type color
        u32 linkCol = colors.pinColors[static_cast<usize>(startPin->type)];
        if (m_SelectedLinkId == link.id)
            linkCol = colors.linkSelected;

        dl->AddBezierCubic(p1, cp1, cp2, p2, linkCol, thickness);
    }
}

void NodeGraphEditor::DrawMinimap(ImDrawList* dl, ImVec2 canvasPos, ImVec2 canvasSize,
                                    const NodeGraphData& data,
                                    const NodeGraphColors& colors, f32 uiScale) {
    if (data.GetNodes().empty()) return;

    f32 mmSize = MINIMAP_SIZE * uiScale;
    f32 mmMargin = MINIMAP_MARGIN * uiScale;
    ImVec2 mmPos(canvasPos.x + canvasSize.x - mmSize - mmMargin,
                 canvasPos.y + canvasSize.y - mmSize - mmMargin);
    ImVec2 mmEnd(mmPos.x + mmSize, mmPos.y + mmSize);

    // Background
    dl->AddRectFilled(mmPos, mmEnd, colors.minimapBg, 4.0f);

    // Compute bounds of all nodes
    f32 minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (auto& n : data.GetNodes()) {
        minX = std::min(minX, n.position.x);
        minY = std::min(minY, n.position.y);
        maxX = std::max(maxX, n.position.x + NODE_WIDTH);
        maxY = std::max(maxY, n.position.y + NODE_HEADER_H + PIN_ROW_H);
    }

    f32 rangeX = maxX - minX;
    f32 rangeY = maxY - minY;
    if (rangeX < 1.0f) rangeX = 1.0f;
    if (rangeY < 1.0f) rangeY = 1.0f;

    // Add padding
    f32 pad = 50.0f;
    minX -= pad; minY -= pad;
    rangeX += pad * 2; rangeY += pad * 2;

    // Fit to minimap (maintain aspect ratio)
    f32 scaleX = (mmSize - 8.0f) / rangeX;
    f32 scaleY = (mmSize - 8.0f) / rangeY;
    f32 mmScale = std::min(scaleX, scaleY);

    // Draw nodes as small rectangles
    for (auto& n : data.GetNodes()) {
        f32 nx = mmPos.x + 4.0f + (n.position.x - minX) * mmScale;
        f32 ny = mmPos.y + 4.0f + (n.position.y - minY) * mmScale;
        f32 nw = NODE_WIDTH * mmScale;
        f32 nh = (NODE_HEADER_H + PIN_ROW_H) * mmScale;
        nw = std::max(nw, 3.0f);
        nh = std::max(nh, 2.0f);

        u32 col = colors.minimapNode;
        if (n.id == m_SelectedNodeId)
            col = colors.nodeSelected;
        dl->AddRectFilled(ImVec2(nx, ny), ImVec2(nx + nw, ny + nh), col, 1.0f);
    }

    // Draw viewport rectangle
    f32 vpX = -m_ScrollOffset.x / m_Zoom;
    f32 vpY = -m_ScrollOffset.y / m_Zoom;
    f32 vpW = canvasSize.x / m_Zoom;
    f32 vpH = canvasSize.y / m_Zoom;

    f32 vx = mmPos.x + 4.0f + (vpX - minX) * mmScale;
    f32 vy = mmPos.y + 4.0f + (vpY - minY) * mmScale;
    f32 vw = vpW * mmScale;
    f32 vh = vpH * mmScale;
    dl->AddRect(ImVec2(vx, vy), ImVec2(vx + vw, vy + vh),
        colors.minimapViewport, 0.0f, 0, 1.5f);

    // Border
    dl->AddRect(mmPos, mmEnd, colors.nodeBorder, 4.0f, 0, 1.0f);
}

void NodeGraphEditor::DrawContextMenu(NodeGraphData& data, NodeGraphCallbacks& callbacks,
                                        ImVec2 canvasPos, f32 uiScale) {
    if (m_ShowContextMenu) {
        m_ShowContextMenu = false;

        // If custom context menu handler is set, use that instead
        if (callbacks.OnContextMenu) {
            Math::Vector2 spawnPos = ScreenToCanvas(m_ContextMenuPos, canvasPos);
            callbacks.OnContextMenu(spawnPos);
            return;
        }

        ImGui::OpenPopup("##NodeGraphCtx");
    }

    if (ImGui::BeginPopup("##NodeGraphCtx")) {
        Math::Vector2 spawnPos = ScreenToCanvas(m_ContextMenuPos, canvasPos);

        for (auto& cat : callbacks.contextMenuCategories) {
            if (ImGui::BeginMenu(cat.name.c_str())) {
                for (auto& [name, action] : cat.items) {
                    if (ImGui::MenuItem(name.c_str())) {
                        action(spawnPos);
                    }
                }
                ImGui::EndMenu();
            }
        }

        ImGui::EndPopup();
    }
}

// ============================================================================
// NodeGraphEditor — Interaction
// ============================================================================

void NodeGraphEditor::HandleCanvasInteraction(ImVec2 canvasPos, ImVec2 canvasSize,
                                                NodeGraphData& data,
                                                NodeGraphCallbacks& callbacks,
                                                f32 uiScale) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mousePos = io.MousePos;

    f32 s = m_Zoom * uiScale;

    // --- Zoom (scroll wheel) ---
    if (io.MouseWheel != 0.0f) {
        f32 oldZoom = m_Zoom;
        m_Zoom *= (io.MouseWheel > 0) ? 1.1f : (1.0f / 1.1f);
        m_Zoom = std::max(ZOOM_MIN, std::min(ZOOM_MAX, m_Zoom));

        // Zoom toward mouse position
        f32 zoomRatio = m_Zoom / oldZoom;
        m_ScrollOffset.x = mousePos.x - canvasPos.x -
            (mousePos.x - canvasPos.x - m_ScrollOffset.x) * zoomRatio;
        m_ScrollOffset.y = mousePos.y - canvasPos.y -
            (mousePos.y - canvasPos.y - m_ScrollOffset.y) * zoomRatio;
    }

    // --- Pan (middle mouse drag or Alt+left drag) ---
    bool panDrag = ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 1.0f) ||
                   (io.KeyAlt && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f));
    if (panDrag) {
        m_ScrollOffset.x += io.MouseDelta.x;
        m_ScrollOffset.y += io.MouseDelta.y;
        return;  // Don't process other interactions while panning
    }

    // --- Hit test pins for drag-to-connect ---
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt) {
        for (auto& node : data.GetNodes()) {
            auto checkPins = [&](const std::vector<Pin>& pins) {
                for (auto& pin : pins) {
                    ImVec2 pinPos = GetPinScreenPos(node, pin, canvasPos, uiScale);
                    f32 r = PIN_RADIUS * s + 4.0f;
                    f32 dx = mousePos.x - pinPos.x;
                    f32 dy = mousePos.y - pinPos.y;
                    if (dx * dx + dy * dy < r * r) {
                        m_IsDraggingPin = true;
                        m_DragStartPin = pin.id;
                        m_DragEndPos = mousePos;
                        return true;
                    }
                }
                return false;
            };
            if (checkPins(node.inputs)) break;
            if (checkPins(node.outputs)) break;
        }
    }

    // Update drag-to-connect
    if (m_IsDraggingPin) {
        m_DragEndPos = mousePos;

        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            m_IsDraggingPin = false;

            // Find target pin under mouse
            auto* startPin = data.FindPin(m_DragStartPin);
            if (startPin) {
                for (auto& node : data.GetNodes()) {
                    auto checkTarget = [&](const std::vector<Pin>& pins) {
                        for (auto& pin : pins) {
                            if (pin.id == m_DragStartPin) continue;
                            if (pin.kind == startPin->kind) continue;  // Same kind can't connect
                            if (pin.nodeId == startPin->nodeId) continue;  // Same node

                            ImVec2 pinPos = GetPinScreenPos(node, pin, canvasPos, uiScale);
                            f32 r = PIN_RADIUS * s + 6.0f;
                            f32 dx = mousePos.x - pinPos.x;
                            f32 dy = mousePos.y - pinPos.y;
                            if (dx * dx + dy * dy < r * r) {
                                // Validate
                                bool canCreate = true;
                                if (callbacks.CanCreateLink) {
                                    canCreate = callbacks.CanCreateLink(*startPin, pin);
                                }
                                if (canCreate && !data.HasLinkBetween(startPin->id, pin.id)) {
                                    PinId outPin = (startPin->kind == PinKind::Output)
                                        ? startPin->id : pin.id;
                                    PinId inPin = (startPin->kind == PinKind::Input)
                                        ? startPin->id : pin.id;
                                    LinkId lid = data.AddLink(outPin, inPin);
                                    if (callbacks.OnLinkCreated) callbacks.OnLinkCreated(lid);
                                }
                                return true;
                            }
                        }
                        return false;
                    };
                    if (checkTarget(node.inputs)) break;
                    if (checkTarget(node.outputs)) break;
                }
            }
        }
        return;
    }

    // --- Node drag ---
    if (m_IsDraggingNode) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            auto* node = data.FindNode(m_DragNodeId);
            if (node && !HasFlag(node->flags, NodeFlags::NoMove)) {
                node->position.x += io.MouseDelta.x / m_Zoom;
                node->position.y += io.MouseDelta.y / m_Zoom;
            }
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
            m_IsDraggingNode = false;
        }
        return;
    }

    // --- Left click: select node or link, start node drag ---
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.KeyAlt) {
        bool hitNode = false;

        // Check nodes in reverse order (top-most first)
        auto& nodes = data.GetNodes();
        for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
            ImVec2 nodePos = CanvasToScreen(it->position, canvasPos);
            ImVec2 nodeSize = GetNodeSize(*it, uiScale);
            if (mousePos.x >= nodePos.x && mousePos.x <= nodePos.x + nodeSize.x &&
                mousePos.y >= nodePos.y && mousePos.y <= nodePos.y + nodeSize.y) {
                m_SelectedNodeId = it->id;
                m_SelectedLinkId = 0;
                m_IsDraggingNode = true;
                m_DragNodeId = it->id;
                if (callbacks.OnNodeSelected) callbacks.OnNodeSelected(it->id);
                hitNode = true;
                break;
            }
        }

        // Check links if no node hit
        if (!hitNode) {
            bool hitLink = false;
            for (auto& link : data.GetLinks()) {
                auto* sp = data.FindPin(link.startPinId);
                auto* ep = data.FindPin(link.endPinId);
                if (!sp || !ep) continue;
                auto* sn = data.FindNode(sp->nodeId);
                auto* en = data.FindNode(ep->nodeId);
                if (!sn || !en) continue;

                ImVec2 p1 = GetPinScreenPos(*sn, *sp, canvasPos, uiScale);
                ImVec2 p2 = GetPinScreenPos(*en, *ep, canvasPos, uiScale);

                // Simple proximity test to bezier curve midpoint
                ImVec2 mid((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
                f32 dx = mousePos.x - mid.x;
                f32 dy = mousePos.y - mid.y;

                // Test at multiple points along the curve
                f32 minDist = 1e9f;
                f32 cpDist = std::max(std::abs(p2.x - p1.x) * 0.5f, 40.0f * s);
                ImVec2 cp1(p1.x + cpDist, p1.y);
                ImVec2 cp2(p2.x - cpDist, p2.y);

                for (f32 t = 0.0f; t <= 1.0f; t += 0.05f) {
                    f32 u = 1.0f - t;
                    f32 bx = u*u*u*p1.x + 3*u*u*t*cp1.x + 3*u*t*t*cp2.x + t*t*t*p2.x;
                    f32 by = u*u*u*p1.y + 3*u*u*t*cp1.y + 3*u*t*t*cp2.y + t*t*t*p2.y;
                    f32 d = (mousePos.x - bx) * (mousePos.x - bx) +
                            (mousePos.y - by) * (mousePos.y - by);
                    if (d < minDist) minDist = d;
                }

                if (minDist < 64.0f) {  // ~8px tolerance
                    m_SelectedLinkId = link.id;
                    m_SelectedNodeId = 0;
                    if (callbacks.OnLinkSelected) callbacks.OnLinkSelected(link.id);
                    hitLink = true;
                    break;
                }
            }

            // Clear selection if clicked empty space (start box select)
            if (!hitLink) {
                m_SelectedNodeId = 0;
                m_SelectedLinkId = 0;
                if (callbacks.OnSelectionCleared) callbacks.OnSelectionCleared();
                m_IsBoxSelecting = true;
                m_BoxSelectStart = mousePos;
            }
        }
    }

    // Box selection release
    if (m_IsBoxSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_IsBoxSelecting = false;
        // Select first node fully within box
        ImVec2 bMin(std::min(m_BoxSelectStart.x, mousePos.x),
                    std::min(m_BoxSelectStart.y, mousePos.y));
        ImVec2 bMax(std::max(m_BoxSelectStart.x, mousePos.x),
                    std::max(m_BoxSelectStart.y, mousePos.y));
        for (auto& node : data.GetNodes()) {
            ImVec2 np = CanvasToScreen(node.position, canvasPos);
            ImVec2 ns = GetNodeSize(node, uiScale);
            if (np.x >= bMin.x && np.y >= bMin.y &&
                np.x + ns.x <= bMax.x && np.y + ns.y <= bMax.y) {
                m_SelectedNodeId = node.id;
                if (callbacks.OnNodeSelected) callbacks.OnNodeSelected(node.id);
                break;
            }
        }
    }

    // --- Right click: context menu ---
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
        m_ShowContextMenu = true;
        m_ContextMenuPos = mousePos;
    }
}

void NodeGraphEditor::HandleKeyboard(NodeGraphData& data, NodeGraphCallbacks& callbacks) {
    // Delete selected node or link
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        if (m_SelectedNodeId != 0) {
            auto* node = data.FindNode(m_SelectedNodeId);
            if (node && !HasFlag(node->flags, NodeFlags::NoDelete)) {
                NodeId id = m_SelectedNodeId;
                m_SelectedNodeId = 0;
                if (callbacks.OnNodeDeleted) callbacks.OnNodeDeleted(id);
                data.RemoveNode(id);
            }
        } else if (m_SelectedLinkId != 0) {
            LinkId id = m_SelectedLinkId;
            m_SelectedLinkId = 0;
            if (callbacks.OnLinkDeleted) callbacks.OnLinkDeleted(id);
            data.RemoveLink(id);
        }
    }

    // Tab: cycle through nodes
    if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
        auto& nodes = data.GetNodes();
        if (!nodes.empty()) {
            m_TabCycleIndex = (m_TabCycleIndex + 1) % nodes.size();
            m_SelectedNodeId = nodes[m_TabCycleIndex].id;
            m_SelectedLinkId = 0;
            if (callbacks.OnNodeSelected) callbacks.OnNodeSelected(m_SelectedNodeId);
        }
    }

    // Escape: clear selection
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        m_SelectedNodeId = 0;
        m_SelectedLinkId = 0;
        m_IsDraggingPin = false;
        m_IsDraggingNode = false;
        m_IsBoxSelecting = false;
        if (callbacks.OnSelectionCleared) callbacks.OnSelectionCleared();
    }
}

// ============================================================================
// View Controls
// ============================================================================

void NodeGraphEditor::FitAllNodes(const NodeGraphData& data) {
    if (data.GetNodes().empty()) return;

    f32 minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (auto& n : data.GetNodes()) {
        minX = std::min(minX, n.position.x);
        minY = std::min(minY, n.position.y);
        maxX = std::max(maxX, n.position.x + NODE_WIDTH);
        maxY = std::max(maxY, n.position.y + NODE_HEADER_H + PIN_ROW_H * 2);
    }

    f32 centerX = (minX + maxX) * 0.5f;
    f32 centerY = (minY + maxY) * 0.5f;

    m_Zoom = 1.0f;
    m_ScrollOffset.x = -centerX * m_Zoom + 300.0f;
    m_ScrollOffset.y = -centerY * m_Zoom + 200.0f;
}

void NodeGraphEditor::CenterOnNode(const GraphNode& node) {
    m_ScrollOffset.x = -node.position.x * m_Zoom + 300.0f;
    m_ScrollOffset.y = -node.position.y * m_Zoom + 200.0f;
}

} // namespace Editor
} // namespace Enjin
