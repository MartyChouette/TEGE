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

    bool HasBgColor()     const { return bgColor.x >= 0.0f; }
    bool HasTextColor()   const { return textColor.x >= 0.0f; }
    bool HasBorderColor() const { return borderColor.x >= 0.0f; }
    bool HasBgAlpha()     const { return bgAlpha >= 0.0f; }
    bool HasBorderRadius() const { return borderRadius >= 0.0f; }
    bool HasBorderWidth() const { return borderWidth >= 0.0f; }
    bool HasFontSize()    const { return fontSize >= 0.0f; }

    NineSliceConfig nineSlice; // Empty = flat color fallback
};

// Widget-specific data (union-like via separate fields)
struct UIWidgetData {
    // Label / Button
    std::string text;
    u8 textAlignH = 1; // 0=left, 1=center, 2=right
    u8 textAlignV = 1; // 0=top, 1=center, 2=bottom

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

    // Dropdown (Phase 2+)
    std::vector<std::string> options;
    i32 selectedOption = 0;

    // TextInput (Phase 2+)
    std::string inputText;
    std::string placeholder;
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

// A single UI element in the canvas
struct UIElement {
    u32 id = 0;
    std::string name;
    UIWidgetType type = UIWidgetType::Panel;
    bool visible = true;
    bool enabled = true;

    // Layout
    UIAnchor anchor;

    // Style
    UIStyleOverride style;

    // Widget data
    UIWidgetData data;

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
};

} // namespace Enjin::GUI
