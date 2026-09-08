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

namespace ECS { class World; }

namespace Renderer {

class VulkanContext;
class VulkanBuffer;

// Configuration for the DDGI probe system
// Clamp a requested probe grid to something the atlas allocator can actually
// build. Both a slider and a hand-edited scene file are user input, and a zero
// or a wild value here becomes a zero-sized or multi-gigabyte image rather than
// a visible mistake. Ranges match what the editor offers, and the ceilings keep
// the packed atlas inside a comfortable 2048x2048.
//
// Pure and free-standing on purpose: it is the part worth testing, and a test
// must not need a Vulkan device to run it.
ENJIN_API void ClampDDGIGridShape(i32& probeCountX, i32& probeCountY, i32& probeCountZ,
                                  i32& voxelResolution, u32& octResolution);

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

    // --- Input wiring -------------------------------------------------------
    // Two of the three passes have hard inputs the engine does not provide yet.
    // The system stays gated (no dispatches) until these are called; the
    // Settings panel reflects the same state. See Update() for the gate.
    //
    // Voxelize inputs: the packed vertex/index/instance SSBOs (bindings 1-3 of
    // gpu_voxelize.comp) — the merged static-geometry arena, once it is wired.
    void SetGeometryBuffers(VkBuffer vertices, usize vertexBytes,
                            VkBuffer indices, usize indexBytes,
                            VkBuffer instances, usize instanceBytes,
                            u32 triangleCount, u32 instanceCount);
    // Sample-pass inputs: screen depth + world-space normal textures
    // (bindings 1-2 of ddgi_sample.comp).
    void SetGBuffer(VkImageView depthView, VkImageView normalView, VkSampler sampler);
    bool HasGeometryInputs() const { return m_GeometryBound; }
    bool HasGBufferInputs() const { return m_GBufferBound; }

    // Get the screen-space irradiance texture for the PBR shader to sample
    VkImageView GetIrradianceView() const { return m_IrradianceView; }
    VkSampler GetIrradianceSampler() const { return m_IrradianceSampler; }

    // Probe irradiance atlas — sampled directly by the PBR fragment shader
    // (binding 22) for the direct-lookup apply, and for debug visualization.
    VkImageView GetProbeAtlasView() const { return m_ProbeIrradianceView; }
    VkSampler GetProbeAtlasSampler() const { return m_IrradianceSampler; }
    u32 GetProbeAtlasWidth() const { return m_ProbeAtlasWidth; }
    u32 GetProbeAtlasHeight() const { return m_ProbeAtlasHeight; }
    // True once geometry is fed and the probe atlas holds usable data.
    bool IsActive() const { return m_Config.enabled && m_GeometryBound; }

    // Configuration
    const DDGIConfig& GetConfig() const { return m_Config; }
    void SetEnabled(bool enabled) { m_Config.enabled = enabled; }

    // The subset that can be changed with no reallocation. The grid SHAPE
    // (probe counts, oct resolution, voxel resolution) is not here because it
    // sizes the voxel grid and the probe atlas: it goes through
    // RequestGridRebuild instead, which defers the destroy and recreate to a
    // safe point in the frame.
    void SetRuntimeTunables(f32 gridSpacing, const Math::Vector3& gridOrigin,
                            f32 voxelWorldExtent, u32 raysPerProbe,
                            f32 maxTraceDistance, u32 amortizationRate,
                            f32 hysteresis) {
        m_Config.gridSpacing      = gridSpacing;
        m_Config.gridOrigin       = gridOrigin;
        m_Config.voxelWorldExtent = voxelWorldExtent;
        // Both are divisors in the probe update, and a scene file is
        // user-editable text, so a zero has to be absorbed here.
        m_Config.raysPerProbe     = raysPerProbe ? raysPerProbe : 1u;
        m_Config.amortizationRate = amortizationRate ? amortizationRate : 1u;
        m_Config.maxTraceDistance = maxTraceDistance;
        m_Config.hysteresis       = hysteresis;
    }
    bool IsEnabled() const { return m_Config.enabled; }

    // Ask for a new grid shape. Recorded, clamped, and applied later by
    // RebuildGrid -- NEVER applied here, because these values size GPU images
    // that the frames in flight are still reading. A request equal to the
    // current shape is dropped, so holding a slider does not rebuild per frame.
    void RequestGridRebuild(i32 probeCountX, i32 probeCountY, i32 probeCountZ,
                            i32 voxelResolution, u32 octResolution);
    bool HasPendingGridRebuild() const { return m_GridRebuildPending; }

    // Destroy and recreate the voxel grid and probe atlas at the requested
    // shape. Returns true when the resources were actually replaced, which
    // means every descriptor pointing at the old atlas is now dangling and the
    // caller MUST rebind before anything samples it.
    //
    // CALLER CONTRACT: the GPU must be idle. This destroys images that recorded
    // command buffers may still reference, so it belongs in
    // RenderSystem::FlushPendingChanges after a full wait, never mid-frame.
    bool RebuildGrid();

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
    // Authoritative probe-atlas dimensions (set in CreateProbeAtlas)
    u32 m_ProbeAtlasWidth = 0;
    u32 m_ProbeAtlasHeight = 0;

    // Compute pipelines (via helper — creates layout + pipeline + descriptors)
    ComputePipelineSetup m_VoxelizeSetup;
    ComputePipelineSetup m_ProbeUpdateSetup;
    ComputePipelineSetup m_ProbeSampleSetup;
    bool m_PipelinesCreated = false;

    // Uniform buffers
    std::unique_ptr<VulkanBuffer> m_VoxelParamsUBO;
    std::unique_ptr<VulkanBuffer> m_DDGIParamsUBO;
    std::unique_ptr<VulkanBuffer> m_SampleParamsUBO;

    // Scene dirty tracking
    bool m_NeedsRevoxelize = true;
    u32 m_VoxelizeFrameInterval = 30; // Re-voxelize every N frames (dynamic scenes)

    // Input availability (see SetGeometryBuffers / SetGBuffer)
    bool m_GeometryBound = false;
    bool m_GBufferBound = false;
    u32 m_TriangleCount = 0;
    u32 m_InstanceCount = 0;
    bool m_LoggedInactive = false;
    // Image layout lifecycle
    bool m_VoxelGridInitialized = false;   // cleared to "empty = far" once
    bool m_AtlasesInitialized = false;     // probe atlases out of UNDEFINED
    bool m_AtlasesReadable = false;        // atlases left SHADER_READ_ONLY for the PBR pass

    bool m_Initialized = false;

    // Pending grid shape (see RequestGridRebuild). Held rather than applied so
    // the reallocation happens at a point where no frame is reading the atlas.
    bool m_GridRebuildPending = false;
    i32 m_PendingProbeCountX = 0;
    i32 m_PendingProbeCountY = 0;
    i32 m_PendingProbeCountZ = 0;
    i32 m_PendingVoxelResolution = 0;
    u32 m_PendingOctResolution = 0;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
