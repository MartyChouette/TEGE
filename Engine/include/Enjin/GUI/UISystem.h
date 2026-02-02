#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UIEvents.h"

namespace Enjin::GUI {

class ENJIN_API UISystem {
public:
    // Main update: layout + render + input for all UICanvasComponents in the world
    // vpW/vpH = viewport dimensions, deltaTime for animations
    void Update(ECS::World* world, f32 vpW, f32 vpH, f32 deltaTime);

    // Access the event bus for registering listeners
    UIEventBus& GetEventBus() { return m_EventBus; }

    // Editor-facing API: compute layout and render preview for a single canvas
    void ComputeLayoutForCanvas(UICanvasComponent& canvas, f32 vpW, f32 vpH);
    void RenderCanvasPreview(const UICanvasComponent& canvas);

private:
    UIEventBus m_EventBus;

    // Layout pass: compute rects for all elements in a canvas
    void ComputeLayout(UICanvasComponent& canvas, f32 vpW, f32 vpH);
    void ComputeElementRect(UIElement& element, const UIRect& parentRect, f32 scaleFactor);

    // Render pass: draw all visible elements
    void RenderCanvas(const UICanvasComponent& canvas);
    void RenderElement(const UIElement& element, const UITheme& theme);

    // Individual widget renderers
    void RenderPanel(const UIElement& element, const UITheme& theme);
    void RenderButton(const UIElement& element, const UITheme& theme);
    void RenderLabel(const UIElement& element, const UITheme& theme);
    void RenderImage(const UIElement& element, const UITheme& theme);
    void RenderProgressBar(const UIElement& element, const UITheme& theme);
    void RenderSlider(const UIElement& element, const UITheme& theme);
    void RenderCheckbox(const UIElement& element, const UITheme& theme);
    void RenderToggle(const UIElement& element, const UITheme& theme);
    void RenderPlaceholder(const UIElement& element, const UITheme& theme);

    // Input pass: hit test and process interactions
    void ProcessInput(UICanvasComponent& canvas, f32 vpW, f32 vpH);
};

} // namespace Enjin::GUI
