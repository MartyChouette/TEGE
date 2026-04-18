#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/GPUTypes.h"

namespace Enjin::Renderer {

// Abstract buffer manager — backend implementations wrap VulkanBuffer or WGPUBuffer.
// RenderSystem uses this interface exclusively for all GPU buffer operations.
class ENJIN_API IGPUBufferManager {
public:
    virtual ~IGPUBufferManager() = default;

    // Create a GPU buffer. Returns an invalid handle on failure.
    virtual GPUBufferHandle CreateBuffer(const GPUBufferDesc& desc) = 0;

    // Create a GPU buffer with initial data upload.
    virtual GPUBufferHandle CreateBufferWithData(const GPUBufferDesc& desc, const void* data) = 0;

    // Destroy a buffer and free its GPU memory.
    virtual void DestroyBuffer(GPUBufferHandle handle) = 0;

    // Upload data to an existing buffer. Offset is in bytes from buffer start.
    virtual void UploadData(GPUBufferHandle handle, const void* data, u64 size, u64 offset = 0) = 0;

    // Map a host-visible buffer for CPU access. Returns nullptr if not mappable.
    virtual void* MapBuffer(GPUBufferHandle handle) = 0;

    // Unmap a previously mapped buffer.
    virtual void UnmapBuffer(GPUBufferHandle handle) = 0;

    // Query buffer size.
    virtual u64 GetBufferSize(GPUBufferHandle handle) const = 0;

    // Check if a handle is still valid (not destroyed, correct generation).
    virtual bool IsValid(GPUBufferHandle handle) const = 0;
};

} // namespace Enjin::Renderer
