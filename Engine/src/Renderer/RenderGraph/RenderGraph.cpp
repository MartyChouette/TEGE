#include "Enjin/Renderer/RenderGraph/RenderGraph.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <algorithm>
#include <queue>

namespace Enjin {
namespace Renderer {

// RenderPassNode implementation
RenderPassNode::RenderPassNode(const std::string& name)
    : m_Name(name) {
}

void RenderPassNode::AddColorInput(ResourceHandle handle) {
    m_Inputs.push_back(handle);
}

void RenderPassNode::AddColorOutput(ResourceHandle handle) {
    m_Outputs.push_back(handle);
}

void RenderPassNode::AddDepthInput(ResourceHandle handle) {
    m_Inputs.push_back(handle);
}

void RenderPassNode::AddDepthOutput(ResourceHandle handle) {
    m_Outputs.push_back(handle);
}

void RenderPassNode::AddSampledImage(ResourceHandle handle) {
    m_SampledImages.push_back(handle);
}

void RenderPassNode::AddStorageImage(ResourceHandle handle) {
    m_StorageImages.push_back(handle);
}

void RenderPassNode::AddUniformBuffer(ResourceHandle handle) {
    m_UniformBuffers.push_back(handle);
}

void RenderPassNode::AddStorageBuffer(ResourceHandle handle) {
    m_StorageBuffers.push_back(handle);
}

void RenderPassNode::SetExecuteCallback(std::function<void(VkCommandBuffer)> callback) {
    m_ExecuteCallback = callback;
}

void RenderPassNode::Execute(VkCommandBuffer cmd) const {
    if (m_ExecuteCallback) {
        m_ExecuteCallback(cmd);
    }
}

// ResourceNode implementation
ResourceNode::ResourceNode(const std::string& name, ResourceType type)
    : m_Name(name), m_Type(type) {
}

void ResourceNode::SetImage(VkImage image, VkFormat format, u32 width, u32 height) {
    m_Image = image;
    m_Format = format;
    m_Width = width;
    m_Height = height;
}

void ResourceNode::SetBuffer(VkBuffer buffer, VkDeviceSize size) {
    m_Buffer = buffer;
    m_Size = size;
}

// RenderGraph implementation
RenderGraph::RenderGraph(VulkanContext* context)
    : m_Context(context) {
    ENJIN_LOG_INFO(Renderer, "RenderGraph created");
}

RenderGraph::~RenderGraph() {
    Clear();
}

ResourceHandle RenderGraph::AddResource(const std::string& name, ResourceType type) {
    ResourceHandle handle = static_cast<ResourceHandle>(m_Resources.size());
    auto resource = std::make_unique<ResourceNode>(name, type);
    resource->SetHandle(handle);
    m_Resources.push_back(std::move(resource));
    m_ResourceNameMap[name] = handle;
    return handle;
}

ResourceNode* RenderGraph::GetResource(ResourceHandle handle) {
    if (handle >= m_Resources.size()) {
        return nullptr;
    }
    return m_Resources[handle].get();
}

RenderPassNode* RenderGraph::AddRenderPass(const std::string& name) {
    auto pass = std::make_unique<RenderPassNode>(name);
    RenderPassNode* passPtr = pass.get();
    m_Passes.push_back(std::move(pass));
    return passPtr;
}

bool RenderGraph::Build() {
    ENJIN_LOG_INFO(Renderer, "Building render graph...");
    
    ResolveDependencies();
    TopologicalSort();
    
    m_Built = true;
    ENJIN_LOG_INFO(Renderer, "Render graph built with %zu passes", m_OrderedPasses.size());
    return true;
}

void RenderGraph::Execute(VkCommandBuffer cmd) {
    if (!m_Built) {
        ENJIN_LOG_ERROR(Renderer, "Render graph not built - call Build() first");
        return;
    }
    
    for (RenderPassNode* pass : m_OrderedPasses) {
        InsertBarriers(cmd, pass);
        pass->Execute(cmd);
    }
}

void RenderGraph::Clear() {
    m_Resources.clear();
    m_Passes.clear();
    m_ResourceNameMap.clear();
    m_OrderedPasses.clear();
    m_Built = false;
}

ResourceHandle RenderGraph::GetResourceHandle(const std::string& name) const {
    auto it = m_ResourceNameMap.find(name);
    if (it != m_ResourceNameMap.end()) {
        return it->second;
    }
    return INVALID_RESOURCE_HANDLE;
}

void RenderGraph::ResolveDependencies() {
    // Build producer map: resource handle -> index of the pass that writes it
    m_ResourceProducer.clear();
    m_PassDependencies.clear();
    m_PassDependencies.resize(m_Passes.size());

    for (usize i = 0; i < m_Passes.size(); ++i) {
        auto* pass = m_Passes[i].get();

        // Outputs and storage images mark this pass as producer
        for (ResourceHandle h : pass->GetOutputs()) {
            m_ResourceProducer[h] = static_cast<u32>(i);
        }
        for (ResourceHandle h : pass->GetStorageImages()) {
            m_ResourceProducer[h] = static_cast<u32>(i);
        }
        for (ResourceHandle h : pass->GetStorageBuffers()) {
            m_ResourceProducer[h] = static_cast<u32>(i);
        }
    }

    // Build dependency edges: if pass i reads a resource produced by pass j, then i depends on j
    for (usize i = 0; i < m_Passes.size(); ++i) {
        auto* pass = m_Passes[i].get();
        auto addDep = [&](ResourceHandle h) {
            auto it = m_ResourceProducer.find(h);
            if (it != m_ResourceProducer.end() && it->second != static_cast<u32>(i)) {
                // Avoid duplicate dependencies
                auto& deps = m_PassDependencies[i];
                u32 producer = it->second;
                bool found = false;
                for (u32 d : deps) {
                    if (d == producer) { found = true; break; }
                }
                if (!found) deps.push_back(producer);
            }
        };

        for (ResourceHandle h : pass->GetInputs()) addDep(h);
        for (ResourceHandle h : pass->GetSampledImages()) addDep(h);
        for (ResourceHandle h : pass->GetUniformBuffers()) addDep(h);
    }
}

void RenderGraph::TopologicalSort() {
    // Kahn's algorithm for topological ordering
    usize n = m_Passes.size();
    m_OrderedPasses.clear();
    m_OrderedPasses.reserve(n);

    // Compute in-degree for each pass
    std::vector<u32> inDegree(n, 0);
    for (usize i = 0; i < n; ++i) {
        for (u32 dep : m_PassDependencies[i]) {
            (void)dep;
            // dep -> i edge means i has +1 in-degree
        }
    }
    // Actually count properly: for each pass i, each dependency in m_PassDependencies[i] is an incoming edge
    for (usize i = 0; i < n; ++i) {
        inDegree[i] = static_cast<u32>(m_PassDependencies[i].size());
    }

    // Start with passes that have no dependencies
    std::queue<u32> ready;
    for (u32 i = 0; i < static_cast<u32>(n); ++i) {
        if (inDegree[i] == 0) {
            ready.push(i);
        }
    }

    // Build adjacency list (forward edges: producer -> dependents)
    std::vector<std::vector<u32>> dependents(n);
    for (usize i = 0; i < n; ++i) {
        for (u32 dep : m_PassDependencies[i]) {
            dependents[dep].push_back(static_cast<u32>(i));
        }
    }

    while (!ready.empty()) {
        u32 current = ready.front();
        ready.pop();
        m_OrderedPasses.push_back(m_Passes[current].get());

        for (u32 dependent : dependents[current]) {
            inDegree[dependent]--;
            if (inDegree[dependent] == 0) {
                ready.push(dependent);
            }
        }
    }

    // If not all passes are ordered, there's a cycle - fall back to insertion order for remaining
    if (m_OrderedPasses.size() < n) {
        ENJIN_LOG_WARN(Renderer, "Render graph has cyclic dependencies, using fallback ordering for %zu passes",
                       n - m_OrderedPasses.size());
        for (auto& pass : m_Passes) {
            bool found = false;
            for (auto* ordered : m_OrderedPasses) {
                if (ordered == pass.get()) { found = true; break; }
            }
            if (!found) {
                m_OrderedPasses.push_back(pass.get());
            }
        }
    }

    // Assign execution order
    for (u32 i = 0; i < static_cast<u32>(m_OrderedPasses.size()); ++i) {
        m_OrderedPasses[i]->SetOrder(i);
    }
}

void RenderGraph::InsertBarriers(VkCommandBuffer cmd, RenderPassNode* pass) {
    // Insert memory barriers for resources used by this pass
    std::vector<VkImageMemoryBarrier> imageBarriers;
    std::vector<VkBufferMemoryBarrier> bufferBarriers;
    
    // Process color/depth attachments
    for (ResourceHandle handle : pass->GetInputs()) {
        ResourceNode* resource = GetResource(handle);
        if (!resource || resource->GetType() != ResourceType::Image) continue;
        
        ResourceState newState = GetRequiredState(ResourceUsage::ColorAttachment, true);
        if (resource->GetCurrentState().imageLayout != newState.imageLayout) {
            imageBarriers.push_back(CreateImageBarrier(resource, resource->GetCurrentState(), newState));
            resource->SetCurrentState(newState);
        }
    }
    
    for (ResourceHandle handle : pass->GetOutputs()) {
        ResourceNode* resource = GetResource(handle);
        if (!resource || resource->GetType() != ResourceType::Image) continue;
        
        ResourceState newState = GetRequiredState(ResourceUsage::ColorAttachment, false);
        if (resource->GetCurrentState().imageLayout != newState.imageLayout) {
            imageBarriers.push_back(CreateImageBarrier(resource, resource->GetCurrentState(), newState));
            resource->SetCurrentState(newState);
        }
    }
    
    // Process sampled images
    for (ResourceHandle handle : pass->GetSampledImages()) {
        ResourceNode* resource = GetResource(handle);
        if (!resource || resource->GetType() != ResourceType::Image) continue;
        
        ResourceState newState = GetRequiredState(ResourceUsage::SampledImage, true);
        if (resource->GetCurrentState().imageLayout != newState.imageLayout) {
            imageBarriers.push_back(CreateImageBarrier(resource, resource->GetCurrentState(), newState));
            resource->SetCurrentState(newState);
        }
    }
    
    // Process storage images
    for (ResourceHandle handle : pass->GetStorageImages()) {
        ResourceNode* resource = GetResource(handle);
        if (!resource || resource->GetType() != ResourceType::Image) continue;
        
        ResourceState newState = GetRequiredState(ResourceUsage::StorageImage, false);
        if (resource->GetCurrentState().imageLayout != newState.imageLayout) {
            imageBarriers.push_back(CreateImageBarrier(resource, resource->GetCurrentState(), newState));
            resource->SetCurrentState(newState);
        }
    }
    
    // Process buffers
    for (ResourceHandle handle : pass->GetUniformBuffers()) {
        ResourceNode* resource = GetResource(handle);
        if (!resource || resource->GetType() != ResourceType::Buffer) continue;
        
        ResourceState newState = GetRequiredState(ResourceUsage::UniformBuffer, true);
        if (resource->GetCurrentState().accessFlags != newState.accessFlags) {
            bufferBarriers.push_back(CreateBufferBarrier(resource, resource->GetCurrentState(), newState));
            resource->SetCurrentState(newState);
        }
    }
    
    // Submit barriers
    if (!imageBarriers.empty() || !bufferBarriers.empty()) {
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        
        vkCmdPipelineBarrier(
            cmd,
            srcStage,
            dstStage,
            0,
            0, nullptr,
            static_cast<u32>(bufferBarriers.size()), bufferBarriers.data(),
            static_cast<u32>(imageBarriers.size()), imageBarriers.data()
        );
    }
}

VkImageMemoryBarrier RenderGraph::CreateImageBarrier(ResourceNode* resource, ResourceState oldState, ResourceState newState) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldState.imageLayout;
    barrier.newLayout = newState.imageLayout;
    barrier.srcAccessMask = oldState.accessFlags;
    barrier.dstAccessMask = newState.accessFlags;
    barrier.image = resource->GetImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    return barrier;
}

VkBufferMemoryBarrier RenderGraph::CreateBufferBarrier(ResourceNode* resource, ResourceState oldState, ResourceState newState) {
    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = oldState.accessFlags;
    barrier.dstAccessMask = newState.accessFlags;
    barrier.buffer = resource->GetBuffer();
    barrier.offset = 0;
    barrier.size = resource->GetSize();
    return barrier;
}

ResourceState RenderGraph::GetRequiredState(ResourceUsage usage, bool isInput) {
    ResourceState state;
    
    switch (usage) {
        case ResourceUsage::ColorAttachment:
            state.imageLayout = isInput ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            state.accessFlags = isInput ? VK_ACCESS_COLOR_ATTACHMENT_READ_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            state.stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            break;
        case ResourceUsage::DepthAttachment:
            state.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            state.accessFlags = isInput ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT : VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            state.stageFlags = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        case ResourceUsage::SampledImage:
            state.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            state.accessFlags = VK_ACCESS_SHADER_READ_BIT;
            state.stageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        case ResourceUsage::StorageImage:
            state.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            state.accessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            state.stageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        case ResourceUsage::UniformBuffer:
            state.accessFlags = VK_ACCESS_UNIFORM_READ_BIT;
            state.stageFlags = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        case ResourceUsage::StorageBuffer:
            state.accessFlags = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            state.stageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        default:
            break;
    }
    
    return state;
}

} // namespace Renderer
} // namespace Enjin
