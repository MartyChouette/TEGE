#include "Enjin/Editor/VectorDrawingEditor.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Enjin {
namespace Editor {

// ============================================================================
// Document Management
// ============================================================================

void VectorDrawingEditor::NewDocument(u32 width, u32 height) {
    m_Document = VectorDocument{};
    m_Document.canvasWidth = width;
    m_Document.canvasHeight = height;

    VectorLayer layer;
    layer.name = "Layer 1";
    m_Document.layers.push_back(layer);
    m_ActiveLayer = 0;
    m_SelectedShape = -1;
    m_UndoStack.clear();
    m_RedoStack.clear();
    m_PenActive = false;
    m_BezierStep = 0;
}

bool VectorDrawingEditor::ExportSVG(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
         << "width=\"" << m_Document.canvasWidth << "\" "
         << "height=\"" << m_Document.canvasHeight << "\" "
         << "viewBox=\"0 0 " << m_Document.canvasWidth << " " << m_Document.canvasHeight << "\">\n";

    // Background
    file << "  <rect width=\"100%\" height=\"100%\" fill=\""
         << ColorToSVGHex(m_Document.backgroundColor) << "\"/>\n";

    // Layers (bottom to top)
    for (auto& layer : m_Document.layers) {
        if (!layer.visible) continue;
        file << "  <g opacity=\"" << layer.opacity << "\">\n";
        for (auto& shape : layer.shapes) {
            file << "    " << ShapeToSVG(shape) << "\n";
        }
        file << "  </g>\n";
    }

    file << "</svg>\n";
    file.close();

    ENJIN_LOG_INFO(Editor, "SVG exported to %s", path.c_str());
    return true;
}

bool VectorDrawingEditor::ExportPNG(const std::string& path, f32 scale) {
    // Export SVG first, then rasterize via SVGLoader
    std::string svgPath = path + ".tmp.svg";
    if (!ExportSVG(svgPath)) return false;

    // The actual PNG export would use SVGLoader::LoadAndRasterize + stbi_write_png
    // For now, just keep the SVG
    ENJIN_LOG_INFO(Editor, "PNG export: SVG generated at %s (rasterization requires SVGLoader)", svgPath.c_str());
    return true;
}

// ============================================================================
// Shape Operations
// ============================================================================

void VectorDrawingEditor::DeleteSelected() {
    if (m_SelectedShape < 0 || m_ActiveLayer < 0) return;
    if (m_ActiveLayer >= (i32)m_Document.layers.size()) return;
    auto& shapes = m_Document.layers[m_ActiveLayer].shapes;
    if (m_SelectedShape >= (i32)shapes.size()) return;

    PushUndoState();
    shapes.erase(shapes.begin() + m_SelectedShape);
    m_SelectedShape = -1;
}

void VectorDrawingEditor::DuplicateSelected() {
    if (m_SelectedShape < 0 || m_ActiveLayer < 0) return;
    if (m_ActiveLayer >= (i32)m_Document.layers.size()) return;
    auto& shapes = m_Document.layers[m_ActiveLayer].shapes;
    if (m_SelectedShape >= (i32)shapes.size()) return;

    PushUndoState();
    VectorShape copy = shapes[m_SelectedShape];
    copy.name = copy.name + " Copy";
    // Offset the duplicate slightly
    for (auto& pt : copy.points) {
        pt.x += 10.0f;
        pt.y += 10.0f;
    }
    shapes.push_back(copy);
    m_SelectedShape = (i32)shapes.size() - 1;
}

bool VectorDrawingEditor::SaveAsSymbol(const std::string& symbolName, const std::string& outputDir) {
    std::string svgPath = outputDir + "/" + symbolName + ".svg";
    if (!ExportSVG(svgPath)) return false;

    m_Document.symbolName = symbolName;
    ENJIN_LOG_INFO(Editor, "Saved vector drawing as symbol: %s", symbolName.c_str());
    return true;
}

// ============================================================================
// Main Render
// ============================================================================

void VectorDrawingEditor::Render(const EditorSettings& settings) {
    if (!HasDocument()) {
        // Show new document dialog
        ImGui::Text("No document open.");
        ImGui::Separator();

        static u32 newW = 550, newH = 400;
        ImGui::SetNextItemWidth(100);
        ImGui::InputScalar("Width", ImGuiDataType_U32, &newW);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::InputScalar("Height", ImGuiDataType_U32, &newH);
        ImGui::SameLine();
        if (ImGui::Button("New Document")) {
            NewDocument(newW, newH);
        }

        // Presets
        ImGui::SameLine();
        if (ImGui::Button("Flash Stage (550x400)")) { NewDocument(550, 400); }
        ImGui::SameLine();
        if (ImGui::Button("HD (1280x720)")) { NewDocument(1280, 720); }

        return;
    }

    // Split: Toolbar at top, canvas center, layers right, properties bottom
    DrawToolbar();
    ImGui::Separator();

    f32 panelHeight = ImGui::GetContentRegionAvail().y;
    f32 layerPanelWidth = 180.0f;
    f32 propertyPanelHeight = 140.0f;

    // Main area: canvas + layer panel
    ImGui::BeginChild("VectorCanvasArea", ImVec2(0, panelHeight - propertyPanelHeight), false);
    {
        f32 canvasWidth = ImGui::GetContentRegionAvail().x - layerPanelWidth - 8;

        // Canvas
        ImGui::BeginChild("VectorCanvas", ImVec2(canvasWidth, 0), true,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        DrawCanvas();
        ImGui::EndChild();

        ImGui::SameLine();

        // Layer panel
        ImGui::BeginChild("VectorLayers", ImVec2(layerPanelWidth, 0), true);
        DrawLayerPanel();
        ImGui::EndChild();
    }
    ImGui::EndChild();

    // Property panel at bottom
    ImGui::BeginChild("VectorProperties", ImVec2(0, 0), true);
    DrawPropertyPanel();
    ImGui::EndChild();
}

// ============================================================================
// Toolbar
// ============================================================================

void VectorDrawingEditor::DrawToolbar() {
    const char* toolNames[] = { "Select", "Line", "Rect", "Ellipse", "Pen", "Bezier", "Star", "Polygon" };

    for (i32 i = 0; i < 8; i++) {
        if (i > 0) ImGui::SameLine();
        bool selected = (i32)m_CurrentTool == i;
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        if (ImGui::Button(toolNames[i], ImVec2(60, 0))) {
            m_CurrentTool = static_cast<VectorTool>(i);
            // Cancel pen/bezier if switching tools
            m_PenActive = false;
            m_BezierStep = 0;
        }
        if (selected) ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Grid toggle
    ImGui::Checkbox("Grid", &m_ShowGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Snap", &m_SnapToGrid);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    ImGui::DragFloat("##GridSize", &m_GridSpacing, 1.0f, 1.0f, 100.0f, "%.0f");

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Zoom
    ImGui::Text("Zoom:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::SliderFloat("##Zoom", &m_Zoom, 0.1f, 8.0f, "%.1fx");
    ImGui::SameLine();
    if (ImGui::Button("1:1")) m_Zoom = 1.0f;

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Export
    if (ImGui::Button("Export SVG")) {
        std::string path = "vector_export.svg";
        ExportSVG(path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save as Symbol")) {
        if (!m_Document.symbolName.empty()) {
            SaveAsSymbol(m_Document.symbolName, ".");
        }
    }

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Undo/Redo
    bool canUndo = !m_UndoStack.empty();
    bool canRedo = !m_RedoStack.empty();
    if (!canUndo) ImGui::BeginDisabled();
    if (ImGui::Button("Undo")) Undo();
    if (!canUndo) ImGui::EndDisabled();
    ImGui::SameLine();
    if (!canRedo) ImGui::BeginDisabled();
    if (ImGui::Button("Redo")) Redo();
    if (!canRedo) ImGui::EndDisabled();
}

// ============================================================================
// Canvas
// ============================================================================

void VectorDrawingEditor::DrawCanvas() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // Background
    dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                      IM_COL32(50, 50, 50, 255));

    // Document area
    f32 docW = m_Document.canvasWidth * m_Zoom;
    f32 docH = m_Document.canvasHeight * m_Zoom;
    f32 ox = canvasPos.x + (canvasSize.x - docW) * 0.5f + m_PanX;
    f32 oy = canvasPos.y + (canvasSize.y - docH) * 0.5f + m_PanY;

    // Document background
    dl->AddRectFilled(ImVec2(ox, oy), ImVec2(ox + docW, oy + docH),
                      m_Document.backgroundColor);
    dl->AddRect(ImVec2(ox, oy), ImVec2(ox + docW, oy + docH),
                IM_COL32(128, 128, 128, 255));

    // Grid
    if (m_ShowGrid && m_Zoom > 0.3f) {
        f32 gridStep = m_GridSpacing * m_Zoom;
        if (gridStep > 4.0f) {
            for (f32 gx = 0; gx <= docW; gx += gridStep) {
                dl->AddLine(ImVec2(ox + gx, oy), ImVec2(ox + gx, oy + docH),
                           IM_COL32(128, 128, 128, 40));
            }
            for (f32 gy = 0; gy <= docH; gy += gridStep) {
                dl->AddLine(ImVec2(ox, oy + gy), ImVec2(ox + docW, oy + gy),
                           IM_COL32(128, 128, 128, 40));
            }
        }
    }

    // Draw all shapes from all visible layers
    for (i32 li = 0; li < (i32)m_Document.layers.size(); li++) {
        auto& layer = m_Document.layers[li];
        if (!layer.visible) continue;
        for (i32 si = 0; si < (i32)layer.shapes.size(); si++) {
            DrawShape(dl, layer.shapes[si], ox, oy, m_Zoom, layer.opacity);
        }
    }

    // Selection handles
    if (m_SelectedShape >= 0 && m_ActiveLayer >= 0 &&
        m_ActiveLayer < (i32)m_Document.layers.size()) {
        auto& shapes = m_Document.layers[m_ActiveLayer].shapes;
        if (m_SelectedShape < (i32)shapes.size()) {
            DrawSelectionHandles(dl, shapes[m_SelectedShape], ox, oy, m_Zoom);
        }
    }

    // Preview shape being drawn
    if (m_IsDrawing && m_CurrentTool != VectorTool::Select &&
        m_CurrentTool != VectorTool::Pen && m_CurrentTool != VectorTool::Bezier) {
        VectorShape preview;
        preview.strokeColor = m_StrokeColor;
        preview.fillColor = m_FillColor;
        preview.strokeWidth = m_StrokeWidth;
        preview.pointCount = m_ShapePointCount;
        preview.innerRadius = m_ShapeInnerRadius;

        switch (m_CurrentTool) {
            case VectorTool::Line:
                preview.type = VectorShapeType::Line;
                preview.points = { m_DragStart, m_DragCurrent };
                break;
            case VectorTool::Rect:
                preview.type = VectorShapeType::Rect;
                preview.points = { m_DragStart, m_DragCurrent };
                break;
            case VectorTool::Ellipse:
                preview.type = VectorShapeType::Ellipse;
                preview.points = { m_DragStart, m_DragCurrent };
                break;
            case VectorTool::Star:
                preview.type = VectorShapeType::Star;
                preview.points = { m_DragStart, m_DragCurrent };
                break;
            case VectorTool::Polygon:
                preview.type = VectorShapeType::Polygon;
                preview.points = { m_DragStart, m_DragCurrent };
                break;
            default: break;
        }
        if (!preview.points.empty()) {
            DrawShape(dl, preview, ox, oy, m_Zoom, 0.7f);
        }
    }

    // Pen tool preview
    if (m_PenActive && !m_PenPoints.empty()) {
        for (usize i = 0; i + 1 < m_PenPoints.size(); i++) {
            dl->AddLine(
                ImVec2(ox + m_PenPoints[i].x * m_Zoom, oy + m_PenPoints[i].y * m_Zoom),
                ImVec2(ox + m_PenPoints[i + 1].x * m_Zoom, oy + m_PenPoints[i + 1].y * m_Zoom),
                m_StrokeColor, m_StrokeWidth * m_Zoom
            );
        }
        // Draw dots at pen points
        for (auto& pt : m_PenPoints) {
            dl->AddCircleFilled(ImVec2(ox + pt.x * m_Zoom, oy + pt.y * m_Zoom),
                               3.0f, IM_COL32(0, 120, 255, 255));
        }
    }

    // Handle input
    ImGui::InvisibleButton("##VectorCanvasBtn", canvasSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    bool mouseDoubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    bool rightClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);

    if (hovered) {
        ImVec2 mousePos = ImGui::GetMousePos();
        f32 canvasX = (mousePos.x - ox) / m_Zoom;
        f32 canvasY = (mousePos.y - oy) / m_Zoom;

        // Snap to grid
        if (m_SnapToGrid && m_GridSpacing > 0) {
            canvasX = std::round(canvasX / m_GridSpacing) * m_GridSpacing;
            canvasY = std::round(canvasY / m_GridSpacing) * m_GridSpacing;
        }

        // Zoom with scroll wheel
        f32 scroll = ImGui::GetIO().MouseWheel;
        if (scroll != 0.0f) {
            f32 oldZoom = m_Zoom;
            m_Zoom = std::clamp(m_Zoom + scroll * 0.1f, 0.1f, 8.0f);
            // Adjust pan to zoom towards mouse
            f32 zoomDelta = m_Zoom / oldZoom;
            m_PanX = mousePos.x - canvasPos.x - (mousePos.x - canvasPos.x - m_PanX) * zoomDelta;
            m_PanY = mousePos.y - canvasPos.y - (mousePos.y - canvasPos.y - m_PanY) * zoomDelta;
        }

        // Middle mouse pan
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_PanX += delta.x;
            m_PanY += delta.y;
        }

        // Tool handling
        switch (m_CurrentTool) {
            case VectorTool::Select:
                HandleSelectTool(canvasX, canvasY, mouseDown, mouseClicked, mouseReleased);
                break;
            case VectorTool::Pen:
                HandlePenTool(canvasX, canvasY, mouseClicked, mouseDoubleClicked, rightClicked);
                break;
            case VectorTool::Bezier:
                HandleBezierTool(canvasX, canvasY, mouseClicked, mouseReleased);
                break;
            default:
                HandleShapeTool(canvasX, canvasY, mouseDown, mouseClicked, mouseReleased);
                break;
        }
    }

    // Keyboard shortcuts
    if (ImGui::IsWindowFocused()) {
        auto& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (io.KeyShift) Redo(); else Undo();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
            DeleteSelected();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
            DuplicateSelected();
        }
    }

    // Status bar
    ImVec2 mousePos = ImGui::GetMousePos();
    f32 mx = (mousePos.x - ox) / m_Zoom;
    f32 my = (mousePos.y - oy) / m_Zoom;
    dl->AddText(ImVec2(canvasPos.x + 4, canvasPos.y + canvasSize.y - 18),
                IM_COL32(200, 200, 200, 200),
                (std::to_string((int)mx) + ", " + std::to_string((int)my) +
                 " | " + std::to_string(m_Document.canvasWidth) + "x" +
                 std::to_string(m_Document.canvasHeight)).c_str());
}

// ============================================================================
// Shape Rendering
// ============================================================================

void VectorDrawingEditor::DrawShape(ImDrawList* dl, const VectorShape& shape,
                                     f32 ox, f32 oy, f32 zoom, f32 layerOpacity) {
    if (shape.points.size() < 2 && shape.type != VectorShapeType::Star &&
        shape.type != VectorShapeType::Polygon) return;

    f32 alpha = shape.opacity * layerOpacity;
    u32 fill = shape.fillColor;
    u32 stroke = shape.strokeColor;

    // Apply layer opacity to alpha channel
    u32 fillAlpha = (u32)((fill >> 24) * alpha) & 0xFF;
    u32 strokeAlpha = (u32)((stroke >> 24) * alpha) & 0xFF;
    fill = (fill & 0x00FFFFFF) | (fillAlpha << 24);
    stroke = (stroke & 0x00FFFFFF) | (strokeAlpha << 24);

    f32 sw = shape.strokeWidth * zoom;

    switch (shape.type) {
        case VectorShapeType::Line: {
            if (shape.points.size() < 2) break;
            dl->AddLine(
                ImVec2(ox + shape.points[0].x * zoom, oy + shape.points[0].y * zoom),
                ImVec2(ox + shape.points[1].x * zoom, oy + shape.points[1].y * zoom),
                stroke, sw
            );
            break;
        }
        case VectorShapeType::Rect: {
            if (shape.points.size() < 2) break;
            ImVec2 p0(ox + shape.points[0].x * zoom, oy + shape.points[0].y * zoom);
            ImVec2 p1(ox + shape.points[1].x * zoom, oy + shape.points[1].y * zoom);
            ImVec2 minP(std::min(p0.x, p1.x), std::min(p0.y, p1.y));
            ImVec2 maxP(std::max(p0.x, p1.x), std::max(p0.y, p1.y));
            dl->AddRectFilled(minP, maxP, fill);
            dl->AddRect(minP, maxP, stroke, 0, 0, sw);
            break;
        }
        case VectorShapeType::Ellipse: {
            if (shape.points.size() < 2) break;
            f32 cx = (shape.points[0].x + shape.points[1].x) * 0.5f * zoom + ox;
            f32 cy = (shape.points[0].y + shape.points[1].y) * 0.5f * zoom + oy;
            f32 rx = std::abs(shape.points[1].x - shape.points[0].x) * 0.5f * zoom;
            f32 ry = std::abs(shape.points[1].y - shape.points[0].y) * 0.5f * zoom;
            // Approximate ellipse with scaled circle
            i32 segments = std::max(24, (i32)(std::max(rx, ry) * 0.5f));
            segments = std::min(segments, 128);
            std::vector<ImVec2> pts(segments);
            for (i32 i = 0; i < segments; i++) {
                f32 angle = (f32)i / (f32)segments * 2.0f * (f32)M_PI;
                pts[i] = ImVec2(cx + std::cos(angle) * rx, cy + std::sin(angle) * ry);
            }
            dl->AddConvexPolyFilled(pts.data(), segments, fill);
            dl->AddPolyline(pts.data(), segments, stroke, ImDrawFlags_Closed, sw);
            break;
        }
        case VectorShapeType::Pen: {
            if (shape.points.size() < 2) break;
            std::vector<ImVec2> pts;
            for (auto& p : shape.points) {
                pts.push_back(ImVec2(ox + p.x * zoom, oy + p.y * zoom));
            }
            // Fill if closed (3+ points)
            if (pts.size() >= 3) {
                dl->AddConvexPolyFilled(pts.data(), (int)pts.size(), fill);
            }
            dl->AddPolyline(pts.data(), (int)pts.size(), stroke, ImDrawFlags_Closed, sw);
            break;
        }
        case VectorShapeType::Bezier: {
            if (shape.points.size() < 4) break;
            dl->AddBezierCubic(
                ImVec2(ox + shape.points[0].x * zoom, oy + shape.points[0].y * zoom),
                ImVec2(ox + shape.points[1].x * zoom, oy + shape.points[1].y * zoom),
                ImVec2(ox + shape.points[2].x * zoom, oy + shape.points[2].y * zoom),
                ImVec2(ox + shape.points[3].x * zoom, oy + shape.points[3].y * zoom),
                stroke, sw
            );
            break;
        }
        case VectorShapeType::Star:
            DrawStarShape(dl, shape, ox, oy, zoom, alpha);
            break;
        case VectorShapeType::Polygon:
            DrawPolygonShape(dl, shape, ox, oy, zoom, alpha);
            break;
    }
}

void VectorDrawingEditor::DrawStarShape(ImDrawList* dl, const VectorShape& shape,
                                         f32 ox, f32 oy, f32 zoom, f32 alpha) {
    if (shape.points.size() < 2) return;
    f32 cx = (shape.points[0].x + shape.points[1].x) * 0.5f;
    f32 cy = (shape.points[0].y + shape.points[1].y) * 0.5f;
    f32 outerR = std::sqrt(
        (shape.points[1].x - shape.points[0].x) * (shape.points[1].x - shape.points[0].x) +
        (shape.points[1].y - shape.points[0].y) * (shape.points[1].y - shape.points[0].y)
    ) * 0.5f;
    f32 innerR = outerR * shape.innerRadius;

    u32 n = std::max(shape.pointCount, (u32)3);
    std::vector<ImVec2> pts;
    for (u32 i = 0; i < n * 2; i++) {
        f32 angle = (f32)i / (f32)(n * 2) * 2.0f * (f32)M_PI - (f32)M_PI * 0.5f;
        f32 r = (i % 2 == 0) ? outerR : innerR;
        pts.push_back(ImVec2(
            ox + (cx + std::cos(angle) * r) * zoom,
            oy + (cy + std::sin(angle) * r) * zoom
        ));
    }

    u32 fillAlpha = (u32)((shape.fillColor >> 24) * alpha) & 0xFF;
    u32 strokeAlpha = (u32)((shape.strokeColor >> 24) * alpha) & 0xFF;
    u32 fill = (shape.fillColor & 0x00FFFFFF) | (fillAlpha << 24);
    u32 stroke = (shape.strokeColor & 0x00FFFFFF) | (strokeAlpha << 24);

    dl->AddConvexPolyFilled(pts.data(), (int)pts.size(), fill);
    dl->AddPolyline(pts.data(), (int)pts.size(), stroke, ImDrawFlags_Closed, shape.strokeWidth * zoom);
}

void VectorDrawingEditor::DrawPolygonShape(ImDrawList* dl, const VectorShape& shape,
                                            f32 ox, f32 oy, f32 zoom, f32 alpha) {
    if (shape.points.size() < 2) return;
    f32 cx = (shape.points[0].x + shape.points[1].x) * 0.5f;
    f32 cy = (shape.points[0].y + shape.points[1].y) * 0.5f;
    f32 r = std::sqrt(
        (shape.points[1].x - shape.points[0].x) * (shape.points[1].x - shape.points[0].x) +
        (shape.points[1].y - shape.points[0].y) * (shape.points[1].y - shape.points[0].y)
    ) * 0.5f;

    u32 n = std::max(shape.pointCount, (u32)3);
    std::vector<ImVec2> pts;
    for (u32 i = 0; i < n; i++) {
        f32 angle = (f32)i / (f32)n * 2.0f * (f32)M_PI - (f32)M_PI * 0.5f;
        pts.push_back(ImVec2(
            ox + (cx + std::cos(angle) * r) * zoom,
            oy + (cy + std::sin(angle) * r) * zoom
        ));
    }

    u32 fillAlpha = (u32)((shape.fillColor >> 24) * alpha) & 0xFF;
    u32 strokeAlpha = (u32)((shape.strokeColor >> 24) * alpha) & 0xFF;
    u32 fill = (shape.fillColor & 0x00FFFFFF) | (fillAlpha << 24);
    u32 stroke = (shape.strokeColor & 0x00FFFFFF) | (strokeAlpha << 24);

    dl->AddConvexPolyFilled(pts.data(), (int)pts.size(), fill);
    dl->AddPolyline(pts.data(), (int)pts.size(), stroke, ImDrawFlags_Closed, shape.strokeWidth * zoom);
}

void VectorDrawingEditor::DrawSelectionHandles(ImDrawList* dl, const VectorShape& shape,
                                                f32 ox, f32 oy, f32 zoom) {
    // Draw handles at each control point
    for (i32 i = 0; i < (i32)shape.points.size(); i++) {
        ImVec2 pt(ox + shape.points[i].x * zoom, oy + shape.points[i].y * zoom);
        dl->AddRectFilled(ImVec2(pt.x - 4, pt.y - 4), ImVec2(pt.x + 4, pt.y + 4),
                          IM_COL32(255, 255, 255, 255));
        dl->AddRect(ImVec2(pt.x - 4, pt.y - 4), ImVec2(pt.x + 4, pt.y + 4),
                    IM_COL32(0, 120, 255, 255));
    }

    // Bounding box for rect/ellipse/star/polygon
    if (shape.points.size() == 2 && shape.type != VectorShapeType::Line) {
        ImVec2 p0(ox + shape.points[0].x * zoom, oy + shape.points[0].y * zoom);
        ImVec2 p1(ox + shape.points[1].x * zoom, oy + shape.points[1].y * zoom);
        ImVec2 minP(std::min(p0.x, p1.x), std::min(p0.y, p1.y));
        ImVec2 maxP(std::max(p0.x, p1.x), std::max(p0.y, p1.y));
        dl->AddRect(minP, maxP, IM_COL32(0, 120, 255, 128), 0, 0, 1.0f);
    }
}

// ============================================================================
// Tool Handling
// ============================================================================

void VectorDrawingEditor::HandleSelectTool(f32 canvasX, f32 canvasY,
                                            bool mouseDown, bool mouseClicked, bool mouseReleased) {
    if (m_ActiveLayer < 0 || m_ActiveLayer >= (i32)m_Document.layers.size()) return;
    auto& layer = m_Document.layers[m_ActiveLayer];

    if (mouseClicked) {
        // Check handle hit first
        if (m_SelectedShape >= 0 && m_SelectedShape < (i32)layer.shapes.size()) {
            i32 handle = HitTestHandle(layer.shapes[m_SelectedShape], canvasX, canvasY);
            if (handle >= 0) {
                m_DragHandle = handle;
                m_DragStart = Math::Vector2(canvasX, canvasY);
                PushUndoState();
                return;
            }
        }

        // Check shape hit (reverse order for top-most first)
        m_SelectedShape = -1;
        for (i32 i = (i32)layer.shapes.size() - 1; i >= 0; i--) {
            if (HitTestShape(layer.shapes[i], canvasX, canvasY) >= 0) {
                m_SelectedShape = i;
                m_DragStart = Math::Vector2(canvasX, canvasY);
                m_DragHandle = -1;
                PushUndoState();
                break;
            }
        }
    }

    if (mouseDown && m_SelectedShape >= 0 && m_SelectedShape < (i32)layer.shapes.size()) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        f32 dx = delta.x / m_Zoom;
        f32 dy = delta.y / m_Zoom;

        if (m_DragHandle >= 0) {
            // Move specific handle
            auto& shape = layer.shapes[m_SelectedShape];
            if (m_DragHandle < (i32)shape.points.size()) {
                shape.points[m_DragHandle].x += dx;
                shape.points[m_DragHandle].y += dy;
            }
        } else if (dx != 0 || dy != 0) {
            // Move entire shape
            auto& shape = layer.shapes[m_SelectedShape];
            for (auto& pt : shape.points) {
                pt.x += dx;
                pt.y += dy;
            }
        }
    }

    if (mouseReleased) {
        m_DragHandle = -1;
    }
}

void VectorDrawingEditor::HandleShapeTool(f32 canvasX, f32 canvasY,
                                           bool mouseDown, bool mouseClicked, bool mouseReleased) {
    if (mouseClicked) {
        m_IsDrawing = true;
        m_DragStart = Math::Vector2(canvasX, canvasY);
        m_DragCurrent = m_DragStart;
    }
    if (mouseDown && m_IsDrawing) {
        m_DragCurrent = Math::Vector2(canvasX, canvasY);
    }
    if (mouseReleased && m_IsDrawing) {
        m_IsDrawing = false;

        // Minimum size check
        f32 dx = std::abs(m_DragCurrent.x - m_DragStart.x);
        f32 dy = std::abs(m_DragCurrent.y - m_DragStart.y);
        if (dx < 2 && dy < 2) return;

        if (m_ActiveLayer >= 0 && m_ActiveLayer < (i32)m_Document.layers.size()) {
            PushUndoState();
            VectorShape shape;
            shape.fillColor = m_FillColor;
            shape.strokeColor = m_StrokeColor;
            shape.strokeWidth = m_StrokeWidth;
            shape.pointCount = m_ShapePointCount;
            shape.innerRadius = m_ShapeInnerRadius;
            shape.points = { m_DragStart, m_DragCurrent };

            switch (m_CurrentTool) {
                case VectorTool::Line:    shape.type = VectorShapeType::Line; break;
                case VectorTool::Rect:    shape.type = VectorShapeType::Rect; break;
                case VectorTool::Ellipse: shape.type = VectorShapeType::Ellipse; break;
                case VectorTool::Star:    shape.type = VectorShapeType::Star; break;
                case VectorTool::Polygon: shape.type = VectorShapeType::Polygon; break;
                default: break;
            }

            shape.name = std::string(m_CurrentTool == VectorTool::Line ? "Line" :
                                      m_CurrentTool == VectorTool::Rect ? "Rect" :
                                      m_CurrentTool == VectorTool::Ellipse ? "Ellipse" :
                                      m_CurrentTool == VectorTool::Star ? "Star" : "Polygon");

            m_Document.layers[m_ActiveLayer].shapes.push_back(shape);
            m_SelectedShape = (i32)m_Document.layers[m_ActiveLayer].shapes.size() - 1;
        }
    }
}

void VectorDrawingEditor::HandlePenTool(f32 canvasX, f32 canvasY,
                                         bool mouseClicked, bool mouseDoubleClicked, bool rightClicked) {
    if (rightClicked && m_PenActive) {
        // Cancel pen
        m_PenActive = false;
        m_PenPoints.clear();
        return;
    }

    if (mouseDoubleClicked && m_PenActive && m_PenPoints.size() >= 2) {
        // Finish pen path
        PushUndoState();
        VectorShape shape;
        shape.type = VectorShapeType::Pen;
        shape.points = m_PenPoints;
        shape.fillColor = m_FillColor;
        shape.strokeColor = m_StrokeColor;
        shape.strokeWidth = m_StrokeWidth;
        shape.name = "Pen Path";

        if (m_ActiveLayer >= 0 && m_ActiveLayer < (i32)m_Document.layers.size()) {
            m_Document.layers[m_ActiveLayer].shapes.push_back(shape);
            m_SelectedShape = (i32)m_Document.layers[m_ActiveLayer].shapes.size() - 1;
        }

        m_PenActive = false;
        m_PenPoints.clear();
        return;
    }

    if (mouseClicked && !mouseDoubleClicked) {
        m_PenActive = true;
        m_PenPoints.push_back(Math::Vector2(canvasX, canvasY));

        // Auto-close if clicking near first point
        if (m_PenPoints.size() >= 3) {
            f32 dist = std::sqrt(
                (canvasX - m_PenPoints[0].x) * (canvasX - m_PenPoints[0].x) +
                (canvasY - m_PenPoints[0].y) * (canvasY - m_PenPoints[0].y)
            );
            if (dist < 8.0f / m_Zoom) {
                m_PenPoints.pop_back(); // Remove the closing click
                PushUndoState();
                VectorShape shape;
                shape.type = VectorShapeType::Pen;
                shape.points = m_PenPoints;
                shape.fillColor = m_FillColor;
                shape.strokeColor = m_StrokeColor;
                shape.strokeWidth = m_StrokeWidth;
                shape.name = "Pen Path";

                if (m_ActiveLayer >= 0 && m_ActiveLayer < (i32)m_Document.layers.size()) {
                    m_Document.layers[m_ActiveLayer].shapes.push_back(shape);
                    m_SelectedShape = (i32)m_Document.layers[m_ActiveLayer].shapes.size() - 1;
                }

                m_PenActive = false;
                m_PenPoints.clear();
            }
        }
    }
}

void VectorDrawingEditor::HandleBezierTool(f32 canvasX, f32 canvasY,
                                            bool mouseClicked, bool mouseReleased) {
    if (mouseClicked) {
        Math::Vector2 pt(canvasX, canvasY);
        switch (m_BezierStep) {
            case 0: m_BezierStart = pt; m_BezierStep = 1; break;
            case 1: m_BezierCP1 = pt; m_BezierStep = 2; break;
            case 2: m_BezierCP2 = pt; m_BezierStep = 3; break;
            case 3: {
                // Finish bezier
                PushUndoState();
                VectorShape shape;
                shape.type = VectorShapeType::Bezier;
                shape.points = { m_BezierStart, m_BezierCP1, m_BezierCP2, pt };
                shape.strokeColor = m_StrokeColor;
                shape.fillColor = m_FillColor;
                shape.strokeWidth = m_StrokeWidth;
                shape.name = "Bezier";

                if (m_ActiveLayer >= 0 && m_ActiveLayer < (i32)m_Document.layers.size()) {
                    m_Document.layers[m_ActiveLayer].shapes.push_back(shape);
                    m_SelectedShape = (i32)m_Document.layers[m_ActiveLayer].shapes.size() - 1;
                }

                m_BezierStep = 0;
                break;
            }
        }
    }
}

// ============================================================================
// Hit Testing
// ============================================================================

i32 VectorDrawingEditor::HitTestShape(const VectorShape& shape, f32 x, f32 y) const {
    f32 threshold = 8.0f / m_Zoom;

    if (shape.points.size() >= 2) {
        switch (shape.type) {
            case VectorShapeType::Line: {
                // Distance to line segment
                f32 x1 = shape.points[0].x, y1 = shape.points[0].y;
                f32 x2 = shape.points[1].x, y2 = shape.points[1].y;
                f32 dx = x2 - x1, dy = y2 - y1;
                f32 lenSq = dx * dx + dy * dy;
                if (lenSq < 0.001f) return -1;
                f32 t = std::clamp(((x - x1) * dx + (y - y1) * dy) / lenSq, 0.0f, 1.0f);
                f32 px = x1 + t * dx, py = y1 + t * dy;
                f32 dist = std::sqrt((x - px) * (x - px) + (y - py) * (y - py));
                return dist < threshold ? 0 : -1;
            }
            case VectorShapeType::Rect: {
                f32 minX = std::min(shape.points[0].x, shape.points[1].x);
                f32 maxX = std::max(shape.points[0].x, shape.points[1].x);
                f32 minY = std::min(shape.points[0].y, shape.points[1].y);
                f32 maxY = std::max(shape.points[0].y, shape.points[1].y);
                return (x >= minX - threshold && x <= maxX + threshold &&
                        y >= minY - threshold && y <= maxY + threshold) ? 0 : -1;
            }
            case VectorShapeType::Ellipse:
            case VectorShapeType::Star:
            case VectorShapeType::Polygon: {
                f32 cx = (shape.points[0].x + shape.points[1].x) * 0.5f;
                f32 cy = (shape.points[0].y + shape.points[1].y) * 0.5f;
                f32 rx = std::abs(shape.points[1].x - shape.points[0].x) * 0.5f + threshold;
                f32 ry = std::abs(shape.points[1].y - shape.points[0].y) * 0.5f + threshold;
                if (rx < 0.001f || ry < 0.001f) return -1;
                f32 dx = (x - cx) / rx, dy = (y - cy) / ry;
                return (dx * dx + dy * dy <= 1.0f) ? 0 : -1;
            }
            default: break;
        }
    }

    // Pen/Bezier — check proximity to any point
    for (auto& pt : shape.points) {
        f32 dist = std::sqrt((x - pt.x) * (x - pt.x) + (y - pt.y) * (y - pt.y));
        if (dist < threshold) return 0;
    }
    return -1;
}

i32 VectorDrawingEditor::HitTestHandle(const VectorShape& shape, f32 x, f32 y) const {
    f32 threshold = 6.0f / m_Zoom;
    for (i32 i = 0; i < (i32)shape.points.size(); i++) {
        f32 dist = std::sqrt(
            (x - shape.points[i].x) * (x - shape.points[i].x) +
            (y - shape.points[i].y) * (y - shape.points[i].y)
        );
        if (dist < threshold) return i;
    }
    return -1;
}

// ============================================================================
// Layer Panel
// ============================================================================

void VectorDrawingEditor::DrawLayerPanel() {
    ImGui::Text("Layers");
    ImGui::Separator();

    if (ImGui::Button("+")) {
        PushUndoState();
        VectorLayer layer;
        layer.name = "Layer " + std::to_string(m_Document.layers.size() + 1);
        m_Document.layers.push_back(layer);
    }
    ImGui::SameLine();
    bool canDelete = m_Document.layers.size() > 1;
    if (!canDelete) ImGui::BeginDisabled();
    if (ImGui::Button("-") && m_ActiveLayer >= 0 &&
        m_ActiveLayer < (i32)m_Document.layers.size()) {
        PushUndoState();
        m_Document.layers.erase(m_Document.layers.begin() + m_ActiveLayer);
        if (m_ActiveLayer >= (i32)m_Document.layers.size()) {
            m_ActiveLayer = (i32)m_Document.layers.size() - 1;
        }
        m_SelectedShape = -1;
    }
    if (!canDelete) ImGui::EndDisabled();

    ImGui::Separator();

    // Layer list (top = front)
    for (i32 i = (i32)m_Document.layers.size() - 1; i >= 0; i--) {
        auto& layer = m_Document.layers[i];
        ImGui::PushID(i);

        bool selected = (i == m_ActiveLayer);
        if (ImGui::Selectable(layer.name.c_str(), selected)) {
            m_ActiveLayer = i;
            m_SelectedShape = -1;
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40);
        ImGui::Checkbox("##vis", &layer.visible);
        ImGui::SameLine();
        ImGui::Checkbox("##lk", &layer.locked);

        ImGui::PopID();
    }

    // Shape list for active layer
    if (m_ActiveLayer >= 0 && m_ActiveLayer < (i32)m_Document.layers.size()) {
        ImGui::Separator();
        ImGui::Text("Shapes");
        auto& shapes = m_Document.layers[m_ActiveLayer].shapes;
        for (i32 i = 0; i < (i32)shapes.size(); i++) {
            ImGui::PushID(1000 + i);
            bool sel = (i == m_SelectedShape);
            std::string label = shapes[i].name.empty() ? ("Shape " + std::to_string(i)) : shapes[i].name;
            if (ImGui::Selectable(label.c_str(), sel)) {
                m_SelectedShape = i;
            }
            ImGui::PopID();
        }
    }
}

// ============================================================================
// Property Panel
// ============================================================================

void VectorDrawingEditor::DrawPropertyPanel() {
    // Drawing colors (always shown)
    ImGui::Text("Drawing Settings:");
    ImGui::SameLine();

    ImVec4 fillVec = ImGui::ColorConvertU32ToFloat4(m_FillColor);
    if (ImGui::ColorEdit4("Fill", &fillVec.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
        m_FillColor = ImGui::ColorConvertFloat4ToU32(fillVec);
    }
    ImGui::SameLine();
    ImGui::Text("Fill");
    ImGui::SameLine();

    ImVec4 strokeVec = ImGui::ColorConvertU32ToFloat4(m_StrokeColor);
    if (ImGui::ColorEdit4("Stroke", &strokeVec.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
        m_StrokeColor = ImGui::ColorConvertFloat4ToU32(strokeVec);
    }
    ImGui::SameLine();
    ImGui::Text("Stroke");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(60);
    ImGui::DragFloat("Width##StrokeW", &m_StrokeWidth, 0.1f, 0.5f, 20.0f, "%.1f");

    // Star/Polygon settings
    if (m_CurrentTool == VectorTool::Star || m_CurrentTool == VectorTool::Polygon) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        i32 pc = (i32)m_ShapePointCount;
        if (ImGui::DragInt("Points", &pc, 0.1f, 3, 24)) {
            m_ShapePointCount = std::clamp((u32)pc, (u32)3, (u32)24);
        }
        if (m_CurrentTool == VectorTool::Star) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            ImGui::DragFloat("Inner", &m_ShapeInnerRadius, 0.01f, 0.1f, 0.9f, "%.2f");
        }
    }

    // Selected shape properties
    if (m_SelectedShape >= 0 && m_ActiveLayer >= 0 &&
        m_ActiveLayer < (i32)m_Document.layers.size()) {
        auto& shapes = m_Document.layers[m_ActiveLayer].shapes;
        if (m_SelectedShape < (i32)shapes.size()) {
            auto& shape = shapes[m_SelectedShape];
            ImGui::Separator();
            ImGui::Text("Selected: %s", shape.name.c_str());
            ImGui::SameLine();

            ImVec4 sf = ImGui::ColorConvertU32ToFloat4(shape.fillColor);
            if (ImGui::ColorEdit4("##ShapeFill", &sf.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                shape.fillColor = ImGui::ColorConvertFloat4ToU32(sf);
            }
            ImGui::SameLine();
            ImGui::Text("Fill");
            ImGui::SameLine();

            ImVec4 ss = ImGui::ColorConvertU32ToFloat4(shape.strokeColor);
            if (ImGui::ColorEdit4("##ShapeStroke", &ss.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
                shape.strokeColor = ImGui::ColorConvertFloat4ToU32(ss);
            }
            ImGui::SameLine();
            ImGui::Text("Stroke");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(60);
            ImGui::DragFloat("##ShapeStrokeW", &shape.strokeWidth, 0.1f, 0.5f, 20.0f, "%.1f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            ImGui::DragFloat("Opacity", &shape.opacity, 0.01f, 0.0f, 1.0f, "%.2f");
        }
    }
}

// ============================================================================
// SVG Export Helpers
// ============================================================================

std::string VectorDrawingEditor::ShapeToSVG(const VectorShape& shape) const {
    std::ostringstream svg;
    std::string fillHex = ColorToSVGHex(shape.fillColor);
    std::string strokeHex = ColorToSVGHex(shape.strokeColor);
    f32 fillOpacity = ((shape.fillColor >> 24) & 0xFF) / 255.0f * shape.opacity;
    f32 strokeOpacity = ((shape.strokeColor >> 24) & 0xFF) / 255.0f * shape.opacity;

    switch (shape.type) {
        case VectorShapeType::Line: {
            if (shape.points.size() < 2) break;
            svg << "<line x1=\"" << shape.points[0].x << "\" y1=\"" << shape.points[0].y
                << "\" x2=\"" << shape.points[1].x << "\" y2=\"" << shape.points[1].y
                << "\" stroke=\"" << strokeHex << "\" stroke-opacity=\"" << strokeOpacity
                << "\" stroke-width=\"" << shape.strokeWidth << "\"/>";
            break;
        }
        case VectorShapeType::Rect: {
            if (shape.points.size() < 2) break;
            f32 x = std::min(shape.points[0].x, shape.points[1].x);
            f32 y = std::min(shape.points[0].y, shape.points[1].y);
            f32 w = std::abs(shape.points[1].x - shape.points[0].x);
            f32 h = std::abs(shape.points[1].y - shape.points[0].y);
            svg << "<rect x=\"" << x << "\" y=\"" << y
                << "\" width=\"" << w << "\" height=\"" << h
                << "\" fill=\"" << fillHex << "\" fill-opacity=\"" << fillOpacity
                << "\" stroke=\"" << strokeHex << "\" stroke-opacity=\"" << strokeOpacity
                << "\" stroke-width=\"" << shape.strokeWidth << "\"/>";
            break;
        }
        case VectorShapeType::Ellipse: {
            if (shape.points.size() < 2) break;
            f32 cx = (shape.points[0].x + shape.points[1].x) * 0.5f;
            f32 cy = (shape.points[0].y + shape.points[1].y) * 0.5f;
            f32 rx = std::abs(shape.points[1].x - shape.points[0].x) * 0.5f;
            f32 ry = std::abs(shape.points[1].y - shape.points[0].y) * 0.5f;
            svg << "<ellipse cx=\"" << cx << "\" cy=\"" << cy
                << "\" rx=\"" << rx << "\" ry=\"" << ry
                << "\" fill=\"" << fillHex << "\" fill-opacity=\"" << fillOpacity
                << "\" stroke=\"" << strokeHex << "\" stroke-opacity=\"" << strokeOpacity
                << "\" stroke-width=\"" << shape.strokeWidth << "\"/>";
            break;
        }
        case VectorShapeType::Pen: {
            if (shape.points.size() < 2) break;
            svg << "<path d=\"M " << shape.points[0].x << " " << shape.points[0].y;
            for (usize i = 1; i < shape.points.size(); i++) {
                svg << " L " << shape.points[i].x << " " << shape.points[i].y;
            }
            svg << " Z\" fill=\"" << fillHex << "\" fill-opacity=\"" << fillOpacity
                << "\" stroke=\"" << strokeHex << "\" stroke-opacity=\"" << strokeOpacity
                << "\" stroke-width=\"" << shape.strokeWidth << "\"/>";
            break;
        }
        case VectorShapeType::Bezier: {
            if (shape.points.size() < 4) break;
            svg << "<path d=\"M " << shape.points[0].x << " " << shape.points[0].y
                << " C " << shape.points[1].x << " " << shape.points[1].y
                << " " << shape.points[2].x << " " << shape.points[2].y
                << " " << shape.points[3].x << " " << shape.points[3].y
                << "\" fill=\"none\" stroke=\"" << strokeHex
                << "\" stroke-opacity=\"" << strokeOpacity
                << "\" stroke-width=\"" << shape.strokeWidth << "\"/>";
            break;
        }
        case VectorShapeType::Star:
        case VectorShapeType::Polygon: {
            if (shape.points.size() < 2) break;
            f32 cx = (shape.points[0].x + shape.points[1].x) * 0.5f;
            f32 cy = (shape.points[0].y + shape.points[1].y) * 0.5f;
            f32 outerR = std::sqrt(
                (shape.points[1].x - shape.points[0].x) * (shape.points[1].x - shape.points[0].x) +
                (shape.points[1].y - shape.points[0].y) * (shape.points[1].y - shape.points[0].y)
            ) * 0.5f;

            u32 n = std::max(shape.pointCount, (u32)3);
            svg << "<polygon points=\"";

            if (shape.type == VectorShapeType::Star) {
                f32 innerR = outerR * shape.innerRadius;
                for (u32 i = 0; i < n * 2; i++) {
                    f32 angle = (f32)i / (f32)(n * 2) * 2.0f * (f32)M_PI - (f32)M_PI * 0.5f;
                    f32 r = (i % 2 == 0) ? outerR : innerR;
                    if (i > 0) svg << " ";
                    svg << (cx + std::cos(angle) * r) << "," << (cy + std::sin(angle) * r);
                }
            } else {
                for (u32 i = 0; i < n; i++) {
                    f32 angle = (f32)i / (f32)n * 2.0f * (f32)M_PI - (f32)M_PI * 0.5f;
                    if (i > 0) svg << " ";
                    svg << (cx + std::cos(angle) * outerR) << "," << (cy + std::sin(angle) * outerR);
                }
            }

            svg << "\" fill=\"" << fillHex << "\" fill-opacity=\"" << fillOpacity
                << "\" stroke=\"" << strokeHex << "\" stroke-opacity=\"" << strokeOpacity
                << "\" stroke-width=\"" << shape.strokeWidth << "\"/>";
            break;
        }
    }

    return svg.str();
}

std::string VectorDrawingEditor::ColorToSVGHex(u32 abgr) const {
    u8 r = (abgr >> 0) & 0xFF;
    u8 g = (abgr >> 8) & 0xFF;
    u8 b = (abgr >> 16) & 0xFF;
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return buf;
}

// ============================================================================
// Undo / Redo
// ============================================================================

void VectorDrawingEditor::PushUndoState() {
    UndoSnapshot snap;
    snap.layers = m_Document.layers;
    snap.activeLayer = m_ActiveLayer;
    m_UndoStack.push_back(snap);
    if (m_UndoStack.size() > MAX_UNDO) {
        m_UndoStack.erase(m_UndoStack.begin());
    }
    m_RedoStack.clear();
}

void VectorDrawingEditor::Undo() {
    if (m_UndoStack.empty()) return;

    UndoSnapshot redo;
    redo.layers = m_Document.layers;
    redo.activeLayer = m_ActiveLayer;
    m_RedoStack.push_back(redo);

    auto& snap = m_UndoStack.back();
    m_Document.layers = snap.layers;
    m_ActiveLayer = snap.activeLayer;
    m_UndoStack.pop_back();
    m_SelectedShape = -1;
}

void VectorDrawingEditor::Redo() {
    if (m_RedoStack.empty()) return;

    UndoSnapshot undo;
    undo.layers = m_Document.layers;
    undo.activeLayer = m_ActiveLayer;
    m_UndoStack.push_back(undo);

    auto& snap = m_RedoStack.back();
    m_Document.layers = snap.layers;
    m_ActiveLayer = snap.activeLayer;
    m_RedoStack.pop_back();
    m_SelectedShape = -1;
}

} // namespace Editor
} // namespace Enjin
