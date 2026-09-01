#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UIEvents.h"
#include "Enjin/Renderer/VectorTessellator.h"

#include <functional>
#include <unordered_map>

struct ImDrawList;

namespace Enjin { namespace Renderer { class Camera; } }

namespace Enjin::GUI {

class ENJIN_API UISystem {
public:
    // Main update: layout + render + input for all UICanvasComponents in the world.
    // vpW/vpH = viewport dimensions, deltaTime for animations.
    // originX/originY = top-left of the viewport in window coordinates: the
    // editor's DOCKED game view sits inside the window, so laying out from
    // (0,0) drew the UI over the editor panels and offset all hit-testing.
    // Fullscreen/player/web callers keep the (0,0) default. Rendering is
    // clipped to the viewport rect so nothing bleeds past its borders.
    // camera: game camera for WORLD-SPACE elements (billboard tags glued to
    // entities — the HUDSystem capability folded into UICanvas). Null = all
    // world-space elements are culled.
    void Update(ECS::World* world, f32 vpW, f32 vpH, f32 deltaTime,
                f32 originX = 0.0f, f32 originY = 0.0f,
                const Renderer::Camera* camera = nullptr);

    // Push live gameplay values (health, coins) into data-bound elements
    // (UIWidgetData::bindField). Called from Update() each frame.
    void SyncDataBindings(ECS::World* world);

    // Access the event bus for registering listeners
    UIEventBus& GetEventBus() { return m_EventBus; }

    // HUD-tier gate (replaces the retired HUDSystem's SetEnabled): canvases
    // with sortOrder >= HUD_SORT_ORDER (the tier the hudWidget migration
    // assigns) are skipped while disabled. Menus and dialogue canvases below
    // that tier are unaffected. Driven by PlayMode, the player boot flow, and
    // the "Set HUD Enabled" visual-script node.
    static constexpr i32 HUD_SORT_ORDER = 100;
    void SetHUDEnabled(bool enabled) { m_HUDEnabled = enabled; }
    bool IsHUDEnabled() const { return m_HUDEnabled; }

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

    // Reduced motion: skip animations, instant transitions
    void SetReducedMotion(bool enabled) { m_ReducedMotion = enabled; }
    bool GetReducedMotion() const { return m_ReducedMotion; }

    // Switch access / one-button mode: auto-cycle focus, single input to select
    void SetSwitchAccessEnabled(bool enabled, f32 scanSpeed = 1.5f) {
        m_SwitchAccessEnabled = enabled;
        m_SwitchScanSpeed = (scanSpeed > 0.1f) ? scanSpeed : 1.5f;
        if (!enabled) { m_SwitchScanIndex = -1; m_SwitchScanTimer = 0.0f; }
    }
    bool GetSwitchAccessEnabled() const { return m_SwitchAccessEnabled; }

    // Dwell-click: hover over focusable element to auto-activate
    void SetDwellClickEnabled(bool enabled, f32 dwellTime = 1.0f) {
        m_DwellClickEnabled = enabled;
        m_DwellClickTime = (dwellTime > 0.1f) ? dwellTime : 1.0f;
    }
    bool GetDwellClickEnabled() const { return m_DwellClickEnabled; }
    f32 GetDwellClickTime() const { return m_DwellClickTime; }

    // Sticky drag: once a slider drag starts, it stays locked until explicit release
    void SetStickyDragEnabled(bool enabled) { m_StickyDragEnabled = enabled; }
    bool GetStickyDragEnabled() const { return m_StickyDragEnabled; }

private:
    UIEventBus m_EventBus;
    bool m_HUDEnabled = true;  // HUD-tier canvas gate (see SetHUDEnabled)
    TextureResolver m_TextureResolver;
    // Unified display P2: canvas-side cache of tessellated SVGs, keyed by path
    // (invalid entries cached too, so a bad file isn't re-parsed every frame).
    std::unordered_map<std::string, Renderer::TessellatedGraphic> m_VectorGraphicCache;
    AnnouncerCallback m_AnnouncerCallback;
    f32 m_FontScale = 1.0f;
    bool m_ReducedMotion = false;

    // Switch access state
    bool m_SwitchAccessEnabled = false;
    f32 m_SwitchScanSpeed = 1.5f;  // Seconds per element
    f32 m_SwitchScanTimer = 0.0f;
    i32 m_SwitchScanIndex = -1;    // Current scanned element index (-1 = none)
    f32 m_SwitchPulsePhase = 0.0f; // Animated pulse phase for scan indicator

    // Dwell-click state (motor accessibility)
    bool m_DwellClickEnabled = false;
    f32 m_DwellClickTime = 1.0f;     // Seconds to hover before auto-click
    f32 m_DwellHoverTimer = 0.0f;    // Current hover duration
    u32 m_DwellHoverElementId = 0;   // Element being hovered for dwell

    // Sticky drag state (motor accessibility)
    bool m_StickyDragEnabled = false;
    bool m_StickyDragActive = false;   // Currently locked in a drag
    u32 m_StickyDragElementId = 0;     // Element being dragged

    // Focus navigation state
    f32 m_NavRepeatTimer = 0.0f;
    static constexpr f32 NAV_REPEAT_DELAY = 0.4f;  // Initial delay before repeat
    static constexpr f32 NAV_REPEAT_RATE  = 0.1f;  // Repeat interval

    // Dropdown state
    u32 m_OpenDropdownId = 0;  // ID of currently open dropdown (0 = none)

    // TextInput state
    f32 m_CursorBlinkTimer = 0.0f;  // Cursor blink phase timer
    bool m_CursorVisible = true;     // Cursor blink state

    // Cached canvas sort list (avoids per-frame allocation)
    struct CanvasEntry { ECS::Entity entity; i32 sortOrder; };
    std::vector<CanvasEntry> m_CachedCanvases;

    // Tooltip state
    u32 m_TooltipHoverElementId = 0;  // Element being hovered for tooltip
    f32 m_TooltipHoverTimer = 0.0f;    // How long element has been hovered
    f32 m_TooltipMouseX = 0.0f;       // Mouse position when hovering started
    f32 m_TooltipMouseY = 0.0f;

    // Layout pass: compute rects for all elements in a canvas
    void ComputeLayout(UICanvasComponent& canvas, f32 vpW, f32 vpH,
                       f32 originX = 0.0f, f32 originY = 0.0f);
    // World-space pass: recenter worldSpace elements on their projected entity
    // position (or cull them) after ComputeLayout
    void ApplyWorldSpaceAnchors(UICanvasComponent& canvas, ECS::World* world,
                                ECS::Entity canvasEntity, const Renderer::Camera* camera,
                                f32 originX, f32 originY, f32 vpW, f32 vpH);
    void ComputeElementRect(UIElement& element, const UIRect& parentRect, f32 scaleFactor);

    // Render pass: draw all visible elements
    void RenderCanvas(const UICanvasComponent& canvas);
    void RenderElement(const UIElement& element, const UITheme& theme, u32 focusedId, const UICanvasComponent& canvas);

    // Individual widget renderers
    void RenderPanel(const UIElement& element, const UITheme& theme);
    void RenderButton(const UIElement& element, const UITheme& theme, bool focused);
    void RenderLabel(const UIElement& element, const UITheme& theme);
    void RenderImage(const UIElement& element, const UITheme& theme);
    void RenderVectorGraphic(const UIElement& element, const UITheme& theme);
    void RenderProgressBar(const UIElement& element, const UITheme& theme);
    void RenderSlider(const UIElement& element, const UITheme& theme);
    void RenderCheckbox(const UIElement& element, const UITheme& theme);
    void RenderToggle(const UIElement& element, const UITheme& theme);
    void RenderDropdown(const UIElement& element, const UITheme& theme, bool focused);
    void RenderTextInput(const UIElement& element, const UITheme& theme, bool focused);
    void RenderRadioGroup(const UIElement& element, const UITheme& theme, bool focused);
    void RenderScrollArea(const UIElement& element, const UITheme& theme, const UICanvasComponent& canvas);
    void RenderGrid(const UIElement& element, const UITheme& theme, const UICanvasComponent& canvas, u32 focusedId);
    void RenderTabGroup(const UIElement& element, const UITheme& theme, const UICanvasComponent& canvas, u32 focusedId);
    void RenderTooltip(const UIElement& element, const UITheme& theme);
    void RenderModal(const UIElement& element, const UITheme& theme, const UICanvasComponent& canvas, u32 focusedId);
    void RenderListView(const UIElement& element, const UITheme& theme, const UICanvasComponent& canvas, bool focused);
    void RenderPlaceholder(const UIElement& element, const UITheme& theme);

    // Focus indicator rendering
    void RenderFocusIndicator(const UIElement& element, const UITheme& theme);

    // Switch access scanning indicator (thicker border, animated pulse)
    void RenderScanIndicator(const UIElement& element, const UITheme& theme);

    // Dwell-click progress indicator
    void RenderDwellProgress(const UIElement& element, f32 progress);

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
