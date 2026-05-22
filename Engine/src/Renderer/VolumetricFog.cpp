#include "Enjin/Renderer/VolumetricFog.h"
#include "Enjin/Renderer/ComputePipelineHelper.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/ClusteredLighting.h"
#include "Enjin/Logging/Log.h"

#if !ENJIN_RENDERER_WEBGPU

namespace Enjin {
namespace Renderer {

VolumetricFogSystem::VolumetricFogSystem(VulkanContext* context)
    : m_Context(context) {}

VolumetricFogSystem::~VolumetricFogSystem() {
    Shutdown();
}

bool VolumetricFogSystem::Initialize(const VolumetricFogConfig& config) {
    if (m_Initialized) return true;
    m_Config = config;

    if (!m_Config.enabled) {
        ENJIN_LOG_INFO(Renderer, "VolumetricFog: disabled by config");
        return true;
    }

    if (!CreateFroxelVolumes()) return false;
    if (!CreateSampler()) return false;
    // Compute pipeline created lazily (needs clustered lighting descriptor layout)

    m_Initialized = true;

    usize memPerVolume = static_cast<usize>(m_Config.froxelCountX) *
                         m_Config.froxelCountY * m_Config.froxelCountZ * 8; // RGBA16F = 8 bytes
    ENJIN_LOG_INFO(Renderer, "VolumetricFog initialized: %ux%ux%u froxels (2x %.1f MB), "
                   "density=%.3f, anisotropy=%.2f, temporal=%.2f",
                   m_Config.froxelCountX, m_Config.froxelCountY, m_Config.froxelCountZ,
                   static_cast<f32>(memPerVolume) / (1024.0f * 1024.0f),
                   m_Config.fogDensity, m_Config.fogAnisotropy, m_Config.temporalBlend);
    return true;
}

void VolumetricFogSystem::Shutdown() {
    if (!m_Initialized) return;
    DestroyResources();
    m_Initialized = false;
}

void VolumetricFogSystem::Update(VkCommandBuffer cmd, f32 time,
                                  const Math::Matrix4& inverseViewProj,
                                  const Math::Matrix4& prevViewProj,
                                  const Math::Matrix4& viewMatrix,
                                  const Math::Vector3& cameraPos,
                                  f32 nearPlane, f32 farPlane,
                                  u32 screenWidth, u32 screenHeight,
                                  const Math::Vector3& sunDirection,
                                  const Math::Vector3& sunColor, f32 sunIntensity) {
    if (!m_Config.enabled || !m_Initialized) return;

    // Lazy pipeline creation
    if (!m_PipelineCreated) {
        using BT = BindType;
        // volumetric_fog.comp bindings:
        // 0=storageImage(froxelVolume), 1=combinedImageSampler(prevFroxelVolume),
        // 2=SSBO(lights), 3=SSBO(lightGrid), 4=SSBO(lightIndices),
        // 5=combinedImageSampler(shadowAtlas), 6=combinedImageSampler(ddgiIrradiance),
        // 7=combinedImageSampler(sceneDepth), 8=UBO(params)
        m_FogSetup.Create(m_Context, {
            {0, BT::StorageImage}, {1, BT::CombinedImageSampler},
            {2, BT::StorageBuffer}, {3, BT::StorageBuffer}, {4, BT::StorageBuffer},
            {5, BT::CombinedImageSampler}, {6, BT::CombinedImageSampler},
            {7, BT::CombinedImageSampler}, {8, BT::UniformBuffer}
        }, "volumetric_fog.comp");

        VkDevice device = m_Context->GetDevice();
        if (m_FogSetup.IsValid()) {
            // Write current froxel volume as storage image output
            m_FogSetup.WriteStorageImage(device, 0, m_FroxelViews[m_CurrentVolume]);
            // Write previous froxel volume for temporal reprojection
            u32 prevVolume = 1 - m_CurrentVolume;
            if (m_FroxelViews[prevVolume] && m_FroxelSampler) {
                m_FogSetup.WriteImage(device, 1, m_FroxelViews[prevVolume], m_FroxelSampler,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            // Clustered lighting buffers written externally via SetClusteredLighting()
        }
        m_PipelineCreated = true;
    }

    if (!m_FogSetup.IsValid()) return;

    // TODO: Upload fog params UBO with current frame data

    // Dispatch: each invocation handles one XY froxel, marches all Z slices
    u32 groupsX = (m_Config.froxelCountX + 7) / 8;
    u32 groupsY = (m_Config.froxelCountY + 7) / 8;
    m_FogSetup.Dispatch(cmd, groupsX, groupsY);
    m_FogSetup.Barrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

    (void)time; (void)inverseViewProj; (void)prevViewProj;
    (void)viewMatrix; (void)cameraPos; (void)nearPlane; (void)farPlane;
    (void)screenWidth; (void)screenHeight;
    (void)sunDirection; (void)sunColor; (void)sunIntensity;
}

void VolumetricFogSystem::SwapVolumes() {
    m_CurrentVolume = 1 - m_CurrentVolume;
}

// --- Resource creation ---

bool VolumetricFogSystem::CreateFroxelVolumes() {
    VkDevice device = m_Context->GetDevice();

    for (u32 i = 0; i < VOLUME_COUNT; ++i) {
        VkImageCreateInfo imageCI{};
        imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCI.imageType = VK_IMAGE_TYPE_3D;
        imageCI.extent = { m_Config.froxelCountX, m_Config.froxelCountY, m_Config.froxelCountZ };
        imageCI.mipLevels = 1;
        imageCI.arrayLayers = 1;
        imageCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageCI, nullptr, &m_FroxelImages[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "VolumetricFog: failed to create froxel volume %u", i);
            return false;
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, m_FroxelImages[i], &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
            memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_FroxelMemory[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "VolumetricFog: failed to allocate froxel memory %u", i);
            return false;
        }
        vkBindImageMemory(device, m_FroxelImages[i], m_FroxelMemory[i], 0);

        VkImageViewCreateInfo viewCI{};
        viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.image = m_FroxelImages[i];
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_3D;
        viewCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        viewCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        if (vkCreateImageView(device, &viewCI, nullptr, &m_FroxelViews[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "VolumetricFog: failed to create froxel view %u", i);
            return false;
        }
    }

    return true;
}

bool VolumetricFogSystem::CreateSampler() {
    VkSamplerCreateInfo samplerCI{};
    samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    if (vkCreateSampler(m_Context->GetDevice(), &samplerCI, nullptr, &m_FroxelSampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "VolumetricFog: failed to create sampler");
        return false;
    }
    return true;
}

void VolumetricFogSystem::DestroyResources() {
    VkDevice device = m_Context->GetDevice();

    m_FogSetup.Destroy(device);
    m_PipelineCreated = false;

    for (u32 i = 0; i < VOLUME_COUNT; ++i) {
        if (m_FroxelViews[i]) { vkDestroyImageView(device, m_FroxelViews[i], nullptr); m_FroxelViews[i] = VK_NULL_HANDLE; }
        if (m_FroxelImages[i]) { vkDestroyImage(device, m_FroxelImages[i], nullptr); m_FroxelImages[i] = VK_NULL_HANDLE; }
        if (m_FroxelMemory[i]) { vkFreeMemory(device, m_FroxelMemory[i], nullptr); m_FroxelMemory[i] = VK_NULL_HANDLE; }
    }

    if (m_FroxelSampler) { vkDestroySampler(device, m_FroxelSampler, nullptr); m_FroxelSampler = VK_NULL_HANDLE; }
    m_ParamsUBO.reset();
}

} // namespace Renderer
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
