#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <functional>
#include <vector>

namespace Enjin {
namespace Renderer {

#if !ENJIN_RENDERER_WEBGPU
class VulkanContext;
class VulkanRenderer;

// Offscreen render target for rendering scenes to a texture (Vulkan only)
class ENJIN_API RenderTarget {
public:
    RenderTarget();
    ~RenderTarget();

    // Texture registration callbacks — set by the editor for ImGui integration.
    // If not set, RenderTarget works headless (no UI texture binding).
    using TextureRegisterFn = std::function<VkDescriptorSet(VkSampler, VkImageView, VkImageLayout)>;
    using TextureUnregisterFn = std::function<void(VkDescriptorSet)>;
    void SetTextureCallbacks(TextureRegisterFn onRegister, TextureUnregisterFn onUnregister);

    // Create the render target with given dimensions
    // `withVelocity` adds a second colour attachment carrying per-pixel screen
    // motion (RG16F), which is what TAA needs to reproject moving OBJECTS
    // rather than just the camera. triangle.frag already writes it at location
    // 1 for the swapchain main pass; this makes the offscreen pass able to
    // receive it too.
    //
    // Off by default on purpose. A target with velocity has a different
    // attachment layout, so every pipeline drawn into it must be built with
    // colorAttachmentCount 2 -- the effect renderers already take that as a
    // parameter and cache per pass. Turning it on for a target whose pipelines
    // still say 1 makes every draw in it incompatible and the image goes black.
    bool Create(VulkanRenderer* renderer, u32 width, u32 height, bool withVelocity = false);

    // Destroy all Vulkan resources
    void Destroy();

    // Resize the render target (destroys and recreates)
    bool Resize(u32 width, u32 height);

    // Begin rendering to this target (begins render pass)
    void Begin(VkCommandBuffer cmd);

    // What Begin() clears the colour attachment to. Defaults to the constant this
    // used to hardcode, so a caller that never sets it is unchanged.
    //
    // CameraComponent::backgroundColor was authored, serialized and shown in the
    // inspector, and read by NO renderer -- desktop cleared to a hardcoded
    // {0.1, 0.1, 0.15} here and web to a different hardcoded {0.4, 0.5, 0.65}, so
    // the same scene had two backgrounds and the authored one had none (2026-09-16).
    void SetClearColor(f32 r, f32 g, f32 b) { m_ClearR = r; m_ClearG = g; m_ClearB = b; }

    // End rendering to this target (ends render pass, transitions for sampling)
    void End(VkCommandBuffer cmd);
    // Pause and resume the pass Begin opened, for work that cannot be recorded
    // inside it (occlusion phase 1 reads this target's depth with compute).
    // Suspend ends the pass WITHOUT End's colour transition, so colour stays in
    // COLOR_ATTACHMENT_OPTIMAL and depth in DEPTH_STENCIL_ATTACHMENT_OPTIMAL.
    // Resume expects depth in DEPTH_STENCIL_READ_ONLY_OPTIMAL (where the Hi-Z
    // build leaves it) and reopens a LOAD-op twin of the pass, render-pass
    // compatible with it, so pipelines built for this target keep working.
    // Resume re-sets viewport and scissor; bindings are the caller's.
    void Suspend(VkCommandBuffer cmd);
    void Resume(VkCommandBuffer cmd);
    bool CanSuspend() const { return m_ResumeRenderPass != VK_NULL_HANDLE; }

    // Capture color attachment pixels to CPU memory (RGBA8, blocking)
    // Returns empty vector on failure. Caller owns the data.
    std::vector<u8> CaptureToPixels() const;

    // The DEPTH attachment, as the raw projected depth the rasterizer wrote
    // (D32_SFLOAT, one float per pixel, row-major from the top). Used to bake
    // a pre-rendered background's depth plate. Converting these to world
    // distances needs the projection they came from, so that is the caller's
    // job -- see Renderer::InvertPlateDepth.
    std::vector<f32> CaptureDepthToPixels() const;

    // Begin/End a single-attachment render pass for post-processing output.
    // Only touches the color image — velocity and depth are left untouched.
    void BeginPPPass(VkCommandBuffer cmd);
    void EndPPPass(VkCommandBuffer cmd);

    // Begin/End a single-attachment pass that LOADS the existing colour instead
    // of clearing it, so something can be drawn ON TOP of the rendered scene.
    //
    // BeginPPPass clears to black by design ("if we see black, PP isn't drawing"),
    // which is right for post-processing -- it reads the scene as a texture and
    // writes a whole new image. Compositing is the other shape: the OIT resolve
    // blends over what is already there, and clearing first would throw the opaque
    // scene away and leave only the transparent objects floating on black.
    void BeginCompositePass(VkCommandBuffer cmd);
    void EndCompositePass(VkCommandBuffer cmd);
    VkRenderPass GetCompositeRenderPass() const { return m_CompositeRenderPass; }

    // Getters
    VkRenderPass GetRenderPass() const { return m_RenderPass; }
    VkRenderPass GetPPRenderPass() const { return m_PPRenderPass; }
    VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
    VkImageView GetColorImageView() const { return m_ColorImageView; }
    VkImageView GetDepthImageView() const { return m_DepthImageView; }
    // Fixed at creation; the OIT pass has to bake the same format into its own
    // depth attachment or the render pass will not accept the framebuffer.
    VkFormat GetDepthFormat() const { return VK_FORMAT_D32_SFLOAT; }
    VkImage GetDepthImage() const { return m_DepthImage; }
    VkSampler GetSampler() const { return m_Sampler; }
    VkDescriptorSet GetImGuiTextureID() const { return m_UIDescriptor; }
    VkImage GetColorImage() const { return m_ColorImage; }

    // Null unless the target was created withVelocity.
    VkImageView GetVelocityImageView() const { return m_VelocityImageView; }
    VkImage GetVelocityImage() const { return m_VelocityImage; }
    bool HasVelocity() const { return m_VelocityImageView != VK_NULL_HANDLE; }
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    bool IsValid() const { return m_Framebuffer != VK_NULL_HANDLE; }

private:
    f32 m_ClearR = 0.1f, m_ClearG = 0.1f, m_ClearB = 0.15f;
    bool m_WantVelocity = false;
    VkImage m_VelocityImage = VK_NULL_HANDLE;
    VkDeviceMemory m_VelocityMemory = VK_NULL_HANDLE;
    VkImageView m_VelocityImageView = VK_NULL_HANDLE;

    bool CreateImages();
    // One-time clear + transition of the fresh color image to SHADER_READ_ONLY_OPTIMAL.
    // Consumers (ImGui viewport widget, PostProcessing source/placeholder bindings)
    // may sample the target before its first Begin/End cycle — without this the
    // first-frame sample hits VK_IMAGE_LAYOUT_UNDEFINED (VUID-09600).
    void InitializeColorLayout();
    bool CreateRenderPass();
    bool CreateFramebuffer();
    bool CreatePPRenderPass();
    bool CreateCompositeRenderPass();
    bool CreatePPFramebuffer();
    bool CreateSampler();
    void RegisterTexture();
    void UnregisterTexture();
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

    // Render pass and framebuffer (color + depth, no MRT velocity)
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    VkRenderPass m_ResumeRenderPass = VK_NULL_HANDLE;   // LOAD twin (see Suspend/Resume)
    VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;

    // Single-attachment render pass for post-processing (color only, no velocity/depth)
    VkRenderPass m_PPRenderPass = VK_NULL_HANDLE;
    VkRenderPass m_CompositeRenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_PPFramebuffer = VK_NULL_HANDLE;

    // Sampler for reading the color output
    VkSampler m_Sampler = VK_NULL_HANDLE;

    // UI texture descriptor (set via callback — headless if no callback registered)
    VkDescriptorSet m_UIDescriptor = VK_NULL_HANDLE;
    TextureRegisterFn m_OnTextureRegister;
    TextureUnregisterFn m_OnTextureUnregister;
};
#endif // !ENJIN_RENDERER_WEBGPU

} // namespace Renderer
} // namespace Enjin
