#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/GPUTypes.h"
#include <string>
#include <vector>

namespace Enjin::Renderer {

// Abstract texture manager — backend implementations wrap Vulkan Texture or WebGPU textures.
// RenderSystem uses this interface exclusively for all GPU texture operations.
class ENJIN_API IGPUTextureManager {
public:
    virtual ~IGPUTextureManager() = default;

    // Create a texture from a descriptor. No data uploaded yet.
    virtual GPUTextureHandle CreateTexture(const GPUTextureDesc& desc) = 0;

    // Create a texture and immediately upload pixel data (RGBA8, 4 bytes/pixel).
    virtual GPUTextureHandle CreateTextureWithData(const GPUTextureDesc& desc, const void* pixelData) = 0;

    // Create a 1x1 solid color texture (useful for defaults).
    virtual GPUTextureHandle CreateSolidColor(u8 r, u8 g, u8 b, u8 a = 255) = 0;

    // Load a texture from raw encoded file data (PNG/JPG bytes — decoded with stb_image).
    virtual GPUTextureHandle LoadFromMemory(const void* fileData, u64 fileSize, const char* label = nullptr) = 0;

    // Destroy a texture and free its GPU memory.
    virtual void DestroyTexture(GPUTextureHandle handle) = 0;

    // Upload raw pixel data to an existing texture (full replacement).
    virtual void UploadData(GPUTextureHandle handle, const void* data, u32 width, u32 height) = 0;

    // Query texture dimensions.
    virtual u32 GetWidth(GPUTextureHandle handle) const = 0;
    virtual u32 GetHeight(GPUTextureHandle handle) const = 0;

    // Check if a handle is still valid.
    virtual bool IsValid(GPUTextureHandle handle) const = 0;
};

} // namespace Enjin::Renderer
