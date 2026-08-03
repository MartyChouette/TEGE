#include "Enjin/Renderer/GPUDriven/DeviceGeneratedCommands.h"
#include "Enjin/Renderer/GPUDriven/GPUCulling.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

// Push constants are 128 bytes (32 u32s), draw indexed is 20 bytes (5 u32s).
// Each sequence is padded to 160 bytes (40 u32s) for alignment.
static constexpr u32 PUSH_CONSTANTS_SIZE = 128;
static constexpr u32 DRAW_COMMAND_SIZE = 20;  // sizeof(VkDrawIndexedIndirectCommand)
static constexpr u32 SEQUENCE_STRIDE = 160;   // Padded for alignment

DeviceGeneratedCommands::DeviceGeneratedCommands() = default;

DeviceGeneratedCommands::~DeviceGeneratedCommands() {
    Shutdown();
}

bool DeviceGeneratedCommands::Initialize(VulkanContext* ctx, u32 maxSequences) {
    m_Context = ctx;
    m_MaxSequences = maxSequences;

    if (!ctx || ctx->GetDevice() == VK_NULL_HANDLE) {
        ENJIN_LOG_WARN(Renderer, "DGC: No valid Vulkan context");
        return false;
    }

    // Check for DGC extension support
    VkPhysicalDevice physDevice = ctx->GetPhysicalDevice();
    u32 extCount = 0;
    vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &extCount, exts.data());

    bool hasEXT = false;
    bool hasNV = false;
    for (const auto& ext : exts) {
        if (strcmp(ext.extensionName, "VK_EXT_device_generated_commands") == 0) {
            hasEXT = true;
        }
        if (strcmp(ext.extensionName, "VK_NV_device_generated_commands") == 0) {
            hasNV = true;
        }
    }

    // Filter out extensions not available at compile time
#if !defined(VK_EXT_device_generated_commands)
    if (hasEXT) {
        ENJIN_LOG_INFO(Renderer, "DGC: Driver supports EXT but Vulkan SDK headers too old, trying NV fallback");
        hasEXT = false;
    }
#endif
#if !defined(VK_NV_device_generated_commands)
    if (hasNV) {
        ENJIN_LOG_INFO(Renderer, "DGC: Driver supports NV but Vulkan SDK headers too old");
        hasNV = false;
    }
#endif

    if (!hasEXT && !hasNV) {
        ENJIN_LOG_INFO(Renderer, "DGC: Neither VK_EXT nor VK_NV device_generated_commands available");
        m_Config.supported = false;
        return false;
    }

    m_Config.useNVExtension = !hasEXT && hasNV;
    if (hasEXT) {
        ENJIN_LOG_INFO(Renderer, "DGC: VK_EXT_device_generated_commands available");
    } else {
        ENJIN_LOG_INFO(Renderer, "DGC: VK_NV_device_generated_commands available (EXT not supported)");
    }

    // Load function pointers
    if (!LoadFunctionPointers()) {
        ENJIN_LOG_WARN(Renderer, "DGC: Failed to load extension function pointers");
        m_Config.supported = false;
        return false;
    }

    // Create buffers
    if (!CreateBuffers(maxSequences)) {
        ENJIN_LOG_WARN(Renderer, "DGC: Failed to create buffers");
        m_Config.supported = false;
        return false;
    }

    // Create compute pipeline for command generation
    if (!CreateComputePipeline()) {
        ENJIN_LOG_WARN(Renderer, "DGC: Command generation compute pipeline not available");
        // This is non-fatal — DGC can still work if the shader is compiled later
    }

    m_Config.supported = true;
    m_Config.enabled = false; // Disabled by default — user must opt in
    ENJIN_LOG_INFO(Renderer, "DGC: Initialized (max sequences: %u, %s extension)",
                   maxSequences, m_Config.useNVExtension ? "NV" : "EXT");
    return true;
}

bool DeviceGeneratedCommands::LoadFunctionPointers() {
    VkDevice device = m_Context->GetDevice();

    if (!m_Config.useNVExtension) {
#if defined(VK_EXT_device_generated_commands)
        // EXT function pointers
        m_vkCreateIndirectCommandsLayoutEXT = reinterpret_cast<PFN_vkCreateIndirectCommandsLayoutEXT>(
            vkGetDeviceProcAddr(device, "vkCreateIndirectCommandsLayoutEXT"));
        m_vkDestroyIndirectCommandsLayoutEXT = reinterpret_cast<PFN_vkDestroyIndirectCommandsLayoutEXT>(
            vkGetDeviceProcAddr(device, "vkDestroyIndirectCommandsLayoutEXT"));
        m_vkCreateIndirectExecutionSetEXT = reinterpret_cast<PFN_vkCreateIndirectExecutionSetEXT>(
            vkGetDeviceProcAddr(device, "vkCreateIndirectExecutionSetEXT"));
        m_vkDestroyIndirectExecutionSetEXT = reinterpret_cast<PFN_vkDestroyIndirectExecutionSetEXT>(
            vkGetDeviceProcAddr(device, "vkDestroyIndirectExecutionSetEXT"));
        m_vkCmdPreprocessGeneratedCommandsEXT = reinterpret_cast<PFN_vkCmdPreprocessGeneratedCommandsEXT>(
            vkGetDeviceProcAddr(device, "vkCmdPreprocessGeneratedCommandsEXT"));
        m_vkCmdExecuteGeneratedCommandsEXT = reinterpret_cast<PFN_vkCmdExecuteGeneratedCommandsEXT>(
            vkGetDeviceProcAddr(device, "vkCmdExecuteGeneratedCommandsEXT"));
        m_vkGetGeneratedCommandsMemoryRequirementsEXT = reinterpret_cast<PFN_vkGetGeneratedCommandsMemoryRequirementsEXT>(
            vkGetDeviceProcAddr(device, "vkGetGeneratedCommandsMemoryRequirementsEXT"));
        m_vkUpdateIndirectExecutionSetPipelineEXT = reinterpret_cast<PFN_vkUpdateIndirectExecutionSetPipelineEXT>(
            vkGetDeviceProcAddr(device, "vkUpdateIndirectExecutionSetPipelineEXT"));

        if (!m_vkCreateIndirectCommandsLayoutEXT || !m_vkCmdExecuteGeneratedCommandsEXT) {
            ENJIN_LOG_WARN(Renderer, "DGC: Missing critical EXT function pointers");
            return false;
        }
#else
        ENJIN_LOG_WARN(Renderer, "DGC: VK_EXT_device_generated_commands not available in Vulkan SDK headers");
        return false;
#endif
    } else {
#if defined(VK_NV_device_generated_commands)
        // NV function pointers
        m_vkCreateIndirectCommandsLayoutNV = reinterpret_cast<PFN_vkCreateIndirectCommandsLayoutNV>(
            vkGetDeviceProcAddr(device, "vkCreateIndirectCommandsLayoutNV"));
        m_vkDestroyIndirectCommandsLayoutNV = reinterpret_cast<PFN_vkDestroyIndirectCommandsLayoutNV>(
            vkGetDeviceProcAddr(device, "vkDestroyIndirectCommandsLayoutNV"));
        m_vkCmdPreprocessGeneratedCommandsNV = reinterpret_cast<PFN_vkCmdPreprocessGeneratedCommandsNV>(
            vkGetDeviceProcAddr(device, "vkCmdPreprocessGeneratedCommandsNV"));
        m_vkCmdExecuteGeneratedCommandsNV = reinterpret_cast<PFN_vkCmdExecuteGeneratedCommandsNV>(
            vkGetDeviceProcAddr(device, "vkCmdExecuteGeneratedCommandsNV"));
        m_vkGetGeneratedCommandsMemoryRequirementsNV = reinterpret_cast<PFN_vkGetGeneratedCommandsMemoryRequirementsNV>(
            vkGetDeviceProcAddr(device, "vkGetGeneratedCommandsMemoryRequirementsNV"));

        if (!m_vkCreateIndirectCommandsLayoutNV || !m_vkCmdExecuteGeneratedCommandsNV) {
            ENJIN_LOG_WARN(Renderer, "DGC: Missing critical NV function pointers");
            return false;
        }
#else
        ENJIN_LOG_WARN(Renderer, "DGC: VK_NV_device_generated_commands not available in Vulkan SDK headers");
        return false;
#endif
    }

    return true;
}

bool DeviceGeneratedCommands::CreateBuffers(u32 maxSequences) {
    // Sequence buffer: holds the generated command data (push constants + draw indexed per object)
    usize sequenceBufferSize = static_cast<usize>(maxSequences) * SEQUENCE_STRIDE;
    m_SequenceBuffer = std::make_unique<VulkanBuffer>(m_Context);
    VkBufferUsageFlags sequenceUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                       VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (!m_SequenceBuffer->Create(sequenceBufferSize, sequenceUsage, false)) {
        ENJIN_LOG_ERROR(Renderer, "DGC: Failed to create sequence buffer (%zu bytes)", sequenceBufferSize);
        return false;
    }

    // Sequence count buffer: atomic counter for number of generated sequences
    m_SequenceCountBuffer = std::make_unique<VulkanBuffer>(m_Context);
    VkBufferUsageFlags countUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if (!m_SequenceCountBuffer->Create(sizeof(u32), countUsage, true)) {
        ENJIN_LOG_ERROR(Renderer, "DGC: Failed to create sequence count buffer");
        return false;
    }

    // Preprocess buffer is allocated lazily in CreateCommandsLayout once we know the size

    ENJIN_LOG_INFO(Renderer, "DGC: Buffers created (sequence buffer: %zu KB)",
                   sequenceBufferSize / 1024);
    return true;
}

bool DeviceGeneratedCommands::CreateComputePipeline() {
    VkDevice device = m_Context->GetDevice();

    // Descriptor set layout for the generation compute shader:
    //   binding 0: CullableObject SSBO (read)
    //   binding 1: ObjectDataGPU SSBO (read)
    //   binding 2: Visibility buffer (read)
    //   binding 3: Sequence buffer (write)
    //   binding 4: Sequence count buffer (read/write)
    std::vector<VkDescriptorSetLayoutBinding> bindings(5);
    for (u32 i = 0; i < 5; ++i) {
        bindings[i] = {};
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_GenerateDescriptorSetLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DGC: Failed to create generate descriptor set layout");
        return false;
    }

    // Push constant range for object count
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(u32);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_GenerateDescriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_GeneratePipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DGC: Failed to create generate pipeline layout");
        return false;
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 5;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_GenerateDescriptorPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DGC: Failed to create generate descriptor pool");
        return false;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_GenerateDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_GenerateDescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &m_GenerateDescriptorSet) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DGC: Failed to allocate generate descriptor set");
        return false;
    }

    // Load compute shader
    VulkanShader computeShader(m_Context);
    const char* shaderPaths[] = {
        "shaders/dgc_generate.comp.spv",
        "Engine/shaders/dgc_generate.comp.spv",
        "../Engine/shaders/dgc_generate.comp.spv",
        "../../Engine/shaders/dgc_generate.comp.spv",
        "bin/shaders/dgc_generate.comp.spv"
    };

    bool loaded = false;
    for (const char* path : shaderPaths) {
        if (computeShader.LoadFromFile(path, false) && computeShader.GetModule() != VK_NULL_HANDLE) {
            loaded = true;
            ENJIN_LOG_INFO(Renderer, "DGC: Loaded command generation shader from: %s", path);
            break;
        }
    }

    if (!loaded) {
        ENJIN_LOG_WARN(Renderer, "DGC: dgc_generate.comp.spv not found — compile with:");
        ENJIN_LOG_WARN(Renderer, "  glslangValidator -V Engine/shaders/dgc_generate.comp -o Engine/shaders/dgc_generate.comp.spv");
        return false;
    }

    // Create compute pipeline
    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_GeneratePipelineLayout;

    VkPipelineShaderStageCreateInfo shaderStage{};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStage.module = computeShader.GetModule();
    shaderStage.pName = "main";
    pipelineInfo.stage = shaderStage;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GeneratePipeline) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "DGC: Failed to create command generation compute pipeline");
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "DGC: Command generation compute pipeline created");
    return true;
}

bool DeviceGeneratedCommands::CreateCommandsLayout(VkPipelineLayout pipelineLayout, VkPipeline pipeline) {
    if (!m_Config.supported) return false;

    m_GraphicsPipelineLayout = pipelineLayout;
    m_GraphicsPipeline = pipeline;

    VkDevice device = m_Context->GetDevice();

    if (!m_Config.useNVExtension) {
#if defined(VK_EXT_device_generated_commands)
        // --- VK_EXT_device_generated_commands path ---

        // Create indirect execution set (required by EXT)
        VkIndirectExecutionSetPipelineInfoEXT pipelineInfoEXT{};
        pipelineInfoEXT.sType = VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_PIPELINE_INFO_EXT;
        pipelineInfoEXT.initialPipeline = pipeline;
        pipelineInfoEXT.maxPipelineCount = 1;

        VkIndirectExecutionSetCreateInfoEXT execSetInfo{};
        execSetInfo.sType = VK_STRUCTURE_TYPE_INDIRECT_EXECUTION_SET_CREATE_INFO_EXT;
        execSetInfo.type = VK_INDIRECT_EXECUTION_SET_INFO_TYPE_PIPELINES_EXT;
        execSetInfo.info.pPipelineInfo = &pipelineInfoEXT;

        if (m_vkCreateIndirectExecutionSetEXT(device, &execSetInfo, nullptr, &m_ExecutionSetEXT) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "DGC: Failed to create indirect execution set (EXT)");
            return false;
        }

        // Define command layout tokens:
        // Token 0: Push constants (128 bytes for model + material)
        // Token 1: Draw indexed
        VkIndirectCommandsLayoutTokenEXT tokens[2] = {};

        // Token 0: Push constants
        tokens[0].sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT;
        tokens[0].type = VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_EXT;
        tokens[0].data.pPushConstant = nullptr; // Will be filled below
        tokens[0].offset = 0;

        VkIndirectCommandsPushConstantTokenEXT pushToken{};
        pushToken.updateRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushToken.updateRange.offset = 0;
        pushToken.updateRange.size = PUSH_CONSTANTS_SIZE;
        tokens[0].data.pPushConstant = &pushToken;

        // Token 1: Draw indexed
        tokens[1].sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_EXT;
        tokens[1].type = VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_EXT;
        tokens[1].offset = PUSH_CONSTANTS_SIZE;

        VkIndirectCommandsLayoutCreateInfoEXT layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_EXT;
        layoutInfo.shaderStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        layoutInfo.pipelineLayout = pipelineLayout;
        layoutInfo.tokenCount = 2;
        layoutInfo.pTokens = tokens;
        layoutInfo.indirectStride = SEQUENCE_STRIDE;

        VkResult result = m_vkCreateIndirectCommandsLayoutEXT(device, &layoutInfo, nullptr, &m_CommandsLayoutEXT);
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "DGC: Failed to create indirect commands layout (EXT): %d", result);
            return false;
        }

        // Query preprocess buffer size
        VkGeneratedCommandsMemoryRequirementsInfoEXT memReqInfo{};
        memReqInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_EXT;
        memReqInfo.indirectCommandsLayout = m_CommandsLayoutEXT;
        memReqInfo.indirectExecutionSet = m_ExecutionSetEXT;
        memReqInfo.maxSequenceCount = m_MaxSequences;

        VkMemoryRequirements2 memReqs{};
        memReqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        m_vkGetGeneratedCommandsMemoryRequirementsEXT(device, &memReqInfo, &memReqs);
        m_PreprocessSize = memReqs.memoryRequirements.size;
#else
        ENJIN_LOG_ERROR(Renderer, "DGC: EXT types not available in SDK headers");
        return false;
#endif

    } else {
#if defined(VK_NV_device_generated_commands)
        // --- VK_NV_device_generated_commands path ---

        // Define command layout tokens (NV variant):
        // Token 0: Push constants
        // Token 1: Draw indexed
        VkIndirectCommandsLayoutTokenNV tokens[2] = {};

        tokens[0].sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV;
        tokens[0].tokenType = VK_INDIRECT_COMMANDS_TOKEN_TYPE_PUSH_CONSTANT_NV;
        tokens[0].stream = 0;
        tokens[0].offset = 0;
        tokens[0].pushconstantPipelineLayout = pipelineLayout;
        tokens[0].pushconstantShaderStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        tokens[0].pushconstantOffset = 0;
        tokens[0].pushconstantSize = PUSH_CONSTANTS_SIZE;

        tokens[1].sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_TOKEN_NV;
        tokens[1].tokenType = VK_INDIRECT_COMMANDS_TOKEN_TYPE_DRAW_INDEXED_NV;
        tokens[1].stream = 0;
        tokens[1].offset = PUSH_CONSTANTS_SIZE;

        u32 stride = SEQUENCE_STRIDE;

        VkIndirectCommandsLayoutCreateInfoNV layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_INDIRECT_COMMANDS_LAYOUT_CREATE_INFO_NV;
        layoutInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        layoutInfo.tokenCount = 2;
        layoutInfo.pTokens = tokens;
        layoutInfo.streamCount = 1;
        layoutInfo.pStreamStrides = &stride;

        VkResult result = m_vkCreateIndirectCommandsLayoutNV(device, &layoutInfo, nullptr, &m_CommandsLayoutNV);
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "DGC: Failed to create indirect commands layout (NV): %d", result);
            return false;
        }

        // Query preprocess buffer size
        VkGeneratedCommandsMemoryRequirementsInfoNV memReqInfo{};
        memReqInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_MEMORY_REQUIREMENTS_INFO_NV;
        memReqInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        memReqInfo.pipeline = pipeline;
        memReqInfo.indirectCommandsLayout = m_CommandsLayoutNV;
        memReqInfo.maxSequencesCount = m_MaxSequences;

        VkMemoryRequirements2 memReqs{};
        memReqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        m_vkGetGeneratedCommandsMemoryRequirementsNV(device, &memReqInfo, &memReqs);
        m_PreprocessSize = memReqs.memoryRequirements.size;
#else
        ENJIN_LOG_ERROR(Renderer, "DGC: NV types not available in SDK headers");
        return false;
#endif
    }

    // Allocate preprocess buffer
    if (m_PreprocessSize > 0) {
        m_PreprocessBuffer = std::make_unique<VulkanBuffer>(m_Context);
        VkBufferUsageFlags preprocessUsage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        if (!m_PreprocessBuffer->Create(m_PreprocessSize, preprocessUsage, false)) {
            ENJIN_LOG_ERROR(Renderer, "DGC: Failed to create preprocess buffer (%llu bytes)",
                           static_cast<unsigned long long>(m_PreprocessSize));
            return false;
        }
        ENJIN_LOG_INFO(Renderer, "DGC: Preprocess buffer allocated (%llu KB)",
                       static_cast<unsigned long long>(m_PreprocessSize / 1024));
    }

    ENJIN_LOG_INFO(Renderer, "DGC: Commands layout created (%s extension, stride=%u bytes)",
                   m_Config.useNVExtension ? "NV" : "EXT", SEQUENCE_STRIDE);
    return true;
}

void DeviceGeneratedCommands::GenerateCommands(
    VkCommandBuffer cmd,
    VkBuffer objectBuffer,
    VkBuffer objectDataBuffer,
    VkBuffer visibilityBuffer,
    u32 objectCount
) {
    if (!m_GeneratePipeline || !m_SequenceBuffer || !m_SequenceCountBuffer) return;

    // Reset sequence count to 0
    vkCmdFillBuffer(cmd, m_SequenceCountBuffer->GetBuffer(), 0, sizeof(u32), 0);

    VkMemoryBarrier fillBarrier{};
    fillBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    fillBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    fillBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &fillBarrier, 0, nullptr, 0, nullptr);

    // Update descriptor set with current frame's buffers
    VkDescriptorBufferInfo bufferInfos[5] = {};

    // binding 0: CullableObject SSBO
    bufferInfos[0].buffer = objectBuffer;
    bufferInfos[0].offset = 0;
    bufferInfos[0].range = VK_WHOLE_SIZE;

    // binding 1: ObjectDataGPU SSBO
    bufferInfos[1].buffer = objectDataBuffer;
    bufferInfos[1].offset = 0;
    bufferInfos[1].range = VK_WHOLE_SIZE;

    // binding 2: Visibility buffer
    bufferInfos[2].buffer = visibilityBuffer;
    bufferInfos[2].offset = 0;
    bufferInfos[2].range = VK_WHOLE_SIZE;

    // binding 3: Sequence buffer (output)
    bufferInfos[3].buffer = m_SequenceBuffer->GetBuffer();
    bufferInfos[3].offset = 0;
    bufferInfos[3].range = VK_WHOLE_SIZE;

    // binding 4: Sequence count buffer
    bufferInfos[4].buffer = m_SequenceCountBuffer->GetBuffer();
    bufferInfos[4].offset = 0;
    bufferInfos[4].range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet writes[5] = {};
    for (u32 i = 0; i < 5; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = m_GenerateDescriptorSet;
        writes[i].dstBinding = i;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].descriptorCount = 1;
        writes[i].pBufferInfo = &bufferInfos[i];
    }

    vkUpdateDescriptorSets(m_Context->GetDevice(), 5, writes, 0, nullptr);

    // Bind and dispatch compute shader
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_GeneratePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_GeneratePipelineLayout, 0, 1, &m_GenerateDescriptorSet, 0, nullptr);

    vkCmdPushConstants(cmd, m_GeneratePipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32), &objectCount);

    u32 workgroups = (objectCount + 63) / 64;
    vkCmdDispatch(cmd, workgroups, 1, 1);

    // Barrier: compute shader writes -> DGC preprocess/execute reads
    VkMemoryBarrier computeBarrier{};
    computeBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    computeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    computeBarrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
        0, 1, &computeBarrier, 0, nullptr, 0, nullptr);
}

void DeviceGeneratedCommands::Preprocess(VkCommandBuffer cmd) {
    if (!m_Config.supported || !m_PreprocessBuffer || !m_SequenceBuffer) return;

    if (!m_Config.useNVExtension) {
#if defined(VK_EXT_device_generated_commands)
        if (!m_CommandsLayoutEXT || !m_ExecutionSetEXT) return;

        VkGeneratedCommandsInfoEXT genInfo{};
        genInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_EXT;
        genInfo.shaderStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        genInfo.indirectExecutionSet = m_ExecutionSetEXT;
        genInfo.indirectCommandsLayout = m_CommandsLayoutEXT;
        genInfo.indirectAddress = m_SequenceBuffer->GetDeviceAddress();
        genInfo.indirectAddressSize = static_cast<VkDeviceSize>(m_MaxSequences) * SEQUENCE_STRIDE;
        genInfo.preprocessAddress = m_PreprocessBuffer->GetDeviceAddress();
        genInfo.preprocessSize = m_PreprocessSize;
        genInfo.sequenceCountAddress = m_SequenceCountBuffer->GetDeviceAddress();
        genInfo.maxSequenceCount = m_MaxSequences;

        m_vkCmdPreprocessGeneratedCommandsEXT(cmd, &genInfo, cmd);
#else
        return;
#endif

    } else {
#if defined(VK_NV_device_generated_commands)
        if (!m_CommandsLayoutNV) return;

        VkGeneratedCommandsInfoNV genInfo{};
        genInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV;
        genInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        genInfo.pipeline = m_GraphicsPipeline;
        genInfo.indirectCommandsLayout = m_CommandsLayoutNV;
        genInfo.streamCount = 1;

        VkIndirectCommandsStreamNV stream{};
        stream.buffer = m_SequenceBuffer->GetBuffer();
        stream.offset = 0;
        genInfo.pStreams = &stream;

        genInfo.sequencesCount = m_MaxSequences;
        genInfo.preprocessBuffer = m_PreprocessBuffer->GetBuffer();
        genInfo.preprocessOffset = 0;
        genInfo.preprocessSize = m_PreprocessSize;
        genInfo.sequencesCountBuffer = m_SequenceCountBuffer->GetBuffer();
        genInfo.sequencesCountOffset = 0;

        m_vkCmdPreprocessGeneratedCommandsNV(cmd, &genInfo);
#else
        return;
#endif
    }

    // Barrier after preprocess
    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void DeviceGeneratedCommands::Execute(VkCommandBuffer cmd) {
    if (!m_Config.supported || !m_SequenceBuffer) return;

    if (!m_Config.useNVExtension) {
#if defined(VK_EXT_device_generated_commands)
        if (!m_CommandsLayoutEXT || !m_ExecutionSetEXT) return;

        VkGeneratedCommandsInfoEXT genInfo{};
        genInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_EXT;
        genInfo.shaderStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        genInfo.indirectExecutionSet = m_ExecutionSetEXT;
        genInfo.indirectCommandsLayout = m_CommandsLayoutEXT;
        genInfo.indirectAddress = m_SequenceBuffer->GetDeviceAddress();
        genInfo.indirectAddressSize = static_cast<VkDeviceSize>(m_MaxSequences) * SEQUENCE_STRIDE;
        genInfo.preprocessAddress = m_PreprocessBuffer ? m_PreprocessBuffer->GetDeviceAddress() : 0;
        genInfo.preprocessSize = m_PreprocessSize;
        genInfo.sequenceCountAddress = m_SequenceCountBuffer->GetDeviceAddress();
        genInfo.maxSequenceCount = m_MaxSequences;

        // isPreprocessed = VK_TRUE since we called Preprocess first
        m_vkCmdExecuteGeneratedCommandsEXT(cmd, VK_TRUE, &genInfo);
#else
        return;
#endif

    } else {
#if defined(VK_NV_device_generated_commands)
        if (!m_CommandsLayoutNV) return;

        VkGeneratedCommandsInfoNV genInfo{};
        genInfo.sType = VK_STRUCTURE_TYPE_GENERATED_COMMANDS_INFO_NV;
        genInfo.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        genInfo.pipeline = m_GraphicsPipeline;
        genInfo.indirectCommandsLayout = m_CommandsLayoutNV;
        genInfo.streamCount = 1;

        VkIndirectCommandsStreamNV stream{};
        stream.buffer = m_SequenceBuffer->GetBuffer();
        stream.offset = 0;
        genInfo.pStreams = &stream;

        genInfo.sequencesCount = m_MaxSequences;
        genInfo.preprocessBuffer = m_PreprocessBuffer->GetBuffer();
        genInfo.preprocessOffset = 0;
        genInfo.preprocessSize = m_PreprocessSize;
        genInfo.sequencesCountBuffer = m_SequenceCountBuffer->GetBuffer();
        genInfo.sequencesCountOffset = 0;

        // isPreprocessed = VK_TRUE since we called Preprocess first
        m_vkCmdExecuteGeneratedCommandsNV(cmd, VK_TRUE, &genInfo);
#else
        return;
#endif
    }
}

VkBuffer DeviceGeneratedCommands::GetSequenceCountBuffer() const {
    return m_SequenceCountBuffer ? m_SequenceCountBuffer->GetBuffer() : VK_NULL_HANDLE;
}

void DeviceGeneratedCommands::Shutdown() {
    if (!m_Context) return;

    VkDevice device = m_Context->GetDevice();
    if (device == VK_NULL_HANDLE) return;

    // Destroy EXT objects
#if defined(VK_EXT_device_generated_commands)
    if (m_CommandsLayoutEXT != VK_NULL_HANDLE && m_vkDestroyIndirectCommandsLayoutEXT) {
        m_vkDestroyIndirectCommandsLayoutEXT(device, m_CommandsLayoutEXT, nullptr);
        m_CommandsLayoutEXT = VK_NULL_HANDLE;
    }
    if (m_ExecutionSetEXT != VK_NULL_HANDLE && m_vkDestroyIndirectExecutionSetEXT) {
        m_vkDestroyIndirectExecutionSetEXT(device, m_ExecutionSetEXT, nullptr);
        m_ExecutionSetEXT = VK_NULL_HANDLE;
    }
#endif

    // Destroy NV objects
#if defined(VK_NV_device_generated_commands)
    if (m_CommandsLayoutNV != VK_NULL_HANDLE && m_vkDestroyIndirectCommandsLayoutNV) {
        m_vkDestroyIndirectCommandsLayoutNV(device, m_CommandsLayoutNV, nullptr);
        m_CommandsLayoutNV = VK_NULL_HANDLE;
    }
#endif

    // Destroy compute pipeline resources
    if (m_GeneratePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_GeneratePipeline, nullptr);
        m_GeneratePipeline = VK_NULL_HANDLE;
    }
    if (m_GeneratePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_GeneratePipelineLayout, nullptr);
        m_GeneratePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_GenerateDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_GenerateDescriptorSetLayout, nullptr);
        m_GenerateDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_GenerateDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_GenerateDescriptorPool, nullptr);
        m_GenerateDescriptorPool = VK_NULL_HANDLE;
    }

    // Destroy buffers
    m_SequenceBuffer.reset();
    m_SequenceCountBuffer.reset();
    m_PreprocessBuffer.reset();

    m_GraphicsPipelineLayout = VK_NULL_HANDLE;
    m_GraphicsPipeline = VK_NULL_HANDLE;
    m_Config = {};
    m_Context = nullptr;
}

} // namespace Renderer
} // namespace Enjin
