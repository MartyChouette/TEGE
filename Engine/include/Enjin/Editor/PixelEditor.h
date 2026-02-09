#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Editor/EditorSettings.h"
#include <string>
#include <vector>
#include <functional>

struct ImDrawList;

namespace Enjin {
namespace Editor {

// Drawing tools
enum class PixelTool : u8 {
    Pencil,
    Eraser,
    Fill,
    Line,
    Rect,
    Ellipse,
    Eyedropper,
    Select,
    PolygonEdit
};

// A single layer in the pixel editor
struct PixelLayer {
    std::string name;
    std::vector<u8> pixels; // RGBA, size = width * height * 4
    bool visible = true;
    f32 opacity = 1.0f;
};

// Named palette
struct PixelPalette {
    std::string name;
    std::vector<u32> colors; // RGBA packed
    i32 selectedIndex = 0;
};

// Retro resolution presets
struct RetroPreset {
    const char* name;
    u32 width, height;
};

// Main pixel editor class
class ENJIN_API PixelEditor {
public:
    PixelEditor() = default;
    ~PixelEditor() = default;

    // Canvas management
    void NewCanvas(u32 width, u32 height);
    bool LoadImage(const std::string& path);
    bool ExportPNG(const std::string& path);

    // Main render (called every frame when panel is visible)
    void Render(const EditorSettings& settings);

    // State queries
    bool HasCanvas() const { return m_Width > 0 && m_Height > 0 && !m_Layers.empty(); }
    u32 GetCanvasWidth() const { return m_Width; }
    u32 GetCanvasHeight() const { return m_Height; }

private:
    // Rendering
    void DrawToolbar();
    void DrawCanvasArea();
    void DrawLayerPanel();
    void DrawPalettePanel();

    // Tool operations
    void ApplyTool(i32 x, i32 y);
    void DrawPixel(i32 x, i32 y, u32 color);
    void ErasePixel(i32 x, i32 y);
    void FloodFill(i32 x, i32 y, u32 newColor);
    void DrawLine(i32 x0, i32 y0, i32 x1, i32 y1, u32 color);
    void DrawRect(i32 x0, i32 y0, i32 x1, i32 y1, u32 color, bool filled);
    void DrawEllipse(i32 cx, i32 cy, i32 rx, i32 ry, u32 color, bool filled);

    // Layer compositing
    void CompositeImage(std::vector<u8>& output) const;

    // Undo
    void PushUndoState();
    void Undo();
    void Redo();

    // Helpers
    u32 GetPixel(i32 x, i32 y) const;
    bool InBounds(i32 x, i32 y) const { return x >= 0 && y >= 0 && x < (i32)m_Width && y < (i32)m_Height; }

    // Canvas state
    u32 m_Width = 0;
    u32 m_Height = 0;
    std::vector<PixelLayer> m_Layers;
    i32 m_ActiveLayer = 0;

    // Tool state
    PixelTool m_CurrentTool = PixelTool::Pencil;
    u32 m_ForegroundColor = 0xFF000000; // Black (RGBA = ABGR in packed)
    u32 m_BackgroundColor = 0xFFFFFFFF; // White
    bool m_IsDrawing = false;
    i32 m_DragStartX = -1, m_DragStartY = -1;
    i32 m_LastDrawX = -1, m_LastDrawY = -1;

    // View state
    f32 m_Zoom = 8.0f;
    f32 m_PanX = 0.0f, m_PanY = 0.0f;
    bool m_ShowGrid = true;

    // Undo state
    struct UndoSnapshot {
        std::vector<u8> layerData;
        i32 layerIndex;
    };
    std::vector<UndoSnapshot> m_UndoStack;
    std::vector<UndoSnapshot> m_RedoStack;
    static constexpr usize MAX_UNDO = 50;

    // Palette
    PixelPalette m_Palette;

    // ========== Polygon Collider Editor ==========

    struct PolygonEditState {
        std::vector<Math::Vector2> vertices; // pixel coords
        i32 hoveredVertex = -1;
        i32 dragVertex = -1;
        bool closed = false;
    };
    PolygonEditState m_PolygonEdit;

    void DrawPolygonOverlay(f32 originX, f32 originY, f32 pixelSize, ImDrawList* dl);
    void HandlePolygonInput(i32 pixelX, i32 pixelY, f32 originX, f32 originY, f32 pixelSize);

    // ========== Animation / Onion Skinning ==========

    struct OnionSkinSettings {
        bool enabled = false;
        i32 framesBefore = 2;
        i32 framesAfter = 1;
        f32 opacity = 0.3f;
        u32 beforeTint = 0x50FF6464;  // Red tint (ABGR)
        u32 afterTint  = 0x5064FF64;  // Green tint (ABGR)
    };

    struct AnimationFrame {
        std::vector<PixelLayer> layers;
    };

    std::vector<AnimationFrame> m_AnimFrames;
    i32 m_CurrentAnimFrame = 0;
    OnionSkinSettings m_OnionSkin;

    // Animation preview state
    f32 m_PreviewFps = 12.0f;
    f32 m_PreviewTimer = 0.0f;
    bool m_PreviewPlaying = false;

    void DrawAnimationTimeline();
    void SwitchToFrame(i32 frameIndex);
    void SaveCurrentFrameToAnim();

public:
    // Preset list
    static const RetroPreset s_RetroPresets[];

    // Prefab export (for Phase 11)
    bool ExportAsPrefab(const std::string& outputPath);
private:
};

} // namespace Editor
} // namespace Enjin
