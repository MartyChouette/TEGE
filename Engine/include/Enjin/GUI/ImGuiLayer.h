#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Editor/EditorSettings.h"
#include <string>
#include <unordered_map>
#include <cmath>

struct ImFont;
struct ImVec4;

namespace Enjin {

class Window;

namespace GUI {

// ============================================================================
// Spring Easing Utilities — micro-interaction animation helpers
// ============================================================================

// Spring physics state for smooth UI animations
struct SpringState {
    f32 value = 0.0f;
    f32 velocity = 0.0f;
    f32 target = 0.0f;
};

// Spring easing parameters (matches cubic-bezier(0.175, 0.885, 0.32, 1.275))
struct SpringParams {
    f32 stiffness = 180.0f;    // Spring constant (higher = snappier)
    f32 damping = 12.0f;       // Damping coefficient (higher = less overshoot)
    f32 mass = 1.0f;           // Oscillator mass
};

// Advance a spring simulation by deltaTime seconds
// Returns true if the spring is still moving (not yet settled)
inline bool AdvanceSpring(SpringState& state, const SpringParams& params, f32 deltaTime) {
    f32 displacement = state.value - state.target;
    f32 springForce = -params.stiffness * displacement;
    f32 dampingForce = -params.damping * state.velocity;
    f32 acceleration = (springForce + dampingForce) / params.mass;
    state.velocity += acceleration * deltaTime;
    state.value += state.velocity * deltaTime;
    // Settle when close enough
    if (std::fabs(displacement) < 0.001f && std::fabs(state.velocity) < 0.001f) {
        state.value = state.target;
        state.velocity = 0.0f;
        return false;
    }
    return true;
}

// Evaluate cubic-bezier(0.175, 0.885, 0.32, 1.275) spring easing curve at t (0-1)
inline f32 SpringEase(f32 t) {
    // Approximate spring overshoot curve
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    f32 t2 = t * t;
    f32 t3 = t2 * t;
    // Bezier approximation: overshoots to ~1.05 then settles to 1.0
    return 2.70158f * t3 - 1.70158f * t2;  // easeInBack variant
}

// Smooth linear easing (for hover fades, 100ms at 60fps ≈ 6 frames)
inline f32 SmoothFade(f32 current, f32 target, f32 speed, f32 deltaTime) {
    if (current < target) {
        current += speed * deltaTime;
        if (current > target) current = target;
    } else if (current > target) {
        current -= speed * deltaTime;
        if (current < target) current = target;
    }
    return current;
}

// Color interpolation helper (RGBA)
struct ColorTransition {
    f32 r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
    f32 targetR = 0.0f, targetG = 0.0f, targetB = 0.0f, targetA = 1.0f;

    void SetTarget(f32 tr, f32 tg, f32 tb, f32 ta = 1.0f) {
        targetR = tr; targetG = tg; targetB = tb; targetA = ta;
    }

    void Update(f32 speed, f32 deltaTime) {
        r = SmoothFade(r, targetR, speed, deltaTime);
        g = SmoothFade(g, targetG, speed, deltaTime);
        b = SmoothFade(b, targetB, speed, deltaTime);
        a = SmoothFade(a, targetA, speed, deltaTime);
    }

    bool IsSettled() const {
        return r == targetR && g == targetG && b == targetB && a == targetA;
    }
};

// Per-widget hover animation tracker
// Usage: call Update() each frame with ImGui::IsItemHovered() result
struct HoverAnimation {
    f32 hoverAlpha = 0.0f;         // 0 = not hovered, 1 = fully hovered
    f32 pressScale = 1.0f;         // 1 = normal, < 1 = pressed (bounce)
    SpringState pressSpring;       // Spring for press bounce animation

    void Update(bool isHovered, bool isPressed, f32 deltaTime) {
        // Hover fade (100ms = speed of 10.0)
        f32 hoverTarget = isHovered ? 1.0f : 0.0f;
        hoverAlpha = SmoothFade(hoverAlpha, hoverTarget, 10.0f, deltaTime);

        // Press bounce via spring
        pressSpring.target = isPressed ? 0.95f : 1.0f;
        SpringParams springParams;
        springParams.stiffness = 300.0f;
        springParams.damping = 15.0f;
        AdvanceSpring(pressSpring, springParams, deltaTime);
        pressScale = pressSpring.value;
    }
};

// Global animation state manager (stores hover/press state per ImGui ID)
class ENJIN_API UIAnimationState {
public:
    static UIAnimationState& Get();

    // Get or create hover animation for an ImGui widget ID
    HoverAnimation& GetHoverAnim(u32 widgetId);

    // Update delta time (call once per frame)
    void SetDeltaTime(f32 dt) { m_DeltaTime = dt; }
    f32 GetDeltaTime() const { return m_DeltaTime; }

    // Clean up stale entries (call periodically)
    void Cleanup();

private:
    UIAnimationState() = default;
    std::unordered_map<u32, HoverAnimation> m_HoverAnims;
    f32 m_DeltaTime = 0.016f;
};

// Configuration for editor custom fonts
struct EditorFontConfig {
    std::string bodyFontPath;       // Path to body/UI font TTF/OTF (empty = default)
    std::string headingFontPath;    // Path to heading font TTF/OTF (empty = default)
    std::string monoFontPath;       // Path to monospace font TTF/OTF (empty = default)
    f32 bodyFontSize = 15.0f;
    f32 headingFontSize = 20.0f;
    f32 h2FontSize = 17.0f;         // Section titles (uses heading font) — must
                                    // read visibly larger than body or the type
                                    // hierarchy disappears
    f32 smallFontSize = 12.0f;      // Labels, hints (uses body font)
    f32 monoFontSize = 14.0f;
};

// ImGui integration layer for Vulkan
class ENJIN_API ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();

    // Initialize ImGui with Vulkan renderer
    bool Initialize(Window* window, Renderer::VulkanRenderer* renderer,
                    const EditorFontConfig& fontConfig = EditorFontConfig{});
    void Shutdown();

    // Call at the beginning of each frame
    void BeginFrame();

    // Call at the end of each frame (before present)
    void EndFrame(VkCommandBuffer commandBuffer);

    // Enable/disable ImGui rendering
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // Show the demo window (useful for testing)
    void ShowDemoWindow(bool* open = nullptr);

    // Custom font access (returns nullptr if not loaded, fall back to ImGui default)
    ImFont* GetBodyFont() const { return m_BodyFont; }
    ImFont* GetHeadingFont() const { return m_HeadingFont; }
    ImFont* GetH2Font() const { return m_H2Font; }
    ImFont* GetSmallFont() const { return m_SmallFont; }
    ImFont* GetMonoFont() const { return m_MonoFont; }

    // Static access to the section (H2) typeface for free-function widget
    // helpers (Editor::UI::SectionHeader) — refreshed by every LoadFonts
    static ImFont* SectionFont() { return s_SectionFont; }

    // Reload fonts with new configuration (requires atlas rebuild)
    void ReloadFonts(const EditorFontConfig& fontConfig);

    // Apply editor theme (call after Initialize)
    // If accentColors is provided and useCustom is true, accent colors override theme defaults
    void ApplyTheme(Editor::EditorTheme theme, const Editor::AccentColorConfig* accentColors = nullptr);

    // Set global UI scale (wraps ImGui font global scale)
    void SetGlobalScale(f32 scale);

    // Recreate ImGui's main pipeline when the render pass changes (e.g., MSAA toggle).
    // Call after the renderer's render pass has been recreated.
    void UpdateRenderPass(VkRenderPass renderPass, VkSampleCountFlagBits msaaSamples);

private:
    bool CreateDescriptorPool();
    void DestroyDescriptorPool();
    void LoadFonts(const EditorFontConfig& fontConfig);

    Renderer::VulkanRenderer* m_Renderer = nullptr;
    Window* m_Window = nullptr;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    bool m_Initialized = false;
    bool m_Enabled = true;

    // Custom fonts (5-level typography system)
    ImFont* m_BodyFont = nullptr;
    ImFont* m_HeadingFont = nullptr;  // H1 (23px bold)
    ImFont* m_H2Font = nullptr;       // H2 (18px section titles)
    ImFont* m_SmallFont = nullptr;    // Small (14px labels/hints)
    ImFont* m_MonoFont = nullptr;
    static inline ImFont* s_SectionFont = nullptr;  // = m_H2Font of the live layer
};

} // namespace GUI
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
