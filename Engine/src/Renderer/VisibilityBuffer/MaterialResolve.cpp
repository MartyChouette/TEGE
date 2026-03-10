#include "Enjin/Renderer/VisibilityBuffer/MaterialResolve.h"
#include "Enjin/Renderer/VisibilityBuffer/VisibilityBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

// Update the material resolve descriptor set with actual buffer/image bindings.
// Called once after visibility pass resources are ready, or when buffers change.
bool UpdateResolveDescriptors(
    VulkanContext* context,
    VkDescriptorSet descriptorSet,
    VkImageView visibilityBufferView,
    VkSampler depthSampler,
    VkImageView depthBufferView,
    VkImageView outputColorView,
    VkBuffer vertexBuffer,
    VkDeviceSize vertexBufferSize,
    VkBuffer objectDataBuffer,
    VkDeviceSize objectDataBufferSize
) {
    if (!context || descriptorSet == VK_NULL_HANDLE) return false;

    // Binding 0: Visibility buffer (storage image, R32G32_UINT)
    VkDescriptorImageInfo visInfo{};
    visInfo.imageView = visibilityBufferView;
    visInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Binding 1: Depth buffer (sampled image)
    VkDescriptorImageInfo depthInfo{};
    depthInfo.sampler = depthSampler;
    depthInfo.imageView = depthBufferView;
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    // Binding 2: Output color (storage image)
    VkDescriptorImageInfo outputInfo{};
    outputInfo.imageView = outputColorView;
    outputInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Binding 3: Vertex buffer (SSBO)
    VkDescriptorBufferInfo vertexBufInfo{};
    vertexBufInfo.buffer = vertexBuffer;
    vertexBufInfo.offset = 0;
    vertexBufInfo.range = vertexBufferSize > 0 ? vertexBufferSize : VK_WHOLE_SIZE;

    // Binding 4: Object data (SSBO)
    VkDescriptorBufferInfo objectBufInfo{};
    objectBufInfo.buffer = objectDataBuffer;
    objectBufInfo.offset = 0;
    objectBufInfo.range = objectDataBufferSize > 0 ? objectDataBufferSize : VK_WHOLE_SIZE;

    VkWriteDescriptorSet writes[5] = {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].pImageInfo = &visInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].pImageInfo = &depthInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = descriptorSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[2].pImageInfo = &outputInfo;

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = descriptorSet;
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].pBufferInfo = &vertexBufInfo;

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = descriptorSet;
    writes[4].dstBinding = 4;
    writes[4].descriptorCount = 1;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[4].pBufferInfo = &objectBufInfo;

    vkUpdateDescriptorSets(context->GetDevice(), 5, writes, 0, nullptr);
    return true;
}

} // namespace Renderer
} // namespace Enjin
