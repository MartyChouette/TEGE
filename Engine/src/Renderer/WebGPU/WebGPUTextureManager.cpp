#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUTextureManager.h"
#include "Enjin/Logging/Log.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Enjin::Renderer {

WebGPUTextureManager::WebGPUTextureManager(WebGPURenderer* renderer)
    : m_Renderer(renderer) {}

WebGPUTextureManager::~WebGPUTextureManager() {
    Shutdown();
}

void WebGPUTextureManager::Shutdown() {
    m_Pool.ForEach([this](WebGPUTextureSlot& slot) {
        m_Renderer->DestroyTexture(slot.nativeHandle);
    });
    m_Pool.Clear();
}

// Translate GPUTextureFormat to WGPUTextureFormat
static WGPUTextureFormat TranslateFormat(GPUTextureFormat fmt) {
    switch (fmt) {
        case GPUTextureFormat::RGBA8Unorm:          return WGPUTextureFormat_RGBA8Unorm;
        case GPUTextureFormat::RGBA8Srgb:           return WGPUTextureFormat_RGBA8UnormSrgb;
        case GPUTextureFormat::BGRA8Unorm:          return WGPUTextureFormat_BGRA8Unorm;
        case GPUTextureFormat::BGRA8Srgb:           return WGPUTextureFormat_BGRA8UnormSrgb;
        case GPUTextureFormat::RGBA16Float:         return WGPUTextureFormat_RGBA16Float;
        case GPUTextureFormat::RGBA32Float:         return WGPUTextureFormat_RGBA32Float;
        case GPUTextureFormat::R8Unorm:             return WGPUTextureFormat_R8Unorm;
        case GPUTextureFormat::RG8Unorm:            return WGPUTextureFormat_RG8Unorm;
        case GPUTextureFormat::Depth24PlusStencil8: return WGPUTextureFormat_Depth24PlusStencil8;
        case GPUTextureFormat::Depth32Float:        return WGPUTextureFormat_Depth32Float;
        case GPUTextureFormat::Depth16Unorm:        return WGPUTextureFormat_Depth16Unorm;
    }
    return WGPUTextureFormat_RGBA8Unorm;
}

// Translate GPUTextureUsage to WGPUTextureUsage
static WGPUTextureUsage TranslateUsage(GPUTextureUsage usage) {
    WGPUTextureUsage result = 0;
    if (static_cast<u32>(usage) & static_cast<u32>(GPUTextureUsage::Sampled))          result |= WGPUTextureUsage_TextureBinding;
    if (static_cast<u32>(usage) & static_cast<u32>(GPUTextureUsage::Storage))          result |= WGPUTextureUsage_StorageBinding;
    if (static_cast<u32>(usage) & static_cast<u32>(GPUTextureUsage::RenderAttachment)) result |= WGPUTextureUsage_RenderAttachment;
    if (static_cast<u32>(usage) & static_cast<u32>(GPUTextureUsage::CopySrc))          result |= WGPUTextureUsage_CopySrc;
    if (static_cast<u32>(usage) & static_cast<u32>(GPUTextureUsage::CopyDst))          result |= WGPUTextureUsage_CopyDst;
    return result ? result : WGPUTextureUsage_TextureBinding;
}

GPUTextureHandle WebGPUTextureManager::CreateTexture(const GPUTextureDesc& desc) {
    WGPUTextureFormat wgpuFmt = TranslateFormat(desc.format);
    WGPUTextureUsage wgpuUsage = TranslateUsage(desc.usage);
    auto native = m_Renderer->CreateTexture(desc.width, desc.height, wgpuFmt, wgpuUsage, nullptr);
    if (!native.texture) {
        ENJIN_LOG_ERROR(Core, "WebGPUTextureManager: CreateTexture failed (%ux%u, fmt=%d)",
                        desc.width, desc.height, static_cast<int>(desc.format));
        return {};
    }

    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->nativeHandle = native;
    slot->width = desc.width;
    slot->height = desc.height;
    return GPUTextureHandle{handle};
}

GPUTextureHandle WebGPUTextureManager::CreateTextureWithData(const GPUTextureDesc& desc, const void* pixelData) {
    WGPUTextureFormat wgpuFmt = TranslateFormat(desc.format);
    WGPUTextureUsage wgpuUsage = TranslateUsage(desc.usage);
    auto native = m_Renderer->CreateTexture(desc.width, desc.height, wgpuFmt, wgpuUsage, pixelData);
    if (!native.texture) {
        ENJIN_LOG_ERROR(Core, "WebGPUTextureManager: CreateTextureWithData failed (%ux%u)", desc.width, desc.height);
        return {};
    }

    auto handle = m_Pool.Allocate();
    auto* slot = m_Pool.Get(handle);
    slot->nativeHandle = native;
    slot->width = desc.width;
    slot->height = desc.height;
    return GPUTextureHandle{handle};
}

GPUTextureHandle WebGPUTextureManager::CreateSolidColor(u8 r, u8 g, u8 b, u8 a) {
    u8 pixel[4] = { r, g, b, a };
    GPUTextureDesc desc;
    desc.width = 1;
    desc.height = 1;
    return CreateTextureWithData(desc, pixel);
}

GPUTextureHandle WebGPUTextureManager::LoadFromMemory(const void* fileData, u64 fileSize, const char* label) {
    int w, h, channels;
    stbi_uc* pixels = stbi_load_from_memory(
        static_cast<const stbi_uc*>(fileData), static_cast<int>(fileSize),
        &w, &h, &channels, 4);
    if (!pixels) {
        ENJIN_LOG_WARN(Core, "WebGPUTextureManager: Failed to decode image%s%s",
                       label ? ": " : "", label ? label : "");
        return {};
    }

    GPUTextureDesc desc;
    desc.width = static_cast<u32>(w);
    desc.height = static_cast<u32>(h);
    auto handle = CreateTextureWithData(desc, pixels);
    stbi_image_free(pixels);
    return handle;
}

void WebGPUTextureManager::DestroyTexture(GPUTextureHandle handle) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot) return;
    m_Renderer->DestroyTexture(slot->nativeHandle);
    m_Pool.Free(handle.id);
}

void WebGPUTextureManager::UploadData(GPUTextureHandle handle, const void* data, u32 width, u32 height) {
    auto* slot = m_Pool.Get(handle.id);
    if (!slot || !data) return;
    m_Renderer->UploadTexture(slot->nativeHandle, data, width, height);
    slot->width = width;
    slot->height = height;
}

u32 WebGPUTextureManager::GetWidth(GPUTextureHandle handle) const {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->width : 0;
}

u32 WebGPUTextureManager::GetHeight(GPUTextureHandle handle) const {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? slot->height : 0;
}

bool WebGPUTextureManager::IsValid(GPUTextureHandle handle) const {
    return m_Pool.IsValid(handle.id);
}

const WebGPUTextureHandle* WebGPUTextureManager::GetNativeTexture(GPUTextureHandle handle) const {
    auto* slot = m_Pool.Get(handle.id);
    return slot ? &slot->nativeHandle : nullptr;
}

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
