#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Platform/Window.h"
#include "Enjin/Logging/Log.h"
#include <emscripten.h>
#include <emscripten/html5.h>

namespace Enjin {

// Browser canvas-based window implementation for Emscripten/WebGPU builds.
// Replaces the GLFW window used on desktop — maps browser events to the same
// Window interface so the rest of the engine (Input, Application) is unchanged.
class WebWindow : public Window {
public:
    WebWindow(const WindowDesc& desc) : m_Desc(desc) {
        // Set canvas size to match requested dimensions
        emscripten_set_canvas_element_size("#game-canvas",
                                           static_cast<int>(desc.width),
                                           static_cast<int>(desc.height));

        // Register resize callback so we track browser window changes
        emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, this, false,
            [](int eventType, const EmscriptenUiEvent* uiEvent, void* userData) -> EM_BOOL {
                (void)eventType;
                auto* self = static_cast<WebWindow*>(userData);
                // Update cached size
                int w = 0, h = 0;
                emscripten_get_canvas_element_size("#game-canvas", &w, &h);
                self->m_Width = static_cast<u32>(w);
                self->m_Height = static_cast<u32>(h);
                if (self->m_ResizeCallback) {
                    self->m_ResizeCallback(self->m_Width, self->m_Height);
                }
                return EM_TRUE;
            });

        // Register focus/blur callbacks
        emscripten_set_focus_callback("#game-canvas", this, false,
            [](int eventType, const EmscriptenFocusEvent* focusEvent, void* userData) -> EM_BOOL {
                (void)eventType; (void)focusEvent;
                auto* self = static_cast<WebWindow*>(userData);
                self->m_Focused = true;
                if (self->m_FocusCallback) self->m_FocusCallback(true);
                return EM_TRUE;
            });
        emscripten_set_blur_callback("#game-canvas", this, false,
            [](int eventType, const EmscriptenFocusEvent* focusEvent, void* userData) -> EM_BOOL {
                (void)eventType; (void)focusEvent;
                auto* self = static_cast<WebWindow*>(userData);
                self->m_Focused = false;
                if (self->m_FocusCallback) self->m_FocusCallback(false);
                return EM_TRUE;
            });

        m_Width = desc.width;
        m_Height = desc.height;

        ENJIN_LOG_INFO(Core, "WebWindow created: %ux%u", m_Width, m_Height);
    }

    ~WebWindow() override = default;

    void PollEvents() override {
        // Browser dispatches events automatically; nothing to poll.
    }

    bool ShouldClose() const override { return m_ShouldClose; }

    void Close() override { m_ShouldClose = true; }

    u32 GetWidth() const override { return m_Width; }
    u32 GetHeight() const override { return m_Height; }

    Math::Vector2 GetSize() const override {
        return Math::Vector2(static_cast<f32>(m_Width), static_cast<f32>(m_Height));
    }

    void* GetNativeHandle() const override { return nullptr; }
    void* GetPlatformWindowHandle() const override { return nullptr; }

    void SetEventCallback(const EventCallback& callback) override { m_EventCallback = callback; }
    void SetResizeCallback(const ResizeCallback& callback) override { m_ResizeCallback = callback; }
    void SetFocusCallback(const FocusCallback& callback) override { m_FocusCallback = callback; }
    void SetIconifyCallback(const IconifyCallback& callback) override { m_IconifyCallback = callback; }
    void SetDropCallback(const DropCallback& callback) override { m_DropCallback = callback; }

    bool IsFocused() const override { return m_Focused; }
    bool IsIconified() const override { return false; } // Browsers don't iconify canvases

    void SetIcon(const char* iconPath) override { (void)iconPath; } // Not applicable for web
    void WaitEvents() override {} // No blocking wait in browser

private:
    WindowDesc m_Desc;
    u32 m_Width = 0;
    u32 m_Height = 0;
    bool m_ShouldClose = false;
    bool m_Focused = true;

    EventCallback m_EventCallback;
    ResizeCallback m_ResizeCallback;
    FocusCallback m_FocusCallback;
    IconifyCallback m_IconifyCallback;
    DropCallback m_DropCallback;
};

Window* CreateWindow(const WindowDesc& desc) {
    return new WebWindow(desc);
}

void DestroyWindow(Window* window) {
    delete window;
}

} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
