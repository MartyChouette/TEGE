#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"

namespace Enjin {

class Window;

namespace GUI {

// ImGui integration layer for Vulkan
class ENJIN_API ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer();

    // Initialize ImGui with Vulkan renderer
    bool Initialize(Window* window, Renderer::VulkanRenderer* renderer);
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

private:
    bool CreateDescriptorPool();
    void DestroyDescriptorPool();

    Renderer::VulkanRenderer* m_Renderer = nullptr;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    bool m_Initialized = false;
    bool m_Enabled = true;
};

} // namespace GUI
} // namespace Enjin
