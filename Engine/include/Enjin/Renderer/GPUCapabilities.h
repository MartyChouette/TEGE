#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/GPUTypes.h"

namespace Enjin::Renderer {

// GPU feature capabilities — queried at init, used for runtime feature gating.
// RenderSystem checks these before using advanced features so the same code
// path works on Vulkan (full features) and WebGPU (subset).
struct GPUCapabilities {
    // --- Feature support ---
    bool hasPushConstants = false;
    bool hasAsyncCompute = false;
    bool hasRayTracing = false;
    bool hasBindlessResources = false;
    bool hasMultiDrawIndirect = false;
    bool hasMultiDrawIndirectCount = false;
    bool hasDeviceGeneratedCommands = false;
    bool hasComputeShaders = false;
    bool hasMeshShaders = false;
    bool hasVariableRateShading = false;
    bool hasConservativeRasterization = false;
    bool hasDepthClamp = false;
    bool hasWideLines = false;
    bool hasFillModeNonSolid = false;
    bool hasTimestampQueries = false;
    bool hasOcclusionQueries = false;

    // --- Limits ---
    u32 maxPushConstantSize = 0;       // 128 on Vulkan, 0 on WebGPU
    u32 maxBindGroups = 4;             // 4 on both
    u32 maxTextureSize = 4096;
    u32 maxStorageBufferSize = 0;
    u32 maxUniformBufferSize = 65536;
    u32 maxVertexAttributes = 16;
    u32 maxColorAttachments = 4;
    u32 maxMSAASamples = 1;
    u32 maxComputeWorkgroupSize = 0;

    // --- Formats ---
    GPUTextureFormat preferredSwapchainFormat = GPUTextureFormat::BGRA8Srgb;
    GPUTextureFormat preferredDepthFormat = GPUTextureFormat::Depth24PlusStencil8;

    // Convenience helpers
    bool CanDoShadows() const { return true; }  // Both backends can do depth passes
    bool CanDoPostProcess() const { return true; }  // Both can render to texture
    bool CanDoSkinning() const { return hasComputeShaders || true; }  // Vertex skinning works everywhere
    bool CanDoGPUCulling() const { return hasComputeShaders && hasMultiDrawIndirect; }
};

// Populate capabilities from Vulkan device properties
inline GPUCapabilities MakeVulkanCapabilities() {
    GPUCapabilities caps;
    caps.hasPushConstants = true;
    caps.hasAsyncCompute = true;  // Checked per-device at init
    caps.hasRayTracing = false;   // Checked per-device at init
    caps.hasBindlessResources = true;
    caps.hasMultiDrawIndirect = true;
    caps.hasMultiDrawIndirectCount = true;
    caps.hasDeviceGeneratedCommands = false;  // Extension check
    caps.hasComputeShaders = true;
    caps.hasMeshShaders = false;   // Extension check
    caps.hasVariableRateShading = false;  // Extension check
    caps.hasDepthClamp = true;
    caps.hasWideLines = true;
    caps.hasFillModeNonSolid = true;
    caps.hasTimestampQueries = true;
    caps.hasOcclusionQueries = true;
    caps.maxPushConstantSize = 128;
    caps.maxBindGroups = 4;
    caps.maxTextureSize = 16384;
    caps.maxStorageBufferSize = 128 * 1024 * 1024;
    caps.maxUniformBufferSize = 65536;
    caps.maxVertexAttributes = 32;
    caps.maxColorAttachments = 8;
    caps.maxMSAASamples = 8;
    caps.maxComputeWorkgroupSize = 1024;
    caps.preferredSwapchainFormat = GPUTextureFormat::BGRA8Srgb;
    caps.preferredDepthFormat = GPUTextureFormat::Depth24PlusStencil8;
    return caps;
}

inline GPUCapabilities MakeWebGPUCapabilities() {
    GPUCapabilities caps;
    caps.hasPushConstants = false;
    caps.hasAsyncCompute = false;
    caps.hasRayTracing = false;
    caps.hasBindlessResources = false;
    caps.hasMultiDrawIndirect = false;
    caps.hasMultiDrawIndirectCount = false;
    caps.hasDeviceGeneratedCommands = false;
    caps.hasComputeShaders = true;  // WebGPU has compute shaders
    caps.hasMeshShaders = false;
    caps.hasVariableRateShading = false;
    caps.hasDepthClamp = false;
    caps.hasWideLines = false;
    caps.hasFillModeNonSolid = false;
    caps.hasTimestampQueries = false;
    caps.hasOcclusionQueries = false;
    caps.maxPushConstantSize = 0;
    caps.maxBindGroups = 4;
    caps.maxTextureSize = 8192;
    caps.maxStorageBufferSize = 128 * 1024 * 1024;
    caps.maxUniformBufferSize = 65536;
    caps.maxVertexAttributes = 16;
    caps.maxColorAttachments = 4;
    caps.maxMSAASamples = 4;
    caps.maxComputeWorkgroupSize = 256;
    caps.preferredSwapchainFormat = GPUTextureFormat::BGRA8Unorm;
    caps.preferredDepthFormat = GPUTextureFormat::Depth24PlusStencil8;
    return caps;
}

inline GPUCapabilities MakeMetalCapabilities() {
    GPUCapabilities caps;
    caps.hasPushConstants = false;  // Metal uses setBytes (similar, mapped via inline constants)
    caps.hasAsyncCompute = true;
    caps.hasRayTracing = true;     // Metal has ray tracing via MPSRayIntersector
    caps.hasBindlessResources = true;  // Argument buffers
    caps.hasMultiDrawIndirect = true;
    caps.hasMultiDrawIndirectCount = false;
    caps.hasDeviceGeneratedCommands = true;  // Indirect command buffers
    caps.hasComputeShaders = true;
    caps.hasMeshShaders = true;    // Metal 3 mesh shaders (Apple Silicon)
    caps.hasVariableRateShading = true;  // Metal 2.4+
    caps.hasDepthClamp = true;
    caps.hasWideLines = false;
    caps.hasFillModeNonSolid = true;
    caps.hasTimestampQueries = true;
    caps.hasOcclusionQueries = true;
    caps.maxPushConstantSize = 0;  // Use inline constants (4KB via setBytes)
    caps.maxBindGroups = 4;
    caps.maxTextureSize = 16384;
    caps.maxStorageBufferSize = 256 * 1024 * 1024;
    caps.maxUniformBufferSize = 65536;
    caps.maxVertexAttributes = 31;
    caps.maxColorAttachments = 8;
    caps.maxMSAASamples = 8;
    caps.maxComputeWorkgroupSize = 1024;
    caps.preferredSwapchainFormat = GPUTextureFormat::BGRA8Srgb;
    caps.preferredDepthFormat = GPUTextureFormat::Depth32Float;
    return caps;
}

} // namespace Enjin::Renderer
