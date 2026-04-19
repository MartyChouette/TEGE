#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPURenderEncoder.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipelineManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBufferManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBindGroupManager.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

namespace Enjin::Renderer {

static WGPUStringView wgpuStr(const char* s) { return {s, WGPU_STRLEN}; }

WebGPURenderEncoder::WebGPURenderEncoder(WebGPURenderer* renderer,
                                         WGPURenderPassEncoder passEncoder,
                                         WebGPUPipelineManager* pipelineMgr,
                                         WebGPUBufferManager* bufferMgr,
                                         WebGPUBindGroupManager* bindGroupMgr)
    : m_Renderer(renderer), m_PassEncoder(passEncoder),
      m_PipelineMgr(pipelineMgr), m_BufferMgr(bufferMgr), m_BindGroupMgr(bindGroupMgr) {
    CreateInlineConstantResources();
}

WebGPURenderEncoder::~WebGPURenderEncoder() {
    if (m_InlineConstantBindGroup) wgpuBindGroupRelease(m_InlineConstantBindGroup);
    if (m_InlineConstantLayout) wgpuBindGroupLayoutRelease(m_InlineConstantLayout);
    if (m_InlineConstantBuffer) wgpuBufferRelease(m_InlineConstantBuffer);
}

void WebGPURenderEncoder::CreateInlineConstantResources() {
    WGPUDevice device = m_Renderer->GetDevice();

    // Create the inline constant UBO
    WGPUBufferDescriptor bufDesc{};
    bufDesc.size = INLINE_CONSTANT_SIZE;
    bufDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    m_InlineConstantBuffer = wgpuDeviceCreateBuffer(device, &bufDesc);

    // Create bind group layout for group 3
    WGPUBindGroupLayoutEntry entry{};
    entry.binding = 0;
    entry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    entry.buffer.type = WGPUBufferBindingType_Uniform;
    entry.buffer.minBindingSize = INLINE_CONSTANT_SIZE;

    WGPUBindGroupLayoutDescriptor layoutDesc{};
    layoutDesc.entryCount = 1;
    layoutDesc.entries = &entry;
    m_InlineConstantLayout = wgpuDeviceCreateBindGroupLayout(device, &layoutDesc);

    // Create bind group
    WGPUBindGroupEntry bgEntry{};
    bgEntry.binding = 0;
    bgEntry.buffer = m_InlineConstantBuffer;
    bgEntry.offset = 0;
    bgEntry.size = INLINE_CONSTANT_SIZE;

    WGPUBindGroupDescriptor bgDesc{};
    bgDesc.layout = m_InlineConstantLayout;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    m_InlineConstantBindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);
}

void WebGPURenderEncoder::BindPipeline(GPUPipelineHandle pipeline) {
    WGPURenderPipeline native = m_PipelineMgr->GetNativePipeline(pipeline);
    if (!native) return;
    wgpuRenderPassEncoderSetPipeline(m_PassEncoder, native);
}

void WebGPURenderEncoder::SetBindGroup(u32 index, GPUBindGroupHandle group,
                                        u32 dynamicOffsetCount, const u32* dynamicOffsets) {
    WGPUBindGroup native = m_BindGroupMgr->GetNativeGroup(group);
    if (!native) return;
    wgpuRenderPassEncoderSetBindGroup(m_PassEncoder, index, native,
                                      dynamicOffsetCount, dynamicOffsets);
}

void WebGPURenderEncoder::SetVertexBuffer(u32 slot, GPUBufferHandle buffer, u64 offset) {
    WGPUBuffer native = m_BufferMgr->GetNativeBuffer(buffer);
    if (!native) return;
    wgpuRenderPassEncoderSetVertexBuffer(m_PassEncoder, slot, native, offset, WGPU_WHOLE_SIZE);
}

void WebGPURenderEncoder::SetIndexBuffer(GPUBufferHandle buffer, GPUIndexFormat format, u64 offset) {
    WGPUBuffer native = m_BufferMgr->GetNativeBuffer(buffer);
    if (!native) return;
    WGPUIndexFormat wgpuFmt = (format == GPUIndexFormat::Uint16)
        ? WGPUIndexFormat_Uint16 : WGPUIndexFormat_Uint32;
    wgpuRenderPassEncoderSetIndexBuffer(m_PassEncoder, native, wgpuFmt, offset, WGPU_WHOLE_SIZE);
}

void WebGPURenderEncoder::SetInlineConstants(u32 offset, u32 size, const void* data) {
    if (!data || size == 0 || offset + size > INLINE_CONSTANT_SIZE) return;

    // Write data to the reserved UBO
    wgpuQueueWriteBuffer(m_Renderer->GetQueue(), m_InlineConstantBuffer, offset, data, size);

    // Bind the inline constant group at the reserved slot
    wgpuRenderPassEncoderSetBindGroup(m_PassEncoder, INLINE_CONSTANT_GROUP,
                                      m_InlineConstantBindGroup, 0, nullptr);
}

void WebGPURenderEncoder::Draw(u32 vertexCount, u32 instanceCount,
                                u32 firstVertex, u32 firstInstance) {
    wgpuRenderPassEncoderDraw(m_PassEncoder, vertexCount, instanceCount,
                              firstVertex, firstInstance);
}

void WebGPURenderEncoder::DrawIndexed(u32 indexCount, u32 instanceCount,
                                       u32 firstIndex, i32 baseVertex, u32 firstInstance) {
    wgpuRenderPassEncoderDrawIndexed(m_PassEncoder, indexCount, instanceCount,
                                      firstIndex, baseVertex, firstInstance);
}

void WebGPURenderEncoder::SetViewport(f32 x, f32 y, f32 width, f32 height,
                                       f32 minDepth, f32 maxDepth) {
    wgpuRenderPassEncoderSetViewport(m_PassEncoder, x, y, width, height, minDepth, maxDepth);
}

void WebGPURenderEncoder::SetScissor(u32 x, u32 y, u32 width, u32 height) {
    wgpuRenderPassEncoderSetScissorRect(m_PassEncoder, x, y, width, height);
}

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
