#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Editor/EditorSettings.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <imgui.h>

namespace Enjin {
namespace Editor {

// ============================================================================
// Pin & Node Types
// ============================================================================

// Pin type determines color and compatibility
// Colors use Wong colorblind-safe palette
enum class PinType : u8 {
    Flow,       // White         — execution flow
    Bool,       // Vermillion    — true/false
    Float,      // Bluish-green  — floating point
    Int,        // Sky-blue      — integer
    String,     // Reddish-purple — text
    Vector3,    // Yellow        — 3D vector
    Color,      // Orange        — color value
    Entity,     // Blue          — entity reference
    Any,        // Grey          — accepts anything
    COUNT
};

enum class PinKind : u8 {
    Input,
    Output
};

// Flags for node behavior
enum class NodeFlags : u32 {
    None        = 0,
    NoDelete    = 1 << 0,   // Cannot be deleted (e.g., Entry node)
    NoMove      = 1 << 1,   // Cannot be dragged
    Highlighted = 1 << 2,   // Glowing border (e.g., active state in play mode)
    Error       = 1 << 3,   // Red error border
};

inline NodeFlags operator|(NodeFlags a, NodeFlags b) {
    return static_cast<NodeFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline NodeFlags operator&(NodeFlags a, NodeFlags b) {
    return static_cast<NodeFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}
inline bool HasFlag(NodeFlags flags, NodeFlags flag) {
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

// ============================================================================
// Data Structures
// ============================================================================

using NodeId = u32;
using PinId  = u32;
using LinkId = u32;

struct Pin {
    PinId   id = 0;
    std::string name;
    PinType type = PinType::Any;
    PinKind kind = PinKind::Input;
    NodeId  nodeId = 0;
};

struct GraphNode {
    NodeId  id = 0;
    std::string title;
    Math::Vector2 position;             // Canvas-space position
    Math::Vector3 headerColor = Math::Vector3(0.3f, 0.3f, 0.6f);
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
    NodeFlags flags = NodeFlags::None;
    u64 userData = 0;                   // Consumer-defined payload
};

struct GraphLink {
    LinkId  id = 0;
    PinId   startPinId = 0;            // Output pin
    PinId   endPinId = 0;              // Input pin
    u64     userData = 0;
};

// ============================================================================
// Graph Data Model
// ============================================================================

class ENJIN_API NodeGraphData {
public:
    NodeGraphData() = default;

    // Node operations
    NodeId  AddNode(const std::string& title, Math::Vector2 position,
                    Math::Vector3 headerColor = Math::Vector3(0.3f, 0.3f, 0.6f));
    void    RemoveNode(NodeId id);
    GraphNode* FindNode(NodeId id);
    const GraphNode* FindNode(NodeId id) const;

    // Pin operations
    PinId   AddPin(NodeId nodeId, const std::string& name, PinType type, PinKind kind);
    Pin*    FindPin(PinId id);
    const Pin* FindPin(PinId id) const;
    NodeId  GetPinOwner(PinId id) const;

    // Link operations
    LinkId  AddLink(PinId startPin, PinId endPin, u64 userData = 0);
    void    RemoveLink(LinkId id);
    GraphLink* FindLink(LinkId id);
    const GraphLink* FindLink(LinkId id) const;

    // Queries
    std::vector<LinkId> GetLinksForPin(PinId pinId) const;
    std::vector<LinkId> GetLinksForNode(NodeId nodeId) const;
    bool    HasLinkBetween(PinId a, PinId b) const;

    // Accessors
    const std::vector<GraphNode>& GetNodes() const { return m_Nodes; }
    const std::vector<GraphLink>& GetLinks() const { return m_Links; }
    std::vector<GraphNode>& GetNodes() { return m_Nodes; }
    std::vector<GraphLink>& GetLinks() { return m_Links; }

    // Serialization
    nlohmann::json ToJson() const;
    void FromJson(const nlohmann::json& j);

    // Clear all data
    void Clear();

private:
    std::vector<GraphNode> m_Nodes;
    std::vector<GraphLink> m_Links;
    NodeId m_NextNodeId = 1;
    PinId  m_NextPinId  = 1;
    LinkId m_NextLinkId = 1;
};

// ============================================================================
// Theme Colors
// ============================================================================

struct NodeGraphColors {
    // Canvas
    u32 canvasBg        = 0xFF1E1E1E;
    u32 gridMinor       = 0xFF2A2A2A;
    u32 gridMajor       = 0xFF3A3A3A;

    // Node
    u32 nodeBody        = 0xFF2D2D30;
    u32 nodeBorder      = 0xFF4D4D50;
    u32 nodeSelected    = 0xFFE0A030;
    u32 nodeHighlighted = 0xFF40C040;
    u32 nodeError       = 0xFFC04040;
    u32 nodeText        = 0xFFE0E0E0;

    // Link
    u32 linkDefault     = 0xFFB0B0B0;
    u32 linkSelected    = 0xFFE0A030;
    u32 linkPreview     = 0x80FFFFFF;

    // Selection box
    u32 selectionFill   = 0x203060A0;
    u32 selectionBorder = 0xFF4080E0;

    // Minimap
    u32 minimapBg       = 0xC0101010;
    u32 minimapNode     = 0x80808080;
    u32 minimapViewport = 0x40FFFFFF;

    // Pin type colors (Wong colorblind-safe)
    u32 pinColors[static_cast<usize>(PinType::COUNT)] = {
        0xFFFFFFFF,  // Flow      — white
        0xFF3333E0,  // Bool      — vermillion (BGR: 0xE03333)
        0xFF99CC00,  // Float     — bluish-green (BGR: 0x00CC99)
        0xFFEBCE56,  // Int       — sky-blue (BGR: 0x56CEEB)
        0xFFCC66CC,  // String    — reddish-purple
        0xFF00E6F0,  // Vector3   — yellow (BGR: 0xF0E600)
        0xFF0099E6,  // Color     — orange (BGR: 0xE69900)
        0xFFE07030,  // Entity    — blue (BGR: 0x3070E0)
        0xFF909090,  // Any       — grey
    };

    // Derive colors from editor theme
    static NodeGraphColors FromTheme(const EditorSettings& settings);
};

// ============================================================================
// Callbacks
// ============================================================================

struct ContextMenuCategory {
    std::string name;
    std::vector<std::pair<std::string, std::function<void(Math::Vector2)>>> items;
};

struct NodeGraphCallbacks {
    // Validation: can a link be created between these pins?
    std::function<bool(const Pin& from, const Pin& to)> CanCreateLink;

    // Notifications
    std::function<void(NodeId)> OnNodeCreated;
    std::function<void(NodeId)> OnNodeDeleted;
    std::function<void(LinkId)> OnLinkCreated;
    std::function<void(LinkId)> OnLinkDeleted;

    // Selection
    std::function<void(NodeId)> OnNodeSelected;
    std::function<void(LinkId)> OnLinkSelected;
    std::function<void()> OnSelectionCleared;

    // Custom node body rendering (called after default header/pins)
    std::function<void(const GraphNode&, ImDrawList*, ImVec2 bodyMin, ImVec2 bodyMax, f32 zoom)> DrawNodeContent;

    // Right-click context menu categories
    std::vector<ContextMenuCategory> contextMenuCategories;
};

// ============================================================================
// Node Graph Editor Widget
// ============================================================================

class ENJIN_API NodeGraphEditor {
public:
    NodeGraphEditor() = default;

    // Call inside an ImGui window (between Begin/End)
    void Render(NodeGraphData& data, NodeGraphCallbacks& callbacks,
                const NodeGraphColors& colors, f32 uiScale = 1.0f);

    // State queries
    NodeId GetSelectedNodeId() const { return m_SelectedNodeId; }
    LinkId GetSelectedLinkId() const { return m_SelectedLinkId; }
    void   SetSelectedNode(NodeId id) { m_SelectedNodeId = id; m_SelectedLinkId = 0; }
    void   ClearSelection() { m_SelectedNodeId = 0; m_SelectedLinkId = 0; }

    // View controls
    void   FitAllNodes(const NodeGraphData& data);
    void   CenterOnNode(const GraphNode& node);
    Math::Vector2 GetScrollOffset() const { return m_ScrollOffset; }
    f32    GetZoom() const { return m_Zoom; }
    void   SetScrollOffset(Math::Vector2 offset) { m_ScrollOffset = offset; }
    void   SetZoom(f32 zoom) { m_Zoom = zoom; }

    // Minimap toggle
    bool   IsMinimapVisible() const { return m_ShowMinimap; }
    void   SetMinimapVisible(bool v) { m_ShowMinimap = v; }

private:
    // Canvas state
    Math::Vector2 m_ScrollOffset;
    f32 m_Zoom = 1.0f;
    static constexpr f32 ZOOM_MIN = 0.2f;
    static constexpr f32 ZOOM_MAX = 3.0f;

    // Selection
    NodeId m_SelectedNodeId = 0;
    LinkId m_SelectedLinkId = 0;

    // Drag-to-connect state
    bool   m_IsDraggingPin = false;
    PinId  m_DragStartPin = 0;
    ImVec2 m_DragEndPos;

    // Node drag
    bool   m_IsDraggingNode = false;
    NodeId m_DragNodeId = 0;

    // Box selection
    bool   m_IsBoxSelecting = false;
    ImVec2 m_BoxSelectStart;

    // Context menu
    bool   m_ShowContextMenu = false;
    ImVec2 m_ContextMenuPos;

    // Minimap
    bool   m_ShowMinimap = true;

    // Keyboard navigation
    usize  m_TabCycleIndex = 0;

    // Rendering helpers
    void DrawGrid(ImDrawList* dl, ImVec2 canvasPos, ImVec2 canvasSize,
                  const NodeGraphColors& colors, f32 uiScale);
    void DrawNodes(ImDrawList* dl, ImVec2 canvasPos, NodeGraphData& data,
                   NodeGraphCallbacks& callbacks, const NodeGraphColors& colors, f32 uiScale);
    void DrawLinks(ImDrawList* dl, ImVec2 canvasPos, NodeGraphData& data,
                   const NodeGraphColors& colors, f32 uiScale);
    void DrawMinimap(ImDrawList* dl, ImVec2 canvasPos, ImVec2 canvasSize,
                     const NodeGraphData& data, const NodeGraphColors& colors, f32 uiScale);
    void DrawContextMenu(NodeGraphData& data, NodeGraphCallbacks& callbacks,
                         ImVec2 canvasPos, f32 uiScale);

    // Interaction
    void HandleCanvasInteraction(ImVec2 canvasPos, ImVec2 canvasSize,
                                 NodeGraphData& data, NodeGraphCallbacks& callbacks,
                                 f32 uiScale);
    void HandleKeyboard(NodeGraphData& data, NodeGraphCallbacks& callbacks);

    // Coordinate transforms
    ImVec2 CanvasToScreen(Math::Vector2 canvasPos, ImVec2 origin) const;
    Math::Vector2 ScreenToCanvas(ImVec2 screenPos, ImVec2 origin) const;

    // Pin position (screen space)
    ImVec2 GetPinScreenPos(const GraphNode& node, const Pin& pin, ImVec2 canvasOrigin, f32 uiScale) const;

    // Node bounds (screen space)
    ImVec2 GetNodeSize(const GraphNode& node, f32 uiScale) const;
};

} // namespace Editor
} // namespace Enjin
