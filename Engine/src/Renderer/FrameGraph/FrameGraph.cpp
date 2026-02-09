#include "Enjin/Renderer/FrameGraph/FrameGraph.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <queue>

namespace Enjin {
namespace Renderer {

FGResourceHandle FrameGraph::CreateTexture(const std::string& name, const FGTextureDesc& desc) {
    FGResourceHandle handle;
    handle.idx = static_cast<u32>(m_Resources.size());

    FGResource resource;
    resource.name = name;
    resource.desc = desc;
    resource.isImported = false;
    m_Resources.push_back(std::move(resource));

    return handle;
}

FGResourceHandle FrameGraph::ImportTexture(const std::string& name, VkImage image, VkImageLayout layout) {
    FGResourceHandle handle;
    handle.idx = static_cast<u32>(m_Resources.size());

    FGResource resource;
    resource.name = name;
    resource.importedImage = image;
    resource.importedLayout = layout;
    resource.currentLayout = layout;
    resource.isImported = true;
    m_Resources.push_back(std::move(resource));

    return handle;
}

FGPass& FrameGraph::AddPass(const std::string& name, VkPipelineStageFlags stage) {
    m_Passes.emplace_back();
    FGPass& pass = m_Passes.back();
    pass.name = name;
    pass.stage = stage;
    m_Compiled = false;
    return pass;
}

bool FrameGraph::Compile() {
    // Update resource first/last use pass indices
    for (auto& res : m_Resources) {
        res.firstUsePass = UINT32_MAX;
        res.lastUsePass = 0;
    }

    for (u32 passIdx = 0; passIdx < static_cast<u32>(m_Passes.size()); ++passIdx) {
        const auto& pass = m_Passes[passIdx];
        for (const auto& handle : pass.reads) {
            if (!handle.IsValid() || handle.idx >= m_Resources.size()) continue;
            m_Resources[handle.idx].firstUsePass = std::min(m_Resources[handle.idx].firstUsePass, passIdx);
            m_Resources[handle.idx].lastUsePass = std::max(m_Resources[handle.idx].lastUsePass, passIdx);
        }
        for (const auto& handle : pass.writes) {
            if (!handle.IsValid() || handle.idx >= m_Resources.size()) continue;
            m_Resources[handle.idx].firstUsePass = std::min(m_Resources[handle.idx].firstUsePass, passIdx);
            m_Resources[handle.idx].lastUsePass = std::max(m_Resources[handle.idx].lastUsePass, passIdx);
        }
    }

    if (!TopologicalSort()) {
        ENJIN_LOG_ERROR(Renderer, "Frame graph compilation failed: cyclic dependency detected");
        return false;
    }

    ComputeBarriers();

    m_Compiled = true;
    return true;
}

bool FrameGraph::TopologicalSort() {
    u32 passCount = static_cast<u32>(m_Passes.size());
    if (passCount == 0) {
        m_ExecutionOrder.clear();
        return true;
    }

    // Build adjacency graph based on resource dependencies
    // If pass A writes resource R and pass B reads R (A < B), then B depends on A
    std::vector<std::vector<u32>> adj(passCount);
    std::vector<u32> inDegree(passCount, 0);

    // Map: resource index -> last pass that writes it
    std::unordered_map<u32, u32> lastWriter;

    for (u32 passIdx = 0; passIdx < passCount; ++passIdx) {
        const auto& pass = m_Passes[passIdx];

        // For each read, add edge from the last writer to this pass
        for (const auto& handle : pass.reads) {
            if (!handle.IsValid()) continue;
            auto it = lastWriter.find(handle.idx);
            if (it != lastWriter.end() && it->second != passIdx) {
                adj[it->second].push_back(passIdx);
                inDegree[passIdx]++;
            }
        }

        // Record this pass as writer for its outputs
        for (const auto& handle : pass.writes) {
            if (!handle.IsValid()) continue;
            lastWriter[handle.idx] = passIdx;
        }
    }

    // Kahn's algorithm for topological sort
    std::queue<u32> ready;
    for (u32 i = 0; i < passCount; ++i) {
        if (inDegree[i] == 0) ready.push(i);
    }

    m_ExecutionOrder.clear();
    m_ExecutionOrder.reserve(passCount);

    while (!ready.empty()) {
        u32 current = ready.front();
        ready.pop();
        m_ExecutionOrder.push_back(current);

        for (u32 next : adj[current]) {
            if (--inDegree[next] == 0) {
                ready.push(next);
            }
        }
    }

    if (m_ExecutionOrder.size() != passCount) {
        return false; // Cycle detected
    }

    // Assign execution order to passes
    for (u32 i = 0; i < static_cast<u32>(m_ExecutionOrder.size()); ++i) {
        m_Passes[m_ExecutionOrder[i]].order = i;
    }

    return true;
}

VkImageLayout FrameGraph::DeduceLayout(const FGResource& resource, bool isWrite) const {
    if (isWrite) {
        // Depth attachment or color attachment based on format
        if (resource.desc.format == VK_FORMAT_D32_SFLOAT ||
            resource.desc.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            resource.desc.format == VK_FORMAT_D24_UNORM_S8_UINT) {
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        if (resource.desc.usage & VK_IMAGE_USAGE_STORAGE_BIT) {
            return VK_IMAGE_LAYOUT_GENERAL;
        }
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    } else {
        // Read: shader read or depth read
        if (resource.desc.format == VK_FORMAT_D32_SFLOAT ||
            resource.desc.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            resource.desc.format == VK_FORMAT_D24_UNORM_S8_UINT) {
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

VkAccessFlags FrameGraph::DeduceAccess(const FGResource& resource, bool isWrite) const {
    if (isWrite) {
        if (resource.desc.format == VK_FORMAT_D32_SFLOAT ||
            resource.desc.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            resource.desc.format == VK_FORMAT_D24_UNORM_S8_UINT) {
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        if (resource.desc.usage & VK_IMAGE_USAGE_STORAGE_BIT) {
            return VK_ACCESS_SHADER_WRITE_BIT;
        }
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    } else {
        return VK_ACCESS_SHADER_READ_BIT;
    }
}

void FrameGraph::ComputeBarriers() {
    m_PassBarriers.resize(m_ExecutionOrder.size());

    // Reset resource state to initial
    for (auto& res : m_Resources) {
        if (res.isImported) {
            res.currentLayout = res.importedLayout;
        } else {
            res.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
        res.currentAccess = 0;
        res.currentStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    for (u32 execIdx = 0; execIdx < static_cast<u32>(m_ExecutionOrder.size()); ++execIdx) {
        u32 passIdx = m_ExecutionOrder[execIdx];
        const auto& pass = m_Passes[passIdx];
        auto& barriers = m_PassBarriers[execIdx];
        barriers.barriers.clear();

        // Process reads
        for (const auto& handle : pass.reads) {
            if (!handle.IsValid() || handle.idx >= m_Resources.size()) continue;
            auto& res = m_Resources[handle.idx];
            VkImageLayout needed = DeduceLayout(res, false);
            VkAccessFlags neededAccess = DeduceAccess(res, false);

            if (res.currentLayout != needed || res.currentAccess != neededAccess) {
                Barrier b;
                b.resource = handle;
                b.oldLayout = res.currentLayout;
                b.newLayout = needed;
                b.srcAccess = res.currentAccess;
                b.dstAccess = neededAccess;
                b.srcStage = res.currentStage;
                b.dstStage = pass.stage;
                barriers.barriers.push_back(b);

                res.currentLayout = needed;
                res.currentAccess = neededAccess;
                res.currentStage = pass.stage;
            }
        }

        // Process writes
        for (const auto& handle : pass.writes) {
            if (!handle.IsValid() || handle.idx >= m_Resources.size()) continue;
            auto& res = m_Resources[handle.idx];
            VkImageLayout needed = DeduceLayout(res, true);
            VkAccessFlags neededAccess = DeduceAccess(res, true);

            if (res.currentLayout != needed || res.currentAccess != neededAccess) {
                Barrier b;
                b.resource = handle;
                b.oldLayout = res.currentLayout;
                b.newLayout = needed;
                b.srcAccess = res.currentAccess;
                b.dstAccess = neededAccess;
                b.srcStage = res.currentStage;
                b.dstStage = pass.stage;
                barriers.barriers.push_back(b);
            }

            res.currentLayout = needed;
            res.currentAccess = neededAccess;
            res.currentStage = pass.stage;
        }
    }
}

void FrameGraph::Execute(VkCommandBuffer commandBuffer) {
    if (!m_Compiled) {
        ENJIN_LOG_WARN(Renderer, "Frame graph not compiled, call Compile() first");
        return;
    }

    for (u32 execIdx = 0; execIdx < static_cast<u32>(m_ExecutionOrder.size()); ++execIdx) {
        u32 passIdx = m_ExecutionOrder[execIdx];
        const auto& pass = m_Passes[passIdx];

        // Insert deduced barriers before pass execution
        const auto& barriers = m_PassBarriers[execIdx];
        if (!barriers.barriers.empty()) {
            std::vector<VkImageMemoryBarrier> imageBarriers;
            imageBarriers.reserve(barriers.barriers.size());

            VkPipelineStageFlags srcStages = 0;
            VkPipelineStageFlags dstStages = 0;

            for (const auto& b : barriers.barriers) {
                if (!b.resource.IsValid() || b.resource.idx >= m_Resources.size()) continue;
                const auto& res = m_Resources[b.resource.idx];
                if (!res.isImported || res.importedImage == VK_NULL_HANDLE) continue;

                VkImageMemoryBarrier imgBarrier{};
                imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imgBarrier.srcAccessMask = b.srcAccess;
                imgBarrier.dstAccessMask = b.dstAccess;
                imgBarrier.oldLayout = b.oldLayout;
                imgBarrier.newLayout = b.newLayout;
                imgBarrier.image = res.importedImage;
                imgBarrier.subresourceRange.aspectMask =
                    (res.desc.format == VK_FORMAT_D32_SFLOAT ||
                     res.desc.format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                     res.desc.format == VK_FORMAT_D24_UNORM_S8_UINT)
                    ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
                imgBarrier.subresourceRange.baseMipLevel = 0;
                imgBarrier.subresourceRange.levelCount = res.desc.mipLevels;
                imgBarrier.subresourceRange.baseArrayLayer = 0;
                imgBarrier.subresourceRange.layerCount = res.desc.arrayLayers;

                imageBarriers.push_back(imgBarrier);
                srcStages |= b.srcStage;
                dstStages |= b.dstStage;
            }

            if (!imageBarriers.empty()) {
                if (srcStages == 0) srcStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                if (dstStages == 0) dstStages = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

                vkCmdPipelineBarrier(commandBuffer,
                    srcStages, dstStages,
                    0, 0, nullptr, 0, nullptr,
                    static_cast<u32>(imageBarriers.size()), imageBarriers.data());
            }
        }

        // Execute the pass
        if (pass.execute) {
            pass.execute(commandBuffer);
        }
    }
}

void FrameGraph::Reset() {
    // Reset resource states to initial for next frame
    for (auto& res : m_Resources) {
        if (res.isImported) {
            res.currentLayout = res.importedLayout;
        } else {
            res.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
        res.currentAccess = 0;
        res.currentStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
    // Keep compiled state — passes and resources don't change frame-to-frame
}

void FrameGraph::Clear() {
    m_Resources.clear();
    m_Passes.clear();
    m_ExecutionOrder.clear();
    m_PassBarriers.clear();
    m_Compiled = false;
}

} // namespace Renderer
} // namespace Enjin
