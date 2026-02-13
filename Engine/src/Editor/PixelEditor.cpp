#include "Enjin/Editor/PixelEditor.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Logging/Log.h"
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <queue>
#include <cstring>

namespace Enjin {
namespace Editor {

// ============================================================================
// RETRO PRESETS
// ============================================================================

const RetroPreset PixelEditor::s_RetroPresets[] = {
    {"Game Boy",    160, 144},
    {"NES",         256, 240},
    {"SNES",        256, 224},
    {"GBA",         240, 160},
    {"Genesis",     320, 224},
    {"Custom 16x16",  16,  16},
    {"Custom 32x32",  32,  32},
    {"Custom 64x64",  64,  64},
    {"Custom 128x128",128, 128},
};
static constexpr usize NUM_RETRO_PRESETS = sizeof(PixelEditor::s_RetroPresets) / sizeof(RetroPreset);

// ============================================================================
// CANVAS MANAGEMENT
// ============================================================================

void PixelEditor::NewCanvas(u32 width, u32 height) {
    m_Width = width;
    m_Height = height;
    m_Layers.clear();
    m_UndoStack.clear();
    m_RedoStack.clear();

    // Create initial layer
    PixelLayer layer;
    layer.name = "Background";
    layer.pixels.resize(width * height * 4, 0); // Transparent
    m_Layers.push_back(std::move(layer));
    m_ActiveLayer = 0;

    // Default palette
    m_Palette.name = "Default";
    m_Palette.colors = {
        0xFF000000, 0xFFFFFFFF, 0xFF0000FF, 0xFF00FF00,
        0xFFFF0000, 0xFF00FFFF, 0xFFFF00FF, 0xFFFFFF00,
        0xFF808080, 0xFF404040, 0xFFC0C0C0, 0xFF000080,
        0xFF008000, 0xFF800000, 0xFF008080, 0xFF800080
    };
    m_Palette.selectedIndex = 0;

    // Reset view
    m_Zoom = std::min(400.0f / static_cast<f32>(width), 400.0f / static_cast<f32>(height));
    m_PanX = 0.0f;
    m_PanY = 0.0f;

    ENJIN_LOG_INFO(Editor, "PixelEditor: New canvas %ux%u", width, height);
}

bool PixelEditor::LoadImage(const std::string& path) {
    int w, h, channels;
    u8* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!data) {
        ENJIN_LOG_ERROR(Editor, "PixelEditor: Failed to load '%s'", path.c_str());
        return false;
    }

    // S8: Validate dimensions don't exceed reasonable max to prevent overflow
    if (w > 8192 || h > 8192 || w <= 0 || h <= 0) {
        ENJIN_LOG_ERROR(Editor, "PixelEditor: Image dimensions too large (%dx%d, max 8192)", w, h);
        stbi_image_free(data);
        return false;
    }

    NewCanvas(static_cast<u32>(w), static_cast<u32>(h));

    // Copy pixels into first layer — use size_t for safe calculation
    size_t pixelDataSize = static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
    std::memcpy(m_Layers[0].pixels.data(), data, pixelDataSize);
    stbi_image_free(data);

    ENJIN_LOG_INFO(Editor, "PixelEditor: Loaded '%s' (%dx%d)", path.c_str(), w, h);
    return true;
}

bool PixelEditor::ExportPNG(const std::string& path) {
    if (!HasCanvas()) return false;

    std::vector<u8> composited;
    CompositeImage(composited);

    int result = stbi_write_png(path.c_str(), static_cast<int>(m_Width), static_cast<int>(m_Height),
                                4, composited.data(), static_cast<int>(m_Width * 4));

    if (result) {
        ENJIN_LOG_INFO(Editor, "PixelEditor: Exported to '%s'", path.c_str());
    } else {
        ENJIN_LOG_ERROR(Editor, "PixelEditor: Failed to export to '%s'", path.c_str());
    }
    return result != 0;
}

// ============================================================================
// RENDERING
// ============================================================================

void PixelEditor::Render(const EditorSettings& settings) {
    DrawToolbar();

    if (!HasCanvas()) {
        ImGui::TextDisabled("No canvas. Create a new one or load an image.");
        return;
    }

    // Layout: palette on left, canvas in center, layers on right
    f32 paletteW = 60.0f;
    f32 layerW = 150.0f;

    // Palette panel
    ImGui::BeginChild("##pe_palette", ImVec2(paletteW, 0), true);
    DrawPalettePanel();
    ImGui::EndChild();

    ImGui::SameLine();

    // Canvas area
    f32 canvasW = ImGui::GetContentRegionAvail().x - layerW - 8.0f;
    ImGui::BeginChild("##pe_canvas", ImVec2(canvasW, 0), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawCanvasArea();
    ImGui::EndChild();

    ImGui::SameLine();

    // Layers panel
    ImGui::BeginChild("##pe_layers", ImVec2(layerW, 0), true);
    DrawLayerPanel();
    ImGui::EndChild();

    // Animation timeline (below everything)
    if (!m_AnimFrames.empty() || HasCanvas()) {
        DrawAnimationTimeline();
    }
}

// ============================================================================
// TOOLBAR
// ============================================================================

void PixelEditor::DrawToolbar() {
    // New canvas
    if (ImGui::Button("New")) {
        ImGui::OpenPopup("NewCanvasPopup");
    }

    if (ImGui::BeginPopup("NewCanvasPopup")) {
        ImGui::Text("Presets:");
        for (usize i = 0; i < NUM_RETRO_PRESETS; i++) {
            if (ImGui::MenuItem(s_RetroPresets[i].name)) {
                NewCanvas(s_RetroPresets[i].width, s_RetroPresets[i].height);
            }
        }
        ImGui::Separator();
        static int customW = 64, customH = 64;
        ImGui::InputInt("Width", &customW);
        ImGui::InputInt("Height", &customH);
        customW = std::clamp(customW, 1, 1024);
        customH = std::clamp(customH, 1, 1024);
        if (ImGui::Button("Create")) {
            NewCanvas(static_cast<u32>(customW), static_cast<u32>(customH));
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        ImGui::OpenPopup("LoadImagePopup");
    }
    if (ImGui::BeginPopup("LoadImagePopup")) {
        static char pathBuf[512] = "";
        ImGui::InputText("Path", pathBuf, sizeof(pathBuf));
        if (ImGui::Button("Load")) {
            LoadImage(pathBuf);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    if (ImGui::Button("Export")) {
        ImGui::OpenPopup("ExportPopup");
    }
    if (ImGui::BeginPopup("ExportPopup")) {
        static char exportBuf[512] = "sprite.png";
        ImGui::InputText("Path", exportBuf, sizeof(exportBuf));
        if (ImGui::Button("Save PNG")) {
            ExportPNG(exportBuf);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Tool selection
    const char* toolNames[] = {"Pencil", "Eraser", "Fill", "Line", "Rect", "Ellipse", "Eyedropper", "Select", "Polygon"};
    for (int i = 0; i < 9; i++) {
        if (i > 0) ImGui::SameLine();
        bool selected = (static_cast<int>(m_CurrentTool) == i);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.55f, 0.38f, 1.0f));
        if (ImGui::SmallButton(toolNames[i])) {
            m_CurrentTool = static_cast<PixelTool>(i);
        }
        if (selected) ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Undo/Redo
    if (ImGui::SmallButton("Undo")) Undo();
    ImGui::SameLine();
    if (ImGui::SmallButton("Redo")) Redo();

    ImGui::SameLine();
    ImGui::Checkbox("Grid", &m_ShowGrid);

    if (HasCanvas()) {
        ImGui::SameLine();
        ImGui::Text("%ux%u  Zoom: %.0fx", m_Width, m_Height, m_Zoom);
    }

    ImGui::Separator();
}

// ============================================================================
// CANVAS RENDERING AND INPUT
// ============================================================================

void PixelEditor::DrawCanvasArea() {
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background (checkerboard pattern for transparency)
    dl->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                      IM_COL32(40, 40, 40, 255));

    // Composite layers
    std::vector<u8> composited;
    CompositeImage(composited);

    // Draw pixels
    f32 pixelSize = m_Zoom;
    f32 originX = canvasPos.x + m_PanX + canvasSize.x * 0.5f - m_Width * pixelSize * 0.5f;
    f32 originY = canvasPos.y + m_PanY + canvasSize.y * 0.5f - m_Height * pixelSize * 0.5f;

    for (u32 y = 0; y < m_Height; y++) {
        for (u32 x = 0; x < m_Width; x++) {
            u32 idx = (y * m_Width + x) * 4;
            u8 r = composited[idx];
            u8 g = composited[idx + 1];
            u8 b = composited[idx + 2];
            u8 a = composited[idx + 3];

            if (a == 0) continue; // Skip fully transparent

            f32 px = originX + x * pixelSize;
            f32 py = originY + y * pixelSize;

            // Clip to visible area
            if (px + pixelSize < canvasPos.x || px > canvasPos.x + canvasSize.x) continue;
            if (py + pixelSize < canvasPos.y || py > canvasPos.y + canvasSize.y) continue;

            dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pixelSize, py + pixelSize),
                              IM_COL32(r, g, b, a));
        }
    }

    // Grid overlay
    if (m_ShowGrid && pixelSize >= 4.0f) {
        ImU32 gridColor = IM_COL32(80, 80, 80, 100);
        for (u32 x = 0; x <= m_Width; x++) {
            f32 px = originX + x * pixelSize;
            if (px >= canvasPos.x && px <= canvasPos.x + canvasSize.x) {
                dl->AddLine(ImVec2(px, std::max(originY, canvasPos.y)),
                            ImVec2(px, std::min(originY + m_Height * pixelSize, canvasPos.y + canvasSize.y)),
                            gridColor);
            }
        }
        for (u32 y = 0; y <= m_Height; y++) {
            f32 py = originY + y * pixelSize;
            if (py >= canvasPos.y && py <= canvasPos.y + canvasSize.y) {
                dl->AddLine(ImVec2(std::max(originX, canvasPos.x), py),
                            ImVec2(std::min(originX + m_Width * pixelSize, canvasPos.x + canvasSize.x), py),
                            gridColor);
            }
        }
    }

    // Canvas border
    dl->AddRect(ImVec2(originX, originY),
                ImVec2(originX + m_Width * pixelSize, originY + m_Height * pixelSize),
                IM_COL32(200, 200, 200, 255));

    // Polygon collider overlay
    if (m_CurrentTool == PixelTool::PolygonEdit) {
        DrawPolygonOverlay(originX, originY, pixelSize, dl);
    }

    // Handle input
    ImGui::InvisibleButton("##pe_canvas_btn", canvasSize);
    bool isHovered = ImGui::IsItemHovered();

    if (isHovered) {
        ImVec2 mousePos = ImGui::GetMousePos();
        i32 pixelX = static_cast<i32>((mousePos.x - originX) / pixelSize);
        i32 pixelY = static_cast<i32>((mousePos.y - originY) / pixelSize);

        // Zoom with scroll wheel
        f32 wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            f32 oldZoom = m_Zoom;
            m_Zoom = std::clamp(m_Zoom + wheel * m_Zoom * 0.15f, 1.0f, 64.0f);
            // Adjust pan to zoom toward mouse
            f32 zoomRatio = m_Zoom / oldZoom;
            m_PanX = mousePos.x - canvasPos.x - canvasSize.x * 0.5f -
                     (mousePos.x - canvasPos.x - canvasSize.x * 0.5f - m_PanX) * zoomRatio;
            m_PanY = mousePos.y - canvasPos.y - canvasSize.y * 0.5f -
                     (mousePos.y - canvasPos.y - canvasSize.y * 0.5f - m_PanY) * zoomRatio;
        }

        // Middle mouse drag to pan
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_PanX += delta.x;
            m_PanY += delta.y;
        }

        // Tool usage with left mouse
        if (m_CurrentTool == PixelTool::PolygonEdit) {
            HandlePolygonInput(pixelX, pixelY, originX, originY, pixelSize);
        } else if (m_CurrentTool == PixelTool::Eyedropper) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && InBounds(pixelX, pixelY)) {
                m_ForegroundColor = GetPixel(pixelX, pixelY);
            }
        } else if (m_CurrentTool == PixelTool::Line || m_CurrentTool == PixelTool::Rect || m_CurrentTool == PixelTool::Ellipse) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_DragStartX = pixelX;
                m_DragStartY = pixelY;
                m_IsDrawing = true;
                PushUndoState();
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && m_IsDrawing) {
                if (m_CurrentTool == PixelTool::Line) {
                    DrawLine(m_DragStartX, m_DragStartY, pixelX, pixelY, m_ForegroundColor);
                } else if (m_CurrentTool == PixelTool::Rect) {
                    DrawRect(m_DragStartX, m_DragStartY, pixelX, pixelY, m_ForegroundColor, false);
                } else if (m_CurrentTool == PixelTool::Ellipse) {
                    i32 cx = (m_DragStartX + pixelX) / 2;
                    i32 cy = (m_DragStartY + pixelY) / 2;
                    i32 rx = std::abs(pixelX - m_DragStartX) / 2;
                    i32 ry = std::abs(pixelY - m_DragStartY) / 2;
                    DrawEllipse(cx, cy, rx, ry, m_ForegroundColor, false);
                }
                m_IsDrawing = false;
            }
        } else if (m_CurrentTool == PixelTool::Fill) {
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && InBounds(pixelX, pixelY)) {
                PushUndoState();
                FloodFill(pixelX, pixelY, m_ForegroundColor);
            }
        } else {
            // Pencil/Eraser: continuous drawing
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                PushUndoState();
                m_IsDrawing = true;
                m_LastDrawX = pixelX;
                m_LastDrawY = pixelY;
                ApplyTool(pixelX, pixelY);
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_IsDrawing) {
                // Interpolate between last and current position for smooth strokes
                if (pixelX != m_LastDrawX || pixelY != m_LastDrawY) {
                    if (m_CurrentTool == PixelTool::Pencil) {
                        DrawLine(m_LastDrawX, m_LastDrawY, pixelX, pixelY, m_ForegroundColor);
                    } else {
                        DrawLine(m_LastDrawX, m_LastDrawY, pixelX, pixelY, 0x00000000);
                    }
                    m_LastDrawX = pixelX;
                    m_LastDrawY = pixelY;
                }
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                m_IsDrawing = false;
            }
        }

        // Status bar
        if (InBounds(pixelX, pixelY)) {
            ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + 4, canvasPos.y + canvasSize.y - 18));
            ImGui::Text("(%d, %d)", pixelX, pixelY);
        }
    }

    // Keyboard undo/redo
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (ImGui::GetIO().KeyShift) Redo();
        else Undo();
    }
}

// ============================================================================
// PALETTE PANEL
// ============================================================================

void PixelEditor::DrawPalettePanel() {
    ImGui::Text("Palette");
    ImGui::Separator();

    // Foreground/background color preview
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    u8 fr = m_ForegroundColor & 0xFF;
    u8 fg = (m_ForegroundColor >> 8) & 0xFF;
    u8 fb = (m_ForegroundColor >> 16) & 0xFF;
    u8 fa = (m_ForegroundColor >> 24) & 0xFF;

    dl->AddRectFilled(cursor, ImVec2(cursor.x + 32, cursor.y + 32), IM_COL32(fr, fg, fb, fa));
    dl->AddRect(cursor, ImVec2(cursor.x + 32, cursor.y + 32), IM_COL32(255, 255, 255, 200));
    ImGui::Dummy(ImVec2(32, 36));

    // Color picker
    float col[4] = {fr / 255.0f, fg / 255.0f, fb / 255.0f, fa / 255.0f};
    if (ImGui::ColorEdit4("##fg_color", col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar)) {
        m_ForegroundColor = IM_COL32(
            static_cast<u8>(col[0] * 255),
            static_cast<u8>(col[1] * 255),
            static_cast<u8>(col[2] * 255),
            static_cast<u8>(col[3] * 255));
    }

    ImGui::Separator();

    // Palette grid
    f32 swatchSize = 20.0f;
    f32 availW = ImGui::GetContentRegionAvail().x;
    u32 cols = std::max(1u, static_cast<u32>(availW / (swatchSize + 2)));

    for (usize i = 0; i < m_Palette.colors.size(); i++) {
        if (i > 0 && (i % cols) != 0) ImGui::SameLine(0, 2);

        u32 c = m_Palette.colors[i];
        ImVec2 pos = ImGui::GetCursorScreenPos();
        dl = ImGui::GetWindowDrawList();

        dl->AddRectFilled(pos, ImVec2(pos.x + swatchSize, pos.y + swatchSize),
                          c);

        bool selected = (m_Palette.selectedIndex == static_cast<i32>(i));
        if (selected) {
            dl->AddRect(pos, ImVec2(pos.x + swatchSize, pos.y + swatchSize),
                        IM_COL32(255, 255, 0, 255), 0, 0, 2.0f);
        }

        char id[32];
        snprintf(id, sizeof(id), "##pal%zu", i);
        if (ImGui::InvisibleButton(id, ImVec2(swatchSize, swatchSize))) {
            m_Palette.selectedIndex = static_cast<i32>(i);
            m_ForegroundColor = c;
        }
    }
}

// ============================================================================
// LAYER PANEL
// ============================================================================

void PixelEditor::DrawLayerPanel() {
    ImGui::Text("Layers");
    ImGui::Separator();

    if (ImGui::SmallButton("+")) {
        PixelLayer layer;
        layer.name = "Layer " + std::to_string(m_Layers.size());
        layer.pixels.resize(m_Width * m_Height * 4, 0);
        m_Layers.push_back(std::move(layer));
        m_ActiveLayer = static_cast<i32>(m_Layers.size()) - 1;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-") && m_Layers.size() > 1) {
        m_Layers.erase(m_Layers.begin() + m_ActiveLayer);
        if (m_ActiveLayer >= static_cast<i32>(m_Layers.size())) {
            m_ActiveLayer = static_cast<i32>(m_Layers.size()) - 1;
        }
    }

    // List layers (top = last drawn = front)
    for (i32 i = static_cast<i32>(m_Layers.size()) - 1; i >= 0; i--) {
        auto& layer = m_Layers[i];
        ImGui::PushID(i);

        bool isActive = (i == m_ActiveLayer);
        if (isActive) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.32f, 0.52f, 0.35f, 1.0f));
        }

        if (ImGui::Button(layer.name.c_str(), ImVec2(-30, 0))) {
            m_ActiveLayer = i;
        }

        if (isActive) {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();
        ImGui::Checkbox("##vis", &layer.visible);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Visibility");
        }

        ImGui::PopID();
    }
}

// ============================================================================
// TOOL OPERATIONS
// ============================================================================

void PixelEditor::ApplyTool(i32 x, i32 y) {
    if (!InBounds(x, y)) return;

    switch (m_CurrentTool) {
        case PixelTool::Pencil:
            DrawPixel(x, y, m_ForegroundColor);
            break;
        case PixelTool::Eraser:
            ErasePixel(x, y);
            break;
        default:
            break;
    }
}

void PixelEditor::DrawPixel(i32 x, i32 y, u32 color) {
    if (!InBounds(x, y) || m_ActiveLayer < 0 || m_ActiveLayer >= static_cast<i32>(m_Layers.size())) return;

    auto& layer = m_Layers[m_ActiveLayer];
    u32 idx = (y * m_Width + x) * 4;
    layer.pixels[idx + 0] = color & 0xFF;
    layer.pixels[idx + 1] = (color >> 8) & 0xFF;
    layer.pixels[idx + 2] = (color >> 16) & 0xFF;
    layer.pixels[idx + 3] = (color >> 24) & 0xFF;
}

void PixelEditor::ErasePixel(i32 x, i32 y) {
    DrawPixel(x, y, 0x00000000);
}

void PixelEditor::FloodFill(i32 startX, i32 startY, u32 newColor) {
    if (!InBounds(startX, startY)) return;
    if (m_ActiveLayer < 0 || m_ActiveLayer >= static_cast<i32>(m_Layers.size())) return;

    u32 oldColor = GetPixel(startX, startY);
    if (oldColor == newColor) return;

    auto& layer = m_Layers[m_ActiveLayer];

    std::queue<std::pair<i32, i32>> q;
    q.push({startX, startY});

    // Scanline flood fill
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();

        if (!InBounds(x, y)) continue;

        u32 idx = (y * m_Width + x) * 4;
        u32 current = layer.pixels[idx] | (layer.pixels[idx + 1] << 8) |
                      (layer.pixels[idx + 2] << 16) | (layer.pixels[idx + 3] << 24);
        if (current != oldColor) continue;

        DrawPixel(x, y, newColor);

        q.push({x + 1, y});
        q.push({x - 1, y});
        q.push({x, y + 1});
        q.push({x, y - 1});
    }
}

void PixelEditor::DrawLine(i32 x0, i32 y0, i32 x1, i32 y1, u32 color) {
    // Bresenham's line algorithm
    i32 dx = std::abs(x1 - x0);
    i32 dy = -std::abs(y1 - y0);
    i32 sx = x0 < x1 ? 1 : -1;
    i32 sy = y0 < y1 ? 1 : -1;
    i32 err = dx + dy;

    while (true) {
        DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        i32 e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void PixelEditor::DrawRect(i32 x0, i32 y0, i32 x1, i32 y1, u32 color, bool filled) {
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);

    if (filled) {
        for (i32 y = y0; y <= y1; y++) {
            for (i32 x = x0; x <= x1; x++) {
                DrawPixel(x, y, color);
            }
        }
    } else {
        for (i32 x = x0; x <= x1; x++) {
            DrawPixel(x, y0, color);
            DrawPixel(x, y1, color);
        }
        for (i32 y = y0; y <= y1; y++) {
            DrawPixel(x0, y, color);
            DrawPixel(x1, y, color);
        }
    }
}

void PixelEditor::DrawEllipse(i32 cx, i32 cy, i32 rx, i32 ry, u32 color, bool filled) {
    if (rx <= 0 || ry <= 0) return;

    // Midpoint ellipse algorithm
    i32 x = 0, y = ry;
    i64 rxSq = static_cast<i64>(rx) * rx;
    i64 rySq = static_cast<i64>(ry) * ry;
    i64 p1 = rySq - rxSq * ry + rxSq / 4;

    // Region 1
    while (2 * rySq * x <= 2 * rxSq * y) {
        if (filled) {
            for (i32 xi = cx - x; xi <= cx + x; xi++) {
                DrawPixel(xi, cy + y, color);
                DrawPixel(xi, cy - y, color);
            }
        } else {
            DrawPixel(cx + x, cy + y, color);
            DrawPixel(cx - x, cy + y, color);
            DrawPixel(cx + x, cy - y, color);
            DrawPixel(cx - x, cy - y, color);
        }

        x++;
        if (p1 < 0) {
            p1 += 2 * rySq * x + rySq;
        } else {
            y--;
            p1 += 2 * rySq * x - 2 * rxSq * y + rySq;
        }
    }

    // Region 2
    i64 p2 = rySq * (x * 2 + 1) * (x * 2 + 1) / 4 + rxSq * (static_cast<i64>(y) - 1) * (y - 1) - rxSq * rySq;
    while (y >= 0) {
        if (filled) {
            for (i32 xi = cx - x; xi <= cx + x; xi++) {
                DrawPixel(xi, cy + y, color);
                DrawPixel(xi, cy - y, color);
            }
        } else {
            DrawPixel(cx + x, cy + y, color);
            DrawPixel(cx - x, cy + y, color);
            DrawPixel(cx + x, cy - y, color);
            DrawPixel(cx - x, cy - y, color);
        }

        y--;
        if (p2 > 0) {
            p2 += -2 * rxSq * y + rxSq;
        } else {
            x++;
            p2 += 2 * rySq * x - 2 * rxSq * y + rxSq;
        }
    }
}

// ============================================================================
// LAYER COMPOSITING
// ============================================================================

void PixelEditor::CompositeImage(std::vector<u8>& output) const {
    output.resize(m_Width * m_Height * 4, 0);

    for (const auto& layer : m_Layers) {
        if (!layer.visible) continue;

        for (u32 i = 0; i < m_Width * m_Height; i++) {
            u32 idx = i * 4;
            u8 sr = layer.pixels[idx];
            u8 sg = layer.pixels[idx + 1];
            u8 sb = layer.pixels[idx + 2];
            u8 sa = static_cast<u8>(layer.pixels[idx + 3] * layer.opacity);

            if (sa == 0) continue;

            u8 dr = output[idx];
            u8 dg = output[idx + 1];
            u8 db = output[idx + 2];
            u8 da = output[idx + 3];

            // Alpha blending: src over dst
            f32 srcA = sa / 255.0f;
            f32 dstA = da / 255.0f;
            f32 outA = srcA + dstA * (1.0f - srcA);

            if (outA > 0.0f) {
                output[idx]     = static_cast<u8>((sr * srcA + dr * dstA * (1.0f - srcA)) / outA);
                output[idx + 1] = static_cast<u8>((sg * srcA + dg * dstA * (1.0f - srcA)) / outA);
                output[idx + 2] = static_cast<u8>((sb * srcA + db * dstA * (1.0f - srcA)) / outA);
                output[idx + 3] = static_cast<u8>(outA * 255.0f);
            }
        }
    }
}

// ============================================================================
// UNDO/REDO
// ============================================================================

void PixelEditor::PushUndoState() {
    if (m_ActiveLayer < 0 || m_ActiveLayer >= static_cast<i32>(m_Layers.size())) return;

    UndoSnapshot snap;
    snap.layerData = m_Layers[m_ActiveLayer].pixels;
    snap.layerIndex = m_ActiveLayer;

    m_UndoStack.push_back(std::move(snap));
    if (m_UndoStack.size() > MAX_UNDO) {
        m_UndoStack.erase(m_UndoStack.begin());
    }

    m_RedoStack.clear();
}

void PixelEditor::Undo() {
    if (m_UndoStack.empty()) return;

    auto& snap = m_UndoStack.back();
    if (snap.layerIndex >= 0 && snap.layerIndex < static_cast<i32>(m_Layers.size())) {
        // Save current state to redo stack
        UndoSnapshot redo;
        redo.layerData = m_Layers[snap.layerIndex].pixels;
        redo.layerIndex = snap.layerIndex;
        m_RedoStack.push_back(std::move(redo));

        // Restore from undo
        m_Layers[snap.layerIndex].pixels = snap.layerData;
    }

    m_UndoStack.pop_back();
}

void PixelEditor::Redo() {
    if (m_RedoStack.empty()) return;

    auto& snap = m_RedoStack.back();
    if (snap.layerIndex >= 0 && snap.layerIndex < static_cast<i32>(m_Layers.size())) {
        // Save current state to undo stack
        UndoSnapshot undo;
        undo.layerData = m_Layers[snap.layerIndex].pixels;
        undo.layerIndex = snap.layerIndex;
        m_UndoStack.push_back(std::move(undo));

        // Restore from redo
        m_Layers[snap.layerIndex].pixels = snap.layerData;
    }

    m_RedoStack.pop_back();
}

// ============================================================================
// HELPERS
// ============================================================================

u32 PixelEditor::GetPixel(i32 x, i32 y) const {
    if (!InBounds(x, y) || m_ActiveLayer < 0 || m_ActiveLayer >= static_cast<i32>(m_Layers.size())) return 0;

    const auto& layer = m_Layers[m_ActiveLayer];
    u32 idx = (y * m_Width + x) * 4;
    return layer.pixels[idx] | (layer.pixels[idx + 1] << 8) |
           (layer.pixels[idx + 2] << 16) | (layer.pixels[idx + 3] << 24);
}

// ============================================================================
// ANIMATION TIMELINE & ONION SKINNING
// ============================================================================

void PixelEditor::DrawAnimationTimeline() {
    ImGui::Separator();
    ImGui::Text("Animation");
    ImGui::SameLine();

    // Add frame
    if (ImGui::SmallButton("+Frame")) {
        SaveCurrentFrameToAnim();
        AnimationFrame newFrame;
        newFrame.layers = m_Layers; // Copy current layers
        // Clear all pixels in the new frame
        for (auto& layer : newFrame.layers) {
            std::fill(layer.pixels.begin(), layer.pixels.end(), 0);
        }
        m_AnimFrames.insert(m_AnimFrames.begin() + m_CurrentAnimFrame + 1, std::move(newFrame));
        m_CurrentAnimFrame++;
        SwitchToFrame(m_CurrentAnimFrame);
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Dup")) {
        SaveCurrentFrameToAnim();
        AnimationFrame dup;
        dup.layers = m_Layers;
        m_AnimFrames.insert(m_AnimFrames.begin() + m_CurrentAnimFrame + 1, std::move(dup));
        m_CurrentAnimFrame++;
        SwitchToFrame(m_CurrentAnimFrame);
    }

    ImGui::SameLine();
    if (m_AnimFrames.size() > 1 && ImGui::SmallButton("-Frame")) {
        m_AnimFrames.erase(m_AnimFrames.begin() + m_CurrentAnimFrame);
        if (m_CurrentAnimFrame >= static_cast<i32>(m_AnimFrames.size())) {
            m_CurrentAnimFrame = static_cast<i32>(m_AnimFrames.size()) - 1;
        }
        SwitchToFrame(m_CurrentAnimFrame);
    }

    // Frame thumbnails
    ImGui::Text("Frame %d / %zu", m_CurrentAnimFrame + 1, m_AnimFrames.size());

    f32 thumbW = 32.0f;
    for (usize i = 0; i < m_AnimFrames.size(); i++) {
        if (i > 0) ImGui::SameLine();
        bool isCurrent = (static_cast<i32>(i) == m_CurrentAnimFrame);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImU32 bgColor = isCurrent ? IM_COL32(80, 120, 200, 255) : IM_COL32(60, 60, 60, 255);
        dl->AddRectFilled(pos, ImVec2(pos.x + thumbW, pos.y + thumbW), bgColor);
        dl->AddRect(pos, ImVec2(pos.x + thumbW, pos.y + thumbW), IM_COL32(180, 180, 180, 255));

        char id[32];
        snprintf(id, sizeof(id), "##aframe%zu", i);
        if (ImGui::InvisibleButton(id, ImVec2(thumbW, thumbW))) {
            SaveCurrentFrameToAnim();
            m_CurrentAnimFrame = static_cast<i32>(i);
            SwitchToFrame(m_CurrentAnimFrame);
        }
    }

    // Onion skin settings
    ImGui::Checkbox("Onion Skin", &m_OnionSkin.enabled);
    if (m_OnionSkin.enabled) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::DragInt("Before", &m_OnionSkin.framesBefore, 0.1f, 0, 5);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::DragInt("After", &m_OnionSkin.framesAfter, 0.1f, 0, 5);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat("Opa", &m_OnionSkin.opacity, 0.01f, 0.05f, 0.8f);
    }

    // Preview controls
    ImGui::SameLine();
    if (ImGui::SmallButton(m_PreviewPlaying ? "Stop" : "Play")) {
        m_PreviewPlaying = !m_PreviewPlaying;
        m_PreviewTimer = 0.0f;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    ImGui::DragFloat("FPS", &m_PreviewFps, 0.5f, 1.0f, 60.0f);

    // Advance preview
    if (m_PreviewPlaying && !m_AnimFrames.empty()) {
        m_PreviewTimer += ImGui::GetIO().DeltaTime;
        f32 frameDuration = (m_PreviewFps > 0.0f) ? (1.0f / m_PreviewFps) : 0.1f;
        if (m_PreviewTimer >= frameDuration) {
            m_PreviewTimer -= frameDuration;
            SaveCurrentFrameToAnim();
            m_CurrentAnimFrame = (m_CurrentAnimFrame + 1) % static_cast<i32>(m_AnimFrames.size());
            SwitchToFrame(m_CurrentAnimFrame);
        }
    }
}

void PixelEditor::SwitchToFrame(i32 frameIndex) {
    if (frameIndex < 0 || frameIndex >= static_cast<i32>(m_AnimFrames.size())) return;
    m_Layers = m_AnimFrames[frameIndex].layers;
    if (m_ActiveLayer >= static_cast<i32>(m_Layers.size())) {
        m_ActiveLayer = 0;
    }
}

void PixelEditor::SaveCurrentFrameToAnim() {
    if (m_AnimFrames.empty()) {
        // Initialize animation with current canvas as first frame
        AnimationFrame frame;
        frame.layers = m_Layers;
        m_AnimFrames.push_back(std::move(frame));
        m_CurrentAnimFrame = 0;
    } else if (m_CurrentAnimFrame >= 0 && m_CurrentAnimFrame < static_cast<i32>(m_AnimFrames.size())) {
        m_AnimFrames[m_CurrentAnimFrame].layers = m_Layers;
    }
}

bool PixelEditor::ExportAsPrefab(const std::string& outputPath) {
    // Export all frames as a sprite sheet PNG, then create prefab
    if (!HasCanvas()) return false;

    // Ensure we have at least one animation frame
    if (m_AnimFrames.empty()) {
        SaveCurrentFrameToAnim();
    }

    u32 frameCount = static_cast<u32>(m_AnimFrames.size());

    // Layout: horizontal strip
    u32 sheetW = m_Width * frameCount;
    u32 sheetH = m_Height;
    std::vector<u8> sheetPixels(sheetW * sheetH * 4, 0);

    for (u32 f = 0; f < frameCount; f++) {
        auto savedLayers = m_Layers;
        m_Layers = m_AnimFrames[f].layers;
        std::vector<u8> framePixels;
        CompositeImage(framePixels);
        m_Layers = savedLayers;

        for (u32 y = 0; y < m_Height; y++) {
            u32 srcOffset = y * m_Width * 4;
            u32 dstOffset = (y * sheetW + f * m_Width) * 4;
            std::memcpy(&sheetPixels[dstOffset], &framePixels[srcOffset], m_Width * 4);
        }
    }

    // Save sprite sheet PNG
    std::string sheetPath = outputPath + ".png";
    int pngResult = stbi_write_png(sheetPath.c_str(), static_cast<int>(sheetW), static_cast<int>(sheetH),
                                   4, sheetPixels.data(), static_cast<int>(sheetW * 4));
    if (!pngResult) {
        ENJIN_LOG_ERROR(Editor, "PixelEditor: Failed to export sprite sheet to '%s'", sheetPath.c_str());
        return false;
    }

    // Build prefab with Sprite2D + AnimatedSprite2D components
    Assets::Prefab prefab("PixelArt_Sprite");

    Assets::PrefabEntityData entityData;
    entityData.name = "PixelArt_Sprite";
    entityData.parentIndex = -1;

    // Transform component
    {
        Assets::PrefabComponentData comp;
        comp.typeName = "Transform";
        comp.vec3Properties["position"] = Math::Vector3(0, 0, 0);
        comp.vec3Properties["rotation"] = Math::Vector3(0, 0, 0);
        comp.vec3Properties["scale"] = Math::Vector3(1, 1, 1);
        entityData.components.push_back(comp);
    }

    // Sprite2D component
    {
        Assets::PrefabComponentData comp;
        comp.typeName = "Sprite2D";
        comp.stringProperties["texturePath"] = sheetPath;
        comp.floatProperties["srcX"] = 0;
        comp.floatProperties["srcY"] = 0;
        comp.floatProperties["srcWidth"] = static_cast<f32>(m_Width);
        comp.floatProperties["srcHeight"] = static_cast<f32>(m_Height);
        comp.floatProperties["sizeX"] = static_cast<f32>(m_Width) / 32.0f;  // Scale to world units
        comp.floatProperties["sizeY"] = static_cast<f32>(m_Height) / 32.0f;
        comp.floatProperties["pivotX"] = 0.5f;
        comp.floatProperties["pivotY"] = 0.5f;
        entityData.components.push_back(comp);
    }

    // AnimatedSprite2D component (if multiple frames)
    if (frameCount > 1) {
        Assets::PrefabComponentData comp;
        comp.typeName = "AnimatedSprite2D";
        comp.intProperties["frameCount"] = static_cast<i32>(frameCount);
        comp.floatProperties["frameWidth"] = static_cast<f32>(m_Width);
        comp.floatProperties["frameHeight"] = static_cast<f32>(m_Height);
        comp.floatProperties["fps"] = m_PreviewFps;
        comp.boolProperties["loop"] = true;
        comp.boolProperties["playing"] = true;

        // Store frame positions
        for (u32 f = 0; f < frameCount; f++) {
            std::string prefix = "frame" + std::to_string(f) + "_";
            comp.floatProperties[prefix + "srcX"] = static_cast<f32>(f * m_Width);
            comp.floatProperties[prefix + "srcY"] = 0.0f;
            comp.floatProperties[prefix + "duration"] = (m_PreviewFps > 0.0f) ? (1.0f / m_PreviewFps) : 0.1f;
        }
        entityData.components.push_back(comp);
    }

    // Box collider based on canvas dimensions
    {
        Assets::PrefabComponentData comp;
        comp.typeName = "BoxCollider";
        comp.vec3Properties["size"] = Math::Vector3(
            static_cast<f32>(m_Width) / 32.0f,
            static_cast<f32>(m_Height) / 32.0f,
            0.1f);
        comp.vec3Properties["offset"] = Math::Vector3(0, 0, 0);
        entityData.components.push_back(comp);
    }

    prefab.AddEntity(entityData);

    // Save prefab file
    std::string prefabPath = outputPath + ".enjprefab";
    bool saved = Assets::PrefabManager::Get().SavePrefab(prefab, prefabPath);

    if (saved) {
        ENJIN_LOG_INFO(Editor, "PixelEditor: Exported %u-frame prefab to '%s'", frameCount, prefabPath.c_str());
    } else {
        ENJIN_LOG_ERROR(Editor, "PixelEditor: Failed to save prefab to '%s'", prefabPath.c_str());
    }

    ENJIN_LOG_INFO(Editor, "PixelEditor: Exported sprite sheet to '%s'", sheetPath.c_str());
    return saved;
}

// ============================================================================
// POLYGON COLLIDER EDITOR
// ============================================================================

void PixelEditor::DrawPolygonOverlay(f32 originX, f32 originY, f32 pixelSize, ImDrawList* dl) {
    const auto& verts = m_PolygonEdit.vertices;
    if (verts.empty()) return;

    // Convert pixel coords to screen coords
    auto toScreen = [&](const Math::Vector2& p) -> ImVec2 {
        return ImVec2(originX + p.x * pixelSize, originY + p.y * pixelSize);
    };

    // Draw edges
    ImU32 lineColor = IM_COL32(0, 200, 255, 200);
    ImU32 fillColor = IM_COL32(0, 200, 255, 40);
    usize count = verts.size();

    if (m_PolygonEdit.closed && count >= 3) {
        // Draw filled polygon (convex approximation for ImGui)
        std::vector<ImVec2> screenPts;
        screenPts.reserve(count);
        for (const auto& v : verts) screenPts.push_back(toScreen(v));
        dl->AddConvexPolyFilled(screenPts.data(), (int)screenPts.size(), fillColor);
    }

    for (usize i = 0; i < count; i++) {
        usize next = (i + 1) % count;
        if (!m_PolygonEdit.closed && next == 0 && i == count - 1) break;
        dl->AddLine(toScreen(verts[i]), toScreen(verts[next]), lineColor, 2.0f);
    }

    // Draw vertices as circles
    for (usize i = 0; i < count; i++) {
        ImVec2 sp = toScreen(verts[i]);
        ImU32 dotColor = (static_cast<i32>(i) == m_PolygonEdit.hoveredVertex)
                             ? IM_COL32(255, 255, 0, 255)
                             : IM_COL32(0, 255, 128, 255);
        dl->AddCircleFilled(sp, 5.0f, dotColor);
        dl->AddCircle(sp, 5.0f, IM_COL32(255, 255, 255, 200));
    }
}

void PixelEditor::HandlePolygonInput(i32 pixelX, i32 pixelY, f32 originX, f32 originY, f32 pixelSize) {
    ImVec2 mousePos = ImGui::GetMousePos();
    auto& state = m_PolygonEdit;

    auto toScreen = [&](const Math::Vector2& p) -> ImVec2 {
        return ImVec2(originX + p.x * pixelSize, originY + p.y * pixelSize);
    };

    // Find hovered vertex
    state.hoveredVertex = -1;
    f32 closestDist = 10.0f; // Screen-space radius
    for (usize i = 0; i < state.vertices.size(); i++) {
        ImVec2 sp = toScreen(state.vertices[i]);
        f32 dx = mousePos.x - sp.x;
        f32 dy = mousePos.y - sp.y;
        f32 d = std::sqrt(dx * dx + dy * dy);
        if (d < closestDist) {
            closestDist = d;
            state.hoveredVertex = static_cast<i32>(i);
        }
    }

    // Drag vertex
    if (state.dragVertex >= 0) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            state.vertices[state.dragVertex] = Math::Vector2(
                static_cast<f32>(pixelX), static_cast<f32>(pixelY));
        } else {
            state.dragVertex = -1;
        }
        return;
    }

    // Right-click to delete vertex
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && state.hoveredVertex >= 0) {
        state.vertices.erase(state.vertices.begin() + state.hoveredVertex);
        state.hoveredVertex = -1;
        if (state.vertices.size() < 3) state.closed = false;
        return;
    }

    // Left-click
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (state.hoveredVertex >= 0) {
            // Click on first vertex to close polygon
            if (state.hoveredVertex == 0 && !state.closed && state.vertices.size() >= 3) {
                state.closed = true;
            } else {
                // Start dragging
                state.dragVertex = state.hoveredVertex;
            }
        } else if (!state.closed) {
            // Add new vertex
            state.vertices.push_back(Math::Vector2(
                static_cast<f32>(pixelX), static_cast<f32>(pixelY)));
        }
    }
}

} // namespace Editor
} // namespace Enjin
