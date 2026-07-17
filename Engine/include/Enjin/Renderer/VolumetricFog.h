#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Renderer/ComputePipelineHelper.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace Enjin {
namespace Renderer {

class VulkanContext;
class VulkanBuffer;
class ClusteredLightingSystem;

// Configuration for volumetric fog
struct VolumetricFogConfig {
    // Froxel grid dimensions (match clustered lighting by default)
    u32 froxelCountX = 160;     // Higher res than cluster grid for smoother fog
    u32 froxelCountY = 90;
    u32 froxelCountZ = 64;

    // Fog properties
    Math::Vector3 fogAlbedo = {0.9f, 0.9f, 1.0f}; // Slight blue tint
    f32 fogDensity = 0.02f;
    f32 fogHeightFalloff = 0.1f;    // Exponential height decay
    f32 fogBaseHeight = 0.0f;        // Height of maximum density
    f32 fogAnisotropy = 0.3f;        // Henyey-Greenstein g parameter

    // Temporal reprojection
    f32 temporalBlend = 0.9f;        // 0.9 = heavy reuse, 0.0 = no history

    // Noise
    f32 noiseScale = 0.1f;
    f32 noiseStrength = 0.3f;
    f32 windSpeedX = 1.0f;
    f32 windSpeedZ = 0.5f;

    // Quality scaling
    bool enabled = true;
};

// Volumetric fog system — froxel-based participating media.
//
// Pipeline:
//   1. Compute pass: per-froxel density + in-scattered light from clustered lights
//   2. PBR shader: samples froxel volume along view ray for fog contribution
//
// Integrates with:
//   - ClusteredLightingSystem (reuses light grid for per-froxel light iteration)
//   - ShadowAtlas (shadow-tested light scattering)
//   - DDGIProbeSystem (indirect fog illumination)
//   - OITManager (composites with transparent objects)
class ENJIN_API VolumetricFogSystem {
public:
    VolumetricFogSystem(VulkanContext* context);
    ~VolumetricFogSystem();

    bool Initialize(const VolumetricFogConfig& config = VolumetricFogConfig{});
    void Shutdown();

    // Per-frame update — dispatches compute shader to fill froxel volume
    void Update(VkCommandBuffer cmd, f32 time,
                const Math::Matrix4& inverseViewProj,
                const Math::Matrix4& prevViewProj,
                const Math::Matrix4& viewMatrix,
                const Math::Vector3& cameraPos,
                f32 nearPlane, f32 farPlane,
                u32 screenWidth, u32 screenHeight,
                const Math::Vector3& sunDirection,
                const Math::Vector3& sunColor, f32 sunIntensity);

    // Swap froxel volumes for temporal reprojection (call at end of frame)
    void SwapVolumes();

    // Shader resources for PBR fragment shader
    VkImageView GetFroxelVolumeView() const { return m_FroxelViews[m_CurrentVolume]; }
    VkSampler GetFroxelSampler() const { return m_FroxelSampler; }

    // Configuration
    VolumetricFogConfig& GetConfig() { return m_Config; }
    const VolumetricFogConfig& GetConfig() const { return m_Config; }
    void SetEnabled(bool enabled) { m_Config.enabled = enabled; }
    bool IsEnabled() const { return m_Config.enabled && m_Initialized; }

    // Connect to clustered lighting for light data access
    void SetClusteredLighting(ClusteredLightingSystem* cls) { m_ClusteredLighting = cls; }

private:
    bool CreateFroxelVolumes();
    bool CreateComputePipeline();
    bool CreateSampler();
    void DestroyResources();

    VulkanContext* m_Context = nullptr;
    VolumetricFogConfig m_Config;
    ClusteredLightingSystem* m_ClusteredLighting = nullptr;

    // Double-buffered froxel volumes for temporal reprojection
    static constexpr u32 VOLUME_COUNT = 2;
    u32 m_CurrentVolume = 0;
    VkImage m_FroxelImages[VOLUME_COUNT] = {};
    VkDeviceMemory m_FroxelMemory[VOLUME_COUNT] = {};
    VkImageView m_FroxelViews[VOLUME_COUNT] = {};
    VkSampler m_FroxelSampler = VK_NULL_HANDLE;

    // Compute pipeline (via helper)
    ComputePipelineSetup m_FogSetup;
    bool m_PipelineCreated = false;
    bool m_VolumesInitialized = false;   // first-frame layout transitions + history clear done
    bool m_ClusterBuffersBound = false;  // bindings 2-4 written (clustered SSBOs exist lazily)
    bool m_DisabledCleared = false;      // one-shot neutral clear after a runtime disable

    // Uniform buffer
    std::unique_ptr<VulkanBuffer> m_ParamsUBO;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
