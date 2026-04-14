#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <memory>

namespace Enjin {
namespace Effects {

// Per-instance fluid cell data uploaded to GPU each frame
struct FluidCellInstanceData {
    Math::Vector3 position;  // World-space cell center
    f32 size;                // Cell world size
    f32 colorR, colorG, colorB;
    f32 alpha;
};

// GPU instanced renderer for fluid simulation cells.
// Follows the same architecture as ParticleRenderer: shared quad mesh,
// per-instance buffer, alpha-blended depth-tested pipeline.
class ENJIN_API FluidRenderer {
public:
    FluidRenderer() = default;
    ~FluidRenderer();

    bool Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout);
    void Shutdown();

    void RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);
    bool ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout);

    // Set the fluid simulation to read grid data from
    void SetFluidSimulation(FluidSimulation* sim) { m_Simulation = sim; }

    void Render(VkCommandBuffer commandBuffer,
                const std::vector<VkDescriptorSet>& descriptorSets,
                u32 currentFrame,
                ECS::World* world,
                u32 viewportWidth = 0,
                u32 viewportHeight = 0);

private:
    void CreateQuadBuffers();
    void CreateInstanceBuffer();
    void CreatePipeline(VkDescriptorSetLayout sharedLayout);
    void CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);

    Renderer::VulkanRenderer* m_Renderer = nullptr;
    FluidSimulation* m_Simulation = nullptr;

    std::unique_ptr<Renderer::VulkanBuffer> m_QuadVertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_QuadIndexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_InstanceBuffer;
    static constexpr u32 MAX_FLUID_CELLS = 16384;

    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;

    std::vector<FluidCellInstanceData> m_InstanceDataCache;

    bool m_Initialized = false;
};

} // namespace Effects
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
