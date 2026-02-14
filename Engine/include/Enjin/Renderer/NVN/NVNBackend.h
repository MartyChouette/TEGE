#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/RenderBackend.h"
#include "Enjin/Logging/Log.h"
#include <string>

/**
 * @file NVNBackend.h
 * @brief Nintendo Switch NVN graphics API backend stub
 *
 * The actual NVN SDK is under NDA and not distributed with Enjin.
 * This file provides the interface and stub implementations that log
 * "NVN not available" for each call. When the NVN SDK is available,
 * replace the stub bodies with real NVN API calls.
 *
 * Everything is guarded by ENJIN_PLATFORM_SWITCH.
 */

namespace Enjin {
namespace Renderer {

// NVN hardware capabilities for Nintendo Switch (Tegra X1)
struct NVNCapabilities {
    u32 maxTextureSize = 4096;
    u32 maxVertexBuffers = 16;
    u32 maxUniformBufferSize = 65536;
    u32 maxStorageBufferSize = 134217728;  // 128 MB
    u32 maxColorAttachments = 8;
    u32 maxComputeWorkGroupSize = 1024;
    u32 maxComputeWorkGroupCount = 65535;
    u32 shaderModelMajor = 5;
    u32 shaderModelMinor = 3;

    // Switch 1 hardware specs (NVIDIA Tegra X1)
    u32 gpuCoreCount = 256;         // Maxwell GPU cores
    u32 systemRAMMB = 4096;         // 4 GB total system RAM
    u32 vramDedicatedMB = 0;        // Unified memory (no dedicated VRAM)
    u32 maxResolutionHandheld = 720;
    u32 maxResolutionDocked = 1080;
    f32 gpuClockHandheldMHz = 307.2f;
    f32 gpuClockDockedMHz = 768.0f;
    f32 cpuClockHandheldMHz = 1020.0f;
    f32 cpuClockDockedMHz = 1020.0f;

    bool supportsASTCCompression = true;
    bool supportsComputeShaders = true;
    bool supportsTessellation = false;   // Limited on Tegra X1
    bool supportsGeometryShaders = true;
    bool supportsMultiDrawIndirect = true;
    bool supportsConditionalRendering = false;
};

// Memory budget recommendations for Switch
struct NVNMemoryBudget {
    u64 totalAvailableBytes = 0;
    u64 gameHeapBytes = 0;           // General game allocations
    u64 graphicsHeapBytes = 0;       // GPU resources (textures, buffers)
    u64 audioHeapBytes = 0;          // Audio buffers
    u64 systemReservedBytes = 0;     // OS + system services
};

#ifdef ENJIN_PLATFORM_SWITCH

/**
 * @brief NVN render backend for Nintendo Switch
 *
 * Implements IRenderBackend using the NVN graphics API.
 * Currently a stub -- all methods log warnings and return failure.
 */
class ENJIN_API NVNBackend : public IRenderBackend {
public:
    NVNBackend();
    ~NVNBackend() override;

    // --- IRenderBackend interface ---
    bool Initialize(u32 width, u32 height) override;
    void Shutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;
    void Resize(u32 width, u32 height) override;

    const char* GetBackendName() const override;
    PlatformCapabilities GetCapabilities() const override;

    // --- NVN-specific ---
    NVNCapabilities GetNVNCapabilities() const;

    /**
     * @brief Get recommended memory budget based on current performance mode
     * @return Conservative memory limits for handheld or docked mode
     */
    NVNMemoryBudget GetRecommendedMemoryBudget() const;

    /**
     * @brief Check if we are currently in docked mode
     * @return true if docked, false if handheld
     */
    bool IsDockedMode() const;

    /**
     * @brief Create an NVN texture resource (stub)
     * @param width Texture width
     * @param height Texture height
     * @param format Texture format identifier
     * @return Handle or 0 on failure
     */
    u64 CreateTexture(u32 width, u32 height, u32 format);

    /**
     * @brief Destroy an NVN texture resource (stub)
     * @param handle Texture handle returned from CreateTexture
     */
    void DestroyTexture(u64 handle);

    /**
     * @brief Create an NVN vertex buffer (stub)
     * @param sizeBytes Buffer size in bytes
     * @param data Initial data pointer (may be nullptr)
     * @return Handle or 0 on failure
     */
    u64 CreateVertexBuffer(u64 sizeBytes, const void* data);

    /**
     * @brief Destroy an NVN vertex buffer (stub)
     */
    void DestroyVertexBuffer(u64 handle);

    /**
     * @brief Create an NVN shader program (stub)
     * @param vertexData Compiled NVN vertex shader bytecode
     * @param vertexSize Size in bytes
     * @param fragmentData Compiled NVN fragment shader bytecode
     * @param fragmentSize Size in bytes
     * @return Handle or 0 on failure
     */
    u64 CreateShaderProgram(const void* vertexData, u64 vertexSize,
                            const void* fragmentData, u64 fragmentSize);

    /**
     * @brief Destroy an NVN shader program (stub)
     */
    void DestroyShaderProgram(u64 handle);

    /**
     * @brief Submit a draw call (stub)
     * @param vertexCount Number of vertices
     * @param instanceCount Number of instances
     */
    void Draw(u32 vertexCount, u32 instanceCount);

    /**
     * @brief Submit an indexed draw call (stub)
     * @param indexCount Number of indices
     * @param instanceCount Number of instances
     */
    void DrawIndexed(u32 indexCount, u32 instanceCount);

private:
    bool m_Initialized = false;
    bool m_DockedMode = false;
    u32 m_Width = 0;
    u32 m_Height = 0;
};

#endif // ENJIN_PLATFORM_SWITCH

} // namespace Renderer
} // namespace Enjin
