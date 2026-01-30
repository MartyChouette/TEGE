#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Enjin {
namespace Renderer {

// Forward declarations
class VulkanRenderer;

// Resource handle in render graph
using ResourceHandle = u32;
constexpr ResourceHandle INVALID_RESOURCE_HANDLE = UINT32_MAX;

// Resource type
enum class ResourceType {
    Image,
    Buffer,
    Attachment
};

// Resource usage
enum class ResourceUsage {
    ColorAttachment,
    DepthAttachment,
    SampledImage,
    StorageImage,
    UniformBuffer,
    StorageBuffer,
    TransferSrc,
    TransferDst
};

// Resource state
struct ResourceState {
    VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkAccessFlags accessFlags = 0;
    VkPipelineStageFlags stageFlags = 0;
};

// Render pass node in graph
class RenderPassNode {
public:
    RenderPassNode(const std::string& name);
    
    void AddColorInput(ResourceHandle handle);
    void AddColorOutput(ResourceHandle handle);
    void AddDepthInput(ResourceHandle handle);
    void AddDepthOutput(ResourceHandle handle);
    void AddSampledImage(ResourceHandle handle);
    void AddStorageImage(ResourceHandle handle);
    void AddUniformBuffer(ResourceHandle handle);
    void AddStorageBuffer(ResourceHandle handle);
    
    void SetExecuteCallback(std::function<void(VkCommandBuffer)> callback);
    
    const std::string& GetName() const { return m_Name; }
    const std::vector<ResourceHandle>& GetInputs() const { return m_Inputs; }
    const std::vector<ResourceHandle>& GetOutputs() const { return m_Outputs; }
    const std::vector<ResourceHandle>& GetSampledImages() const { return m_SampledImages; }
    const std::vector<ResourceHandle>& GetStorageImages() const { return m_StorageImages; }
    const std::vector<ResourceHandle>& GetUniformBuffers() const { return m_UniformBuffers; }
    const std::vector<ResourceHandle>& GetStorageBuffers() const { return m_StorageBuffers; }
    
    void Execute(VkCommandBuffer cmd) const;
    
    u32 GetOrder() const { return m_Order; }
    void SetOrder(u32 order) { m_Order = order; }

private:
    std::string m_Name;
    std::vector<ResourceHandle> m_Inputs;
    std::vector<ResourceHandle> m_Outputs;
    std::vector<ResourceHandle> m_SampledImages;
    std::vector<ResourceHandle> m_StorageImages;
    std::vector<ResourceHandle> m_UniformBuffers;
    std::vector<ResourceHandle> m_StorageBuffers;
    std::function<void(VkCommandBuffer)> m_ExecuteCallback;
    u32 m_Order = 0;
};

// Resource node in graph
class ResourceNode {
public:
    ResourceNode(const std::string& name, ResourceType type);
    
    void SetImage(VkImage image, VkFormat format, u32 width, u32 height);
    void SetBuffer(VkBuffer buffer, VkDeviceSize size);
    
    ResourceHandle GetHandle() const { return m_Handle; }
    void SetHandle(ResourceHandle handle) { m_Handle = handle; }
    
    const std::string& GetName() const { return m_Name; }
    ResourceType GetType() const { return m_Type; }
    
    VkImage GetImage() const { return m_Image; }
    VkBuffer GetBuffer() const { return m_Buffer; }
    VkFormat GetFormat() const { return m_Format; }
    u32 GetWidth() const { return m_Width; }
    u32 GetHeight() const { return m_Height; }
    VkDeviceSize GetSize() const { return m_Size; }
    
    ResourceState GetCurrentState() const { return m_CurrentState; }
    void SetCurrentState(const ResourceState& state) { m_CurrentState = state; }

private:
    std::string m_Name;
    ResourceType m_Type;
    ResourceHandle m_Handle = INVALID_RESOURCE_HANDLE;
    
    VkImage m_Image = VK_NULL_HANDLE;
    VkBuffer m_Buffer = VK_NULL_HANDLE;
    VkFormat m_Format = VK_FORMAT_UNDEFINED;
    u32 m_Width = 0;
    u32 m_Height = 0;
    VkDeviceSize m_Size = 0;
    
    ResourceState m_CurrentState;
};

// Render graph - automatic pass ordering and resource management
class ENJIN_API RenderGraph {
public:
    RenderGraph(VulkanContext* context);
    ~RenderGraph();

    // Add resource
    ResourceHandle AddResource(const std::string& name, ResourceType type);
    ResourceNode* GetResource(ResourceHandle handle);
    
    // Add render pass
    RenderPassNode* AddRenderPass(const std::string& name);
    
    // Build graph (resolve dependencies, order passes)
    bool Build();
    
    // Execute graph (record commands)
    void Execute(VkCommandBuffer cmd);
    
    // Clear graph
    void Clear();
    
    // Get resource by name
    ResourceHandle GetResourceHandle(const std::string& name) const;

private:
    // Dependency resolution
    void ResolveDependencies();
    void TopologicalSort();
    
    // Barrier insertion
    void InsertBarriers(VkCommandBuffer cmd, RenderPassNode* pass);
    VkImageMemoryBarrier CreateImageBarrier(ResourceNode* resource, ResourceState oldState, ResourceState newState);
    VkBufferMemoryBarrier CreateBufferBarrier(ResourceNode* resource, ResourceState oldState, ResourceState newState);
    
    // State tracking
    ResourceState GetRequiredState(ResourceUsage usage, bool isInput);
    
    VulkanContext* m_Context = nullptr;
    std::vector<std::unique_ptr<ResourceNode>> m_Resources;
    std::vector<std::unique_ptr<RenderPassNode>> m_Passes;
    std::unordered_map<std::string, ResourceHandle> m_ResourceNameMap;
    std::vector<RenderPassNode*> m_OrderedPasses;
    bool m_Built = false;

    // Dependency graph data (populated by ResolveDependencies)
    std::unordered_map<ResourceHandle, u32> m_ResourceProducer; // resource -> pass index
    std::vector<std::vector<u32>> m_PassDependencies;           // pass -> [dependency pass indices]
};

} // namespace Renderer
} // namespace Enjin
