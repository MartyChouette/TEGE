#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UIEvents.h"

#include <functional>

struct ImDrawList;

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

    // Texture resolver: path -> (ImTextureID as void*, width, height). Set by EditorLayer or Player.
    using TextureResolver = std::function<void*(const std::string& path, u32& outW, u32& outH)>;
    void SetTextureResolver(TextureResolver resolver) { m_TextureResolver = std::move(resolver); }

    // Focus management
    void SetFocus(UICanvasComponent& canvas, u32 elementId);
    void ClearFocus(UICanvasComponent& canvas);
    u32 GetFocusedElementId(const UICanvasComponent& canvas) const { return canvas.focusedElementId; }

    // Announcer callback for screen reader support (called on focus change)
    using AnnouncerCallback = std::function<void(const std::string& text)>;
    void SetAnnouncerCallback(AnnouncerCallback cb) { m_AnnouncerCallback = std::move(cb); }

    // Font scale multiplier (applied to all resolved font sizes at render time)
    void SetFontScale(f32 scale) { m_FontScale = (scale > 0.1f) ? scale : 1.0f; }
    f32 GetFontScale() const { return m_FontScale; }

private:
    UIEventBus m_EventBus;
    TextureResolver m_TextureResolver;
    AnnouncerCallback m_AnnouncerCallback;
    f32 m_FontScale = 1.0f;

    // Focus navigation state
    f32 m_NavRepeatTimer = 0.0f;
    static constexpr f32 NAV_REPEAT_DELAY = 0.4f;  // Initial delay before repeat
    static constexpr f32 NAV_REPEAT_RATE  = 0.1f;  // Repeat interval

    // Layout pass: compute rects for all elements in a canvas
    void ComputeLayout(UICanvasComponent& canvas, f32 vpW, f32 vpH);
    void ComputeElementRect(UIElement& element, const UIRect& parentRect, f32 scaleFactor);

    // Render pass: draw all visible elements
    void RenderCanvas(const UICanvasComponent& canvas);
    void RenderElement(const UIElement& element, const UITheme& theme, u32 focusedId);

    // Individual widget renderers
    void RenderPanel(const UIElement& element, const UITheme& theme);
    void RenderButton(const UIElement& element, const UITheme& theme, bool focused);
    void RenderLabel(const UIElement& element, const UITheme& theme);
    void RenderImage(const UIElement& element, const UITheme& theme);
    void RenderProgressBar(const UIElement& element, const UITheme& theme);
    void RenderSlider(const UIElement& element, const UITheme& theme);
    void RenderCheckbox(const UIElement& element, const UITheme& theme);
    void RenderToggle(const UIElement& element, const UITheme& theme);
    void RenderPlaceholder(const UIElement& element, const UITheme& theme);

    // Focus indicator rendering
    void RenderFocusIndicator(const UIElement& element, const UITheme& theme);

    // Nine-slice rendering helper
    void DrawNineSlice(ImDrawList* dl, const UIRect& rect, void* texId,
                       u32 texW, u32 texH, const NineSliceConfig& config, u32 tint);

    // Resolve nine-slice config: element override -> theme default
    const NineSliceConfig& ResolveNineSlice(const UIElement& element, const UITheme& theme) const;

    // Input pass: hit test and process interactions
    void ProcessInput(UICanvasComponent& canvas, f32 vpW, f32 vpH);

    // Focus navigation: Tab/DPad/Arrow key navigation with repeat
    void ProcessFocusNavigation(UICanvasComponent& canvas, f32 deltaTime);

    // Activate the currently focused element (Enter/Space/Gamepad-A)
    void ActivateFocusedElement(UICanvasComponent& canvas);

    // Adjust slider value when focused (Left/Right keys)
    void AdjustSliderValue(UIElement& element, i32 direction);

    // Build sorted tab order from focusable elements
    std::vector<UIElement*> BuildTabOrder(UICanvasComponent& canvas);

    // Check if an element can receive focus
    bool IsElementFocusable(const UIElement& element) const;
};

} // namespace Enjin::GUI
