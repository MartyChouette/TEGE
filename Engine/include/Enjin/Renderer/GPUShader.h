#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/GPUTypes.h"
#include <string>

namespace Enjin::Renderer {

// Abstract shader manager — backend implementations load SPIR-V (Vulkan),
// WGSL (WebGPU), or MSL (Metal).
class ENJIN_API IGPUShaderManager {
public:
    virtual ~IGPUShaderManager() = default;

    // Load a shader from raw binary/text data.
    // Vulkan: data is SPIR-V binary (u32 array).
    // WebGPU: data is WGSL source text (null-terminated string).
    // Metal:  data is MSL source text or metallib binary.
    virtual GPUShaderHandle LoadShader(const void* data, u64 size, GPUShaderStage stage,
                                       const char* label = nullptr) = 0;

    // Load a shader from a file path.
    virtual GPUShaderHandle LoadShaderFromFile(const std::string& path, GPUShaderStage stage,
                                                const char* label = nullptr) = 0;

    // Destroy a shader module.
    virtual void DestroyShader(GPUShaderHandle handle) = 0;

    // Check if a handle is valid.
    virtual bool IsValid(GPUShaderHandle handle) const = 0;
};

} // namespace Enjin::Renderer
