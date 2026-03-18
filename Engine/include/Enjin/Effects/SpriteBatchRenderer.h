#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <functional>
#include <string>

namespace Enjin {
namespace Effects {

class SpriteTextureAtlas;  // Forward declaration
struct AtlasRegion;        // Forward declaration for SpriteEntry::cachedAtlasRegion

// Per-instance sprite data uploaded to GPU each frame.
// Matches the sprite.vert shader instance attribute layout (locations 2-7).
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
    f32 _pad1;                // Padding to 16-byte boundary
    f32 _pad2;
    f32 _pad3;
};

// GPU instanced sprite batch renderer for 2D Sprite2DComponent entities.
// Follows the same architecture as ParticleRenderer/WeatherRenderer: shared quad
// mesh, per-instance buffer, alpha-blended depth-tested pipeline.
// Sprites are grouped by texture atlas and rendered in batched instanced draw calls
// to minimize per-entity overhead.
class ENJIN_API SpriteBatchRenderer {
public:
    SpriteBatchRenderer() = default;
    ~SpriteBatchRenderer();

    bool Initialize(Renderer::VulkanRenderer* renderer, VkDescriptorSetLayout sharedLayout);
    void Shutdown();

    // Recreate pipeline for a different render pass (e.g. render target vs swapchain)
    void RecreateForRenderPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout);

    // Hot-reload shaders from disk (compile GLSL → SPIR-V, recreate pipeline)
    bool ReloadShaders(const std::string& shaderDir, VkDescriptorSetLayout sharedLayout);

    // Gather all visible Sprite2DComponent entities, sort by texture and layer,
    // batch by texture, and render with instanced draw calls.
    // textureBindCallback: called by the renderer to bind the correct texture
    //   descriptor before each texture-grouped batch. Receives base texture path
    //   and normal map path (empty = no normal map, use default white).
    // viewportWidth/Height: 0 = use swapchain extent, >0 = override (for render targets)
    // litMode: when true, uses the lit pipeline with LightingUBO for 2.5D sprite lighting
    void Render(VkCommandBuffer commandBuffer,
                const std::vector<VkDescriptorSet>& descriptorSets,
                u32 currentFrame,
                ECS::World* world,
                const std::function<void(const std::string& texturePath, const std::string& normalMapPath)>& textureBindCallback,
                u32 viewportWidth = 0,
                u32 viewportHeight = 0,
                bool litMode = false);

    // Set the texture atlas for batching sprites with different textures into one draw call
    void SetAtlas(SpriteTextureAtlas* atlas) { m_Atlas = atlas; }

private:
    void CreateQuadBuffers();
    void CreateInstanceBuffer();
    void CreatePipeline(VkDescriptorSetLayout sharedLayout);
    void CreatePipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout);
    void CreateLitPipeline(VkDescriptorSetLayout sharedLayout);
    void CreateLitPipelineWithPass(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout);

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

    // Reusable shadow pass vectors (cleared each frame, capacity preserved)
    std::vector<SpriteInstanceData> m_ShadowInstances;
    struct ShadowBatchEntry {
        std::string textureKey;
        u32 instanceIdx;
    };
    std::vector<ShadowBatchEntry> m_ShadowBatchEntries;
    std::vector<SpriteInstanceData> m_SortedShadowInstances;

    // --- Delta sprite sorting ---
    // Persistent sorted sprite list across frames. Rebuilt (O(N)) each frame to
    // pick up visibility/entity changes, but only re-sorted when sort keys change.
    struct SpriteEntry {
        ECS::Entity entity;
        i32 sortingLayer;
        i32 orderInLayer;
        const ECS::Sprite2DComponent* sprite;
        const AtlasRegion* cachedAtlasRegion;
        bool isAtlased;
        usize textureHash;
        usize normalMapHash;
    };
    std::vector<SpriteEntry> m_SortedSprites;

    // Hash of all sort keys from the previous frame. When the hash matches, the
    // relative order of sprites hasn't changed and we can skip the O(N log N) sort.
    // The hash is computed over (entity, sortingLayer, orderInLayer, textureHash,
    // normalMapHash, isAtlased) for every visible sprite in insertion order.
    usize m_LastSortKeyHash = 0;

    // Texture atlas for packing small sprites into a single draw call
    SpriteTextureAtlas* m_Atlas = nullptr;

    bool m_Initialized = false;
};

} // namespace Effects
} // namespace Enjin
