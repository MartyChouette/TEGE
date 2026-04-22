#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUBindGroupManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBufferManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUTextureManager.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Logging/Log.h"

namespace Enjin::Renderer {

WebGPUBindGroupManager::WebGPUBindGroupManager(WebGPURenderer* renderer,
                                               WebGPUBufferManager* bufferMgr,
                                               WebGPUTextureManager* textureMgr)
    : m_Renderer(renderer), m_BufferMgr(bufferMgr), m_TextureMgr(textureMgr) {}

WebGPUBindGroupManager::~WebGPUBindGroupManager() {
    Shutdown();
}

void WebGPUBindGroupManager::Shutdown() {
    m_GroupPool.ForEach([](WebGPUBindGroupSlot& slot) {
        if (slot.group) { wgpuBindGroupRelease(slot.group); slot.group = nullptr; }
    });
    m_GroupPool.Clear();
    m_LayoutPool.ForEach([](WebGPUBindGroupLayoutSlot& slot) {
        if (slot.layout) { wgpuBindGroupLayoutRelease(slot.layout); slot.layout = nullptr; }
    });
    m_LayoutPool.Clear();
}

static WGPUBufferBindingType TranslateBufferType(GPUBindingType type) {
    switch (type) {
        case GPUBindingType::UniformBuffer:        return WGPUBufferBindingType_Uniform;
        case GPUBindingType::StorageBuffer:        return WGPUBufferBindingType_Storage;
        case GPUBindingType::StorageBufferReadOnly: return WGPUBufferBindingType_ReadOnlyStorage;
        default: return WGPUBufferBindingType_Undefined;
    }
}

static WGPUShaderStage TranslateVisibility(GPUShaderStage stage) {
    WGPUShaderStage flags = 0;
    if (stage & GPUShaderStage::Vertex)   flags |= WGPUShaderStage_Vertex;
    if (stage & GPUShaderStage::Fragment) flags |= WGPUShaderStage_Fragment;
    if (stage & GPUShaderStage::Compute)  flags |= WGPUShaderStage_Compute;
    return static_cast<WGPUShaderStage>(flags);
}

GPUBindGroupLayoutHandle WebGPUBindGroupManager::CreateBindGroupLayout(const GPUBindGroupLayoutDesc& desc) {
    std::vector<WGPUBindGroupLayoutEntry> entries(desc.entries.size());
    for (usize i = 0; i < desc.entries.size(); i++) {
        auto& e = entries[i];
        auto& src = desc.entries[i];
        std::memset(&e, 0, sizeof(e));
        e.binding = src.binding;
        e.visibility = TranslateVisibility(src.visibility);

        switch (src.type) {
            case GPUBindingType::UniformBuffer:
            case GPUBindingType::StorageBuffer:
            case GPUBindingType::StorageBufferReadOnly:
                e.buffer.type = TranslateBufferType(src.type);
                e.buffer.minBindingSize = src.minBufferBindingSize;
                break;
            case GPUBindingType::SampledTexture:
                e.texture.sampleType = WGPUTextureSampleType_Float;
                e.texture.viewDimension = WGPUTextureViewDimension_2D;
                break;
            case GPUBindingType::DepthTexture:
                e.texture.sampleType = WGPUTextureSampleType_Depth;
                e.texture.viewDimension = WGPUTextureViewDimension_2D;
                break;
            case GPUBindingType::DepthTextureCube:
                e.texture.sampleType = WGPUTextureSampleType_Depth;
                e.texture.viewDimension = WGPUTextureViewDimension_Cube;
                break;
            case GPUBindingType::StorageTexture:
                e.storageTexture.access = WGPUStorageTextureAccess_WriteOnly;
                e.storageTexture.format = WGPUTextureFormat_RGBA8Unorm;
                e.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
                break;
            case GPUBindingType::Sampler:
                e.sampler.type = WGPUSamplerBindingType_Filtering;
                break;
            case GPUBindingType::ComparisonSampler:
                e.sampler.type = WGPUSamplerBindingType_Comparison;
                break;
        }
    }

    WGPUBindGroupLayoutDescriptor layoutDesc{};
    layoutDesc.entryCount = static_cast<u32>(entries.size());
    layoutDesc.entries = entries.data();

    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(m_Renderer->GetDevice(), &layoutDesc);
    if (!layout) {
        ENJIN_LOG_ERROR(Core, "WebGPUBindGroupManager: Failed to create bind group layout");
        return {};
    }

    auto handle = m_LayoutPool.Allocate();
    auto* slot = m_LayoutPool.Get(handle);
    slot->layout = layout;
    return GPUBindGroupLayoutHandle{handle};
}

GPUBindGroupHandle WebGPUBindGroupManager::CreateBindGroup(const GPUBindGroupDesc& desc) {
    auto* layoutSlot = m_LayoutPool.Get(desc.layout.id);
    if (!layoutSlot || !layoutSlot->layout) {
        ENJIN_LOG_ERROR(Core, "WebGPUBindGroupManager: Invalid layout handle");
        return {};
    }

    std::vector<WGPUBindGroupEntry> entries(desc.entries.size());
    for (usize i = 0; i < desc.entries.size(); i++) {
        auto& e = entries[i];
        auto& src = desc.entries[i];
        std::memset(&e, 0, sizeof(e));
        e.binding = src.binding;

        // Buffer binding
        if (src.buffer.IsValid()) {
            e.buffer = m_BufferMgr->GetNativeBuffer(src.buffer);
            e.offset = src.bufferOffset;
            e.size = src.bufferSize > 0 ? src.bufferSize : m_BufferMgr->GetBufferSize(src.buffer);
        }
        // Texture binding
        if (src.texture.IsValid()) {
            auto* native = m_TextureMgr->GetNativeTexture(src.texture);
            if (native) e.textureView = native->view;
        }
        // Sampler binding
        if (src.sampler.IsValid()) {
            auto* native = m_TextureMgr->GetNativeTexture(src.sampler);
            if (native) e.sampler = native->sampler;
        }
    }

    WGPUBindGroupDescriptor groupDesc{};
    groupDesc.layout = layoutSlot->layout;
    groupDesc.entryCount = static_cast<u32>(entries.size());
    groupDesc.entries = entries.data();

    WGPUBindGroup group = wgpuDeviceCreateBindGroup(m_Renderer->GetDevice(), &groupDesc);
    if (!group) {
        ENJIN_LOG_ERROR(Core, "WebGPUBindGroupManager: Failed to create bind group");
        return {};
    }

    auto handle = m_GroupPool.Allocate();
    auto* slot = m_GroupPool.Get(handle);
    slot->group = group;
    return GPUBindGroupHandle{handle};
}

void WebGPUBindGroupManager::DestroyBindGroup(GPUBindGroupHandle handle) {
    auto* slot = m_GroupPool.Get(handle.id);
    if (!slot) return;
    if (slot->group) { wgpuBindGroupRelease(slot->group); slot->group = nullptr; }
    m_GroupPool.Free(handle.id);
}

void WebGPUBindGroupManager::DestroyBindGroupLayout(GPUBindGroupLayoutHandle handle) {
    auto* slot = m_LayoutPool.Get(handle.id);
    if (!slot) return;
    if (slot->layout) { wgpuBindGroupLayoutRelease(slot->layout); slot->layout = nullptr; }
    m_LayoutPool.Free(handle.id);
}

bool WebGPUBindGroupManager::IsValid(GPUBindGroupHandle handle) const {
    return m_GroupPool.IsValid(handle.id);
}

bool WebGPUBindGroupManager::IsValidLayout(GPUBindGroupLayoutHandle handle) const {
    return m_LayoutPool.IsValid(handle.id);
}

WGPUBindGroupLayout WebGPUBindGroupManager::GetNativeLayout(GPUBindGroupLayoutHandle handle) {
    auto* slot = m_LayoutPool.Get(handle.id);
    return slot ? slot->layout : nullptr;
}

WGPUBindGroup WebGPUBindGroupManager::GetNativeGroup(GPUBindGroupHandle handle) {
    auto* slot = m_GroupPool.Get(handle.id);
    return slot ? slot->group : nullptr;
}

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
