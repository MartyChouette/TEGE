#include "Enjin/Renderer/ReflectionProbeSystem.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/RenderTarget.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/ReflectionProbe.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <cmath>
#include <cstring>

namespace Enjin {
namespace Renderer {

ReflectionProbeSystem::~ReflectionProbeSystem() {
    Shutdown();
}

void ReflectionProbeSystem::Initialize(VulkanContext* context) {
    m_Context = context;
    m_Initialized = true;
}

void ReflectionProbeSystem::Shutdown() {
    if (!m_Context) return;

    m_Context->WaitForGPU();

    for (auto& [entity, cubemap] : m_BakedCubemaps) {
        DestroyCubemap(cubemap);
    }
    m_BakedCubemaps.clear();
    m_PendingBakes.clear();
    m_ActiveBakedDescriptorValid = false;
    m_Initialized = false;
}

ReflectionProbeData ReflectionProbeSystem::FindNearestProbe(
    ECS::World* world, const Math::Vector3& position) const {

    ReflectionProbeData result{};
    m_ActiveBakedDescriptorValid = false;

    if (!world) return result;

    i32 bestPriority = -1;
    f32 bestWeight = 0.0f;
    u64 bestEntity = 0;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ReflectionProbeComponent>()) {
        auto* probe = world->GetComponent<ECS::ReflectionProbeComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!probe || !transform || !probe->isActive) continue;

        // Compute world-space AABB from probe center + box offsets
        Math::Vector3 center = transform->position;
        Math::Vector3 worldMin(center.x + probe->boxMin.x,
                               center.y + probe->boxMin.y,
                               center.z + probe->boxMin.z);
        Math::Vector3 worldMax(center.x + probe->boxMax.x,
                               center.y + probe->boxMax.y,
                               center.z + probe->boxMax.z);

        // Check if position is inside the probe's AABB
        if (position.x < worldMin.x || position.x > worldMax.x ||
            position.y < worldMin.y || position.y > worldMax.y ||
            position.z < worldMin.z || position.z > worldMax.z) {
            continue;
        }

        // Compute blend weight based on distance from box edges
        f32 blend = probe->blendDistance;
        f32 weight = 1.0f;
        if (blend > 0.001f) {
            f32 dx0 = position.x - worldMin.x;
            f32 dx1 = worldMax.x - position.x;
            f32 dy0 = position.y - worldMin.y;
            f32 dy1 = worldMax.y - position.y;
            f32 dz0 = position.z - worldMin.z;
            f32 dz1 = worldMax.z - position.z;

            f32 minDist = Math::Min(dx0, Math::Min(dx1,
                          Math::Min(dy0, Math::Min(dy1,
                          Math::Min(dz0, dz1)))));

            weight = Math::Clamp(minDist / blend, 0.0f, 1.0f);
        }

        // Pick the probe with highest priority, then best blend weight
        i32 probePriority = static_cast<i32>(probe->priority);
        if (probePriority > bestPriority ||
            (probePriority == bestPriority && weight > bestWeight)) {
            bestPriority = probePriority;
            bestWeight = weight;
            bestEntity = entity;

            result.probePosition = center;
            result.intensity = probe->intensity * weight;
            result.boxMin = worldMin;
            result.boxMax = worldMax;
            result.blendDistance = probe->blendDistance;
            result.isBaked = (probe->baked && probe->cubemapTextureId >= 0) ? 1.0f : 0.0f;
        }
    }

    // If the best probe is baked, set up the active cubemap descriptor
    if (result.isBaked > 0.5f && bestEntity != 0) {
        auto it = m_BakedCubemaps.find(bestEntity);
        if (it != m_BakedCubemaps.end() && it->second.view != VK_NULL_HANDLE) {
            m_ActiveBakedDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            m_ActiveBakedDescriptor.imageView = it->second.view;
            m_ActiveBakedDescriptor.sampler = it->second.sampler;
            m_ActiveBakedDescriptorValid = true;
            m_ActiveBakedMipLevels = it->second.mipLevels;
        }
    }

    return result;
}

void ReflectionProbeSystem::RequestBake(u64 probeEntity) {
    // Avoid duplicate requests
    for (u64 id : m_PendingBakes) {
        if (id == probeEntity) return;
    }
    m_PendingBakes.push_back(probeEntity);
    ENJIN_LOG_INFO(Renderer, "Reflection probe bake queued for entity %llu (will bake next frame)", probeEntity);
}

void ReflectionProbeSystem::ProcessPendingBakes(ECS::World* world, ECS::RenderSystem* renderSystem) {
    if (m_PendingBakes.empty()) return;

    // Process all pending bakes
    for (u64 entity : m_PendingBakes) {
        BakeProbeInternal(world, renderSystem, entity);
    }
    m_PendingBakes.clear();
}

bool ReflectionProbeSystem::BakeProbeInternal(ECS::World* world, ECS::RenderSystem* renderSystem, u64 probeEntity) {
    if (!world || !renderSystem || !m_Context || !m_Initialized) {
        ENJIN_LOG_ERROR(Renderer, "ReflectionProbeSystem::BakeProbe - not initialized or null parameters");
        return false;
    }

    auto* probe = world->GetComponent<ECS::ReflectionProbeComponent>(static_cast<ECS::Entity>(probeEntity));
    auto* transform = world->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(probeEntity));
    if (!probe || !transform) {
        ENJIN_LOG_ERROR(Renderer, "ReflectionProbeSystem::BakeProbe - entity missing required components");
        return false;
    }

    VulkanRenderer* vulkanRenderer = renderSystem->GetVulkanRenderer();
    if (!vulkanRenderer) {
        ENJIN_LOG_ERROR(Renderer, "ReflectionProbeSystem::BakeProbe - no VulkanRenderer available");
        return false;
    }

    u32 resolution = probe->resolution;
    if (resolution < 32) resolution = 32;
    if (resolution > 1024) resolution = 1024;

    Math::Vector3 probePos = transform->position;

    ENJIN_LOG_INFO(Renderer, "Baking reflection probe at (%.1f, %.1f, %.1f) resolution %u...",
        probePos.x, probePos.y, probePos.z, resolution);

    // Ensure GPU is idle before modifying resources
    m_Context->WaitForGPU();

    // Destroy previous cubemap if it exists
    auto existing = m_BakedCubemaps.find(probeEntity);
    if (existing != m_BakedCubemaps.end()) {
        DestroyCubemap(existing->second);
        m_BakedCubemaps.erase(existing);
    }

    // Create the cubemap image (6 layers, RGBA8)
    BakedCubemap cubemap;
    if (!CreateCubemapImage(resolution, cubemap)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create cubemap image for probe bake");
        return false;
    }

    // Cubemap face directions and up vectors (standard OpenGL cubemap convention)
    // Order: +X, -X, +Y, -Y, +Z, -Z
    struct FaceDef {
        Math::Vector3 dir;
        Math::Vector3 up;
    };

    FaceDef faces[6] = {
        { Math::Vector3( 1.0f,  0.0f,  0.0f), Math::Vector3(0.0f, -1.0f,  0.0f) },  // +X
        { Math::Vector3(-1.0f,  0.0f,  0.0f), Math::Vector3(0.0f, -1.0f,  0.0f) },  // -X
        { Math::Vector3( 0.0f,  1.0f,  0.0f), Math::Vector3(0.0f,  0.0f,  1.0f) },  // +Y
        { Math::Vector3( 0.0f, -1.0f,  0.0f), Math::Vector3(0.0f,  0.0f, -1.0f) },  // -Y
        { Math::Vector3( 0.0f,  0.0f,  1.0f), Math::Vector3(0.0f, -1.0f,  0.0f) },  // +Z
        { Math::Vector3( 0.0f,  0.0f, -1.0f), Math::Vector3(0.0f, -1.0f,  0.0f) },  // -Z
    };

    // Create a temporary render target for face captures
    auto faceTarget = std::make_unique<RenderTarget>();
    if (!faceTarget->Create(vulkanRenderer, resolution, resolution)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target for probe bake");
        DestroyCubemap(cubemap);
        return false;
    }

    // Render each face and capture pixels
    std::vector<std::vector<u8>> facePixels(6);
    usize expectedFaceBytes = static_cast<usize>(resolution) * static_cast<usize>(resolution) * 4;
    bool allFacesOk = true;

    for (int face = 0; face < 6; ++face) {
        // Set up camera for this face: 90 degree FOV, 1:1 aspect
        Camera faceCamera;
        Math::Vector3 target(probePos.x + faces[face].dir.x,
                             probePos.y + faces[face].dir.y,
                             probePos.z + faces[face].dir.z);
        faceCamera.SetLookAt(probePos, target, faces[face].up);
        faceCamera.SetPerspective(90.0f, 1.0f, 0.1f, 1000.0f);

        // Run shadow pass for this camera (needs its own command buffer submission)
        renderSystem->RenderShadowPassForCamera(&faceCamera);
        m_Context->WaitForGPU();

        // Begin a frame, render the scene to the target, end the frame
        if (!vulkanRenderer->BeginFrameVulkan()) {
            ENJIN_LOG_WARN(Renderer, "Probe bake face %d: BeginFrame failed", face);
            facePixels[face].resize(expectedFaceBytes, 128);
            allFacesOk = false;
            continue;
        }

        VkCommandBuffer cmd = vulkanRenderer->GetCurrentCommandBuffer();
        if (cmd == VK_NULL_HANDLE) {
            ENJIN_LOG_WARN(Renderer, "Probe bake face %d: no command buffer", face);
            vulkanRenderer->EndFrame();
            facePixels[face].resize(expectedFaceBytes, 128);
            allFacesOk = false;
            continue;
        }

        // Begin render target, render scene, end render target
        faceTarget->Begin(cmd);
        renderSystem->RenderToTarget(faceTarget.get(), &faceCamera);
        faceTarget->End(cmd);

        // End the frame (submits command buffer)
        vulkanRenderer->EndFrame();
        m_Context->WaitForGPU();

        // Capture rendered pixels from the render target
        auto pixels = faceTarget->CaptureToPixels();
        if (pixels.size() >= expectedFaceBytes) {
            facePixels[face] = std::move(pixels);
        } else {
            ENJIN_LOG_WARN(Renderer, "Probe bake face %d: capture returned %zu bytes, expected %zu",
                face, pixels.size(), expectedFaceBytes);
            facePixels[face].resize(expectedFaceBytes, 128);
            allFacesOk = false;
        }
    }

    if (!allFacesOk) {
        ENJIN_LOG_WARN(Renderer, "Some probe faces failed to render, cubemap may have artifacts");
    }

    // Clean up the render target
    faceTarget->Destroy();
    faceTarget.reset();

    // Upload all 6 faces to the cubemap
    if (!UploadFacesToCubemap(cubemap, facePixels, resolution)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload face data to cubemap");
        DestroyCubemap(cubemap);
        return false;
    }

    // Store the baked cubemap
    m_BakedCubemaps[probeEntity] = cubemap;

    // Update the component
    probe->baked = true;
    probe->cubemapTextureId = static_cast<i32>(probeEntity & 0x7FFFFFFF);

    ENJIN_LOG_INFO(Renderer, "Reflection probe baked successfully (%ux%u per face, 6 faces)",
        resolution, resolution);

    return true;
}

VkDescriptorImageInfo ReflectionProbeSystem::GetBakedCubemapDescriptor(u64 probeEntity) const {
    VkDescriptorImageInfo info{};
    auto it = m_BakedCubemaps.find(probeEntity);
    if (it != m_BakedCubemaps.end() && it->second.view != VK_NULL_HANDLE) {
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = it->second.view;
        info.sampler = it->second.sampler;
    }
    return info;
}

bool ReflectionProbeSystem::CreateCubemapImage(u32 resolution, BakedCubemap& cubemap) {
    VkDevice device = m_Context->GetDevice();
    cubemap.resolution = resolution;

    // Prefiltered mip chain: mip 0 is the sharp capture, each higher mip is a
    // linear-downsampled (blurrier) copy that stands in for rougher surfaces.
    // Cap the chain so the coarsest mip stays >= 4x4 (a 1x1 mip is a single
    // averaged color and adds no useful glossy detail).
    u32 fullMips = 1;
    { u32 r = resolution; while (r > 4) { r >>= 1; ++fullMips; } }
    u32 mipLevels = Math::Min(fullMips, 6u);
    cubemap.mipLevels = mipLevels;

    // Create cubemap image (6 layers, RGBA8, mip chain, transfer src+dst + sampled)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { resolution, resolution, 1 };
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 6;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // TRANSFER_SRC so mip N can be blitted from mip N-1 during prefiltering.
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &cubemap.image) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create probe cubemap image");
        return false;
    }

    // Allocate device-local memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, cubemap.image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &cubemap.memory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate probe cubemap memory");
        vkDestroyImage(device, cubemap.image, nullptr);
        cubemap.image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(device, cubemap.image, cubemap.memory, 0);

    // Create cubemap image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = cubemap.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    if (vkCreateImageView(device, &viewInfo, nullptr, &cubemap.view) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create probe cubemap image view");
        vkFreeMemory(device, cubemap.memory, nullptr);
        vkDestroyImage(device, cubemap.image, nullptr);
        cubemap.image = VK_NULL_HANDLE;
        cubemap.memory = VK_NULL_HANDLE;
        return false;
    }

    // Create sampler with linear filtering and clamp-to-edge
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<f32>(mipLevels);
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &cubemap.sampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create probe cubemap sampler");
        vkDestroyImageView(device, cubemap.view, nullptr);
        vkFreeMemory(device, cubemap.memory, nullptr);
        vkDestroyImage(device, cubemap.image, nullptr);
        cubemap = {};
        return false;
    }

    return true;
}

bool ReflectionProbeSystem::UploadFacesToCubemap(
    BakedCubemap& cubemap, const std::vector<std::vector<u8>>& facePixels, u32 resolution) {

    if (facePixels.size() < 6 || !m_Context) return false;

    VkDevice device = m_Context->GetDevice();
    usize faceBytes = static_cast<usize>(resolution) * static_cast<usize>(resolution) * 4;
    usize totalBytes = faceBytes * 6;

    // Create staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = totalBytes;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create probe cubemap staging buffer");
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate probe cubemap staging memory");
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return false;
    }
    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    // Copy face data to staging buffer
    void* mapped = nullptr;
    if (vkMapMemory(device, stagingMemory, 0, totalBytes, 0, &mapped) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to map probe cubemap staging memory");
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return false;
    }

    for (int f = 0; f < 6; ++f) {
        usize srcBytes = std::min(facePixels[f].size(), faceBytes);
        memcpy(static_cast<u8*>(mapped) + f * faceBytes, facePixels[f].data(), srcBytes);
        // Zero-fill remainder if source is short
        if (srcBytes < faceBytes) {
            memset(static_cast<u8*>(mapped) + f * faceBytes + srcBytes, 128, faceBytes - srcBytes);
        }
    }
    vkUnmapMemory(device, stagingMemory);

    // Create temporary command pool and buffer for upload
    VkCommandPool tempPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &tempPool) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return false;
    }

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = tempPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, tempPool, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    const u32 mipLevels = cubemap.mipLevels;

    // Transition ALL mips of ALL 6 faces to TRANSFER_DST_OPTIMAL.
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = cubemap.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy each face's captured pixels into mip 0.
    for (u32 f = 0; f < 6; ++f) {
        VkBufferImageCopy region{};
        region.bufferOffset = f * faceBytes;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = f;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { resolution, resolution, 1 };

        vkCmdCopyBufferToImage(cmd, stagingBuffer, cubemap.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    // Prefilter: generate each successive mip by a linear blit-downsample from
    // the previous mip (all 6 faces at once). This is a cheap glossy prefilter —
    // rough surfaces sample the blurrier high mips. Every mip level is blitted
    // independently per face, which cube-face seams tolerate at these blur levels.
    i32 mipW = static_cast<i32>(resolution);
    i32 mipH = static_cast<i32>(resolution);
    for (u32 i = 1; i < mipLevels; ++i) {
        // Source mip (i-1): TRANSFER_DST -> TRANSFER_SRC
        VkImageMemoryBarrier toSrc = barrier;
        toSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSrc.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toSrc.subresourceRange.baseMipLevel = i - 1;
        toSrc.subresourceRange.levelCount = 1;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toSrc);

        i32 nextW = mipW > 1 ? mipW / 2 : 1;
        i32 nextH = mipH > 1 ? mipH / 2 : 1;

        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 6;
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipW, mipH, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 6;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { nextW, nextH, 1 };

        vkCmdBlitImage(cmd,
            cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        mipW = nextW;
        mipH = nextH;
    }

    // Final layout transitions to SHADER_READ_ONLY_OPTIMAL. Mips 0..n-2 ended in
    // TRANSFER_SRC (they were blit sources); the last mip is still TRANSFER_DST.
    if (mipLevels > 1) {
        VkImageMemoryBarrier srcToRead = barrier;
        srcToRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        srcToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        srcToRead.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcToRead.subresourceRange.baseMipLevel = 0;
        srcToRead.subresourceRange.levelCount = mipLevels - 1;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &srcToRead);
    }
    VkImageMemoryBarrier lastToRead = barrier;
    lastToRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    lastToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    lastToRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    lastToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    lastToRead.subresourceRange.baseMipLevel = mipLevels - 1;
    lastToRead.subresourceRange.levelCount = 1;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &lastToRead);

    vkEndCommandBuffer(cmd);

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to submit reflection probe cubemap upload commands");
    }
    vkQueueWaitIdle(m_Context->GetGraphicsQueue());

    // Cleanup staging resources
    vkDestroyCommandPool(device, tempPool, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);

    return true;
}

void ReflectionProbeSystem::DestroyCubemap(BakedCubemap& cubemap) {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (cubemap.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, cubemap.sampler, nullptr);
    }
    if (cubemap.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, cubemap.view, nullptr);
    }
    if (cubemap.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, cubemap.image, nullptr);
    }
    if (cubemap.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, cubemap.memory, nullptr);
    }
    cubemap = {};
}

} // namespace Renderer
} // namespace Enjin
