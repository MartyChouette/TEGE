#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <memory>

namespace Enjin {
namespace Renderer {

class VulkanBuffer;

/// Configuration for Device Generated Commands (DGC).
/// DGC allows the GPU to generate its own command stream (push constants + draw calls)
/// without CPU involvement, eliminating all CPU-side draw call submission overhead.
struct DeviceGeneratedCommandsConfig {
    bool enabled = false;      // User toggle (disabled by default, opt-in)
    bool supported = false;    // Set at init from extension query
    bool useNVExtension = false; // True if only VK_NV_device_generated_commands is available
};

/// Device Generated Commands system.
///
/// Replaces the multi-draw indirect path with a fully GPU-driven command stream:
///   1. GPU culling (existing) determines visible objects
///   2. DGC generation compute shader writes command sequences for each visible object
///   3. Preprocess validates the command stream
///   4. Execute runs the entire draw pass from GPU-generated commands
///
/// Each command sequence consists of:
///   - Push constants (model matrix + material data from ObjectData SSBO)
///   - Draw indexed (index count/offset/vertex offset from CullableObject)
///
/// This eliminates the need for the parallaxScale = -1.0 sentinel hack used by
/// the multi-draw indirect path, since each object gets its own push constants.
///
/// Falls back to existing multi-draw indirect when DGC is not supported.
class ENJIN_API DeviceGeneratedCommands {
public:
    DeviceGeneratedCommands();
    ~DeviceGeneratedCommands();

    /// Initialize DGC. Checks extension support and creates resources.
    /// @param ctx Vulkan context (must have been initialized with DGC extensions enabled)
    /// @param maxSequences Maximum number of draw sequences (matches GPUCulling max objects)
    /// @return true if DGC is supported and initialized
    bool Initialize(VulkanContext* ctx, u32 maxSequences);

    /// Whether DGC is supported on this device
    bool IsSupported() const { return m_Config.supported; }

    /// Whether DGC is currently enabled (supported + user toggle)
    bool IsEnabled() const { return m_Config.supported && m_Config.enabled; }

    /// Enable or disable DGC at runtime
    void SetEnabled(bool enabled) { m_Config.enabled = enabled; }

    /// Get the configuration
    const DeviceGeneratedCommandsConfig& GetConfig() const { return m_Config; }

    /// Create the indirect commands layout describing the command sequence.
    /// Must be called after the graphics pipeline is created.
    /// @param pipelineLayout The graphics pipeline layout (for push constant ranges)
    /// @param pipeline The graphics pipeline to bind
    /// @return true if layout creation succeeded
    bool CreateCommandsLayout(VkPipelineLayout pipelineLayout, VkPipeline pipeline);

    /// Record the DGC generation compute shader dispatch.
    /// Reads from the visibility buffer (post-culling) and ObjectData SSBO,
    /// writes command sequences into the sequence buffer.
    /// @param cmd Command buffer to record into
    /// @param objectBuffer CullableObject SSBO (from GPUCulling)
    /// @param objectDataBuffer ObjectData SSBO (per-object material/transform)
    /// @param visibilityBuffer Per-object visibility flags (from GPU culling)
    /// @param objectCount Number of objects submitted for culling
    void GenerateCommands(
        VkCommandBuffer cmd,
        VkBuffer objectBuffer,
        VkBuffer objectDataBuffer,
        VkBuffer visibilityBuffer,
        u32 objectCount
    );

    /// Preprocess: GPU validates the generated command stream.
    /// Must be called after GenerateCommands and before Execute.
    /// @param cmd Command buffer to record into
    void Preprocess(VkCommandBuffer cmd);

    /// Execute: GPU runs the generated commands — the entire draw pass.
    /// This replaces vkCmdDrawIndexedIndirectCount for DGC-enabled rendering.
    /// @param cmd Command buffer to record into
    void Execute(VkCommandBuffer cmd);

    /// Get the sequence count buffer (for reading back draw count if needed)
    VkBuffer GetSequenceCountBuffer() const;

    /// Clean up all resources
    void Shutdown();

private:
    bool LoadFunctionPointers();
    bool CreateComputePipeline();
    bool CreateBuffers(u32 maxSequences);
    bool CreateDescriptorResources();

    VulkanContext* m_Context = nullptr;
    DeviceGeneratedCommandsConfig m_Config;
    u32 m_MaxSequences = 0;

    // --- EXT extension objects (VK_EXT_device_generated_commands, Vulkan 1.3.275+) ---
#if defined(VK_EXT_device_generated_commands)
    VkIndirectCommandsLayoutEXT m_CommandsLayoutEXT = VK_NULL_HANDLE;
    VkIndirectExecutionSetEXT m_ExecutionSetEXT = VK_NULL_HANDLE;

    // EXT function pointers (loaded dynamically via vkGetDeviceProcAddr)
    PFN_vkCreateIndirectCommandsLayoutEXT m_vkCreateIndirectCommandsLayoutEXT = nullptr;
    PFN_vkDestroyIndirectCommandsLayoutEXT m_vkDestroyIndirectCommandsLayoutEXT = nullptr;
    PFN_vkCreateIndirectExecutionSetEXT m_vkCreateIndirectExecutionSetEXT = nullptr;
    PFN_vkDestroyIndirectExecutionSetEXT m_vkDestroyIndirectExecutionSetEXT = nullptr;
    PFN_vkCmdPreprocessGeneratedCommandsEXT m_vkCmdPreprocessGeneratedCommandsEXT = nullptr;
    PFN_vkCmdExecuteGeneratedCommandsEXT m_vkCmdExecuteGeneratedCommandsEXT = nullptr;
    PFN_vkGetGeneratedCommandsMemoryRequirementsEXT m_vkGetGeneratedCommandsMemoryRequirementsEXT = nullptr;
    PFN_vkUpdateIndirectExecutionSetPipelineEXT m_vkUpdateIndirectExecutionSetPipelineEXT = nullptr;
#endif

    // --- NV fallback objects (VK_NV_device_generated_commands) ---
#if defined(VK_NV_device_generated_commands)
    VkIndirectCommandsLayoutNV m_CommandsLayoutNV = VK_NULL_HANDLE;

    // NV function pointers (loaded dynamically)
    PFN_vkCreateIndirectCommandsLayoutNV m_vkCreateIndirectCommandsLayoutNV = nullptr;
    PFN_vkDestroyIndirectCommandsLayoutNV m_vkDestroyIndirectCommandsLayoutNV = nullptr;
    PFN_vkCmdPreprocessGeneratedCommandsNV m_vkCmdPreprocessGeneratedCommandsNV = nullptr;
    PFN_vkCmdExecuteGeneratedCommandsNV m_vkCmdExecuteGeneratedCommandsNV = nullptr;
    PFN_vkGetGeneratedCommandsMemoryRequirementsNV m_vkGetGeneratedCommandsMemoryRequirementsNV = nullptr;
#endif

    // Pipeline state for DGC execution
    VkPipelineLayout m_GraphicsPipelineLayout = VK_NULL_HANDLE; // Not owned
    VkPipeline m_GraphicsPipeline = VK_NULL_HANDLE;             // Not owned

    // Command generation compute pipeline
    VkPipeline m_GeneratePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_GeneratePipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_GenerateDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_GenerateDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool m_GenerateDescriptorPool = VK_NULL_HANDLE;

    // Buffers
    std::unique_ptr<VulkanBuffer> m_SequenceBuffer;       // Input command sequences for DGC
    std::unique_ptr<VulkanBuffer> m_SequenceCountBuffer;  // Atomic count of generated sequences
    std::unique_ptr<VulkanBuffer> m_PreprocessBuffer;     // Scratch buffer for DGC preprocessing

    // Cached preprocess size requirements
    VkDeviceSize m_PreprocessSize = 0;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
