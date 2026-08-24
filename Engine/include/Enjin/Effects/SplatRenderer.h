#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Assets/SplatLoader.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace Enjin {
namespace Renderer {
class VulkanContext;
class VulkanBuffer;
class VulkanShader;
class VulkanPipeline;
}

namespace Effects {

// Gaussian splat cloud renderer: one loaded cloud drawn as instanced quads
// with per-splat covariance projection in the vertex shader (see splat.vert).
// Correct alpha compositing needs back-to-front order, so the instance buffer
// is re-sorted on the CPU and re-uploaded - throttled to camera movement, not
// per frame (a photoreal capture is static; only the viewpoint changes).
class ENJIN_API SplatRenderer {
public:
    void Initialize(Renderer::VulkanContext* context);
    void Shutdown();

    // Frame-safe caller contract (FlushPendingChanges): creates GPU buffers.
    void LoadSplats(Assets::SplatData&& data);
    void Clear();
    u32 GetCount() const { return m_Count; }

    // Re-sorts back-to-front when the camera moved enough since the last sort.
    // Frame-safe caller contract: re-uploads the instance buffer.
    void SortIfNeeded(const Math::Matrix4& view, const Math::Matrix4& model);

    // Pipeline per render pass (mirrors GPUParticleSystem's cache)
    void EnsureDrawPipeline(VkRenderPass renderPass, VkDescriptorSetLayout sharedLayout,
                            u32 colorAttachmentCount);
    void RecreateDrawPipeline();   // drop all cached pipelines (pass recreated)

    void Render(VkCommandBuffer cmd, VkDescriptorSet sharedSet,
                const Math::Matrix4& model, f32 viewportW, f32 viewportH,
                f32 opacityScale, f32 splatScale);

private:
    Renderer::VulkanContext* m_Context = nullptr;
    std::vector<Assets::SplatInstance> m_Cpu;      // unsorted source of truth
    std::vector<Assets::SplatInstance> m_Sorted;   // scratch for upload
    std::vector<u32> m_Order;
    std::unique_ptr<Renderer::VulkanBuffer> m_InstanceBuffer;
    u32 m_Count = 0;

    Math::Vector3 m_LastSortPos{};
    Math::Vector3 m_LastSortFwd{};
    bool m_NeedInitialSort = true;

    std::unique_ptr<Renderer::VulkanShader> m_VS;
    std::unique_ptr<Renderer::VulkanShader> m_FS;
    struct PipelineEntry {
        VkRenderPass pass = VK_NULL_HANDLE;
        std::unique_ptr<Renderer::VulkanPipeline> pipeline;
    };
    std::vector<PipelineEntry> m_Pipelines;
    Renderer::VulkanPipeline* m_Current = nullptr;
};

} // namespace Effects
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
