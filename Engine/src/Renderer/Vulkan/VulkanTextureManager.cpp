#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Renderer/Vulkan/VulkanTextureManager.h"
#include "Enjin/Logging/Log.h"

#include "stb_image.h"

namespace Enjin::Renderer {

VulkanTextureManager::VulkanTextureManager(VulkanContext* context)
    : m_Context(context) {}

VulkanTextureManager::~VulkanTextureManager() {
    Shutdown();
}

void VulkanTextureManager::Shutdown() {
    m_Pool.ForEach([](VulkanTextureSlot& slot) {
        if (slot.texture) slot.texture->Destroy();
    });
    m_Pool.Clear();
}

GPUTextureHandle VulkanTextureManager::CreateTexture(const GPUTextureDesc& desc) {
    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->texture = std::make_unique<Texture>(m_Context);
    slot->width = desc.width;
    slot->height = desc.height;
    // Texture is created but no data uploaded — caller uses UploadData next
    return GPUTextureHandle{handle};
}

GPUTextureHandle VulkanTextureManager::CreateTextureWithData(const GPUTextureDesc& desc, const void* pixelData) {
    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->texture = std::make_unique<Texture>(m_Context);
    if (!slot->texture->CreateFromData(pixelData, desc.width, desc.height, 4)) {
        ENJIN_LOG_ERROR(Core, "VulkanTextureManager: Failed to create texture (%ux%u)", desc.width, desc.height);
        m_Pool.Free(handle);
        return {};
    }
    slot->width = desc.width;
    slot->height = desc.height;
    return GPUTextureHandle{handle};
}

GPUTextureHandle VulkanTextureManager::CreateSolidColor(u8 r, u8 g, u8 b, u8 a) {
    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->texture = std::make_unique<Texture>(m_Context);
    if (!slot->texture->CreateSolidColor(r, g, b, a)) {
        ENJIN_LOG_ERROR(Core, "VulkanTextureManager: Failed to create solid color texture");
        m_Pool.Free(handle);
        return {};
    }
    slot->width = 1;
    slot->height = 1;
    return GPUTextureHandle{handle};
}

GPUTextureHandle VulkanTextureManager::LoadFromMemory(const void* fileData, u64 fileSize, const char* label) {
    int w, h, channels;
    stbi_uc* pixels = stbi_load_from_memory(
        static_cast<const stbi_uc*>(fileData), static_cast<int>(fileSize),
        &w, &h, &channels, 4);
    if (!pixels) {
        ENJIN_LOG_WARN(Core, "VulkanTextureManager: Failed to decode image%s%s",
                       label ? ": " : "", label ? label : "");
        return {};
    }

    GPUTextureDesc desc;
    desc.width = static_cast<u32>(w);
    desc.height = static_cast<u32>(h);
    desc.format = GPUTextureFormat::RGBA8Unorm;
    auto handle = CreateTextureWithData(desc, pixels);
    stbi_image_free(pixels);
    return handle;
}

void VulkanTextureManager::DestroyTexture(GPUTextureHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot) return;
    if (slot->texture) slot->texture->Destroy();
    m_Pool.Free(handle.id);
}

void VulkanTextureManager::UploadData(GPUTextureHandle handle, const void* data, u32 width, u32 height) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot || !slot->texture || !data) return;
    // Recreate from data (Vulkan textures don't support partial re-upload easily)
    slot->texture->CreateFromData(data, width, height, 4);
    slot->width = width;
    slot->height = height;
}

u32 VulkanTextureManager::GetWidth(GPUTextureHandle handle) const {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->width : 0;
}

u32 VulkanTextureManager::GetHeight(GPUTextureHandle handle) const {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->height : 0;
}

bool VulkanTextureManager::IsValid(GPUTextureHandle handle) const {
    return m_Pool.IsValid(handle.id);
}

Texture* VulkanTextureManager::GetNativeTexture(GPUTextureHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->texture.get() : nullptr;
}

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU
