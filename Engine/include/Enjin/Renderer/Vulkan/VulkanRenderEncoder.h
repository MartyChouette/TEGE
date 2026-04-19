#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/GPURenderEncoder.h"
#include <vulkan/vulkan.h>

namespace Enjin::Renderer {

class VulkanPipelineManager;
class VulkanBufferManager;
class VulkanBindGroupManager;

// Vulkan implementation of IRenderEncoder.
// Wraps a VkCommandBuffer — all draw calls forward to vkCmd* functions.
// SetInlineConstants maps to vkCmdPushConstants (native, zero overhead).
class ENJIN_API VulkanRenderEncoder : public IRenderEncoder {
public:
    VulkanRenderEncoder(VkCommandBuffer cmd, VkPipelineLayout currentLayout,
                        VulkanPipelineManager* pipelineMgr,
                        VulkanBufferManager* bufferMgr,
                        VulkanBindGroupManager* bindGroupMgr);

    void BindPipeline(GPUPipelineHandle pipeline) override;
    void SetBindGroup(u32 index, GPUBindGroupHandle group,
                      u32 dynamicOffsetCount, const u32* dynamicOffsets) override;
    void SetVertexBuffer(u32 slot, GPUBufferHandle buffer, u64 offset) override;
    void SetIndexBuffer(GPUBufferHandle buffer, GPUIndexFormat format, u64 offset) override;
    void SetInlineConstants(u32 offset, u32 size, const void* data) override;
    void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override;
    void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
                     i32 baseVertex, u32 firstInstance) override;
    void DrawIndexedIndirect(GPUBufferHandle buffer, u64 offset, u32 drawCount) override;
    void DrawIndexedIndirectCount(GPUBufferHandle argsBuffer, u64 argsOffset,
                                  GPUBufferHandle countBuffer, u64 countOffset,
                                  u32 maxDrawCount) override;
    void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth, f32 maxDepth) override;
    void SetScissor(u32 x, u32 y, u32 width, u32 height) override;

    VkCommandBuffer GetCommandBuffer() const { return m_Cmd; }

private:
    VkCommandBuffer m_Cmd;
    VkPipelineLayout m_CurrentLayout;
    VulkanPipelineManager* m_PipelineMgr;
    VulkanBufferManager* m_BufferMgr;
    VulkanBindGroupManager* m_BindGroupMgr;
};

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU
