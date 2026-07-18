#include "Enjin/Renderer/DDGIProbeSystem.h"
#include "Enjin/Renderer/ComputePipelineHelper.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Logging/Log.h"

#if !ENJIN_RENDERER_WEBGPU

namespace Enjin {
namespace Renderer {

DDGIProbeSystem::DDGIProbeSystem(VulkanContext* context)
    : m_Context(context) {}

DDGIProbeSystem::~DDGIProbeSystem() {
    Shutdown();
}

bool DDGIProbeSystem::Initialize(const DDGIConfig& config) {
    if (m_Initialized) return true;
    m_Config = config;

    // Resources are created regardless of the enabled flag so the runtime
    // Settings toggle works (Update gates dispatch on enabled + geometry). The
    // probe atlas is bound to the main PBR pass for the direct fragment-shader
    // apply, so it must exist even when DDGI starts off.
    if (!CreateVoxelGrid()) return false;
    if (!CreateProbeAtlas()) return false;
    if (!CreateSampler()) return false;
    // Compute pipelines are created lazily on first Update() to allow
    // shader hot-reload and deferred descriptor pool creation.

    m_Initialized = true;

    ENJIN_LOG_INFO(Renderer, "DDGI initialized: %dx%dx%d probes (spacing %.1f), "
                   "voxel grid %d^3 (extent %.1f), %u rays/probe, amort 1/%u",
                   m_Config.probeCountX, m_Config.probeCountY, m_Config.probeCountZ,
                   m_Config.gridSpacing, m_Config.voxelResolution, m_Config.voxelWorldExtent,
                   m_Config.raysPerProbe, m_Config.amortizationRate);
    return true;
}

void DDGIProbeSystem::Shutdown() {
    if (!m_Initialized) return;
    DestroyResources();
    m_Initialized = false;
}

u32 DDGIProbeSystem::GetTotalProbes() const {
    return static_cast<u32>(m_Config.probeCountX) *
           static_cast<u32>(m_Config.probeCountY) *
           static_cast<u32>(m_Config.probeCountZ);
}

u32 DDGIProbeSystem::GetProbesUpdatedThisFrame() const {
    return GetTotalProbes() / m_Config.amortizationRate;
}

// std140 mirrors of the three shader parameter blocks. vec3/ivec3 align to 16;
// the scalar following each packs into the fourth lane.
struct VoxelParamsUBO {                    // gpu_voxelize.comp binding 4
    Math::Vector3 gridOrigin;  f32 voxelSize;          // 0..15
    i32 gridResolution[3];     u32 triangleCount;      // 16..31
    u32 instanceCount;         u32 _pad0[3];           // 32..47
};
static_assert(sizeof(VoxelParamsUBO) == 48, "must match gpu_voxelize.comp VoxelParams");

struct DDGIParamsUBO {                     // ddgi_probe_update.comp binding 3
    Math::Vector3 gridOrigin;      f32 gridSpacing;        // 0..15
    i32 probeCount[3];             u32 raysPerProbe;       // 16..31
    Math::Vector3 voxelGridOrigin; f32 voxelSize;          // 32..47
    i32 voxelResolution[3];        u32 frameNumber;        // 48..63
    Math::Vector3 sunDirection;    f32 sunIntensity;       // 64..79
    Math::Vector3 sunColor;        f32 maxTraceDistance;   // 80..95
    u32 probeAtlasWidth; u32 octResolution; u32 amortizationRate; f32 hysteresis; // 96..111
};
static_assert(sizeof(DDGIParamsUBO) == 112, "must match ddgi_probe_update.comp DDGIParams");

struct DDGISampleParamsUBO {               // ddgi_sample.comp binding 5
    Math::Matrix4 inverseViewProj;                          // 0..63
    Math::Vector3 gridOrigin;      f32 gridSpacing;        // 64..79
    i32 probeCount[3];             u32 octResolution;      // 80..95
    u32 probeAtlasWidth; u32 probeAtlasHeight; f32 normalBias; f32 viewBias; // 96..111
    f32 screenSize[2];             f32 _pad0[2];           // 112..127
};
static_assert(sizeof(DDGISampleParamsUBO) == 128, "must match ddgi_sample.comp DDGISampleParams");

void DDGIProbeSystem::SetGeometryBuffers(VkBuffer vertices, usize vertexBytes,
                                          VkBuffer indices, usize indexBytes,
                                          VkBuffer instances, usize instanceBytes,
                                          u32 triangleCount, u32 instanceCount) {
    if (!vertices || !indices || !instances || triangleCount == 0) return;
    if (!m_PipelinesCreated) { CreateComputePipelines(); m_PipelinesCreated = true; }
    if (!m_VoxelizeSetup.IsValid()) return;
    VkDevice device = m_Context->GetDevice();
    m_VoxelizeSetup.WriteBuffer(device, 1, vertices, vertexBytes);
    m_VoxelizeSetup.WriteBuffer(device, 2, indices, indexBytes);
    m_VoxelizeSetup.WriteBuffer(device, 3, instances, instanceBytes);
    m_TriangleCount = triangleCount;
    m_InstanceCount = instanceCount;
    m_GeometryBound = true;
    m_NeedsRevoxelize = true;
}

void DDGIProbeSystem::SetGBuffer(VkImageView depthView, VkImageView normalView, VkSampler sampler) {
    if (!depthView || !normalView || !sampler) return;
    if (!m_PipelinesCreated) { CreateComputePipelines(); m_PipelinesCreated = true; }
    if (!m_ProbeSampleSetup.IsValid()) return;
    VkDevice device = m_Context->GetDevice();
    m_ProbeSampleSetup.WriteImage(device, 1, depthView, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_ProbeSampleSetup.WriteImage(device, 2, normalView, sampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_GBufferBound = true;
}

void DDGIProbeSystem::Update(VkCommandBuffer cmd, ECS::World* world, u32 frameNumber,
                              const Math::Vector3& sunDirection, const Math::Vector3& sunColor,
                              f32 sunIntensity, const Math::Matrix4& inverseViewProj,
                              u32 screenWidth, u32 screenHeight) {
    if (!m_Config.enabled || !m_Initialized) return;
    m_FrameNumber = frameNumber;

    // HONESTY GATE. Two of the three passes have hard inputs nothing in the
    // engine provides yet: gpu_voxelize.comp statically reads vertex/index/
    // instance SSBOs (the merged-geometry arena, unwired), and ddgi_sample.comp
    // statically reads G-buffer depth + world-space normals (the forward
    // renderer produces neither). Dispatching with those descriptors unwritten
    // is invalid, and dispatching probe updates against a voxel grid that was
    // never populated just burns GPU. Until SetGeometryBuffers() is called the
    // system idles and says so once — no dead-slider theater.
    if (!m_GeometryBound) {
        if (!m_LoggedInactive) {
            m_LoggedInactive = true;
            ENJIN_LOG_WARN(Renderer,
                "DDGI: idle — no static geometry fed yet (needs pool-eligible 3D meshes via "
                "SetGeometryBuffers). Voxelize/probe plumbing is ready; the passes run once the "
                "scene has geometry.");
        }
        return;
    }
    (void)screenWidth; (void)screenHeight;   // screen-space sample pass removed

    // Lazy pipeline creation (deferred so resources are ready)
    if (!m_PipelinesCreated) {
        CreateComputePipelines();
        m_PipelinesCreated = true;
    }

    // First use: probe atlases out of UNDEFINED into GENERAL (storage images
    // in the update pass; sampled by the sample pass through per-pass barriers).
    if (!m_AtlasesInitialized) {
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        VkImage atlases[2] = { m_ProbeIrradianceImage, m_ProbeDepthImage };
        for (VkImage img : atlases) {
            if (!img) continue;
            VkImageMemoryBarrier b{};
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = img;
            b.subresourceRange = range;
            b.srcAccessMask = 0;
            b.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
        }
        m_AtlasesInitialized = true;
    }

    // Step 1: Re-voxelize scene periodically
    if (m_NeedsRevoxelize || (frameNumber % m_VoxelizeFrameInterval == 0)) {
        Voxelize(cmd, world);
        m_NeedsRevoxelize = false;
    }

    // Step 2: Update probe subset via SDF ray march
    if (m_ProbeUpdateSetup.IsValid() && m_DDGIParamsUBO) {
        DDGIParamsUBO params{};
        params.gridOrigin       = m_Config.gridOrigin;
        params.gridSpacing      = m_Config.gridSpacing;
        params.probeCount[0]    = m_Config.probeCountX;
        params.probeCount[1]    = m_Config.probeCountY;
        params.probeCount[2]    = m_Config.probeCountZ;
        params.raysPerProbe     = m_Config.raysPerProbe;
        f32 half = m_Config.voxelWorldExtent * 0.5f;
        params.voxelGridOrigin  = Math::Vector3(-half, -half, -half);
        params.voxelSize        = m_Config.voxelWorldExtent / static_cast<f32>(m_Config.voxelResolution);
        params.voxelResolution[0] = m_Config.voxelResolution;
        params.voxelResolution[1] = m_Config.voxelResolution;
        params.voxelResolution[2] = m_Config.voxelResolution;
        params.frameNumber      = frameNumber;
        params.sunDirection     = sunDirection;
        params.sunIntensity     = sunIntensity;
        params.sunColor         = sunColor;
        params.maxTraceDistance = m_Config.maxTraceDistance;
        params.probeAtlasWidth  = m_ProbeAtlasWidth;   // authoritative (see CreateProbeAtlas)
        params.octResolution    = m_Config.octResolution;
        params.amortizationRate = m_Config.amortizationRate;
        params.hysteresis       = m_Config.hysteresis;
        m_DDGIParamsUBO->UploadData(&params, sizeof(params));

        // Atlases must be GENERAL for the storage write. Between frames they are
        // left SHADER_READ_ONLY for the PBR pass, so flip them back first.
        auto atlasBarrier = [&](VkImageLayout oldL, VkImageLayout newL,
                                VkAccessFlags srcA, VkAccessFlags dstA,
                                VkPipelineStageFlags srcS, VkPipelineStageFlags dstS) {
            VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            VkImage atlases[2] = { m_ProbeIrradianceImage, m_ProbeDepthImage };
            for (VkImage img : atlases) {
                if (!img) continue;
                VkImageMemoryBarrier b{};
                b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                b.oldLayout = oldL; b.newLayout = newL;
                b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                b.image = img; b.subresourceRange = range;
                b.srcAccessMask = srcA; b.dstAccessMask = dstA;
                vkCmdPipelineBarrier(cmd, srcS, dstS, 0, 0, nullptr, 0, nullptr, 1, &b);
            }
        };
        if (m_AtlasesReadable) {
            atlasBarrier(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                         VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            m_AtlasesReadable = false;
        }

        u32 totalProbes = GetTotalProbes();
        u32 totalWork = totalProbes * m_Config.raysPerProbe;
        u32 workgroups = (totalWork + 63) / 64;
        m_ProbeUpdateSetup.Dispatch(cmd, workgroups);

        // Step 3 (screen-space sample pass) is GONE: the PBR fragment shader
        // samples the probe atlas directly (world pos + normal). So the only
        // post-work is transitioning the atlas GENERAL -> SHADER_READ_ONLY for
        // the main pass to sample this frame.
        atlasBarrier(VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        m_AtlasesReadable = true;
    }
    (void)inverseViewProj; (void)screenWidth; (void)screenHeight;  // sample pass removed
}

void DDGIProbeSystem::Voxelize(VkCommandBuffer cmd, ECS::World* world) {
    (void)world; // Instance data arrives through SetGeometryBuffers

    if (!m_VoxelizeSetup.IsValid() || !m_GeometryBound || !m_VoxelParamsUBO) return;

    VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    // Clear voxel grid to "empty = far away" before writing distances, then
    // put it in GENERAL for the voxelize storage writes. On the first frame
    // the image is UNDEFINED; afterwards it is SHADER_READ_ONLY (probe pass).
    VkImageMemoryBarrier toClear{};
    toClear.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toClear.oldLayout = m_VoxelGridInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                               : VK_IMAGE_LAYOUT_UNDEFINED;
    toClear.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toClear.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toClear.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toClear.image = m_VoxelImage;
    toClear.subresourceRange = range;
    toClear.srcAccessMask = m_VoxelGridInitialized ? VK_ACCESS_SHADER_READ_BIT : 0;
    toClear.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
                         m_VoxelGridInitialized ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                                                : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toClear);
    VkClearColorValue farAway{};
    farAway.float32[0] = m_Config.maxTraceDistance;   // R16F SDF: empty cell = max distance
    vkCmdClearColorImage(cmd, m_VoxelImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &farAway, 1, &range);
    VkImageMemoryBarrier toWrite = toClear;
    toWrite.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toWrite.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toWrite.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toWrite.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toWrite);
    m_VoxelGridInitialized = true;

    // Upload voxelization params with the REAL triangle count from the bound
    // geometry (was a hardcoded 100000 estimate).
    VoxelParamsUBO params{};
    f32 half = m_Config.voxelWorldExtent * 0.5f;
    params.gridOrigin        = Math::Vector3(-half, -half, -half);
    params.voxelSize         = m_Config.voxelWorldExtent / static_cast<f32>(m_Config.voxelResolution);
    params.gridResolution[0] = m_Config.voxelResolution;
    params.gridResolution[1] = m_Config.voxelResolution;
    params.gridResolution[2] = m_Config.voxelResolution;
    params.triangleCount     = m_TriangleCount;
    params.instanceCount     = m_InstanceCount;
    m_VoxelParamsUBO->UploadData(&params, sizeof(params));

    u32 workgroups = (m_TriangleCount + 63) / 64;
    m_VoxelizeSetup.Dispatch(cmd, workgroups);

    // Voxel grid: storage-write -> sampled by the probe update pass.
    VkImageMemoryBarrier toRead = toClear;
    toRead.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toRead.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toRead);

    ENJIN_LOG_INFO(Renderer, "DDGI: voxelized %u triangles / %u instances (%u workgroups)",
                   m_TriangleCount, m_InstanceCount, workgroups);
}

// --- Resource creation ---

bool DDGIProbeSystem::CreateVoxelGrid() {
    VkDevice device = m_Context->GetDevice();
    u32 res = static_cast<u32>(m_Config.voxelResolution);

    // 3D R16F image for SDF
    VkImageCreateInfo imageCI{};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_3D;
    imageCI.extent = { res, res, res };
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.format = VK_FORMAT_R16_SFLOAT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageCI, nullptr, &m_VoxelImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DDGI: failed to create voxel 3D image");
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_VoxelImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_VoxelMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DDGI: failed to allocate voxel memory");
        return false;
    }
    vkBindImageMemory(device, m_VoxelImage, m_VoxelMemory, 0);

    VkImageViewCreateInfo viewCI{};
    viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image = m_VoxelImage;
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_3D;
    viewCI.format = VK_FORMAT_R16_SFLOAT;
    viewCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(device, &viewCI, nullptr, &m_VoxelView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DDGI: failed to create voxel image view");
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "DDGI: created %ux%ux%u voxel SDF (%.1f MB)",
                   res, res, res, static_cast<f32>(memReqs.size) / (1024.0f * 1024.0f));
    return true;
}

bool DDGIProbeSystem::CreateProbeAtlas() {
    VkDevice device = m_Context->GetDevice();

    u32 totalProbes = GetTotalProbes();
    u32 oct = m_Config.octResolution;
    // Pack probes in a square-ish atlas
    u32 probesPerRow = static_cast<u32>(std::ceil(std::sqrt(static_cast<f32>(totalProbes))));
    u32 atlasWidth = probesPerRow * oct;
    u32 atlasHeight = ((totalProbes + probesPerRow - 1) / probesPerRow) * oct;
    // Authoritative dimensions — the probe-update write, the params UBO, and the
    // PBR fragment lookup all derive probesPerRow = width/oct, so this MUST be
    // the single source (probeCountX*probeCountZ*oct was wrong: 512 vs 128).
    m_ProbeAtlasWidth = atlasWidth;
    m_ProbeAtlasHeight = atlasHeight;

    // Irradiance atlas (RGBA16F)
    auto createAtlasImage = [&](VkFormat format, VkImage& image, VkDeviceMemory& memory,
                                 VkImageView& view, const char* name) -> bool {
        VkImageCreateInfo imageCI{};
        imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCI.imageType = VK_IMAGE_TYPE_2D;
        imageCI.extent = { atlasWidth, atlasHeight, 1 };
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;
        imageCI.format = format;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;

        if (vkCreateImage(device, &imageCI, nullptr, &image) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "DDGI: failed to create %s atlas", name);
            return false;
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, image, &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
            memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "DDGI: failed to allocate %s atlas memory", name);
            return false;
        }
        vkBindImageMemory(device, image, memory, 0);

        VkImageViewCreateInfo viewCI{};
        viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image = image;
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.format = format;
        viewCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        if (vkCreateImageView(device, &viewCI, nullptr, &view) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "DDGI: failed to create %s atlas view", name);
            return false;
        }

        ENJIN_LOG_INFO(Renderer, "DDGI: created %s atlas %ux%u (%u probes, %ux%u oct)",
                       name, atlasWidth, atlasHeight, totalProbes, oct, oct);
        return true;
    };

    if (!createAtlasImage(VK_FORMAT_R16G16B16A16_SFLOAT,
                           m_ProbeIrradianceImage, m_ProbeIrradianceMemory,
                           m_ProbeIrradianceView, "irradiance")) return false;

    if (!createAtlasImage(VK_FORMAT_R16G16_SFLOAT,
                           m_ProbeDepthImage, m_ProbeDepthMemory,
                           m_ProbeDepthView, "depth")) return false;

    return true;
}

bool DDGIProbeSystem::CreateScreenIrradiance(u32 width, u32 height) {
    VkDevice device = m_Context->GetDevice();

    VkImageCreateInfo imageCI{};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.extent = { width, height, 1 };
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageCI, nullptr, &m_IrradianceImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DDGI: failed to create screen irradiance image");
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_IrradianceImage, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_IrradianceMemory) != VK_SUCCESS) return false;
    vkBindImageMemory(device, m_IrradianceImage, m_IrradianceMemory, 0);

    VkImageViewCreateInfo viewCI{};
    viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image = m_IrradianceImage;
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    viewCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device, &viewCI, nullptr, &m_IrradianceView) != VK_SUCCESS) return false;

    m_IrradianceWidth = width;
    m_IrradianceHeight = height;
    return true;
}

bool DDGIProbeSystem::CreateComputePipelines() {
    VkDevice device = m_Context->GetDevice();
    using BT = BindType;

    // gpu_voxelize.comp: binding 0=storageImage(voxelGrid), 1=SSBO(vertices),
    //   2=SSBO(indices), 3=SSBO(instances), 4=UBO(params)
    m_VoxelizeSetup.Create(m_Context, {
        {0, BT::StorageImage}, {1, BT::StorageBuffer}, {2, BT::StorageBuffer},
        {3, BT::StorageBuffer}, {4, BT::UniformBuffer}
    }, "gpu_voxelize.comp");

    if (m_VoxelizeSetup.IsValid() && m_VoxelView) {
        m_VoxelizeSetup.WriteStorageImage(device, 0, m_VoxelView);
        m_VoxelParamsUBO = std::make_unique<VulkanBuffer>(m_Context);
        if (m_VoxelParamsUBO->Create(sizeof(VoxelParamsUBO),
                                     static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
                                     /*hostVisible=*/true)) {
            m_VoxelizeSetup.WriteBuffer(device, 4, m_VoxelParamsUBO->GetBuffer(), sizeof(VoxelParamsUBO),
                                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        } else {
            m_VoxelParamsUBO.reset();
        }
        // Geometry SSBOs (bindings 1-3) are written by SetGeometryBuffers.
    }

    // ddgi_probe_update.comp: binding 0=storageImage(irradiance), 1=storageImage(depth),
    //   2=combinedImageSampler(voxelSDF), 3=UBO(params)
    m_ProbeUpdateSetup.Create(m_Context, {
        {0, BT::StorageImage}, {1, BT::StorageImage},
        {2, BT::CombinedImageSampler}, {3, BT::UniformBuffer}
    }, "ddgi_probe_update.comp");

    if (m_ProbeUpdateSetup.IsValid()) {
        if (m_ProbeIrradianceView)
            m_ProbeUpdateSetup.WriteStorageImage(device, 0, m_ProbeIrradianceView);
        if (m_ProbeDepthView)
            m_ProbeUpdateSetup.WriteStorageImage(device, 1, m_ProbeDepthView);
        if (m_VoxelView && m_VoxelSampler)
            m_ProbeUpdateSetup.WriteImage(device, 2, m_VoxelView, m_VoxelSampler,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        m_DDGIParamsUBO = std::make_unique<VulkanBuffer>(m_Context);
        if (m_DDGIParamsUBO->Create(sizeof(DDGIParamsUBO),
                                    static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
                                    /*hostVisible=*/true)) {
            m_ProbeUpdateSetup.WriteBuffer(device, 3, m_DDGIParamsUBO->GetBuffer(), sizeof(DDGIParamsUBO),
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        } else {
            m_DDGIParamsUBO.reset();
        }
    }

    // ddgi_sample.comp (the screen-space sample pass) is intentionally NOT
    // created: the PBR fragment shader samples the probe atlas directly, so the
    // pass, its screen-space irradiance output, and its G-buffer inputs are all
    // gone. SetGBuffer / m_ProbeSampleSetup / m_SampleParamsUBO remain declared
    // but unused for now (removable once the design is settled).

    return true;
}

bool DDGIProbeSystem::CreateSampler() {
    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    if (vkCreateSampler(m_Context->GetDevice(), &samplerCI, nullptr, &m_VoxelSampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DDGI: failed to create voxel sampler");
        return false;
    }

    VkSamplerCreateInfo irrSamplerCI = samplerCI;
    if (vkCreateSampler(m_Context->GetDevice(), &irrSamplerCI, nullptr, &m_IrradianceSampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DDGI: failed to create irradiance sampler");
        return false;
    }

    return true;
}

void DDGIProbeSystem::DestroyResources() {
    VkDevice device = m_Context->GetDevice();

    // Compute pipelines
    m_VoxelizeSetup.Destroy(device);
    m_ProbeUpdateSetup.Destroy(device);
    m_ProbeSampleSetup.Destroy(device);
    m_PipelinesCreated = false;

    // Images
    auto destroyImage = [device](VkImage& img, VkDeviceMemory& mem, VkImageView& view) {
        if (view) { vkDestroyImageView(device, view, nullptr); view = VK_NULL_HANDLE; }
        if (img) { vkDestroyImage(device, img, nullptr); img = VK_NULL_HANDLE; }
        if (mem) { vkFreeMemory(device, mem, nullptr); mem = VK_NULL_HANDLE; }
    };

    destroyImage(m_VoxelImage, m_VoxelMemory, m_VoxelView);
    destroyImage(m_ProbeIrradianceImage, m_ProbeIrradianceMemory, m_ProbeIrradianceView);
    destroyImage(m_ProbeDepthImage, m_ProbeDepthMemory, m_ProbeDepthView);
    destroyImage(m_IrradianceImage, m_IrradianceMemory, m_IrradianceView);

    // Samplers
    if (m_VoxelSampler) { vkDestroySampler(device, m_VoxelSampler, nullptr); m_VoxelSampler = VK_NULL_HANDLE; }
    if (m_IrradianceSampler) { vkDestroySampler(device, m_IrradianceSampler, nullptr); m_IrradianceSampler = VK_NULL_HANDLE; }

    // UBOs
    m_VoxelParamsUBO.reset();
    m_DDGIParamsUBO.reset();
    m_SampleParamsUBO.reset();
}

} // namespace Renderer
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
