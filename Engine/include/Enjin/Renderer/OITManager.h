#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif

namespace Enjin {
namespace Renderer {

// Forward declarations
class VulkanContext;

// OIT weight function for Weighted Blended OIT (McGuire & Bavoil 2013)
enum class OITWeightFunction : u8 {
    DepthBased,     // Weight by depth (closer = higher weight)
    AlphaBased,     // Weight by alpha value
    Combined,       // Depth * alpha combined weighting
    Count
};

// Configuration for Order-Independent Transparency
struct OITConfig {
    bool enabled = false;
    OITWeightFunction weightFunction = OITWeightFunction::DepthBased;
    f32 depthRange = 100.0f;  // Max depth for weight normalization
};

// Weighted Blended Order-Independent Transparency manager
// Uses accumulation + revealage textures (two-pass approach):
//   Pass 1: Render transparent objects → accumulation (RGBA16F) + revealage (R8)
//   Pass 2: Composite over opaque scene using fullscreen quad
class OITManager {
public:
    OITManager() = default;
    ~OITManager();

    // Lifecycle
    bool Initialize(VulkanContext* context, u32 width, u32 height, VkRenderPass opaqueRenderPass = VK_NULL_HANDLE);
    void Shutdown();

    // Resize handling (recreates textures)
    void Resize(u32 width, u32 height);

    // The depth buffer the OPAQUE pass wrote, which transparent geometry tests
    // against but never writes.
    //
    // Without it there is no depth test at all -- the render pass had no depth
    // attachment while its own comment claimed it "reads depth from the opaque
    // pass via depth test". Transparent geometry behind a wall accumulated exactly
    // as if the wall were not there, which is the one thing weighted-blended OIT
    // still needs the depth buffer for.
    //
    // Passing a different view (or VK_NULL_HANDLE) rebuilds the render pass and
    // framebuffer, so the editor's offscreen target and the swapchain can both
    // drive it across a resize.
    void SetSceneDepth(VkImageView depthView, VkFormat depthFormat);

    // Drop the cached depth view WITHOUT touching any GPU object.
    //
    // Needed before a resize: the cached view belongs to the render target's
    // previous depth image, and a resize destroys that image. Resize rebuilds the
    // framebuffer from whatever is cached, so leaving it set hands a freed view to
    // vkCreateFramebuffer -- which reads it, and takes the driver with it. Clearing
    // first costs one extra pass rebuild on a resize frame and nothing otherwise.
    void ForgetSceneDepth() { m_SceneDepthView = VK_NULL_HANDLE;
                              m_SceneDepthFormat = VK_FORMAT_UNDEFINED; }
    bool HasSceneDepth() const { return m_SceneDepthView != VK_NULL_HANDLE; }

    // What the current framebuffer was actually built against.
    //
    // The caller has to be able to check this every frame, because the render
    // target it points at can be destroyed and recreated between the setup and the
    // draw: the editor flushes (which is the only safe place to build this), THEN
    // resizes its render targets, THEN records. On a resize frame the framebuffer
    // built in the flush references a depth image that no longer exists, and
    // vkCmdBeginRenderPass reads the freed handle and takes the driver with it.
    VkImageView GetSceneDepthView() const { return m_SceneDepthView; }
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }

    // The render pass the COMPOSITE resolves into.
    //
    // Initialize() took the swapchain pass, which is right for the player's direct
    // path and wrong for the editor, where the scene is rendered into an offscreen
    // target and the resolve has to happen there. A pipeline is bound to one render
    // pass, so pointing at a different one rebuilds it.
    void SetCompositeTargetPass(VkRenderPass pass);

    // Rendering interface
    // BeginTransparentPass: Bind accumulation + revealage as render targets, clear them
    // Transparent geometry renders with additive blending (accumulation) and zero-multiply (revealage)
    void BeginTransparentPass(VkCommandBuffer cmd);

    // EndTransparentPass: Transition images for reading
    void EndTransparentPass(VkCommandBuffer cmd);

    // CompositePass: Fullscreen quad blending OIT result over opaque framebuffer
    // Note: Requires compiled composite shader — stub until SPIR-V is available
    void CompositePass(VkCommandBuffer cmd);

    // Configuration
    OITConfig& GetConfig() { return m_Config; }
    const OITConfig& GetConfig() const { return m_Config; }
    void SetConfig(const OITConfig& config) { m_Config = config; }

    bool IsInitialized() const { return m_Initialized; }
    bool IsEnabled() const { return m_Config.enabled && m_Initialized; }

    // Access textures for external binding (e.g., composite shader)
    VkImageView GetAccumulationView() const { return m_AccumulationView; }
    VkImageView GetRevealageView() const { return m_RevealageView; }

    // Get OIT render pass (caller uses this to begin transparent geometry rendering)
    VkRenderPass GetTransparentRenderPass() const { return m_TransparentRenderPass; }
    VkFramebuffer GetTransparentFramebuffer() const { return m_TransparentFramebuffer; }

    // Access images for external copy/barrier operations
    VkImage GetAccumulationImage() const { return m_AccumulationImage; }
    VkImage GetRevealageImage() const { return m_RevealageImage; }

private:
    bool CreateTextures(u32 width, u32 height);
    void DestroyTextures();
    bool CreateTransparentRenderPass();
    bool CreateTransparentFramebuffer();
    bool CreateCompositePipeline(VkRenderPass opaqueRenderPass);
    bool CreateCompositeDescriptorSets();
    void DestroyRenderResources();

    VulkanContext* m_Context = nullptr;
    OITConfig m_Config;

    // Accumulation texture (RGBA16F) — stores weighted color sum
    VkImage m_AccumulationImage = VK_NULL_HANDLE;
    VkDeviceMemory m_AccumulationMemory = VK_NULL_HANDLE;
    VkImageView m_AccumulationView = VK_NULL_HANDLE;

    // Revealage texture (R8) — stores product of (1 - alpha)
    VkImage m_RevealageImage = VK_NULL_HANDLE;
    VkDeviceMemory m_RevealageMemory = VK_NULL_HANDLE;
    VkImageView m_RevealageView = VK_NULL_HANDLE;

    // Transparent geometry render pass (MRT: accumulation + revealage)
    VkRenderPass m_TransparentRenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_TransparentFramebuffer = VK_NULL_HANDLE;

    // Composite pass resources (fullscreen quad to blend OIT over opaque scene)
    VkPipeline m_CompositePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_CompositePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_CompositeDescLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_CompositeDescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_CompositeDescSet = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;

    // Not owned: the depth buffer belongs to whoever rendered the opaque pass.
    VkImageView m_SceneDepthView = VK_NULL_HANDLE;
    VkFormat m_SceneDepthFormat = VK_FORMAT_UNDEFINED;

    VkRenderPass m_OpaqueRenderPass = VK_NULL_HANDLE;  // Not owned — the swapchain render pass
    u32 m_Width = 0;
    u32 m_Height = 0;
    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
