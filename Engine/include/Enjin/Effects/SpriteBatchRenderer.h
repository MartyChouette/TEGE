#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace Enjin {
namespace Effects {

// Per-instance sprite data uploaded to GPU each frame.
// Matches the sprite.vert shader instance attribute layout (locations 2-9).
struct SpriteInstanceData {
    Math::Vector3 position;   // World position (location 2)
    f32 sizeX;                // Billboard width in world units (location 3, component 0)
    f32 sizeY;                // Billboard height in world units (location 3, component 1)
    f32 rotation;             // Z-axis rotation in radians (location 4)
    f32 _pad0;                // Padding for alignment
    f32 uvLeft;               // UV rect left   (location 5, component 0)
    f32 uvTop;                // UV rect top    (location 5, component 1)
    f32 uvRight;              // UV rect right  (location 5, component 2)
    f32 uvBottom;             // UV rect bottom (location 5, component 3)
    f32 tintR;                // Tint red   (location 6, component 0)
    f32 tintG;                // Tint green (location 6, component 1)
    f32 tintB;                // Tint blue  (location 6, component 2)
    f32 tintA;                // Tint alpha (location 6, component 3)
    u32 flipFlags;            // Bit 0 = flipX, bit 1 = flipY (location 7)
    // Bindless slots for this sprite's own art (locations 8 and 9). -1 = none:
    // the base colour falls back to white, so a tint-only sprite still draws,
    // and the normal map falls back to the sprite's flat plane normal.
    // Carrying the texture per INSTANCE is what lets every sprite in a scene
    // ride one draw call however many different images they use.
    i32 texIndex;
    i32 normalIndex;
    // Where the sprite's own origin sits inside it (location 10). 0.5,0.5 is
    // the centre; 0,0 puts the bottom-left corner on the entity's position.
    // Same meaning as MeshFactory::CreateSpriteQuad, which is the only place
    // that has ever honoured it.
    f32 pivotX;
    f32 pivotY;
};

// GPU instanced sprite batch renderer for 2D Sprite2DComponent entities.
// Follows the same architecture as ParticleRenderer/WeatherRenderer: shared quad
// mesh, per-instance buffer, alpha-blended depth-tested pipeline.
// Every visible sprite goes into ONE instanced draw: the per-instance data
// carries a bindless texture slot, so two sprites with different images cost
// the same as two with the same image and no grouping by texture is needed.
class ENJIN_API SpriteBatchRenderer {
public:
    SpriteBatchRenderer() = default;
    ~SpriteBatchRenderer();

    // bindlessLayout: set-1 bindless texture layout. Without it sprites can
    // still draw, but only as flat tinted quads -- there is nowhere to sample
    // their art from.
    bool Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout,
                    VkDescriptorSetLayout bindlessLayout = VK_NULL_HANDLE);
    void Shutdown();

    // Recreate pipeline for a different render pass (e.g. render target vs swapchain)
    void RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);

    // Hot-reload shaders from disk (compile GLSL → SPIR-V, recreate pipeline)
    bool ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout);

    // Gather all visible Sprite2DComponent entities, sort by layer, and render
    // them in a single instanced draw.
    // resolveTextureIndex: turns an authored texture path into a bindless slot,
    //   or -1 when there is no texture / it failed to load. Called during
    //   instance-data build, never between draws.
    // bindlessSet: set 1, the array those slots index into.
    // viewportWidth/Height: 0 = use swapchain extent, >0 = override (for render targets)
    // litMode: when true, uses the lit pipeline with LightingUBO for 2.5D sprite lighting
    void Render(VkCommandBuffer commandBuffer,
                const std::vector<VkDescriptorSet>& descriptorSets,
                u32 currentFrame,
                ECS::World* world,
                const std::function<i32(const std::string& path)>& resolveTextureIndex,
                VkDescriptorSet bindlessSet = VK_NULL_HANDLE,
                u32 viewportWidth = 0,
                u32 viewportHeight = 0,
                bool litMode = false);

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
    void CreateLitPipeline(VkDescriptorSetLayout sharedLayout);
    void CreateLitPipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout, u32 colorAttachmentCount = 2);

    Renderer::VulkanRenderer* m_Renderer = nullptr;

    // Shared quad mesh (4 vertices, 6 indices)
    std::unique_ptr<Renderer::VulkanBuffer> m_QuadVertexBuffer;
    std::unique_ptr<Renderer::VulkanBuffer> m_QuadIndexBuffer;

    // Per-instance buffer (host-visible, updated each frame)
    std::unique_ptr<Renderer::VulkanBuffer> m_InstanceBuffer;
    static constexpr u32 MAX_SPRITES = 8192;

    // Unlit pipeline (flat 2D sprites)
    std::unique_ptr<Renderer::VulkanPipeline> m_Pipeline;
    std::unique_ptr<Renderer::VulkanShader> m_VertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_FragmentShader;

    // Lit pipeline (2.5D sprites with Blinn-Phong lighting from LightingUBO)
    std::unique_ptr<Renderer::VulkanPipeline> m_LitPipeline;
    std::unique_ptr<Renderer::VulkanShader> m_LitVertexShader;
    std::unique_ptr<Renderer::VulkanShader> m_LitFragmentShader;

    // Reusable instance data cache to avoid per-frame allocation
    std::vector<SpriteInstanceData> m_InstanceDataCache;
    // Whether each instance in the cache wants the lit pipeline. Parallel to the
    // cache on purpose: it decides where a draw is CUT, not what is uploaded.
    std::vector<u8> m_InstanceLit;

    // Reusable shadow pass vector (cleared each frame, capacity preserved)
    std::vector<SpriteInstanceData> m_ShadowInstances;

    // --- Sprite sorting ---
    // Rebuilt every call, in raw entity iteration order, then sorted. Both halves
    // matter: the rebuild is what picks up visibility and entity changes, and
    // because of it there is never a previous order to reuse.
    struct SpriteEntry {
        ECS::Entity entity;
        i32 sortingLayer;
        i32 orderInLayer;
        const ECS::Sprite2DComponent* sprite;
    };
    std::vector<SpriteEntry> m_SortedSprites;

    // Set 1: the bindless texture array every sprite's art is sampled from.
    VkDescriptorSetLayout m_BindlessLayout = VK_NULL_HANDLE;

    bool m_Initialized = false;
};

} // namespace Effects
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
