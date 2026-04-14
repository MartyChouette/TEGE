#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace Enjin {
namespace Renderer {

// Frame graph resource handle (opaque index)
struct FGResourceHandle {
    u32 idx = UINT32_MAX;
    bool IsValid() const { return idx != UINT32_MAX; }
};

// Resource description for texture resources
struct FGTextureDesc {
    u32 width = 0;
    u32 height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageUsageFlags usage = 0;
    u32 mipLevels = 1;
    u32 arrayLayers = 1;
};

// Tracked resource state
struct FGResource {
    std::string name;
    FGTextureDesc desc;

    // Imported resources (not owned by frame graph)
    VkImage importedImage = VK_NULL_HANDLE;
    VkImageLayout importedLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool isImported = false;

    // Current state for barrier deduction
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags currentAccess = 0;
    VkPipelineStageFlags currentStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    // Reference counting for resource aliasing
    u32 firstUsePass = UINT32_MAX;
    u32 lastUsePass = UINT32_MAX;
};

// Render pass in the frame graph
struct FGPass {
    std::string name;
    std::vector<FGResourceHandle> reads;
    std::vector<FGResourceHandle> writes;
    std::function<void(VkCommandBuffer)> execute;
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    u32 order = 0; // Topological order after compile

    // Add a read dependency
    FGPass& Read(FGResourceHandle handle) { reads.push_back(handle); return *this; }
    // Add a write output
    FGPass& Write(FGResourceHandle handle) { writes.push_back(handle); return *this; }
    // Set the execution callback
    FGPass& SetExecute(std::function<void(VkCommandBuffer)> fn) { execute = std::move(fn); return *this; }
    // Set pipeline stage for barrier scheduling
    FGPass& SetStage(VkPipelineStageFlags s) { stage = s; return *this; }
};

// Frame graph: manages render pass ordering, resource lifetimes, and barrier insertion.
// Usage:
//   1. Create resources (CreateTexture / ImportTexture)
//   2. Add passes (AddPass) with read/write declarations
//   3. Compile() — topological sort, compute barriers, alias resources
//   4. Execute(cmd) — record all passes with auto-barriers
class ENJIN_API FrameGraph {
public:
    FrameGraph() = default;
    ~FrameGraph() = default;

    // Resource creation
    FGResourceHandle CreateTexture(const std::string& name, const FGTextureDesc& desc);
    FGResourceHandle ImportTexture(const std::string& name, VkImage image, VkImageLayout layout);

    // Pass registration
    FGPass& AddPass(const std::string& name, VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    // Compile the graph: topological sort, deduce barriers, identify aliasable resources
    bool Compile();

    // Execute all passes in order with automatic barrier insertion
    void Execute(VkCommandBuffer commandBuffer);

    // Reset for next frame (keeps pass/resource definitions, resets execution state)
    void Reset();

    // Clear all passes and resources (full rebuild)
    void Clear();

    // Debug
    u32 GetPassCount() const { return static_cast<u32>(m_Passes.size()); }
    u32 GetResourceCount() const { return static_cast<u32>(m_Resources.size()); }
    const std::vector<u32>& GetExecutionOrder() const { return m_ExecutionOrder; }

private:
    // Deduced barrier between passes
    struct Barrier {
        FGResourceHandle resource;
        VkImageLayout oldLayout;
        VkImageLayout newLayout;
        VkAccessFlags srcAccess;
        VkAccessFlags dstAccess;
        VkPipelineStageFlags srcStage;
        VkPipelineStageFlags dstStage;
    };

    // Per-pass deduced barriers
    struct PassBarriers {
        std::vector<Barrier> barriers;
    };

    std::vector<FGResource> m_Resources;
    std::vector<FGPass> m_Passes;
    std::vector<u32> m_ExecutionOrder; // Topological order of pass indices
    std::vector<PassBarriers> m_PassBarriers; // Barriers per pass (indexed by execution order)
    bool m_Compiled = false;

    // Deduce the required layout for a resource access
    VkImageLayout DeduceLayout(const FGResource& resource, bool isWrite) const;
    VkAccessFlags DeduceAccess(const FGResource& resource, bool isWrite) const;

    // Build adjacency graph and topological sort
    bool TopologicalSort();
    // Compute barriers between passes
    void ComputeBarriers();
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
