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
};

} // namespace Enjin::GUI
