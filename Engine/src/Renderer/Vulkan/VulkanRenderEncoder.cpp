#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Renderer/Vulkan/VulkanRenderEncoder.h"
#include "Enjin/Renderer/Vulkan/VulkanPipelineManager.h"
#include "Enjin/Renderer/Vulkan/VulkanBufferManager.h"
#include "Enjin/Renderer/Vulkan/VulkanBindGroupManager.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"

namespace Enjin::Renderer {

VulkanRenderEncoder::VulkanRenderEncoder(VkCommandBuffer cmd, VkPipelineLayout currentLayout,
                                         VulkanPipelineManager* pipelineMgr,
                                         VulkanBufferManager* bufferMgr,
                                         VulkanBindGroupManager* bindGroupMgr)
    : m_Cmd(cmd), m_CurrentLayout(currentLayout),
      m_PipelineMgr(pipelineMgr), m_BufferMgr(bufferMgr), m_BindGroupMgr(bindGroupMgr) {}

void VulkanRenderEncoder::BindPipeline(GPUPipelineHandle pipeline) {
    VulkanPipeline* native = m_PipelineMgr->GetNativePipeline(pipeline);
    if (!native) return;
    native->Bind(m_Cmd);
    m_CurrentLayout = native->GetLayout();
}

void VulkanRenderEncoder::SetBindGroup(u32 index, GPUBindGroupHandle group,
                                        u32 dynamicOffsetCount, const u32* dynamicOffsets) {
    VkDescriptorSet set = m_BindGroupMgr->GetNativeSet(group);
    if (set == VK_NULL_HANDLE) return;
    vkCmdBindDescriptorSets(m_Cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_CurrentLayout, index, 1, &set,
                            dynamicOffsetCount, dynamicOffsets);
}

void VulkanRenderEncoder::SetVertexBuffer(u32 slot, GPUBufferHandle buffer, u64 offset) {
    VulkanBuffer* native = m_BufferMgr->GetNativeBuffer(buffer);
    if (!native) return;
    VkBuffer buf = native->GetBuffer();
    VkDeviceSize off = static_cast<VkDeviceSize>(offset);
    vkCmdBindVertexBuffers(m_Cmd, slot, 1, &buf, &off);
}

void VulkanRenderEncoder::SetIndexBuffer(GPUBufferHandle buffer, GPUIndexFormat format, u64 offset) {
    VulkanBuffer* native = m_BufferMgr->GetNativeBuffer(buffer);
    if (!native) return;
    VkIndexType vkFormat = (format == GPUIndexFormat::Uint16) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(m_Cmd, native->GetBuffer(), static_cast<VkDeviceSize>(offset), vkFormat);
}

void VulkanRenderEncoder::SetInlineConstants(u32 offset, u32 size, const void* data) {
    if (!data || size == 0) return;
    vkCmdPushConstants(m_Cmd, m_CurrentLayout, VK_SHADER_STAGE_ALL, offset, size, data);
}

void VulkanRenderEncoder::Draw(u32 vertexCount, u32 instanceCount,
                                u32 firstVertex, u32 firstInstance) {
    vkCmdDraw(m_Cmd, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanRenderEncoder::DrawIndexed(u32 indexCount, u32 instanceCount,
                                       u32 firstIndex, i32 baseVertex, u32 firstInstance) {
    vkCmdDrawIndexed(m_Cmd, indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void VulkanRenderEncoder::DrawIndexedIndirect(GPUBufferHandle buffer, u64 offset, u32 drawCount) {
    VulkanBuffer* native = m_BufferMgr->GetNativeBuffer(buffer);
    if (!native) return;
    vkCmdDrawIndexedIndirect(m_Cmd, native->GetBuffer(),
                             static_cast<VkDeviceSize>(offset), drawCount,
                             sizeof(VkDrawIndexedIndirectCommand));
}

void VulkanRenderEncoder::DrawIndexedIndirectCount(GPUBufferHandle argsBuffer, u64 argsOffset,
                                                     GPUBufferHandle countBuffer, u64 countOffset,
                                                     u32 maxDrawCount) {
    VulkanBuffer* args = m_BufferMgr->GetNativeBuffer(argsBuffer);
    VulkanBuffer* count = m_BufferMgr->GetNativeBuffer(countBuffer);
    if (!args || !count) return;
    vkCmdDrawIndexedIndirectCount(m_Cmd, args->GetBuffer(),
                                  static_cast<VkDeviceSize>(argsOffset),
                                  count->GetBuffer(),
                                  static_cast<VkDeviceSize>(countOffset),
                                  maxDrawCount, sizeof(VkDrawIndexedIndirectCommand));
}

void VulkanRenderEncoder::SetViewport(f32 x, f32 y, f32 width, f32 height,
                                       f32 minDepth, f32 maxDepth) {
    VkViewport viewport{x, y, width, height, minDepth, maxDepth};
    vkCmdSetViewport(m_Cmd, 0, 1, &viewport);
}

void VulkanRenderEncoder::SetScissor(u32 x, u32 y, u32 width, u32 height) {
    VkRect2D scissor{{static_cast<i32>(x), static_cast<i32>(y)}, {width, height}};
    vkCmdSetScissor(m_Cmd, 0, 1, &scissor);
}

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU
