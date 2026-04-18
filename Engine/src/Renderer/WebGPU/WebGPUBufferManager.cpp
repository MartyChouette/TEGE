#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUBufferManager.h"
#include "Enjin/Logging/Log.h"

namespace Enjin::Renderer {

WebGPUBufferManager::WebGPUBufferManager(WebGPURenderer* renderer)
    : m_Renderer(renderer) {}

WebGPUBufferManager::~WebGPUBufferManager() {
    Shutdown();
}

void WebGPUBufferManager::Shutdown() {
    m_Pool.ForEach([this](WebGPUBufferSlot& slot) {
        m_Renderer->DestroyBuffer(slot.nativeHandle);
    });
    m_Pool.Clear();
}

WGPUBufferUsage WebGPUBufferManager::TranslateUsage(GPUBufferUsage usage) {
    WGPUBufferUsage wgpuUsage = 0;
    if (usage & GPUBufferUsage::Vertex)   wgpuUsage |= WGPUBufferUsage_Vertex;
    if (usage & GPUBufferUsage::Index)    wgpuUsage |= WGPUBufferUsage_Index;
    if (usage & GPUBufferUsage::Uniform)  wgpuUsage |= WGPUBufferUsage_Uniform;
    if (usage & GPUBufferUsage::Storage)  wgpuUsage |= WGPUBufferUsage_Storage;
    if (usage & GPUBufferUsage::Indirect) wgpuUsage |= WGPUBufferUsage_Indirect;
    if (usage & GPUBufferUsage::CopySrc)  wgpuUsage |= WGPUBufferUsage_CopySrc;
    if (usage & GPUBufferUsage::CopyDst)  wgpuUsage |= WGPUBufferUsage_CopyDst;
    return static_cast<WGPUBufferUsage>(wgpuUsage);
}

GPUBufferHandle WebGPUBufferManager::CreateBuffer(const GPUBufferDesc& desc) {
    auto native = m_Renderer->CreateBuffer(desc.size, TranslateUsage(desc.usage), nullptr);
    if (!native.buffer) {
        ENJIN_LOG_ERROR(Core, "WebGPUBufferManager: Failed to create buffer (size=%llu)", desc.size);
        return {};
    }
    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->nativeHandle = native;
    return GPUBufferHandle{handle};
}

GPUBufferHandle WebGPUBufferManager::CreateBufferWithData(const GPUBufferDesc& desc, const void* data) {
    auto native = m_Renderer->CreateBuffer(desc.size, TranslateUsage(desc.usage), data);
    if (!native.buffer) {
        ENJIN_LOG_ERROR(Core, "WebGPUBufferManager: Failed to create buffer with data (size=%llu)", desc.size);
        return {};
    }
    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->nativeHandle = native;
    return GPUBufferHandle{handle};
}

void WebGPUBufferManager::DestroyBuffer(GPUBufferHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot) return;
    m_Renderer->DestroyBuffer(slot->nativeHandle);
    m_Pool.Free(handle.id);
}

void WebGPUBufferManager::UploadData(GPUBufferHandle handle, const void* data, u64 size, u64 offset) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot || !data) return;
    m_Renderer->UpdateBuffer(slot->nativeHandle, data, size, offset);
}

void* WebGPUBufferManager::MapBuffer(GPUBufferHandle handle) {
    // WebGPU doesn't support synchronous buffer mapping in the same way as Vulkan.
    // For now, return nullptr — callers should use UploadData instead.
    ENJIN_LOG_WARN(Core, "WebGPUBufferManager: MapBuffer not supported on WebGPU, use UploadData");
    return nullptr;
}

void WebGPUBufferManager::UnmapBuffer(GPUBufferHandle handle) {
    // No-op — see MapBuffer
}

u64 WebGPUBufferManager::GetBufferSize(GPUBufferHandle handle) const {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->nativeHandle.size : 0;
}

bool WebGPUBufferManager::IsValid(GPUBufferHandle handle) const {
    return m_Pool.IsValid(handle.id);
}

WGPUBuffer WebGPUBufferManager::GetNativeBuffer(GPUBufferHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->nativeHandle.buffer : nullptr;
}

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
