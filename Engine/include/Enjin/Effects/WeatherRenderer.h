#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Camera.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <memory>

namespace Enjin {
namespace Effects {

// Per-instance particle data uploaded to GPU each frame
struct ParticleInstanceData {
    Math::Vector3 position;   // World position
    f32 size;                 // Billboard size
    f32 alpha;                // Opacity
    f32 stretchDirX;          // Stretch direction X (screen space)
    f32 stretchDirY;          // Stretch direction Y
    f32 stretch;              // Stretch factor (1.0 = circle, >1 = elongated)
    Math::Vector3 color;      // Per-particle tint (from the emitter's colour over life)
};

// 3D billboard particle renderer for weather effects
// Replaces ImGui 2D overlay with proper Vulkan instanced rendering
class ENJIN_API WeatherRenderer {
public:
    WeatherRenderer() = default;
    ~WeatherRenderer();

    // bindlessLayout: set-1 bindless texture layout for custom rain/snow sprites
    // (VK_NULL_HANDLE = procedural-only, textures disabled)
    bool Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout,
                    VkDescriptorSetLayout bindlessLayout = VK_NULL_HANDLE);
    void Shutdown();

    // Recreate pipeline for a different render pass (e.g. render target vs swapchain)
    void RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);

    // Hot-reload shaders from disk (compile GLSL → SPIR-V, recreate pipeline)
    bool ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout);

    // Upload particle data and render instanced billboards
    // Call within an active render pass, after scene geometry (for depth testing)
    // viewportWidth/Height: 0 = use swapchain extent, >0 = override (for render targets)
    void Render(VkCommandBuffer commandBuffer,
                const std::vector<VkDescriptorSet>& descriptorSets,
                u32 currentFrame,
                const WeatherSystem& weather,
                bool isRain,
                u32 viewportWidth = 0,
                u32 viewportHeight = 0,
                VkDescriptorSet bindlessSet = VK_NULL_HANDLE);

    // Reduced motion: cuts particles to 25%, disables stretch
    void SetReducedMotion(bool enabled) { m_ReducedMotion = enabled; }
    bool GetReducedMotion() const { return m_ReducedMotion; }

private:
    void CreateQuadBuffers();
    void CreateInstanceBuffer();
    void CreatePipeline(VkDescriptorSetLayout sharedLayout);
    void CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);

    // The render pass the pipeline was last built for. VK_NULL_HANDLE means the
    // swapchain pass. ReloadShaders always rebuilt against the swapchain, so
    // hot-reloading a shader while the editor had retargeted this renderer at
    // its offscreen pass produced a pipeline with the wrong attachment count
    // (VUID-07609) instead of the reload you asked for.
    VkRenderPass m_LastRenderPass = VK_NULL_HANDLE;
    u32 m_LastColorAttachmentCount = 2;

    Renderer::VulkanRenderer* m_Renderer = nullptr;
    VkDescriptorSetLayout m_BindlessLayout = VK_NULL_HANDLE;

    // Shared quad mesh (4 vertices, 6 indices)
    std::unique_ptr<Renderer::VulkanBuffer> m_QuadVertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_QuadIndexBuffer;

    // Per-instance buffer (updated each frame)
    std::unique_ptr<Renderer::VulkanBuffer> m_InstanceBuffer;
    static constexpr u32 MAX_PARTICLES = 8000;

    // Pipeline
    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;

    // Reusable instance data cache to avoid per-frame allocation
    std::vector<ParticleInstanceData> m_InstanceDataCache;

    bool m_Initialized = false;
    bool m_ReducedMotion = false;
};

} // namespace Effects
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
