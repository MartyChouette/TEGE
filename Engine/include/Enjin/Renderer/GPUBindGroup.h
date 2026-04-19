#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/GPUTypes.h"

namespace Enjin::Renderer {

// Abstract bind group manager — unifies Vulkan descriptor sets and WebGPU bind groups.
// RenderSystem creates layouts once at init, then creates/destroys bind groups per-frame
// or per-entity as needed.
class ENJIN_API IGPUBindGroupManager {
public:
    virtual ~IGPUBindGroupManager() = default;

    // Create a bind group layout describing the slot types and visibility.
    virtual GPUBindGroupLayoutHandle CreateBindGroupLayout(const GPUBindGroupLayoutDesc& desc) = 0;

    // Create a bind group referencing concrete resources (buffers, textures, samplers).
    virtual GPUBindGroupHandle CreateBindGroup(const GPUBindGroupDesc& desc) = 0;

    // Destroy a bind group (returns resources to pool, does not destroy referenced buffers/textures).
    virtual void DestroyBindGroup(GPUBindGroupHandle handle) = 0;

    // Destroy a layout.
    virtual void DestroyBindGroupLayout(GPUBindGroupLayoutHandle handle) = 0;

    // Validity checks.
    virtual bool IsValid(GPUBindGroupHandle handle) const = 0;
    virtual bool IsValidLayout(GPUBindGroupLayoutHandle handle) const = 0;
};

} // namespace Enjin::Renderer
