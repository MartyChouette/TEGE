#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <array>

namespace Enjin {
namespace Renderer {

VulkanPipeline::VulkanPipeline(VulkanContext* context)
    : m_Context(context) {
}

VulkanPipeline::~VulkanPipeline() {
    Destroy();
}

bool VulkanPipeline::Create(
    const PipelineConfig& config,
    VulkanShader* vertexShader,
    VulkanShader* fragmentShader
) {
    m_OwnsDescriptorSetLayout = true;
    if (!CreateDescriptorSetLayout()) {
        return false;
    }

    if (!CreatePipelineLayout()) {
        return false;
    }

    return CreatePipeline(config, vertexShader, fragmentShader);
}

bool VulkanPipeline::CreateWithLayout(
    const PipelineConfig& config,
    VulkanShader* vertexShader,
    VulkanShader* fragmentShader,
    VkDescriptorSetLayout sharedLayout
) {
    m_OwnsDescriptorSetLayout = false;
    m_DescriptorSetLayout = sharedLayout;

    if (!CreatePipelineLayout()) {
        return false;
    }

    return CreatePipeline(config, vertexShader, fragmentShader);
}

void VulkanPipeline::Destroy() {
    if (m_Pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_Context->GetDevice(), m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }

    if (m_PipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_Context->GetDevice(), m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    // Only destroy layout if we own it (not shared)
    if (m_DescriptorSetLayout != VK_NULL_HANDLE && m_OwnsDescriptorSetLayout) {
        vkDestroyDescriptorSetLayout(m_Context->GetDevice(), m_DescriptorSetLayout, nullptr);
        m_DescriptorSetLayout = VK_NULL_HANDLE;
    }
}

void VulkanPipeline::Bind(VkCommandBuffer commandBuffer) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline);
}

bool VulkanPipeline::CreateDescriptorSetLayout() {
    // C++20 Designated Initializers — clean, readable descriptor layout bindings
    std::array<VkDescriptorSetLayoutBinding, 7> bindings = {{
        {   // Binding 0: model/view/projection matrices (vertex shader)
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr
        },
        {   // Binding 1: lighting data (vertex + fragment shader for shadows)
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        },
        {   // Binding 2: material data (fragment shader)
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        },
        {   // Binding 3: base color texture (fragment shader)
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        },
        {   // Binding 4: shadow map (fragment shader)
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        },
        {   // Binding 5: height map for parallax mapping (fragment shader)
            .binding = 5,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        },
        {   // Binding 6: normal map (fragment shader)
            .binding = 6,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr
        }
    }};

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkResult result = vkCreateDescriptorSetLayout(
        m_Context->GetDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create descriptor set layout: %d", result);
        return false;
    }

    return true;
}

bool VulkanPipeline::CreatePipelineLayout() {
    // Push constant for model matrix + material (per-object data)
    VkPushConstantRange pushConstantRange{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &m_DescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkResult result = vkCreatePipelineLayout(
        m_Context->GetDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create pipeline layout: %d", result);
        return false;
    }

    return true;
}

bool VulkanPipeline::CreatePipeline(
    const PipelineConfig& config,
    VulkanShader* vertexShader,
    VulkanShader* fragmentShader
) {
    // Shader stages
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    shaderStages.push_back(VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertexShader->GetModule(),
        .pName = "main",
        .pSpecializationInfo = nullptr
    });

    // Fragment shader is optional (null for depth-only passes like shadow mapping)
    if (fragmentShader != nullptr) {
        shaderStages.push_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShader->GetModule(),
            .pName = "main",
            .pSpecializationInfo = nullptr
        });
    }

    // Vertex input - position, normal, UV, color, tangent
    VkVertexInputBindingDescription bindingDescription{
        .binding = 0,
        .stride = sizeof(f32) * 16,  // vec3 pos + vec3 normal + vec2 uv + vec4 color + vec4 tangent
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    std::array<VkVertexInputAttributeDescription, 5> attributeDescriptions = {{
        {   // Position (location 0)
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0
        },
        {   // Normal (location 1)
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = sizeof(f32) * 3
        },
        {   // UV (location 2)
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(f32) * 6
        },
        {   // Vertex color (location 3)
            .location = 3,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = sizeof(f32) * 8
        },
        {   // Tangent (location 4)
            .location = 4,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = sizeof(f32) * 12
        }
    }};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<u32>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()
    };

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = config.topology,
        .primitiveRestartEnable = VK_FALSE
    };

    // Viewport and scissor (set dynamically)
    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = 0.0f,
        .height = 0.0f,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    VkRect2D scissor{
        .offset = { 0, 0 },
        .extent = { 0, 0 }
    };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = config.polygonMode,
        .cullMode = config.cullMode,
        .frontFace = config.frontFace,
        .depthBiasEnable = config.depthBiasEnable ? VK_TRUE : VK_FALSE,
        .depthBiasConstantFactor = config.depthBiasConstant,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = config.depthBiasSlope,
        .lineWidth = 1.0f
    };

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = config.msaaSamples,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlending{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        // For depth-only passes (shadow mapping), no color attachments
        .attachmentCount = config.hasColorAttachment ? 1u : 0u,
        .pAttachments = config.hasColorAttachment ? &colorBlendAttachment : nullptr,
        .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f }
    };

    // Dynamic state (viewport and scissor can be changed at runtime)
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<u32>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = config.depthTest ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = config.depthWrite ? VK_TRUE : VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<u32>(shaderStages.size()),
        .pStages = shaderStages.data(),
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = m_PipelineLayout,
        .renderPass = config.renderPass,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkResult result = vkCreateGraphicsPipelines(
        m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create graphics pipeline: %d", result);
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "Graphics pipeline created successfully");
    return true;
}

} // namespace Renderer
} // namespace Enjin
