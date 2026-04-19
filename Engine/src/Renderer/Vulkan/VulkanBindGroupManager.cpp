#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Renderer/Vulkan/VulkanBindGroupManager.h"
#include "Enjin/Renderer/Vulkan/VulkanBufferManager.h"
#include "Enjin/Renderer/Vulkan/VulkanTextureManager.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Texture.h"
#include "Enjin/Logging/Log.h"
#include <array>

namespace Enjin::Renderer {

VulkanBindGroupManager::VulkanBindGroupManager(VulkanContext* context,
                                               VulkanBufferManager* bufferMgr,
                                               VulkanTextureManager* textureMgr)
    : m_Context(context), m_BufferMgr(bufferMgr), m_TextureMgr(textureMgr) {}

VulkanBindGroupManager::~VulkanBindGroupManager() {
    Shutdown();
}

void VulkanBindGroupManager::Shutdown() {
    VkDevice device = m_Context->GetDevice();

    m_SetPool.Clear();  // Sets are freed when pools are destroyed
    m_LayoutPool.ForEach([device](VulkanBindGroupLayoutSlot& slot) {
        if (slot.layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, slot.layout, nullptr);
        }
    });
    m_LayoutPool.Clear();

    for (auto pool : m_DescriptorPools) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    m_DescriptorPools.clear();
    m_SetsAllocated = 0;
}

VkDescriptorType VulkanBindGroupManager::TranslateBindingType(GPUBindingType type) {
    switch (type) {
        case GPUBindingType::UniformBuffer:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case GPUBindingType::StorageBuffer:        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case GPUBindingType::StorageBufferReadOnly: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case GPUBindingType::SampledTexture:        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case GPUBindingType::StorageTexture:        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case GPUBindingType::Sampler:               return VK_DESCRIPTOR_TYPE_SAMPLER;
        case GPUBindingType::ComparisonSampler:     return VK_DESCRIPTOR_TYPE_SAMPLER;
        case GPUBindingType::DepthTexture:          return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    }
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

VkShaderStageFlags VulkanBindGroupManager::TranslateVisibility(GPUShaderStage stage) {
    VkShaderStageFlags flags = 0;
    if (stage & GPUShaderStage::Vertex)   flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stage & GPUShaderStage::Fragment) flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stage & GPUShaderStage::Compute)  flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    return flags;
}

void VulkanBindGroupManager::EnsurePoolCapacity() {
    if (!m_DescriptorPools.empty() && m_SetsAllocated < m_DescriptorPools.size() * SETS_PER_POOL) {
        return;  // Current pools have capacity
    }

    // Create a new pool with generous type counts
    std::array<VkDescriptorPoolSize, 4> poolSizes = {{
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         SETS_PER_POOL * 4 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         SETS_PER_POOL * 2 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          SETS_PER_POOL * 4 },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                SETS_PER_POOL * 4 },
    }};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = SETS_PER_POOL;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool pool;
    if (vkCreateDescriptorPool(m_Context->GetDevice(), &poolInfo, nullptr, &pool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Core, "VulkanBindGroupManager: Failed to create descriptor pool");
        return;
    }
    m_DescriptorPools.push_back(pool);
}

GPUBindGroupLayoutHandle VulkanBindGroupManager::CreateBindGroupLayout(const GPUBindGroupLayoutDesc& desc) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (const auto& entry : desc.entries) {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = entry.binding;
        binding.descriptorType = TranslateBindingType(entry.type);
        binding.descriptorCount = 1;
        binding.stageFlags = TranslateVisibility(entry.visibility);
        bindings.push_back(binding);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(m_Context->GetDevice(), &layoutInfo, nullptr, &layout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Core, "VulkanBindGroupManager: Failed to create descriptor set layout");
        return {};
    }

    auto handle = m_LayoutPool.Allocate();
    auto* slot = m_LayoutPool.Get(handle);
    slot->layout = layout;
    slot->entries = desc.entries;
    return GPUBindGroupLayoutHandle{handle};
}

GPUBindGroupHandle VulkanBindGroupManager::CreateBindGroup(const GPUBindGroupDesc& desc) {
    auto* layoutSlot = m_LayoutPool.Get(desc.layout.id);
    if (!layoutSlot || layoutSlot->layout == VK_NULL_HANDLE) {
        ENJIN_LOG_ERROR(Core, "VulkanBindGroupManager: Invalid layout handle");
        return {};
    }

    EnsurePoolCapacity();

    // Allocate descriptor set from the last pool
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPools.back();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layoutSlot->layout;

    VkDescriptorSet set;
    VkResult result = vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, &set);
    if (result != VK_SUCCESS) {
        // Pool might be full — try creating a new one
        EnsurePoolCapacity();
        allocInfo.descriptorPool = m_DescriptorPools.back();
        if (vkAllocateDescriptorSets(m_Context->GetDevice(), &allocInfo, &set) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Core, "VulkanBindGroupManager: Failed to allocate descriptor set");
            return {};
        }
    }
    m_SetsAllocated++;

    // Write descriptors
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;
    bufferInfos.reserve(desc.entries.size());
    imageInfos.reserve(desc.entries.size());

    for (const auto& entry : desc.entries) {
        // Find the matching layout entry to determine type
        GPUBindingType bindingType = GPUBindingType::UniformBuffer;
        for (const auto& le : layoutSlot->entries) {
            if (le.binding == entry.binding) {
                bindingType = le.type;
                break;
            }
        }

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = entry.binding;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = TranslateBindingType(bindingType);

        if (bindingType == GPUBindingType::UniformBuffer ||
            bindingType == GPUBindingType::StorageBuffer ||
            bindingType == GPUBindingType::StorageBufferReadOnly) {
            VulkanBuffer* buf = m_BufferMgr->GetNativeBuffer(entry.buffer);
            if (!buf) continue;
            auto& info = bufferInfos.emplace_back();
            info.buffer = buf->GetBuffer();
            info.offset = entry.bufferOffset;
            info.range = entry.bufferSize > 0 ? entry.bufferSize : buf->GetSize();
            write.pBufferInfo = &info;
        } else if (bindingType == GPUBindingType::SampledTexture ||
                   bindingType == GPUBindingType::DepthTexture) {
            Texture* tex = m_TextureMgr->GetNativeTexture(entry.texture);
            if (!tex || !tex->IsValid()) continue;
            auto& info = imageInfos.emplace_back();
            info.imageView = tex->GetImageView();
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            info.sampler = VK_NULL_HANDLE;
            write.pImageInfo = &info;
        } else if (bindingType == GPUBindingType::Sampler ||
                   bindingType == GPUBindingType::ComparisonSampler) {
            // Sampler stored via the texture handle's sampler
            Texture* tex = m_TextureMgr->GetNativeTexture(entry.sampler);
            if (!tex || !tex->IsValid()) continue;
            auto& info = imageInfos.emplace_back();
            info.sampler = tex->GetSampler();
            info.imageView = VK_NULL_HANDLE;
            info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            write.pImageInfo = &info;
        }

        writes.push_back(write);
    }

    if (!writes.empty()) {
        vkUpdateDescriptorSets(m_Context->GetDevice(),
            static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }

    auto handle = m_SetPool.Allocate();
    auto* slot = m_SetPool.Get(handle);
    slot->set = set;
    slot->layoutHandle = desc.layout;
    return GPUBindGroupHandle{handle};
}

void VulkanBindGroupManager::DestroyBindGroup(GPUBindGroupHandle handle) {
    auto* slot = m_SetPool.Get(handle.id);
    if (!slot) return;
    // Descriptor sets are returned to pool when pool is destroyed or reset.
    // With FREE_DESCRIPTOR_SET_BIT, we can free individually:
    if (slot->set != VK_NULL_HANDLE && !m_DescriptorPools.empty()) {
        vkFreeDescriptorSets(m_Context->GetDevice(), m_DescriptorPools.back(), 1, &slot->set);
        m_SetsAllocated--;
    }
    m_SetPool.Free(handle.id);
}

void VulkanBindGroupManager::DestroyBindGroupLayout(GPUBindGroupLayoutHandle handle) {
    auto* slot = m_LayoutPool.Get(handle.id);
    if (!slot) return;
    if (slot->layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_Context->GetDevice(), slot->layout, nullptr);
    }
    m_LayoutPool.Free(handle.id);
}

bool VulkanBindGroupManager::IsValid(GPUBindGroupHandle handle) const {
    return m_SetPool.IsValid(handle.id);
}

bool VulkanBindGroupManager::IsValidLayout(GPUBindGroupLayoutHandle handle) const {
    return m_LayoutPool.IsValid(handle.id);
}

VkDescriptorSetLayout VulkanBindGroupManager::GetNativeLayout(GPUBindGroupLayoutHandle handle) {
    auto* slot = m_LayoutPool.Get(handle.id);
    return slot ? slot->layout : VK_NULL_HANDLE;
}

VkDescriptorSet VulkanBindGroupManager::GetNativeSet(GPUBindGroupHandle handle) {
    auto* slot = m_SetPool.Get(handle.id);
    return slot ? slot->set : VK_NULL_HANDLE;
}

} // namespace Enjin::Renderer

#endif // !ENJIN_RENDERER_WEBGPU
