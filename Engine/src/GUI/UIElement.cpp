#include "Enjin/GUI/UICanvas.h"

#include <algorithm>

namespace Enjin::GUI {

u32 UICanvasComponent::AddElement(UIWidgetType type, const std::string& name, u32 parentId) {
    UIElement element;
    element.id = nextElementId++;
    element.name = name;
    element.type = type;
    element.parentId = parentId;

    // Set default anchor sizing based on widget type
    switch (type) {
        case UIWidgetType::Button:
            element.anchor.offsetLeft   = -80.0f;
            element.anchor.offsetRight  = -80.0f;
            element.anchor.offsetTop    = -20.0f;
            element.anchor.offsetBottom = -20.0f;
            break;
        case UIWidgetType::Label:
            element.anchor.offsetLeft   = -100.0f;
            element.anchor.offsetRight  = -100.0f;
            element.anchor.offsetTop    = -12.0f;
            element.anchor.offsetBottom = -12.0f;
            break;
        case UIWidgetType::ProgressBar:
            element.anchor.offsetLeft   = -100.0f;
            element.anchor.offsetRight  = -100.0f;
            element.anchor.offsetTop    = -10.0f;
            element.anchor.offsetBottom = -10.0f;
            break;
        case UIWidgetType::Slider:
            element.anchor.offsetLeft   = -100.0f;
            element.anchor.offsetRight  = -100.0f;
            element.anchor.offsetTop    = -10.0f;
            element.anchor.offsetBottom = -10.0f;
            break;
        case UIWidgetType::Checkbox:
        case UIWidgetType::Toggle:
            element.anchor.offsetLeft   = -12.0f;
            element.anchor.offsetRight  = -12.0f;
            element.anchor.offsetTop    = -12.0f;
            element.anchor.offsetBottom = -12.0f;
            break;
        default:
            element.anchor.offsetLeft   = -100.0f;
            element.anchor.offsetRight  = -100.0f;
            element.anchor.offsetTop    = -60.0f;
            element.anchor.offsetBottom = -60.0f;
            break;
    }

    u32 id = element.id;

    // Register as child of parent
    if (parentId != 0) {
        UIElement* parent = GetElement(parentId);
        if (parent) {
            parent->childIds.push_back(id);
        }
    }

    elements.push_back(std::move(element));
    return id;
}

void UICanvasComponent::RemoveElement(u32 id) {
    // Collect all IDs to remove (element + all descendants)
    std::vector<u32> toRemove;
    toRemove.push_back(id);

    // BFS to find all descendants
    for (usize i = 0; i < toRemove.size(); ++i) {
        u32 current = toRemove[i];
        for (const auto& elem : elements) {
            if (elem.parentId == current) {
                toRemove.push_back(elem.id);
            }
        }
    }

    // Remove from parent's childIds
    UIElement* elem = GetElement(id);
    if (elem && elem->parentId != 0) {
        UIElement* parent = GetElement(elem->parentId);
        if (parent) {
            auto& children = parent->childIds;
            children.erase(std::remove(children.begin(), children.end(), id), children.end());
        }
    }

    // Remove all collected elements
    elements.erase(
        std::remove_if(elements.begin(), elements.end(),
            [&toRemove](const UIElement& e) {
                return std::find(toRemove.begin(), toRemove.end(), e.id) != toRemove.end();
            }),
        elements.end()
    );
}

UIElement* UICanvasComponent::GetElement(u32 id) {
    for (auto& elem : elements) {
        if (elem.id == id) return &elem;
    }
    return nullptr;
}

const UIElement* UICanvasComponent::GetElement(u32 id) const {
    for (const auto& elem : elements) {
        if (elem.id == id) return &elem;
    }
    return nullptr;
}

std::vector<u32> UICanvasComponent::GetRootElementIds() const {
    std::vector<u32> roots;
    for (const auto& elem : elements) {
        if (elem.parentId == 0) {
            roots.push_back(elem.id);
        }
    }
    return roots;
}

std::vector<u32> UICanvasComponent::GetChildIds(u32 parentId) const {
    std::vector<u32> children;
    for (const auto& elem : elements) {
        if (elem.parentId == parentId) {
            children.push_back(elem.id);
        }
    }
    return children;
}

} // namespace Enjin::GUI
