#pragma once
// The RENDERER half of this class is Vulkan-only (guarded below); the
// collider-generation statics are CPU-only and compiled on ALL platforms
// (web draws the same hash-scattered trees via WebGPUVegetationSystem, so
// web play needs the matching trunk colliders). Impl: TreeColliders.cpp.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Transform.h"

#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Effects/WorldTime.h"
#include <vulkan/vulkan.h>
#include <memory>
#endif

namespace Enjin {
namespace Effects {

#if !ENJIN_RENDERER_WEBGPU

// GPU-instanced tree renderer (trunk + canopy)
class ENJIN_API TreeRenderer {
public:
    TreeRenderer() = default;
    ~TreeRenderer();

    // bindlessLayout: set-1 bindless texture layout for custom bark/canopy
    // textures (VK_NULL_HANDLE = procedural-only, textures disabled)
    bool Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout,
                    VkDescriptorSetLayout bindlessLayout = VK_NULL_HANDLE);
    void Shutdown();

    void RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);

    // Hot-reload shaders from disk (compile GLSL → SPIR-V, recreate pipeline)
    bool ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout);

    // mode2D: scatter along X on the XY plane (2D scenes); bindlessSet: set-1
    // textures for volumes with resolved bark/canopy textures
    void Render(VkCommandBuffer commandBuffer,
                const std::vector<VkDescriptorSet>& descriptorSets,
                u32 currentFrame,
                ECS::World* world,
                u32 viewportWidth = 0,
                u32 viewportHeight = 0,
                bool mode2D = false,
                VkDescriptorSet bindlessSet = VK_NULL_HANDLE);

    // Generate box colliders at each tree trunk position within a volume
    // Spawn one static CAPSULE collider entity per trunk, at the same hashed
    // positions the vertex shader places the instances. Static: no renderer
    // state needed. Call at play start (never in edit - the entities would
    // pollute the editable scene and get saved).
    static void GenerateColliders(ECS::World* world, ECS::Entity volumeEntity);
    // All volumes with generateColliders=true. The one call runtimes make.
    static void GenerateAllColliders(ECS::World* world);

    // Seasonal state (driven by WorldTimeSystem)
    void SetSeasonState(Season season, f32 progress);

private:
    Season m_CurrentSeason = Season::Summer;
    f32 m_SeasonProgress = 0.0f;
    void CreateTreeMesh();
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

    std::unique_ptr<Renderer::VulkanBuffer> m_VertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_IndexBuffer;
    u32 m_IndexCount = 0;

    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;

    bool m_Initialized = false;
};

#else // ENJIN_RENDERER_WEBGPU

// Web: rendering is handled by WebGPUVegetationSystem; only the CPU-side
// collider generation exists here (positions match - same integer hash).
class ENJIN_API TreeRenderer {
public:
    static void GenerateColliders(ECS::World* world, ECS::Entity volumeEntity);
    static void GenerateAllColliders(ECS::World* world);
};

#endif // !ENJIN_RENDERER_WEBGPU

} // namespace Effects
} // namespace Enjin
