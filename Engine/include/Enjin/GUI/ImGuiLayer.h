#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Editor/EditorSettings.h"
#include <string>

struct ImFont;

namespace Enjin {

class Window;

namespace GUI {

// Configuration for editor custom fonts
struct EditorFontConfig {
    std::string bodyFontPath;       // Path to body/UI font TTF/OTF (empty = default)
    std::string headingFontPath;    // Path to heading font TTF/OTF (empty = default)
    std::string monoFontPath;       // Path to monospace font TTF/OTF (empty = default)
    f32 bodyFontSize = 17.0f;
    f32 headingFontSize = 23.0f;
    f32 monoFontSize = 16.0f;
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
    ImFont* GetMonoFont() const { return m_MonoFont; }

    // Reload fonts with new configuration (requires atlas rebuild)
    void ReloadFonts(const EditorFontConfig& fontConfig);

    // Apply editor theme (call after Initialize)
    void ApplyTheme(Editor::EditorTheme theme);

    // Set global UI scale (wraps ImGui font global scale)
    void SetGlobalScale(f32 scale);

private:
    bool CreateDescriptorPool();
    void DestroyDescriptorPool();
    void LoadFonts(const EditorFontConfig& fontConfig);

    Renderer::VulkanRenderer* m_Renderer = nullptr;
    Window* m_Window = nullptr;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    bool m_Initialized = false;
    bool m_Enabled = true;

    // Custom fonts
    ImFont* m_BodyFont = nullptr;
    ImFont* m_HeadingFont = nullptr;
    ImFont* m_MonoFont = nullptr;
};

} // namespace GUI
} // namespace Enjin
