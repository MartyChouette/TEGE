#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/GUI/UIElement.h"
#include "Enjin/GUI/UITheme.h"

#include <string>
#include <vector>

namespace Enjin::GUI {

enum class UIScaleMode : u8 {
    ScaleWithScreenSize = 0,  // Scale based on design resolution
    ConstantPixelSize,        // 1:1 pixel mapping
    ConstantPhysicalSize      // DPI-aware
};

// ECS component — attach to an entity to give it a UI canvas
struct UICanvasComponent {
    std::string canvasName = "Canvas";
    bool visible = true;
    i32 sortOrder = 0;  // Higher = rendered on top

    // Design resolution (layout reference)
    f32 designWidth  = 1920.0f;
    f32 designHeight = 1080.0f;
    UIScaleMode scaleMode = UIScaleMode::ScaleWithScreenSize;

    // Theme
    UITheme theme;

    // Element storage (flat vector, tree via parentId/childIds)
    std::vector<UIElement> elements;
    u32 nextElementId = 1;

    // Runtime focus state (not serialized)
    u32 focusedElementId = 0;  // 0 = no focus

    // Add a new element, returns its ID
    u32 AddElement(UIWidgetType type, const std::string& name, u32 parentId = 0);

    // Remove an element and all its children
    void RemoveElement(u32 id);

    // Get element by ID (nullptr if not found)
    UIElement* GetElement(u32 id);
    const UIElement* GetElement(u32 id) const;

    // Get root elements (parentId == 0)
    std::vector<u32> GetRootElementIds() const;

    // Get children of an element
    std::vector<u32> GetChildIds(u32 parentId) const;

    // Duplicate an element (deep copy), returns new ID
    u32 DuplicateElement(u32 id);

    // Reorder element in the elements vector (render order)
    void MoveElementUp(u32 id);    // Swap with previous sibling
    void MoveElementDown(u32 id);  // Swap with next sibling

    // --- Design-space rect helpers (anchor-aware authoring) ---
    // Resolved rect of an element at design resolution (scale 1), through the
    // parent chain. This is THE conversion the editor must use: reading
    // offsetLeft as "x" is only valid for top-left anchors.
    UIRect GetDesignRect(u32 id) const;
    // Write a design-space rect into an element's offsets PRESERVING its
    // anchors. (The old editor path rewrote anchors to top-left on every
    // simple X/Y/W/H edit, which is why authored UI never tracked screen
    // edges or center at other resolutions.)
    void SetDesignRect(u32 id, f32 x, f32 y, f32 w, f32 h);
    // Re-anchor WITHOUT moving the element: per axis 0=min edge, 1=center,
    // 2=max edge, 3=stretch. The element keeps its on-screen rect at design
    // resolution; what changes is which part of the screen it follows when
    // the resolution differs.
    void ApplyAnchorPreset(u32 id, i32 presetX, i32 presetY);
};

} // namespace Enjin::GUI
