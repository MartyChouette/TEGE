#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Camera.h"
#include <vulkan/vulkan.h>
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
};

// 3D billboard particle renderer for weather effects
// Replaces ImGui 2D overlay with proper Vulkan instanced rendering
class ENJIN_API WeatherRenderer {
public:
    WeatherRenderer() = default;
    ~WeatherRenderer();

    bool Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout);
    void Shutdown();

    // Recreate pipeline for a different render pass (e.g. render target vs swapchain)
    void RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout);

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
                u32 viewportHeight = 0);

    // Reduced motion: cuts particles to 25%, disables stretch
    void SetReducedMotion(bool enabled) { m_ReducedMotion = enabled; }
    bool GetReducedMotion() const { return m_ReducedMotion; }

private:
    void CreateQuadBuffers();
    void CreateInstanceBuffer();
    void CreatePipeline(VkDescriptorSetLayout sharedLayout);
    void CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout);

    Renderer::VulkanRenderer* m_Renderer = nullptr;

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
