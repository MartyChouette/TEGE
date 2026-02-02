#include "Enjin/Platform/Window.h"
#include "Enjin/Logging/Log.h"
#include <iostream>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "stb_image.h"

namespace Enjin {

// GLFW implementation of Window
class GLFWWindow : public Window {
public:
    GLFWWindow(const WindowDesc& desc) : m_Desc(desc) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // We'll use Vulkan
        glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);

        // Add error callback before creation
        glfwSetErrorCallback([](int error, const char* description) {
            std::cerr << "GLFW Error " << error << ": " << description << std::endl;
        });

        m_Window = glfwCreateWindow(
            static_cast<int>(desc.width),
            static_cast<int>(desc.height),
            desc.title,
            desc.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
            nullptr
        );

        if (!m_Window) {
            ENJIN_LOG_ERROR(Core, "Failed to create GLFW window");
            return;
        }

        // Set window icon if path provided
        if (desc.iconPath) {
            int iconW = 0, iconH = 0, iconChannels = 0;
            unsigned char* iconPixels = stbi_load(desc.iconPath, &iconW, &iconH, &iconChannels, 4);
            if (iconPixels) {
                GLFWimage icon;
                icon.width = iconW;
                icon.height = iconH;
                icon.pixels = iconPixels;
                glfwSetWindowIcon(m_Window, 1, &icon);
                stbi_image_free(iconPixels);
                ENJIN_LOG_INFO(Core, "Window icon set from: %s (%dx%d)", desc.iconPath, iconW, iconH);
            } else {
                ENJIN_LOG_WARN(Core, "Failed to load window icon: %s", desc.iconPath);
            }
        }

        glfwSetWindowUserPointer(m_Window, this);
        SetupCallbacks();
    }

    ~GLFWWindow() override {
        if (m_Window) {
            glfwDestroyWindow(m_Window);
        }
    }

    void PollEvents() override {
        glfwPollEvents();
    }

    bool ShouldClose() const override {
        return glfwWindowShouldClose(m_Window) != 0;
    }

    void Close() override {
        glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
    }

    u32 GetWidth() const override {
        int width, height;
        glfwGetFramebufferSize(m_Window, &width, &height);
        return static_cast<u32>(width);
    }

    u32 GetHeight() const override {
        int width, height;
        glfwGetFramebufferSize(m_Window, &width, &height);
        return static_cast<u32>(height);
    }

    Math::Vector2 GetSize() const override {
        int width, height;
        glfwGetFramebufferSize(m_Window, &width, &height);
        return Math::Vector2(static_cast<f32>(width), static_cast<f32>(height));
    }

    void* GetNativeHandle() const override {
        return m_Window;
    }

    void SetEventCallback(const EventCallback& callback) override {
        m_EventCallback = callback;
    }

    void SetResizeCallback(const ResizeCallback& callback) override {
        m_ResizeCallback = callback;
    }

    void SetFocusCallback(const FocusCallback& callback) override {
        m_FocusCallback = callback;
    }

    void SetIconifyCallback(const IconifyCallback& callback) override {
        m_IconifyCallback = callback;
    }

    bool IsFocused() const override { return m_Focused; }
    bool IsIconified() const override { return m_Iconified; }

    void WaitEvents() override {
        glfwWaitEvents();
    }

    GLFWwindow* GetGLFWHandle() const { return m_Window; }

private:
    void SetupCallbacks() {
        // Use framebuffer size callback (pixel dimensions) rather than window size
        // callback (screen coordinates) - Vulkan needs pixel dimensions, and this
        // also fires on HiDPI monitor changes where only DPI scale changes.
        glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
            GLFWWindow* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
            if (self && self->m_ResizeCallback) {
                self->m_ResizeCallback(static_cast<u32>(width), static_cast<u32>(height));
            }
        });

        glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* window, int focused) {
            GLFWWindow* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
            if (self) {
                self->m_Focused = (focused == GLFW_TRUE);
                if (self->m_FocusCallback) {
                    self->m_FocusCallback(self->m_Focused);
                }
            }
        });

        glfwSetWindowIconifyCallback(m_Window, [](GLFWwindow* window, int iconified) {
            GLFWWindow* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
            if (self) {
                self->m_Iconified = (iconified == GLFW_TRUE);
                if (self->m_IconifyCallback) {
                    self->m_IconifyCallback(self->m_Iconified);
                }
            }
        });
    }

    GLFWwindow* m_Window = nullptr;
    WindowDesc m_Desc;
    EventCallback m_EventCallback;
    ResizeCallback m_ResizeCallback;
    FocusCallback m_FocusCallback;
    IconifyCallback m_IconifyCallback;
    bool m_Focused = true;
    bool m_Iconified = false;
};

Window* CreateWindow(const WindowDesc& desc) {
    static bool glfwInitialized = false;
    if (!glfwInitialized) {
        // Set error callback for initialization too
        glfwSetErrorCallback([](int error, const char* description) {
            std::cerr << "GLFW Error (Init) " << error << ": " << description << std::endl;
        });

        if (!glfwInit()) {
            ENJIN_LOG_ERROR(Core, "Failed to initialize GLFW");
            std::cerr << "CRITICAL ERROR: Failed to initialize GLFW!" << std::endl;
            return nullptr;
        }
        glfwInitialized = true;
    }

    GLFWWindow* window = new GLFWWindow(desc);
    if (!window->GetGLFWHandle()) {
        delete window;
        return nullptr;
    }
    return window;
}

void DestroyWindow(Window* window) {
    delete window;
}

} // namespace Enjin
