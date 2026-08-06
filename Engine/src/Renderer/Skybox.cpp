#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <memory>
#include "stb_image.h"

namespace Enjin {
namespace Renderer {

Skybox::Skybox() = default;

Skybox::~Skybox() {
    Shutdown();
}

bool Skybox::Initialize(VulkanContext* context) {
    m_Context = context;
    if (!CreateSampler()) {
        ENJIN_LOG_ERROR(Renderer, "Skybox::Initialize - CreateSampler failed");
        return false;
    }
    m_Initialized = true;
    return true;
}

void Skybox::Shutdown() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) return;

    m_Context->WaitForGPU();
    DestroyImage();

    if (m_Sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_Sampler, nullptr);
        m_Sampler = VK_NULL_HANDLE;
    }
    m_Initialized = false;
}

void Skybox::DestroyImage() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (m_CubemapView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_CubemapView, nullptr);
        m_CubemapView = VK_NULL_HANDLE;
    }
    if (m_CubemapImage != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_CubemapImage, nullptr);
        m_CubemapImage = VK_NULL_HANDLE;
    }
    if (m_CubemapMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_CubemapMemory, nullptr);
        m_CubemapMemory = VK_NULL_HANDLE;
    }
}

bool Skybox::CreateCubemapImage(u32 faceSize) {
    DestroyImage();
    m_FaceSize = faceSize;

    VkDevice device = m_Context->GetDevice();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { faceSize, faceSize, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_CubemapImage) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create cubemap image");
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_CubemapImage, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memProps);

    u32 memTypeIndex = UINT32_MAX;
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memTypeIndex = i;
            break;
        }
    }

    if (memTypeIndex == UINT32_MAX) {
        ENJIN_LOG_ERROR(Renderer, "Failed to find memory type for cubemap");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_CubemapMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate cubemap memory");
        return false;
    }

    vkBindImageMemory(device, m_CubemapImage, m_CubemapMemory, 0);

    // Create cubemap image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_CubemapImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_CubemapView) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create cubemap image view");
        return false;
    }

    return true;
}

bool Skybox::CreateProcedural(const Math::Vector3& topColor, const Math::Vector3& bottomColor,
                               const Math::Vector3& horizonColor) {
    const u32 faceSize = 64;

    if (!CreateCubemapImage(faceSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create procedural skybox cubemap image");
        return false;
    }

    // Generate gradient data for each face
    // Face directions: +X, -X, +Y, -Y, +Z, -Z
    struct FaceDir { float dx, dy, dz; };
    FaceDir faceDirs[6] = {
        { 1, 0, 0}, {-1, 0, 0}, { 0, 1, 0}, { 0,-1, 0}, { 0, 0, 1}, { 0, 0,-1}
    };
    // Up vectors for each face
    FaceDir faceUps[6] = {
        { 0,-1, 0}, { 0,-1, 0}, { 0, 0, 1}, { 0, 0,-1}, { 0,-1, 0}, { 0,-1, 0}
    };
    // Right vectors for each face
    FaceDir faceRights[6] = {
        { 0, 0,-1}, { 0, 0, 1}, { 1, 0, 0}, { 1, 0, 0}, { 1, 0, 0}, {-1, 0, 0}
    };

    std::vector<std::unique_ptr<u8[]>> faceData(6);
    usize faceBytes = faceSize * faceSize * 4;

    for (int f = 0; f < 6; ++f) {
        faceData[f] = std::make_unique<u8[]>(faceBytes);
        for (u32 y = 0; y < faceSize; ++y) {
            for (u32 x = 0; x < faceSize; ++x) {
                float u = (x + 0.5f) / faceSize * 2.0f - 1.0f;
                float v = (y + 0.5f) / faceSize * 2.0f - 1.0f;

                // Compute direction and normalize
                float dx = faceDirs[f].dx + faceRights[f].dx * u + faceUps[f].dx * v;
                float dy = faceDirs[f].dy + faceRights[f].dy * u + faceUps[f].dy * v;
                float dz = faceDirs[f].dz + faceRights[f].dz * u + faceUps[f].dz * v;
                float len = std::sqrt(dx*dx + dy*dy + dz*dz);
                dx /= len;
                dy /= len;
                dz /= len;

                // Map vertical direction to color
                float t = dy * 0.5f + 0.5f; // [0, 1] from bottom to top
                Math::Vector3 color;
                // 30% horizon band (0.35..0.65) for a softer atmospheric gradient
                // that reads better in path-traced GI bounce
                if (t > 0.65f) {
                    float blend = (t - 0.65f) / 0.35f;
                    color = horizonColor * (1.0f - blend) + topColor * blend;
                } else if (t < 0.35f) {
                    float blend = (0.35f - t) / 0.35f;
                    color = horizonColor * (1.0f - blend) + bottomColor * blend;
                } else {
                    color = horizonColor;
                }

                usize idx = (y * faceSize + x) * 4;
                faceData[f][idx + 0] = static_cast<u8>(std::clamp(color.x * 255.0f, 0.0f, 255.0f));
                faceData[f][idx + 1] = static_cast<u8>(std::clamp(color.y * 255.0f, 0.0f, 255.0f));
                faceData[f][idx + 2] = static_cast<u8>(std::clamp(color.z * 255.0f, 0.0f, 255.0f));
                faceData[f][idx + 3] = 255;
            }
        }
    }

    UploadFaces(faceData, faceSize);

    ENJIN_LOG_INFO(Renderer, "Created procedural skybox (%ux%u per face)", faceSize, faceSize);
    return true;
}

bool Skybox::CreateSolidColor(const Math::Vector3& color) {
    const u32 faceSize = 4;

    if (!CreateCubemapImage(faceSize)) {
        return false;
    }

    u8 r = static_cast<u8>(std::clamp(color.x * 255.0f, 0.0f, 255.0f));
    u8 g = static_cast<u8>(std::clamp(color.y * 255.0f, 0.0f, 255.0f));
    u8 b = static_cast<u8>(std::clamp(color.z * 255.0f, 0.0f, 255.0f));

    std::vector<std::unique_ptr<u8[]>> faceData(6);
    usize faceBytes = faceSize * faceSize * 4;

    for (int f = 0; f < 6; ++f) {
        faceData[f] = std::make_unique<u8[]>(faceBytes);
        for (u32 i = 0; i < faceSize * faceSize; ++i) {
            faceData[f][i * 4 + 0] = r;
            faceData[f][i * 4 + 1] = g;
            faceData[f][i * 4 + 2] = b;
            faceData[f][i * 4 + 3] = 255;
        }
    }

    UploadFaces(faceData, faceSize);

    ENJIN_LOG_INFO(Renderer, "Created solid color skybox");
    return true;
}

bool Skybox::LoadCubemap(const std::array<std::string, 6>& facePaths) {
    static const char* faceNames[6] = {
        "+X (Right)", "-X (Left)", "+Y (Top)", "-Y (Bottom)", "+Z (Front)", "-Z (Back)"
    };

    // Load the first face to determine dimensions
    int firstWidth = 0, firstHeight = 0, firstChannels = 0;
    bool isHdr = stbi_is_hdr(facePaths[0].c_str()) != 0;

    // Load all 6 faces as RGBA8
    std::vector<std::unique_ptr<u8[]>> faceData(6);
    u32 faceSize = 0;

    for (usize i = 0; i < 6; ++i) {
        if (facePaths[i].empty()) {
            ENJIN_LOG_WARN(Renderer, "Cubemap face %s has empty path, falling back to procedural", faceNames[i]);
            return CreateProcedural(
                Math::Vector3(0.1f, 0.2f, 0.6f),
                Math::Vector3(0.4f, 0.3f, 0.2f),
                Math::Vector3(0.5f, 0.7f, 0.9f)
            );
        }

        int width = 0, height = 0, channels = 0;

        if (isHdr) {
            // Load HDR image as float, then convert to RGBA8
            float* hdrPixels = stbi_loadf(facePaths[i].c_str(), &width, &height, &channels, 4);
            if (!hdrPixels) {
                ENJIN_LOG_WARN(Renderer, "Failed to load HDR cubemap face %s: %s, falling back to procedural",
                    faceNames[i], facePaths[i].c_str());
                return CreateProcedural(
                    Math::Vector3(0.1f, 0.2f, 0.6f),
                    Math::Vector3(0.4f, 0.3f, 0.2f),
                    Math::Vector3(0.5f, 0.7f, 0.9f)
                );
            }

            // Use first face dimensions as the canonical size
            if (i == 0) {
                firstWidth = width;
                firstHeight = height;
                faceSize = static_cast<u32>(std::min(firstWidth, firstHeight));
            }

            // Verify all faces match the first face dimensions
            if (width != firstWidth || height != firstHeight) {
                ENJIN_LOG_WARN(Renderer, "Cubemap face %s size %dx%d differs from first face %dx%d, falling back to procedural",
                    faceNames[i], width, height, firstWidth, firstHeight);
                stbi_image_free(hdrPixels);
                return CreateProcedural(
                    Math::Vector3(0.1f, 0.2f, 0.6f),
                    Math::Vector3(0.4f, 0.3f, 0.2f),
                    Math::Vector3(0.5f, 0.7f, 0.9f)
                );
            }

            // Convert HDR float data to RGBA8 with tone mapping
            usize pixelCount = static_cast<usize>(width) * static_cast<usize>(height);
            faceData[i] = std::make_unique<u8[]>(pixelCount * 4);
            for (usize p = 0; p < pixelCount; ++p) {
                for (usize c = 0; c < 3; ++c) {
                    // Simple Reinhard tone mapping
                    f32 val = hdrPixels[p * 4 + c];
                    val = val / (1.0f + val);
                    faceData[i][p * 4 + c] = static_cast<u8>(std::clamp(val * 255.0f, 0.0f, 255.0f));
                }
                faceData[i][p * 4 + 3] = 255;
            }
            stbi_image_free(hdrPixels);
        } else {
            // Load LDR image directly as RGBA8
            u8* pixels = stbi_load(facePaths[i].c_str(), &width, &height, &channels, 4);
            if (!pixels) {
                ENJIN_LOG_WARN(Renderer, "Failed to load cubemap face %s: %s, falling back to procedural",
                    faceNames[i], facePaths[i].c_str());
                return CreateProcedural(
                    Math::Vector3(0.1f, 0.2f, 0.6f),
                    Math::Vector3(0.4f, 0.3f, 0.2f),
                    Math::Vector3(0.5f, 0.7f, 0.9f)
                );
            }

            // Use first face dimensions as the canonical size
            if (i == 0) {
                firstWidth = width;
                firstHeight = height;
                faceSize = static_cast<u32>(std::min(firstWidth, firstHeight));
            }

            // Verify all faces match the first face dimensions
            if (width != firstWidth || height != firstHeight) {
                ENJIN_LOG_WARN(Renderer, "Cubemap face %s size %dx%d differs from first face %dx%d, falling back to procedural",
                    faceNames[i], width, height, firstWidth, firstHeight);
                stbi_image_free(pixels);
                return CreateProcedural(
                    Math::Vector3(0.1f, 0.2f, 0.6f),
                    Math::Vector3(0.4f, 0.3f, 0.2f),
                    Math::Vector3(0.5f, 0.7f, 0.9f)
                );
            }

            // Transfer ownership to unique_ptr
            usize faceBytes = static_cast<usize>(width) * static_cast<usize>(height) * 4;
            faceData[i] = std::make_unique<u8[]>(faceBytes);
            std::memcpy(faceData[i].get(), pixels, faceBytes);
            stbi_image_free(pixels);
        }
    }

    // Create the Vulkan cubemap image
    if (!CreateCubemapImage(faceSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create cubemap image for loaded faces");
        return CreateProcedural(
            Math::Vector3(0.1f, 0.2f, 0.6f),
            Math::Vector3(0.4f, 0.3f, 0.2f),
            Math::Vector3(0.5f, 0.7f, 0.9f)
        );
    }

    // Upload all 6 faces to the GPU
    UploadFaces(faceData, faceSize);

    ENJIN_LOG_INFO(Renderer, "Loaded cubemap skybox (%ux%u per face, %s)",
        faceSize, faceSize, isHdr ? "HDR" : "LDR");
    return true;
}

void Skybox::UploadFaces(const std::vector<std::unique_ptr<u8[]>>& faceData, u32 faceSize) {
    if (!m_Context || faceData.size() < 6) return;

    VkDevice device = m_Context->GetDevice();
    usize faceBytes = faceSize * faceSize * 4;
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
        ENJIN_LOG_ERROR(Renderer, "Failed to create skybox staging buffer");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memProps);

    u32 memTypeIndex = UINT32_MAX;
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
            memTypeIndex = i;
            break;
        }
    }

    if (memTypeIndex == UINT32_MAX) {
        ENJIN_LOG_ERROR(Renderer, "Failed to find suitable memory type for skybox staging buffer");
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate skybox staging memory");
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return;
    }
    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    // Copy face data to staging buffer
    void* mapped = nullptr;
    if (vkMapMemory(device, stagingMemory, 0, totalBytes, 0, &mapped) != VK_SUCCESS || !mapped) {
        ENJIN_LOG_ERROR(Renderer, "Failed to map skybox staging buffer memory");
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return;
    }
    for (int f = 0; f < 6; ++f) {
        memcpy(static_cast<u8*>(mapped) + f * faceBytes, faceData[f].get(), faceBytes);
    }
    vkUnmapMemory(device, stagingMemory);

    // Create temporary command pool and buffer for upload
    VkCommandPool tempPool = VK_NULL_HANDLE;
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &tempPool) != VK_SUCCESS) {
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingMemory, nullptr);
            return;
        }
    }

    VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
    {
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = tempPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmdBuf) != VK_SUCCESS) {
            vkDestroyCommandPool(device, tempPool, nullptr);
            vkDestroyBuffer(device, stagingBuffer, nullptr);
            vkFreeMemory(device, stagingMemory, nullptr);
            return;
        }
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuf, &beginInfo);

    // Transition to TRANSFER_DST
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_CubemapImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmdBuf,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy each face
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
        region.imageExtent = { faceSize, faceSize, 1 };

        vkCmdCopyBufferToImage(cmdBuf, stagingBuffer, m_CubemapImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    // Transition to SHADER_READ_ONLY
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmdBuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmdBuf);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to submit skybox upload commands");
    }
    vkQueueWaitIdle(m_Context->GetGraphicsQueue());

    vkDestroyCommandPool(device, tempPool, nullptr);

    // Clean up staging
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}

void Skybox::SetConfig(const SkyboxConfig& config) {
    m_Config = config;

    switch (config.type) {
        case SkyboxType::Cubemap:
            LoadCubemap(config.cubemapPaths);
            break;
        case SkyboxType::Procedural:
            CreateProcedural(config.topColor, config.bottomColor, config.horizonColor);
            break;
        case SkyboxType::SolidColor:
            CreateSolidColor(config.solidColor);
            break;
        case SkyboxType::None:
        default:
            DestroyImage();
            break;
    }
}

bool Skybox::CreateSampler() {
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
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    if (vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create skybox sampler");
        return false;
    }
    return true;
}

VkDescriptorImageInfo Skybox::GetDescriptorInfo() const {
    VkDescriptorImageInfo info{};
    info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.imageView = m_CubemapView;
    info.sampler = m_Sampler;
    return info;
}

} // namespace Renderer
} // namespace Enjin
