#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/WebGPU/WebGPUTypes.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

// Vertex buffer layout description (mirrors VulkanPipeline's vertex input)
struct WebGPUVertexAttribute {
    WebGPUVertexFormat format;
    u64 offset;
    u32 shaderLocation;
};

struct WebGPUVertexBufferLayout {
    u64 stride;
    std::vector<WebGPUVertexAttribute> attributes;
    bool perInstance = false;  // false = per-vertex, true = per-instance
};

// Render pipeline configuration
struct WebGPURenderPipelineDesc {
    WGPUShaderModule vertexShader = nullptr;
    WGPUShaderModule fragmentShader = nullptr;
    const char* vertexEntryPoint = "vs_main";
    const char* fragmentEntryPoint = "fs_main";

    std::vector<WebGPUVertexBufferLayout> vertexBuffers;

    WGPUPipelineLayout layout = nullptr;  // null = auto layout

    // Rasterization
    WGPUCullMode cullMode = WGPUCullMode_Back;
    WGPUFrontFace frontFace = WGPUFrontFace_CCW;
    WGPUPrimitiveTopology topology = WGPUPrimitiveTopology_TriangleList;
    bool wireframe = false;

    // Depth/stencil
    bool depthTest = true;
    bool depthWrite = true;
    WGPUCompareFunction depthCompare = WGPUCompareFunction_Less;

    // Color target
    WGPUTextureFormat colorFormat = WGPUTextureFormat_BGRA8Unorm;
    bool blendEnabled = false;

    // Override constants (specialization constants for WebGPU).
    // Passed as WGPUConstantEntry array to fragment shader at pipeline creation.
    // Key = numeric constant ID (matching 'override' declarations in WGSL).
    std::vector<WGPUConstantEntry> fragmentOverrides;

    const char* label = nullptr;
};

// Creates and manages WebGPU render/compute pipelines.
// Mirrors VulkanPipeline's role — pipeline state object creation from shader modules.
class ENJIN_API WebGPUPipeline {
public:
    explicit WebGPUPipeline(WGPUDevice device);
    ~WebGPUPipeline() = default;

    // Create a render pipeline from the given description.
    // Returns the pipeline handle (owned by caller via RAII wrapper).
    WGPURenderPipeline CreateRenderPipeline(const WebGPURenderPipelineDesc& desc);

    // Create bind group layout for the standard descriptor layout:
    //   Group 0: ViewProj UBO (binding 0), Lighting UBO (binding 1)
    //   Group 1: Material UBO (binding 0)
    //   Group 2: Textures (bindings 0-4)
    WGPUBindGroupLayout CreateStandardBindGroupLayout(u32 group);

    // Create a pipeline layout from bind group layouts.
    WGPUPipelineLayout CreatePipelineLayout(const std::vector<WGPUBindGroupLayout>& layouts);

private:
    WGPUDevice m_Device = nullptr;
};

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
