#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include <vulkan/vulkan.h>
#include <memory>

namespace Enjin {

namespace ECS { class World; }

namespace Renderer {

class VulkanContext;
class VulkanBuffer;

// Configuration for the DDGI probe system
struct DDGIConfig {
    // Probe grid
    i32 probeCountX = 8;       // Probes per axis
    i32 probeCountY = 4;
    i32 probeCountZ = 8;
    f32 gridSpacing = 4.0f;    // World units between probes
    Math::Vector3 gridOrigin = {-16.0f, 0.0f, -16.0f}; // Auto-centered if zero

    // Voxel grid (for SDF tracing)
    i32 voxelResolution = 64;  // 64/128/256
    f32 voxelWorldExtent = 50.0f;

    // Ray tracing
    u32 raysPerProbe = 64;     // Rays per probe per update cycle
    f32 maxTraceDistance = 30.0f;
    u32 amortizationRate = 8;  // Update 1/N probes per frame
    f32 hysteresis = 0.97f;    // Temporal blending (0.97 = slow, smooth)

    // Probe atlas
    u32 octResolution = 8;     // Octahedral map resolution per probe (8x8 texels)

    // Quality scaling
    bool enabled = true;
};

// Software-traced Dynamic Diffuse Global Illumination via SDF ray marching.
//
// Architecture:
//   1. GPU voxelization: mesh geometry → 3D SDF texture (gpu_voxelize.comp)
//   2. Probe update: SDF ray march per probe → irradiance atlas (ddgi_probe_update.comp)
//   3. Probe sampling: screen-space irradiance from trilinear probe interpolation (ddgi_sample.comp)
//
// No hardware ray tracing required — runs on all platforms with compute shaders.
// Scales from 8x4x8 grid at 32 rays (low-end) to 16x8x16 at 128 rays (high-end).
class ENJIN_API DDGIProbeSystem {
public:
    DDGIProbeSystem(VulkanContext* context);
    ~DDGIProbeSystem();

    bool Initialize(const DDGIConfig& config = DDGIConfig{});
    void Shutdown();

    // Per-frame update (called from RenderSystem)
    // 1. Optionally re-voxelize if scene changed
    // 2. Update probe subset via SDF ray march
    // 3. Sample probes into screen-space irradiance texture
    void Update(VkCommandBuffer cmd, ECS::World* world, u32 frameNumber,
                const Math::Vector3& sunDirection, const Math::Vector3& sunColor, f32 sunIntensity,
                const Math::Matrix4& inverseViewProj, u32 screenWidth, u32 screenHeight);

    // Voxelize scene (can be called manually or on timer)
    void Voxelize(VkCommandBuffer cmd, ECS::World* world);

    // Get the screen-space irradiance texture for the PBR shader to sample
    VkImageView GetIrradianceView() const { return m_IrradianceView; }
    VkSampler GetIrradianceSampler() const { return m_IrradianceSampler; }

    // Get probe atlas for debug visualization
    VkImageView GetProbeAtlasView() const { return m_ProbeIrradianceView; }

    // Configuration
    const DDGIConfig& GetConfig() const { return m_Config; }
    void SetEnabled(bool enabled) { m_Config.enabled = enabled; }
    bool IsEnabled() const { return m_Config.enabled; }

    // Stats
    u32 GetTotalProbes() const;
    u32 GetProbesUpdatedThisFrame() const;
    u32 GetVoxelResolution() const { return static_cast<u32>(m_Config.voxelResolution); }

private:
    bool CreateVoxelGrid();
    bool CreateProbeAtlas();
    bool CreateScreenIrradiance(u32 width, u32 height);
    bool CreateComputePipelines();
    bool CreateSampler();
    void DestroyResources();

    VulkanContext* m_Context = nullptr;
    DDGIConfig m_Config;
    u32 m_FrameNumber = 0;

    // Voxel grid (3D R16F image — SDF)
    VkImage m_VoxelImage = VK_NULL_HANDLE;
    VkDeviceMemory m_VoxelMemory = VK_NULL_HANDLE;
    VkImageView m_VoxelView = VK_NULL_HANDLE;
    VkSampler m_VoxelSampler = VK_NULL_HANDLE;

    // Probe irradiance atlas (2D RGBA16F)
    VkImage m_ProbeIrradianceImage = VK_NULL_HANDLE;
    VkDeviceMemory m_ProbeIrradianceMemory = VK_NULL_HANDLE;
    VkImageView m_ProbeIrradianceView = VK_NULL_HANDLE;

    // Probe depth atlas (2D RG16F)
    VkImage m_ProbeDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_ProbeDepthMemory = VK_NULL_HANDLE;
    VkImageView m_ProbeDepthView = VK_NULL_HANDLE;

    // Screen-space irradiance output (2D RGBA16F, screen resolution)
    VkImage m_IrradianceImage = VK_NULL_HANDLE;
    VkDeviceMemory m_IrradianceMemory = VK_NULL_HANDLE;
    VkImageView m_IrradianceView = VK_NULL_HANDLE;
    VkSampler m_IrradianceSampler = VK_NULL_HANDLE;
    u32 m_IrradianceWidth = 0;
    u32 m_IrradianceHeight = 0;

    // Compute pipelines
    VkPipeline m_VoxelizePipeline = VK_NULL_HANDLE;
    VkPipeline m_ProbeUpdatePipeline = VK_NULL_HANDLE;
    VkPipeline m_ProbeSamplePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_VoxelizeLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_ProbeUpdateLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_ProbeSampleLayout = VK_NULL_HANDLE;

    // Descriptor sets and layouts
    VkDescriptorSetLayout m_VoxelizeDescLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ProbeUpdateDescLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_ProbeSampleDescLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescPool = VK_NULL_HANDLE;
    VkDescriptorSet m_VoxelizeDescSet = VK_NULL_HANDLE;
    VkDescriptorSet m_ProbeUpdateDescSet = VK_NULL_HANDLE;
    VkDescriptorSet m_ProbeSampleDescSet = VK_NULL_HANDLE;

    // Uniform buffers
    std::unique_ptr<VulkanBuffer> m_VoxelParamsUBO;
    std::unique_ptr<VulkanBuffer> m_DDGIParamsUBO;
    std::unique_ptr<VulkanBuffer> m_SampleParamsUBO;

    // Scene dirty tracking
    bool m_NeedsRevoxelize = true;
    u32 m_VoxelizeFrameInterval = 30; // Re-voxelize every N frames (dynamic scenes)

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
