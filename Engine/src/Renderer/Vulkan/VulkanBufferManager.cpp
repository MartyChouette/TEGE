#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Renderer/Vulkan/VulkanBufferManager.h"
#include "Enjin/Logging/Log.h"

namespace Enjin::Renderer {

VulkanBufferManager::VulkanBufferManager(VulkanContext* context)
    : m_Context(context) {}

VulkanBufferManager::~VulkanBufferManager() {
    Shutdown();
}

void VulkanBufferManager::Shutdown() {
    m_Pool.ForEach([](VulkanBufferSlot& slot) {
        if (slot.buffer) slot.buffer->Destroy();
    });
    m_Pool.Clear();
}

BufferUsage VulkanBufferManager::TranslateUsage(GPUBufferUsage usage) {
    // Map the primary usage. VulkanBuffer::Create takes a single BufferUsage enum
    // but internally ORs in TransferDst. For compound usage, use the primary purpose.
    if (usage & GPUBufferUsage::Vertex)   return BufferUsage::Vertex;
    if (usage & GPUBufferUsage::Index)    return BufferUsage::Index;
    if (usage & GPUBufferUsage::Storage)  return BufferUsage::Storage;
    if (usage & GPUBufferUsage::Indirect) return BufferUsage::IndirectDraw;
    if (usage & GPUBufferUsage::Uniform)  return BufferUsage::Uniform;
    return BufferUsage::Uniform;
}

GPUBufferHandle VulkanBufferManager::CreateBuffer(const GPUBufferDesc& desc) {
    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);

    slot->buffer = std::make_unique<VulkanBuffer>(m_Context);
    if (!slot->buffer->Create(desc.size, TranslateUsage(desc.usage), desc.hostVisible)) {
        ENJIN_LOG_ERROR(Core, "VulkanBufferManager: Failed to create buffer (size=%llu)", desc.size);
        m_Pool.Free(handle);
        return {};
    }
    slot->size = desc.size;
    return GPUBufferHandle{handle};
}

GPUBufferHandle VulkanBufferManager::CreateBufferWithData(const GPUBufferDesc& desc, const void* data) {
    auto h = CreateBuffer(desc);
    if (h.IsValid() && data) {
        UploadData(h, data, desc.size, 0);
    }
    return h;
}

void VulkanBufferManager::DestroyBuffer(GPUBufferHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot) return;
    if (slot->buffer) slot->buffer->Destroy();
    m_Pool.Free(handle.id);
}

void VulkanBufferManager::UploadData(GPUBufferHandle handle, const void* data, u64 size, u64 offset) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot || !slot->buffer || !data) return;
    slot->buffer->UploadData(data, static_cast<usize>(size), static_cast<usize>(offset));
}

void* VulkanBufferManager::MapBuffer(GPUBufferHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot || !slot->buffer) return nullptr;
    return slot->buffer->Map();
}

void VulkanBufferManager::UnmapBuffer(GPUBufferHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot || !slot->buffer) return;
    slot->buffer->Unmap();
}

u64 VulkanBufferManager::GetBufferSize(GPUBufferHandle handle) const {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->size : 0;
}

bool VulkanBufferManager::IsValid(GPUBufferHandle handle) const {
    return m_Pool.IsValid(handle.id);
}

VulkanBuffer* VulkanBufferManager::GetNativeBuffer(GPUBufferHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->buffer.get() : nullptr;
}

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU
