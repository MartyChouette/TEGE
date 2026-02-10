#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Editor/EditorSettings.h"
#include <string>
#include <vector>

struct ImDrawList;

namespace Enjin {
namespace Editor {

// Vector shape types
enum class VectorShapeType : u8 {
    Line,
    Rect,
    Ellipse,
    Pen,
    Bezier,
    Star,
    Polygon
};

// Drawing tools
enum class VectorTool : u8 {
    Select,
    Line,
    Rect,
    Ellipse,
    Pen,
    Bezier,
    Star,
    Polygon
};

// A single vector shape
struct VectorShape {
    VectorShapeType type = VectorShapeType::Rect;
    std::vector<Math::Vector2> points;  // Control points
    u32 fillColor = 0xFFCCCCCC;        // ABGR packed
    u32 strokeColor = 0xFF000000;       // ABGR packed
    f32 strokeWidth = 2.0f;
    f32 opacity = 1.0f;
    bool locked = false;
    std::string name;

    // Star/Polygon specific
    u32 pointCount = 5;         // Number of points for star/polygon
    f32 innerRadius = 0.5f;     // Inner radius ratio for star
};

// A layer of vector shapes
struct VectorLayer {
    std::string name;
    std::vector<VectorShape> shapes;
    bool visible = true;
    bool locked = false;
    f32 opacity = 1.0f;
};

// The full vector document
struct VectorDocument {
    std::vector<VectorLayer> layers;
    u32 canvasWidth = 550;
    u32 canvasHeight = 400;
    u32 backgroundColor = 0xFFFFFFFF;  // ABGR
    std::string symbolName;
};

// Main vector drawing editor class
class ENJIN_API VectorDrawingEditor {
public:
    VectorDrawingEditor() = default;
    ~VectorDrawingEditor() = default;

    // Document management
    void NewDocument(u32 width, u32 height);
    bool ExportSVG(const std::string& path);
    bool ExportPNG(const std::string& path, f32 scale = 1.0f);

    // Main render (called every frame when panel is visible)
    void Render(const EditorSettings& settings);

    // State queries
    bool HasDocument() const { return !m_Document.layers.empty(); }

    // Shape operations
    void DeleteSelected();
    void DuplicateSelected();

    // Symbol library integration
    bool SaveAsSymbol(const std::string& symbolName, const std::string& outputDir);

    // Access document (for Flash Timeline symbol library)
    VectorDocument& GetDocument() { return m_Document; }
    const VectorDocument& GetDocument() const { return m_Document; }

private:
    // Rendering
    void DrawToolbar();
    void DrawCanvas();
    void DrawLayerPanel();
    void DrawPropertyPanel();

    // Canvas rendering helpers
    void DrawShape(ImDrawList* dl, const VectorShape& shape, f32 ox, f32 oy, f32 zoom, f32 layerOpacity);
    void DrawStarShape(ImDrawList* dl, const VectorShape& shape, f32 ox, f32 oy, f32 zoom, f32 layerOpacity);
    void DrawPolygonShape(ImDrawList* dl, const VectorShape& shape, f32 ox, f32 oy, f32 zoom, f32 layerOpacity);
    void DrawSelectionHandles(ImDrawList* dl, const VectorShape& shape, f32 ox, f32 oy, f32 zoom);

    // Tool interactions
    void HandleSelectTool(f32 canvasX, f32 canvasY, bool mouseDown, bool mouseClicked, bool mouseReleased);
    void HandleShapeTool(f32 canvasX, f32 canvasY, bool mouseDown, bool mouseClicked, bool mouseReleased);
    void HandlePenTool(f32 canvasX, f32 canvasY, bool mouseClicked, bool mouseDoubleClicked, bool rightClicked);
    void HandleBezierTool(f32 canvasX, f32 canvasY, bool mouseClicked, bool mouseReleased);

    // Hit testing
    i32 HitTestShape(const VectorShape& shape, f32 x, f32 y) const;
    i32 HitTestHandle(const VectorShape& shape, f32 x, f32 y) const;

    // SVG generation helpers
    std::string ShapeToSVG(const VectorShape& shape) const;
    std::string ColorToSVGHex(u32 abgr) const;

    // Undo
    void PushUndoState();
    void Undo();
    void Redo();

    // Document
    VectorDocument m_Document;
    i32 m_ActiveLayer = 0;

    // Tool state
    VectorTool m_CurrentTool = VectorTool::Select;
    i32 m_SelectedShape = -1;       // Index in active layer
    i32 m_DragHandle = -1;          // Which handle is being dragged
    bool m_IsDrawing = false;
    Math::Vector2 m_DragStart;
    Math::Vector2 m_DragCurrent;

    // Pen tool state
    std::vector<Math::Vector2> m_PenPoints;
    bool m_PenActive = false;

    // Bezier tool state
    Math::Vector2 m_BezierStart;
    Math::Vector2 m_BezierCP1;
    Math::Vector2 m_BezierCP2;
    i32 m_BezierStep = 0;  // 0=start, 1=cp1, 2=cp2, 3=end

    // View state
    f32 m_Zoom = 1.0f;
    f32 m_PanX = 0.0f, m_PanY = 0.0f;
    bool m_ShowGrid = true;
    bool m_SnapToGrid = true;
    f32 m_GridSpacing = 10.0f;

    // Current drawing colors
    u32 m_FillColor = 0xFFCCCCCC;
    u32 m_StrokeColor = 0xFF000000;
    f32 m_StrokeWidth = 2.0f;
    u32 m_ShapePointCount = 5;
    f32 m_ShapeInnerRadius = 0.5f;

    // Undo state
    struct UndoSnapshot {
        std::vector<VectorLayer> layers;
        i32 activeLayer;
    };
    std::vector<UndoSnapshot> m_UndoStack;
    std::vector<UndoSnapshot> m_RedoStack;
    static constexpr usize MAX_UNDO = 50;
};

} // namespace Editor
} // namespace Enjin
