#include "Enjin/Renderer/Vulkan/BindlessResources.h"
#include "Enjin/Renderer/Vulkan/VulkanImage.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <algorithm>

namespace Enjin {
namespace Renderer {

BindlessResourceManager::BindlessResourceManager(VulkanContext* context)
    : m_Context(context) {
}

BindlessResourceManager::~BindlessResourceManager() {
    Shutdown();
}

bool BindlessResourceManager::Initialize() {
    ENJIN_LOG_INFO(Renderer, "Initializing Bindless Resource Manager...");

    // Check for descriptor indexing extension
    // In production, you'd check VkPhysicalDeviceFeatures2 for VkPhysicalDeviceDescriptorIndexingFeatures
    
    if (!CreateDescriptorSetLayout()) {
        return false;
    }

    if (!AllocateDescriptorSet()) {
        return false;
    }

    // Pre-allocate texture/buffer arrays
    m_Textures.resize(MAX_TEXTURES);
    m_Buffers.resize(MAX_BUFFERS);
    m_TextureCount = 0;
    m_BufferCount = 0;
    m_TextureHighWater = 0;
    m_BufferHighWater = 0;

    ENJIN_LOG_INFO(Renderer, "Bindless Resource Manager initialized (max textures: %u, max buffers: %u)", 
        MAX_TEXTURES, MAX_BUFFERS);
    return true;
}

void BindlessResourceManager::Shutdown() {
    if (m_DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_Context->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    if (m_DescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_Context->GetDevice(), m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }

    m_Textures.clear();
    m_Buffers.clear();
    m_FreeTextureSlots.clear();
    m_FreeBufferSlots.clear();
}

BindlessHandle BindlessResourceManager::RegisterTexture(VkImageView imageView, VkSampler sampler) {
    if (!imageView || !sampler) {
        ENJIN_LOG_ERROR(Renderer, "Invalid texture parameters");
        return INVALID_BINDLESS_HANDLE;
    }

    BindlessHandle handle = INVALID_BINDLESS_HANDLE;

    // Reuse free slot if available, otherwise append at high-water mark
    if (!m_FreeTextureSlots.empty()) {
        handle = m_FreeTextureSlots.back();
        m_FreeTextureSlots.pop_back();
    } else if (m_TextureHighWater < MAX_TEXTURES) {
        handle = m_TextureHighWater;
    }

    if (handle == INVALID_BINDLESS_HANDLE || handle >= MAX_TEXTURES) {
        ENJIN_LOG_ERROR(Renderer, "No free texture slots available");
        return INVALID_BINDLESS_HANDLE;
    }

    m_Textures[handle].imageView = imageView;
    m_Textures[handle].sampler = sampler;
    m_Textures[handle].valid = true;
    ++m_TextureCount;
    if (handle + 1 > m_TextureHighWater) m_TextureHighWater = handle + 1;

    m_Dirty = true;
    ENJIN_LOG_DEBUG(Renderer, "Registered texture at handle %u", handle);
    return handle;
}

BindlessHandle BindlessResourceManager::RegisterTexture(VulkanImage* image, VkSampler sampler) {
    if (!image || !image->GetImageView()) {
        ENJIN_LOG_ERROR(Renderer, "Invalid VulkanImage");
        return INVALID_BINDLESS_HANDLE;
    }
    
    // Use default sampler if none provided
    if (sampler == VK_NULL_HANDLE) {
        sampler = CreateDefaultSampler();
        if (sampler == VK_NULL_HANDLE) {
            return INVALID_BINDLESS_HANDLE;
        }
    }
    
    return RegisterTexture(image->GetImageView(), sampler);
}

VkSampler BindlessResourceManager::CreateDefaultSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    VkSampler sampler;
    VkResult result = vkCreateSampler(m_Context->GetDevice(), &samplerInfo, nullptr, &sampler);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create default sampler: %d", result);
        return VK_NULL_HANDLE;
    }
    
    return sampler;
}

void BindlessResourceManager::UnregisterTexture(BindlessHandle handle) {
    if (handle >= MAX_TEXTURES || !m_Textures[handle].valid) {
        return;
    }

    m_Textures[handle].valid = false;
    m_Textures[handle].imageView = VK_NULL_HANDLE;
    m_Textures[handle].sampler = VK_NULL_HANDLE;
    if (m_TextureCount > 0) --m_TextureCount;

    m_FreeTextureSlots.push_back(handle);
    m_Dirty = true;
}

BindlessHandle BindlessResourceManager::RegisterBuffer(VkBuffer buffer, VkDescriptorType type) {
    if (!buffer) {
        ENJIN_LOG_ERROR(Renderer, "Invalid buffer");
        return INVALID_BINDLESS_HANDLE;
    }

    BindlessHandle handle = INVALID_BINDLESS_HANDLE;

    // Reuse free slot if available, otherwise append at high-water mark
    if (!m_FreeBufferSlots.empty()) {
        handle = m_FreeBufferSlots.back();
        m_FreeBufferSlots.pop_back();
    } else if (m_BufferHighWater < MAX_BUFFERS) {
        handle = m_BufferHighWater;
    }

    if (handle == INVALID_BINDLESS_HANDLE || handle >= MAX_BUFFERS) {
        ENJIN_LOG_ERROR(Renderer, "No free buffer slots available");
        return INVALID_BINDLESS_HANDLE;
    }

    m_Buffers[handle].buffer = buffer;
    m_Buffers[handle].type = type;
    m_Buffers[handle].valid = true;
    ++m_BufferCount;
    if (handle + 1 > m_BufferHighWater) m_BufferHighWater = handle + 1;

    m_Dirty = true;
    ENJIN_LOG_DEBUG(Renderer, "Registered buffer at handle %u", handle);
    return handle;
}

void BindlessResourceManager::UnregisterBuffer(BindlessHandle handle) {
    if (handle >= MAX_BUFFERS || !m_Buffers[handle].valid) {
        return;
    }

    m_Buffers[handle].valid = false;
    m_Buffers[handle].buffer = VK_NULL_HANDLE;
    if (m_BufferCount > 0) --m_BufferCount;

    m_FreeBufferSlots.push_back(handle);
    m_Dirty = true;
}

void BindlessResourceManager::UpdateDescriptorSet() {
    if (!m_Dirty) {
        return;
    }

    RebuildDescriptorSet();
    m_Dirty = false;
}

bool BindlessResourceManager::CreateDescriptorSetLayout() {
    // Bindless descriptor set layout with descriptor indexing
    // Uses VK_EXT_descriptor_indexing extension features
    
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    // Binding 0: Combined Image Samplers (bindless textures)
    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 0;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = MAX_TEXTURES;
    textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    textureBinding.pImmutableSamplers = nullptr;
    bindings.push_back(textureBinding);

    // Binding 1: Storage Buffers (bindless buffers)
    VkDescriptorSetLayoutBinding bufferBinding{};
    bufferBinding.binding = 1;
    bufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bufferBinding.descriptorCount = MAX_BUFFERS;
    bufferBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    bufferBinding.pImmutableSamplers = nullptr;
    bindings.push_back(bufferBinding);

    // Enable descriptor indexing features
    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
    
    std::vector<VkDescriptorBindingFlagsEXT> flags(bindings.size());
    // VARIABLE_DESCRIPTOR_COUNT can only be set on the last binding in the layout (Vulkan spec).
    // Binding 0 (textures) gets PARTIALLY_BOUND + UPDATE_AFTER_BIND only.
    flags[0] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
               VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
    flags[1] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
               VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
               VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
    
    bindingFlags.bindingCount = static_cast<u32>(flags.size());
    bindingFlags.pBindingFlags = flags.data();

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = &bindingFlags;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;

    VkResult result = vkCreateDescriptorSetLayout(
        m_Context->GetDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout);
    
    if (result != VK_SUCCESS) {
        // Fallback: Try without extension features
        ENJIN_LOG_WARN(Renderer, "Failed to create bindless descriptor layout with extensions, trying fallback");
        
        layoutInfo.pNext = nullptr;
        layoutInfo.flags = 0;
        result = vkCreateDescriptorSetLayout(
            m_Context->GetDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout);
        
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create descriptor set layout: %d", result);
            return false;
        }
    }

    return true;
}

bool BindlessResourceManager::AllocateDescriptorSet() {
    // Create descriptor pool with update-after-bind flag
    std::vector<VkDescriptorPoolSize> poolSizes(2);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = MAX_TEXTURES;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = MAX_BUFFERS;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkResult result = vkCreateDescriptorPool(
        m_Context->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool);
    if (result != VK_SUCCESS) {
        // Fallback: Try without extension flag
        ENJIN_LOG_WARN(Renderer, "Failed to create bindless descriptor pool with extensions, trying fallback");
        
        poolInfo.flags = 0;
        result = vkCreateDescriptorPool(
            m_Context->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool);
        
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create descriptor pool: %d", result);
            return false;
        }
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_DescriptorSetLayout;

    result = vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, &m_DescriptorSet);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate descriptor set: %d", result);
        return false;
    }

    return true;
}

void BindlessResourceManager::RebuildDescriptorSet() {
    // Only iterate up to the high-water mark, not the full MAX capacity.
    // With PARTIALLY_BOUND, unused slots beyond the high-water mark are never accessed.
    u32 texCount = m_TextureHighWater;
    u32 bufCount = m_BufferHighWater;

    if (texCount == 0 && bufCount == 0) return;

    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorBufferInfo> bufferInfos;

    // Collect textures up to high-water mark
    if (texCount > 0) {
        imageInfos.resize(texCount);
        for (u32 i = 0; i < texCount; ++i) {
            if (m_Textures[i].valid) {
                imageInfos[i].imageView = m_Textures[i].imageView;
                imageInfos[i].sampler = m_Textures[i].sampler;
                imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            } else {
                // Null descriptor for unused slots (valid with PARTIALLY_BOUND)
                imageInfos[i].imageView = VK_NULL_HANDLE;
                imageInfos[i].sampler = VK_NULL_HANDLE;
                imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
        }

        VkWriteDescriptorSet textureWrite{};
        textureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        textureWrite.dstSet = m_DescriptorSet;
        textureWrite.dstBinding = 0;
        textureWrite.dstArrayElement = 0;
        textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureWrite.descriptorCount = texCount;
        textureWrite.pImageInfo = imageInfos.data();
        writes.push_back(textureWrite);
    }

    // Collect buffers up to high-water mark
    if (bufCount > 0) {
        bufferInfos.resize(bufCount);
        for (u32 i = 0; i < bufCount; ++i) {
            if (m_Buffers[i].valid) {
                bufferInfos[i].buffer = m_Buffers[i].buffer;
                bufferInfos[i].offset = 0;
                bufferInfos[i].range = VK_WHOLE_SIZE;
            } else {
                // Null descriptor for unused slots (valid with PARTIALLY_BOUND)
                bufferInfos[i].buffer = VK_NULL_HANDLE;
                bufferInfos[i].offset = 0;
                bufferInfos[i].range = 0;
            }
        }

        VkWriteDescriptorSet bufferWrite{};
        bufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bufferWrite.dstSet = m_DescriptorSet;
        bufferWrite.dstBinding = 1;
        bufferWrite.dstArrayElement = 0;
        bufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bufferWrite.descriptorCount = bufCount;
        bufferWrite.pBufferInfo = bufferInfos.data();
        writes.push_back(bufferWrite);
    }

    // Update descriptor set
    vkUpdateDescriptorSets(m_Context->GetDevice(),
        static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

    ENJIN_LOG_DEBUG(Renderer, "Rebuilt bindless descriptor set (%u textures, %u buffers)",
        m_TextureCount, m_BufferCount);
}

} // namespace Renderer
} // namespace Enjin
