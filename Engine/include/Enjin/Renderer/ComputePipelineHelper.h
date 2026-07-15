#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Logging/Log.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <initializer_list>

namespace Enjin {
namespace Renderer {

// Descriptor binding type shorthand
enum class BindType : u8 {
    StorageBuffer,
    UniformBuffer,
    StorageImage,
    SampledImage,
    Sampler,
    CombinedImageSampler
};

// Helper to reduce Vulkan compute pipeline creation boilerplate.
// Creates descriptor set layout, pipeline layout, pipeline, descriptor pool,
// and allocates + writes descriptor sets in a single call chain.
struct ENJIN_API ComputePipelineSetup {
    VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkDescriptorPool descPool = VK_NULL_HANDLE;
    VkDescriptorSet descSet = VK_NULL_HANDLE;

    // Create everything from a binding list and shader path.
    // bindings: list of {binding index, type} pairs.
    // Returns true if all Vulkan objects were created successfully.
    bool Create(VulkanContext* context,
                const std::vector<std::pair<u32, BindType>>& bindings,
                const char* shaderName) {

        VkDevice device = context->GetDevice();

        // --- Descriptor set layout ---
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings(bindings.size());
        for (usize i = 0; i < bindings.size(); ++i) {
            layoutBindings[i] = {};
            layoutBindings[i].binding = bindings[i].first;
            layoutBindings[i].descriptorCount = 1;
            layoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            switch (bindings[i].second) {
                case BindType::StorageBuffer:       layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; break;
                case BindType::UniformBuffer:       layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
                case BindType::StorageImage:        layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; break;
                case BindType::SampledImage:        layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE; break;
                case BindType::Sampler:             layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; break;
                case BindType::CombinedImageSampler:layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; break;
            }
        }

        VkDescriptorSetLayoutCreateInfo layoutCI{};
        layoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutCI.bindingCount = static_cast<u32>(layoutBindings.size());
        layoutCI.pBindings = layoutBindings.data();
        if (vkCreateDescriptorSetLayout(device, &layoutCI, nullptr, &descLayout) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "ComputePipelineHelper: failed to create desc layout for '%s'", shaderName);
            return false;
        }

        // --- Pipeline layout ---
        VkPipelineLayoutCreateInfo plCI{};
        plCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plCI.setLayoutCount = 1;
        plCI.pSetLayouts = &descLayout;
        if (vkCreatePipelineLayout(device, &plCI, nullptr, &pipelineLayout) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "ComputePipelineHelper: failed to create pipeline layout for '%s'", shaderName);
            return false;
        }

        // --- Load shader ---
        VulkanShader shader(context);
        // NOTE: the exe sets cwd to its own directory (Application.cpp:78). In a dev
        // build that is build/bin/Release/, so repo-root Engine/shaders/ is THREE
        // levels up (../../../). The prior list stopped at two levels, so every
        // file-loaded compute shader (particle sim, DDGI voxelize/probe, volumetric
        // fog) silently fell through to the null-pipeline fallback. Keep the deployed
        // "shaders/" next-to-exe path first; add the deeper dev-tree depths.
        std::string paths[] = {
            std::string("shaders/") + shaderName + ".spv",
            std::string("Engine/shaders/") + shaderName + ".spv",
            std::string("../Engine/shaders/") + shaderName + ".spv",
            std::string("../../Engine/shaders/") + shaderName + ".spv",
            std::string("../../../Engine/shaders/") + shaderName + ".spv",
            std::string("../../../../Engine/shaders/") + shaderName + ".spv"
        };
        bool loaded = false;
        for (const auto& path : paths) {
            if (shader.LoadFromFile(path.c_str()) && shader.GetModule() != VK_NULL_HANDLE) {
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            ENJIN_LOG_WARN(Renderer, "ComputePipelineHelper: shader '%s' not found (will load at runtime)", shaderName);
            // Pipeline stays null — system gracefully handles this
            return true;
        }

        // --- Pipeline ---
        VkPipelineShaderStageCreateInfo stageCI{};
        stageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageCI.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageCI.module = shader.GetModule();
        stageCI.pName = "main";

        VkComputePipelineCreateInfo pipelineCI{};
        pipelineCI.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineCI.layout = pipelineLayout;
        pipelineCI.stage = stageCI;
        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &pipeline) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "ComputePipelineHelper: failed to create pipeline for '%s'", shaderName);
            return false;
        }

        // --- Descriptor pool + set ---
        // Count descriptor types needed
        u32 storageCount = 0, uniformCount = 0, imageCount = 0, samplerCount = 0;
        for (const auto& b : bindings) {
            switch (b.second) {
                case BindType::StorageBuffer: storageCount++; break;
                case BindType::UniformBuffer: uniformCount++; break;
                case BindType::StorageImage:  imageCount++; break;
                case BindType::SampledImage:  imageCount++; break;
                case BindType::Sampler:       samplerCount++; break;
                case BindType::CombinedImageSampler: samplerCount++; break;
            }
        }

        std::vector<VkDescriptorPoolSize> poolSizes;
        if (storageCount) poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, storageCount });
        if (uniformCount) poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniformCount });
        if (imageCount)   poolSizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageCount });
        if (samplerCount) poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, samplerCount });

        VkDescriptorPoolCreateInfo poolCI{};
        poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCI.poolSizeCount = static_cast<u32>(poolSizes.size());
        poolCI.pPoolSizes = poolSizes.data();
        poolCI.maxSets = 1;
        if (vkCreateDescriptorPool(device, &poolCI, nullptr, &descPool) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "ComputePipelineHelper: failed to create desc pool for '%s'", shaderName);
            return false;
        }

        VkDescriptorSetAllocateInfo allocCI{};
        allocCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocCI.descriptorPool = descPool;
        allocCI.descriptorSetCount = 1;
        allocCI.pSetLayouts = &descLayout;
        if (vkAllocateDescriptorSets(device, &allocCI, &descSet) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "ComputePipelineHelper: failed to allocate desc set for '%s'", shaderName);
            return false;
        }

        ENJIN_LOG_INFO(Renderer, "ComputePipelineHelper: created pipeline for '%s' (%zu bindings)",
                       shaderName, bindings.size());
        return true;
    }

    // Write a buffer to a descriptor binding
    void WriteBuffer(VkDevice device, u32 binding, VkBuffer buffer, VkDeviceSize size,
                     VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = size;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descSet;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // Write an image to a descriptor binding (storage or sampled)
    void WriteImage(VkDevice device, u32 binding, VkImageView view, VkSampler sampler,
                    VkImageLayout layout, VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = view;
        imageInfo.sampler = sampler;
        imageInfo.imageLayout = layout;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descSet;
        write.dstBinding = binding;
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // Write a storage image to a descriptor binding
    void WriteStorageImage(VkDevice device, u32 binding, VkImageView view,
                           VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL) {
        WriteImage(device, binding, view, VK_NULL_HANDLE, layout, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }

    // Dispatch the compute pipeline
    void Dispatch(VkCommandBuffer cmd, u32 groupsX, u32 groupsY = 1, u32 groupsZ = 1) {
        if (!pipeline) return;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout,
                                 0, 1, &descSet, 0, nullptr);
        vkCmdDispatch(cmd, groupsX, groupsY, groupsZ);
    }

    // Insert a compute → compute/fragment barrier
    void Barrier(VkCommandBuffer cmd,
                 VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                 VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT) {
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    void Destroy(VkDevice device) {
        if (pipeline)       { vkDestroyPipeline(device, pipeline, nullptr); pipeline = VK_NULL_HANDLE; }
        if (pipelineLayout) { vkDestroyPipelineLayout(device, pipelineLayout, nullptr); pipelineLayout = VK_NULL_HANDLE; }
        if (descPool)       { vkDestroyDescriptorPool(device, descPool, nullptr); descPool = VK_NULL_HANDLE; }
        if (descLayout)     { vkDestroyDescriptorSetLayout(device, descLayout, nullptr); descLayout = VK_NULL_HANDLE; }
        descSet = VK_NULL_HANDLE;
    }

    bool IsValid() const { return pipeline != VK_NULL_HANDLE; }
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
