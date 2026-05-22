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

    if (!m_Config.enabled) {
        ENJIN_LOG_INFO(Renderer, "DDGI: disabled by config");
        return true;
    }

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

void DDGIProbeSystem::Update(VkCommandBuffer cmd, ECS::World* world, u32 frameNumber,
                              const Math::Vector3& sunDirection, const Math::Vector3& sunColor,
                              f32 sunIntensity, const Math::Matrix4& inverseViewProj,
                              u32 screenWidth, u32 screenHeight) {
    if (!m_Config.enabled || !m_Initialized) return;
    m_FrameNumber = frameNumber;

    // Recreate screen-space irradiance if resolution changed
    if (screenWidth != m_IrradianceWidth || screenHeight != m_IrradianceHeight) {
        if (m_IrradianceImage) {
            // Destroy old resources
            VkDevice device = m_Context->GetDevice();
            vkDestroyImageView(device, m_IrradianceView, nullptr);
            vkDestroyImage(device, m_IrradianceImage, nullptr);
            vkFreeMemory(device, m_IrradianceMemory, nullptr);
            m_IrradianceView = VK_NULL_HANDLE;
            m_IrradianceImage = VK_NULL_HANDLE;
            m_IrradianceMemory = VK_NULL_HANDLE;
        }
        CreateScreenIrradiance(screenWidth, screenHeight);
    }

    // Step 1: Re-voxelize scene periodically
    if (m_NeedsRevoxelize || (frameNumber % m_VoxelizeFrameInterval == 0)) {
        Voxelize(cmd, world);
        m_NeedsRevoxelize = false;
    }

    // Lazy pipeline creation (deferred so resources are ready)
    if (!m_PipelinesCreated) {
        CreateComputePipelines();
        m_PipelinesCreated = true;
    }

    // Step 2: Update probe subset via SDF ray march
    if (m_ProbeUpdateSetup.IsValid()) {
        // Update UBO with current frame params
        // TODO: Upload DDGIParams struct to m_DDGIParamsUBO

        u32 totalProbes = GetTotalProbes();
        u32 totalWork = totalProbes * m_Config.raysPerProbe;
        u32 workgroups = (totalWork + 63) / 64;
        m_ProbeUpdateSetup.Dispatch(cmd, workgroups);
        m_ProbeUpdateSetup.Barrier(cmd);
    }

    // Step 3: Sample probes into screen-space irradiance
    if (m_ProbeSampleSetup.IsValid() && m_IrradianceView) {
        u32 groupsX = (screenWidth + 7) / 8;
        u32 groupsY = (screenHeight + 7) / 8;
        m_ProbeSampleSetup.Dispatch(cmd, groupsX, groupsY);
        m_ProbeSampleSetup.Barrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    }
}

void DDGIProbeSystem::Voxelize(VkCommandBuffer cmd, ECS::World* world) {
    (void)world; // Used when wiring instance buffer from MergedGeometryBuffer

    if (!m_VoxelizeSetup.IsValid()) return;

    // Clear voxel grid to max distance (empty = far away)
    // TODO: vkCmdClearColorImage or fill compute pass

    // Dispatch voxelization compute shader
    // Each invocation processes one triangle; total = scene triangle count
    // For now, use a conservative estimate
    u32 estimatedTriangles = 100000; // TODO: read from MergedGeometryBuffer stats
    u32 workgroups = (estimatedTriangles + 63) / 64;
    m_VoxelizeSetup.Dispatch(cmd, workgroups);
    m_VoxelizeSetup.Barrier(cmd);

    ENJIN_LOG_INFO(Renderer, "DDGI: voxelized scene (%u workgroups dispatched)", workgroups);
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
    }

    // ddgi_sample.comp: binding 0=storageImage(output), 1=combinedImageSampler(depth),
    //   2=combinedImageSampler(normal), 3=combinedImageSampler(probeIrradiance),
    //   4=combinedImageSampler(probeDepth), 5=UBO(params)
    m_ProbeSampleSetup.Create(m_Context, {
        {0, BT::StorageImage}, {1, BT::CombinedImageSampler},
        {2, BT::CombinedImageSampler}, {3, BT::CombinedImageSampler},
        {4, BT::CombinedImageSampler}, {5, BT::UniformBuffer}
    }, "ddgi_sample.comp");

    if (m_ProbeSampleSetup.IsValid() && m_IrradianceView) {
        m_ProbeSampleSetup.WriteStorageImage(device, 0, m_IrradianceView);
        // G-buffer bindings (1, 2) written externally when available
        if (m_ProbeIrradianceView && m_IrradianceSampler) {
            m_ProbeSampleSetup.WriteImage(device, 3, m_ProbeIrradianceView, m_IrradianceSampler,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        if (m_ProbeDepthView && m_IrradianceSampler) {
            m_ProbeSampleSetup.WriteImage(device, 4, m_ProbeDepthView, m_IrradianceSampler,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

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
