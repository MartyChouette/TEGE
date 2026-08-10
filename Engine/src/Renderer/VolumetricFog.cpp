#include "Enjin/Renderer/VolumetricFog.h"
#include "Enjin/Renderer/ComputePipelineHelper.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/ClusteredLighting.h"
#include "Enjin/Logging/Log.h"

#if !ENJIN_RENDERER_WEBGPU

namespace Enjin {
namespace Renderer {

// std140 mirror of volumetric_fog.comp's FogParams (set 0, binding 8).
// vec3 members align to 16; the scalar that follows each vec3 packs into its
// .w slot. froxelCountZ is followed by a vec3, so std140 inserts 4 bytes of
// padding there — made explicit as _pad0.
struct FogParamsUBO {
    Math::Matrix4 inverseViewProj;   // offset 0
    Math::Matrix4 prevViewProj;      // 64
    Math::Matrix4 viewMatrix;        // 128
    Math::Vector3 cameraPos;         // 192
    f32 nearPlane;                   // 204
    f32 farPlane;                    // 208
    f32 time;                        // 212
    u32 screenWidth;                 // 216
    u32 screenHeight;                // 220
    Math::Vector3 fogAlbedo;         // 224
    f32 fogDensity;                  // 236
    f32 fogHeightFalloff;            // 240
    f32 fogBaseHeight;               // 244
    f32 fogAnisotropy;               // 248
    f32 temporalBlend;               // 252
    f32 noiseScale;                  // 256
    f32 noiseStrength;               // 260
    f32 windSpeedX;                  // 264
    f32 windSpeedZ;                  // 268
    u32 froxelCountX;                // 272
    u32 froxelCountY;                // 276
    u32 froxelCountZ;                // 280
    u32 _pad0;                       // 284 (std140: next vec3 aligns to 288)
    Math::Vector3 sunDirection;      // 288
    f32 sunIntensity;                // 300
    Math::Vector3 sunColor;          // 304
    f32 _pad1;                       // 316
};
static_assert(sizeof(FogParamsUBO) == 320, "FogParamsUBO must match volumetric_fog.comp std140 layout");

VolumetricFogSystem::VolumetricFogSystem(VulkanContext* context)
    : m_Context(context) {}

VolumetricFogSystem::~VolumetricFogSystem() {
    Shutdown();
}

bool VolumetricFogSystem::Initialize(const VolumetricFogConfig& config) {
    if (m_Initialized) return true;
    m_Config = config;

    // Resources are created regardless of the enabled flag. The froxel volume
    // is bound to the main PBR pass and the runtime Settings toggle flips
    // m_Config.enabled with no re-init, so the volumes must exist even when fog
    // starts disabled. Only the per-frame compute dispatch is gated on enabled
    // (see Update); the memory (~2x froxel volume) is the price of a live toggle.
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
    if (!m_Initialized) return;

    // The froxel volume is bound to the main PBR pass (binding 23) from init
    // time, so it must be shader-readable with sane contents BEFORE the first
    // dispatch ever runs (fog may be disabled, or the clustered-light buffers
    // may not exist yet). Transition both volumes out of UNDEFINED exactly
    // once: volume 0 (sampled by PBR) cleared to neutral (no in-scatter, full
    // transmittance), volume 1 (temporal history) cleared to zero.
    if (!m_VolumesInitialized) {
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        for (u32 i = 0; i < VOLUME_COUNT; ++i) {
            VkImageMemoryBarrier toDst{};
            toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toDst.image = m_FroxelImages[i];
            toDst.subresourceRange = range;
            toDst.srcAccessMask = 0;
            toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);
            VkClearColorValue value{};
            if (i == 0) value.float32[3] = 1.0f;   // neutral: transmittance 1
            vkCmdClearColorImage(cmd, m_FroxelImages[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &value, 1, &range);
            VkImageMemoryBarrier toRead = toDst;
            toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0, 0, nullptr, 0, nullptr, 1, &toRead);
        }
        m_VolumesInitialized = true;
        ENJIN_LOG_INFO(Renderer, "VolumetricFog: volumes neutral-cleared (one-shot)");
    }

    if (!m_Config.enabled) {
        // The PBR pass keeps sampling the bound volume, so a runtime disable
        // must clear it once to the neutral value (RGB = no in-scatter,
        // A = 1 = full transmittance) or the last frame's fog lingers forever.
        if (m_VolumesInitialized && !m_DisabledCleared) {
            VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
            VkImageMemoryBarrier toDst{};
            toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            toDst.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toDst.image = m_FroxelImages[0];
            toDst.subresourceRange = range;
            toDst.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst);
            VkClearColorValue neutral{};
            neutral.float32[3] = 1.0f;   // transmittance 1 = clear air
            vkCmdClearColorImage(cmd, m_FroxelImages[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &neutral, 1, &range);
            VkImageMemoryBarrier toRead = toDst;
            toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toRead);
            m_DisabledCleared = true;
        }
        return;
    }
    m_DisabledCleared = false;

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
            // Descriptors are STATIC across the frame loop: volume 0 is always the
            // compute output, volume 1 always the previous frame's result (history).
            // History is produced by copying 0 -> 1 after each dispatch instead of
            // swapping bindings — rewriting a descriptor set the previous frame's
            // command buffer may still be reading is a validation error.
            m_FogSetup.WriteStorageImage(device, 0, m_FroxelViews[0]);
            if (m_FroxelViews[1] && m_FroxelSampler) {
                m_FogSetup.WriteImage(device, 1, m_FroxelViews[1], m_FroxelSampler,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            // Clustered lighting buffers written externally via SetClusteredLighting()

            m_ParamsUBO = std::make_unique<VulkanBuffer>(m_Context);
            if (m_ParamsUBO->Create(sizeof(FogParamsUBO),
                                    static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT),
                                    /*hostVisible=*/true)) {
                m_FogSetup.WriteBuffer(device, 8, m_ParamsUBO->GetBuffer(), sizeof(FogParamsUBO),
                                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            } else {
                ENJIN_LOG_ERROR(Renderer, "VolumetricFog: params UBO creation failed — fog disabled");
                m_ParamsUBO.reset();
            }
        }
        m_PipelineCreated = true;
    }

    if (!m_FogSetup.IsValid() || !m_ParamsUBO) return;

    // The shader statically uses the clustered-light SSBOs (bindings 2-4); the
    // dispatch is invalid until they are written. They are created lazily by
    // ClusteredLightingSystem, so bind them on the first frame they exist.
    // (Bindings 5-7 — shadowAtlas/ddgiIrradiance/sceneDepth — are declared but
    // never statically used by volumetric_fog.comp, so they need no resources.)
    if (!m_ClusterBuffersBound) {
        if (!m_ClusteredLighting) return;
        VkBuffer lightBuf = m_ClusteredLighting->GetLightBuffer();
        VkBuffer gridBuf  = m_ClusteredLighting->GetLightGridBuffer();
        VkBuffer idxBuf   = m_ClusteredLighting->GetLightIndexBuffer();
        if (!lightBuf || !gridBuf || !idxBuf) return;   // clustered pass not up yet
        VkDevice device = m_Context->GetDevice();
        m_FogSetup.WriteBuffer(device, 2, lightBuf, m_ClusteredLighting->GetLightBufferSize());
        m_FogSetup.WriteBuffer(device, 3, gridBuf,  m_ClusteredLighting->GetLightGridBufferSize());
        m_FogSetup.WriteBuffer(device, 4, idxBuf,   m_ClusteredLighting->GetLightIndexBufferSize());
        m_ClusterBuffersBound = true;
    }

    // Upload this frame's parameters (host-visible; ClusteredLighting uses the
    // same single-buffer-per-frame idiom).
    FogParamsUBO params{};
    params.inverseViewProj = inverseViewProj;
    params.prevViewProj    = prevViewProj;
    params.viewMatrix      = viewMatrix;
    params.cameraPos       = cameraPos;
    params.nearPlane       = nearPlane;
    params.farPlane        = farPlane;
    params.time            = time;
    params.screenWidth     = screenWidth;
    params.screenHeight    = screenHeight;
    params.fogAlbedo       = m_Config.fogAlbedo;
    params.fogDensity      = m_Config.fogDensity;
    params.fogHeightFalloff= m_Config.fogHeightFalloff;
    params.fogBaseHeight   = m_Config.fogBaseHeight;
    params.fogAnisotropy   = m_Config.fogAnisotropy;
    params.temporalBlend   = m_Config.temporalBlend;
    params.noiseScale      = m_Config.noiseScale;
    params.noiseStrength   = m_Config.noiseStrength;
    params.windSpeedX      = m_Config.windSpeedX;
    params.windSpeedZ      = m_Config.windSpeedZ;
    params.froxelCountX    = m_Config.froxelCountX;
    params.froxelCountY    = m_Config.froxelCountY;
    params.froxelCountZ    = m_Config.froxelCountZ;
    params.sunDirection    = sunDirection;
    params.sunIntensity    = sunIntensity;
    params.sunColor        = sunColor;
    m_ParamsUBO->UploadData(&params, sizeof(params));

    VkImageSubresourceRange fullRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    auto imageBarrier = [&](VkImage image, VkImageLayout oldL, VkImageLayout newL,
                            VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                            VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage) {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = oldL;
        b.newLayout = newL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange = fullRange;
        b.srcAccessMask = srcAccess;
        b.dstAccessMask = dstAccess;
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
    };

    // Volume 0 is SHADER_READ_ONLY between frames (ensure-init or last frame's
    // post-copy transition) — move it to GENERAL for the compute write.
    imageBarrier(m_FroxelImages[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                 VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_WRITE_BIT,
                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // Dispatch: each invocation handles one XY froxel, marches all Z slices
    u32 groupsX = (m_Config.froxelCountX + 7) / 8;
    u32 groupsY = (m_Config.froxelCountY + 7) / 8;
    m_FogSetup.Dispatch(cmd, groupsX, groupsY);

    // Copy this frame's result into the history volume (0 -> 1), then leave
    // volume 0 shader-readable for the fog apply pass and volume 1 shader-
    // readable for next frame's temporal term.
    imageBarrier(m_FroxelImages[0], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
    imageBarrier(m_FroxelImages[1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkImageCopy region{};
    region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.extent = { m_Config.froxelCountX, m_Config.froxelCountY, m_Config.froxelCountZ };
    vkCmdCopyImage(cmd, m_FroxelImages[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   m_FroxelImages[1], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    imageBarrier(m_FroxelImages[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    imageBarrier(m_FroxelImages[1], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                 VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void VolumetricFogSystem::SwapVolumes() {
    // Obsolete: history is maintained by the 0 -> 1 copy inside Update. Kept as
    // a no-op so existing call sites stay source-compatible.
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
        // TRANSFER usage is required: the neutral-init path clears these with
        // vkCmdClearColorImage and Update copies volume 0 -> 1 for history
        imageCI.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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
