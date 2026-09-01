#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <string>
#include <vector>

namespace Enjin::GUI {

// Widget types — Phase 1 renders the first 8, rest show as placeholder panels
enum class UIWidgetType : u8 {
    Panel = 0,
    Button,
    Label,
    Image,
    ProgressBar,
    Slider,
    Checkbox,
    Toggle,
    // Phase 2+ (defined in data model, rendered as placeholder panels)
    Dropdown,
    TextInput,
    RadioGroup,
    ScrollArea,
    Grid,
    TabGroup,
    Tooltip,
    Modal,
    ListView,
    // Unified display P2: tessellated SVG drawn straight into the draw list
    // (same source + tessellation as the world DisplayGraphicComponent).
    // Reuses data.imagePath (the .svg) + imageTint + imageAlpha.
    VectorGraphic,
    Count
};

// Unity RectTransform-style anchor system
struct UIAnchor {
    // Relative to parent (0-1)
    Math::Vector2 anchorMin = Math::Vector2(0.5f, 0.5f);
    Math::Vector2 anchorMax = Math::Vector2(0.5f, 0.5f);
    // Transform origin (0-1)
    Math::Vector2 pivot = Math::Vector2(0.5f, 0.5f);
    // Pixel insets from anchored edges
    f32 offsetLeft   = 0.0f;
    f32 offsetRight  = 0.0f;
    f32 offsetTop    = 0.0f;
    f32 offsetBottom = 0.0f;
};

// Nine-slice (9-patch) configuration for scalable sprite backgrounds
struct NineSliceConfig {
    std::string texturePath;       // Path to texture file (empty = inactive)
    f32 borderLeft   = 0.0f;       // Left border inset in texels
    f32 borderRight  = 0.0f;       // Right border inset in texels
    f32 borderTop    = 0.0f;       // Top border inset in texels
    f32 borderBottom = 0.0f;       // Bottom border inset in texels

    bool IsActive() const {
        return !texturePath.empty() &&
               (borderLeft > 0 || borderRight > 0 || borderTop > 0 || borderBottom > 0);
    }
};

// Per-element style overrides (empty string / -1 = use theme default)
struct UIStyleOverride {
    Math::Vector3 bgColor      = Math::Vector3(-1, -1, -1); // -1 = use theme
    Math::Vector3 textColor    = Math::Vector3(-1, -1, -1);
    Math::Vector3 borderColor  = Math::Vector3(-1, -1, -1);
    f32 bgAlpha      = -1.0f;
    f32 borderRadius = -1.0f;
    f32 borderWidth  = -1.0f;
    f32 fontSize     = -1.0f;
    Math::Vector3 focusColor = Math::Vector3(-1, -1, -1); // -1 = use theme inputFocused

    bool HasBgColor()     const { return bgColor.x >= 0.0f; }
    bool HasTextColor()   const { return textColor.x >= 0.0f; }
    bool HasBorderColor() const { return borderColor.x >= 0.0f; }
    bool HasBgAlpha()     const { return bgAlpha >= 0.0f; }
    bool HasBorderRadius() const { return borderRadius >= 0.0f; }
    bool HasBorderWidth() const { return borderWidth >= 0.0f; }
    bool HasFontSize()    const { return fontSize >= 0.0f; }
    bool HasFocusColor()  const { return focusColor.x >= 0.0f; }

    NineSliceConfig nineSlice; // Empty = flat color fallback
};

// Widget-specific data (union-like via separate fields)
struct UIWidgetData {
    // Label / Button
    std::string text;
    u8 textAlignH = 1; // 0=left, 1=center, 2=right
    u8 textAlignV = 1; // 0=top, 1=center, 2=bottom

    // Per-character colors for Label/Button text (runtime only, not serialized).
    // Each entry is an ImU32 packed color (IM_COL32 format) for the character at
    // that index. When empty, the element's textColor applies uniformly.
    // Indices beyond the vector size fall back to the element textColor.
    std::vector<u32> charColors;

    // Image
    std::string imagePath;
    Math::Vector3 imageTint = Math::Vector3(1, 1, 1);
    f32 imageAlpha = 1.0f;

    // ProgressBar
    f32 progressValue = 0.5f;
    Math::Vector3 progressFillColor = Math::Vector3(-1, -1, -1); // -1 = use theme

    // Slider
    f32 sliderValue = 0.5f;
    f32 sliderMin   = 0.0f;
    f32 sliderMax   = 1.0f;

    // Checkbox / Toggle
    bool checked = false;

    // Dropdown
    std::vector<std::string> options;
    i32 selectedOption = 0;
    bool dropdownOpen = false;  // Runtime: is list expanded (not serialized)

    // TextInput
    std::string inputText;
    std::string placeholder;
    i32 cursorPos = 0;         // Runtime: cursor position in inputText (not serialized)
    i32 selectionStart = -1;   // Runtime: selection start (-1 = no selection, not serialized)

    // RadioGroup
    // Reuses `options` for labels and `selectedOption` for selected index

    // ScrollArea
    f32 scrollOffset = 0.0f;   // Runtime: current vertical scroll offset (not serialized)

    // Grid
    i32 gridColumns = 2;       // Number of columns in grid layout

    // TabGroup
    i32 activeTabIndex = 0;    // Currently active tab index

    // Tooltip
    f32 tooltipDelay = 0.5f;   // Seconds before tooltip appears on hover
    std::string tooltipText;   // Text displayed in the tooltip

    // ListView
    i32 listSelectedIndex = -1; // Currently selected list item (-1 = none)

    // Data binding: when set, UISystem::Update overwrites this element's display
    // value from live gameplay state each frame -- ONE authored HUD element works
    // identically on every platform. Supported fields:
    //   "health" -- player health percent (ProgressBar fills; Label appends "87%")
    //   "coins"  -- coin pickups (Label appends "collected / total")
    // Empty = static element.
    std::string bindField;
    f32 bindMaxValue = 0.0f;    // "coins": authored total (0 = auto from first-seen count)
    std::string boundText;      // Runtime: composed display text (not serialized)

    // SDF rendering (resolution-independent vector art)
    bool sdfMode = false;
    f32 sdfSoftness = 0.01f;          // Edge softness (smoothstep range)
    f32 sdfOutlineWidth = 0.0f;       // 0 = no outline
    Math::Vector3 sdfOutlineColor = Math::Vector3(0, 0, 0);

    // World-space anchoring (billboard tags glued to entities — the HUDSystem
    // capability folded into UICanvas). When worldSpace is set, the element's
    // anchor POINT each frame is the projected position of worldSourceEntity
    // (+ worldOffset) through the game camera; the authored offsets still size
    // the element around that point. Culled beyond maxRenderDistance or when
    // behind the camera.
    bool worldSpace = false;
    u64 worldSourceEntity = 0;   // Runtime: set by scripts; not serialized (entity ids don't persist)
    Math::Vector3 worldOffset = Math::Vector3(0, 0, 0);
    f32 maxRenderDistance = 50.0f;
};

// Computed rectangle (filled at layout time)
struct UIRect {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 w = 0.0f;
    f32 h = 0.0f;

    bool Contains(f32 px, f32 py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

// Interaction state
struct UIInteractionState {
    bool hovered = false;
    bool pressed = false;
    bool focused = false;
    bool active  = false; // For sliders being dragged
};

// Returns true for widget types that are focusable by default
inline bool IsFocusableType(UIWidgetType type) {
    return type == UIWidgetType::Button   || type == UIWidgetType::Slider ||
           type == UIWidgetType::Checkbox || type == UIWidgetType::Toggle ||
           type == UIWidgetType::Dropdown || type == UIWidgetType::TextInput ||
           type == UIWidgetType::RadioGroup || type == UIWidgetType::ListView;
}

// A single UI element in the canvas
struct UIElement {
    u32 id = 0;
    std::string name;
    UIWidgetType type = UIWidgetType::Panel;
    bool visible = true;
    bool enabled = true;
    bool focusable = true;  // Can receive keyboard/gamepad focus
    i32 tabOrder = 0;       // 0 = auto (element order), >0 = explicit

    // Layout
    UIAnchor anchor;

    // Style
    UIStyleOverride style;

    // Widget data
    UIWidgetData data;

    // Accessibility
    std::string accessibleLabel;  // Screen reader label (falls back to name if empty)

    // Event callback names (dispatched via UIEventBus)
    std::string onClickEvent;
    std::string onValueChangedEvent;
    std::string onSubmitEvent;

    // Hierarchy (0 = root-level element)
    u32 parentId = 0;
    std::vector<u32> childIds;

    // Runtime state (not serialized)
    UIRect computedRect;
    UIInteractionState interaction;
    bool worldCulled = false;  // World-space element behind camera / out of range this frame
};

} // namespace Enjin::GUI
