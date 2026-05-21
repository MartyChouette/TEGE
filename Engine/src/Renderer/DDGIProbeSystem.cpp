#include "Enjin/Renderer/DDGIProbeSystem.h"
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

    // Step 2: Update probe subset
    // TODO: Dispatch ddgi_probe_update.comp with per-frame UBO update
    // Requires: m_ProbeUpdatePipeline, m_ProbeUpdateDescSet, m_DDGIParamsUBO

    // Step 3: Sample probes into screen-space irradiance
    // TODO: Dispatch ddgi_sample.comp
    // Requires: m_ProbeSamplePipeline, m_ProbeSampleDescSet, m_SampleParamsUBO
}

void DDGIProbeSystem::Voxelize(VkCommandBuffer cmd, ECS::World* world) {
    // TODO: Dispatch gpu_voxelize.comp
    // Requires: m_VoxelizePipeline, m_VoxelizeDescSet, m_VoxelParamsUBO,
    //           MergedGeometryBuffer vertex/index SSBOs, instance buffer
    (void)cmd;
    (void)world;
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

    // Pipelines
    if (m_VoxelizePipeline) { vkDestroyPipeline(device, m_VoxelizePipeline, nullptr); m_VoxelizePipeline = VK_NULL_HANDLE; }
    if (m_ProbeUpdatePipeline) { vkDestroyPipeline(device, m_ProbeUpdatePipeline, nullptr); m_ProbeUpdatePipeline = VK_NULL_HANDLE; }
    if (m_ProbeSamplePipeline) { vkDestroyPipeline(device, m_ProbeSamplePipeline, nullptr); m_ProbeSamplePipeline = VK_NULL_HANDLE; }
    if (m_VoxelizeLayout) { vkDestroyPipelineLayout(device, m_VoxelizeLayout, nullptr); m_VoxelizeLayout = VK_NULL_HANDLE; }
    if (m_ProbeUpdateLayout) { vkDestroyPipelineLayout(device, m_ProbeUpdateLayout, nullptr); m_ProbeUpdateLayout = VK_NULL_HANDLE; }
    if (m_ProbeSampleLayout) { vkDestroyPipelineLayout(device, m_ProbeSampleLayout, nullptr); m_ProbeSampleLayout = VK_NULL_HANDLE; }

    // Descriptor sets/layouts/pool
    if (m_VoxelizeDescLayout) { vkDestroyDescriptorSetLayout(device, m_VoxelizeDescLayout, nullptr); m_VoxelizeDescLayout = VK_NULL_HANDLE; }
    if (m_ProbeUpdateDescLayout) { vkDestroyDescriptorSetLayout(device, m_ProbeUpdateDescLayout, nullptr); m_ProbeUpdateDescLayout = VK_NULL_HANDLE; }
    if (m_ProbeSampleDescLayout) { vkDestroyDescriptorSetLayout(device, m_ProbeSampleDescLayout, nullptr); m_ProbeSampleDescLayout = VK_NULL_HANDLE; }
    if (m_DescPool) { vkDestroyDescriptorPool(device, m_DescPool, nullptr); m_DescPool = VK_NULL_HANDLE; }

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
