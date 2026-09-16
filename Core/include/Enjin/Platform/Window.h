#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <functional>

namespace Enjin {

struct WindowDesc {
    u32 width = 1280;
    u32 height = 720;
    const char* title = "Enjin Engine";
    const char* iconPath = nullptr; // Path to PNG icon file (nullptr = no icon)
    bool resizable = true;
    bool fullscreen = false;
    // Create the window without showing it. The swapchain, the render loop and
    // presentation all work normally; nobody sees the frames.
    //
    // For capture runs. A sweep of the example projects opens and closes a
    // window per project and takes over the screen for minutes, which makes it
    // something you cannot run while working, and it cannot run at all on a
    // machine with no desktop session. Hidden rather than a headless Vulkan
    // surface because it needs one GLFW hint and keeps the presentation path
    // identical to the one a player takes -- a separate offscreen path would be
    // a second render path to keep honest, which is the problem this harness
    // exists to find.
    bool visible = true;
};

class ENJIN_API Window {
public:
    using EventCallback = std::function<void()>;
    using ResizeCallback = std::function<void(u32, u32)>;
    using FocusCallback = std::function<void(bool)>;
    using IconifyCallback = std::function<void(bool)>;
    using DropCallback = std::function<void(int count, const char** paths)>;
    using CloseCallback = std::function<bool()>; // Return false to cancel close

    virtual ~Window() = default;

    virtual void PollEvents() = 0;
    virtual bool ShouldClose() const = 0;
    virtual void Close() = 0;

    virtual u32 GetWidth() const = 0;
    virtual u32 GetHeight() const = 0;
    virtual Math::Vector2 GetSize() const = 0;

    virtual void* GetNativeHandle() const = 0; // Returns platform-specific window handle (GLFWwindow*)
    virtual void* GetPlatformWindowHandle() const = 0; // Returns OS-level handle (HWND on Windows)

    virtual void SetEventCallback(const EventCallback& callback) = 0;
    virtual void SetResizeCallback(const ResizeCallback& callback) = 0;
    virtual void SetFocusCallback(const FocusCallback& callback) = 0;
    virtual void SetIconifyCallback(const IconifyCallback& callback) = 0;
    virtual void SetDropCallback(const DropCallback& callback) = 0;
    virtual void SetCloseCallback(const CloseCallback& callback) = 0;

    virtual bool IsFocused() const = 0;
    virtual bool IsIconified() const = 0;

    // Set window title at runtime
    virtual void SetTitle(const char* title) = 0;

    // Set window icon at runtime (PNG path)
    virtual void SetIcon(const char* iconPath) = 0;

    // Fullscreen toggle at runtime
    virtual void SetFullscreen(bool fullscreen) = 0;
    virtual bool IsFullscreen() const = 0;

    // Block until an event occurs (for use when minimized to avoid busy-spin)
    virtual void WaitEvents() = 0;

    // Like WaitEvents but wakes after at most `seconds` even with no event -
    // lets the minimized loop keep servicing background work (MCP server)
    // instead of sleeping until the window is touched again.
    virtual void WaitEventsTimeout(f64 seconds) { (void)seconds; WaitEvents(); }
};

// If Windows.h got included first, its CreateWindow macro would mangle this
// declaration into CreateWindowA(...)
#ifdef CreateWindow
    #undef CreateWindow
#endif

ENJIN_API Window* CreateWindow(const WindowDesc& desc);
ENJIN_API void DestroyWindow(Window* window);

} // namespace Enjin
