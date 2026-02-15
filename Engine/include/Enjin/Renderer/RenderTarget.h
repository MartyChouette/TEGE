#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanRenderer;

// Offscreen render target for rendering scenes to a texture
// Used by Game View panel to display game camera output in ImGui
class ENJIN_API RenderTarget {
public:
    RenderTarget();
    ~RenderTarget();

    // Create the render target with given dimensions
    // renderer is needed for ImGui texture registration
    bool Create(VulkanRenderer* renderer, u32 width, u32 height);

    // Destroy all Vulkan resources
    void Destroy();

    // Resize the render target (destroys and recreates)
    bool Resize(u32 width, u32 height);

    // Begin rendering to this target (begins render pass)
    void Begin(VkCommandBuffer cmd);

    // End rendering to this target (ends render pass, transitions for sampling)
    void End(VkCommandBuffer cmd);

    // Capture color attachment pixels to CPU memory (RGBA8, blocking)
    // Returns empty vector on failure. Caller owns the data.
    std::vector<u8> CaptureToPixels() const;

    // Getters
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
    VkImageView GetColorImageView() const { return m_ColorImageView; }
    VkSampler GetSampler() const { return m_Sampler; }
    VkDescriptorSet GetImGuiTextureID() const { return m_ImGuiDescriptor; }
    VkImage GetColorImage() const { return m_ColorImage; }
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    bool IsValid() const { return m_Framebuffer != VK_NULL_HANDLE; }

private:
    bool CreateImages();
    bool CreateRenderPass();
    bool CreateFramebuffer();
    bool CreateSampler();
    void RegisterImGuiTexture();
    void UnregisterImGuiTexture();
    void DestroyResources();

    VulkanRenderer* m_Renderer = nullptr;
    VulkanContext* m_Context = nullptr;

    u32 m_Width = 0;
    u32 m_Height = 0;

    // Color attachment
    VkImage m_ColorImage = VK_NULL_HANDLE;
    VkDeviceMemory m_ColorMemory = VK_NULL_HANDLE;
    VkImageView m_ColorImageView = VK_NULL_HANDLE;

    // Depth attachment
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;

    // Render pass and framebuffer
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;

    // Sampler for reading the color output
    VkSampler m_Sampler = VK_NULL_HANDLE;

    // ImGui descriptor set for displaying the texture
    VkDescriptorSet m_ImGuiDescriptor = VK_NULL_HANDLE;
};

} // namespace Renderer
} // namespace Enjin
