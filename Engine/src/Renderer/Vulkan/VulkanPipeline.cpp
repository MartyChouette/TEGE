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

void VulkanPipeline::BindVariant(VkCommandBuffer commandBuffer, VkPipeline variant) {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, variant);
}

VkPipeline VulkanPipeline::GetVariant(const MaterialSpecKey& key) {
    if (!m_HasTemplateCI || !m_Context) return m_Pipeline; // Fallback to default
    return m_VariantCache.GetOrCreate(
        m_Context->GetDevice(), m_PipelineLayout, m_RenderPass,
        m_TemplateCICache, key);
}

void VulkanPipeline::Destroy() {
    // Destroy cached pipeline variants before the default pipeline
    if (m_Context) {
        m_VariantCache.Destroy(m_Context->GetDevice());
    }
    m_HasTemplateCI = false;

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
    std::array<VkDescriptorSetLayoutBinding, 21> bindings{};

    // UBO binding 0: model/view/projection matrices (vertex shader)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[0].pImmutableSamplers = nullptr;

    // UBO binding 1: lighting data (vertex + fragment shader for shadows)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[1].pImmutableSamplers = nullptr;

    // SSBO binding 2: batched material data with dynamic offset (fragment shader)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[2].pImmutableSamplers = nullptr;

    // Sampler binding 3: base color texture (fragment shader)
    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[3].pImmutableSamplers = nullptr;

    // Sampler binding 4: shadow map (fragment shader)
    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[4].pImmutableSamplers = nullptr;

    // Sampler binding 5: height map for parallax mapping (fragment shader)
    bindings[5].binding = 5;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[5].pImmutableSamplers = nullptr;

    // Sampler binding 6: normal map (fragment shader)
    bindings[6].binding = 6;
    bindings[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[6].descriptorCount = 1;
    bindings[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[6].pImmutableSamplers = nullptr;

    // SSBO binding 7: bone matrices for skeletal animation (vertex shader)
    bindings[7].binding = 7;
    bindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[7].descriptorCount = 1;
    bindings[7].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[7].pImmutableSamplers = nullptr;

    // Sampler binding 8: metallic-roughness texture (fragment shader)
    bindings[8].binding = 8;
    bindings[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[8].descriptorCount = 1;
    bindings[8].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[8].pImmutableSamplers = nullptr;

    // Sampler binding 9: emissive texture (fragment shader)
    bindings[9].binding = 9;
    bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[9].descriptorCount = 1;
    bindings[9].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[9].pImmutableSamplers = nullptr;

    // Sampler binding 10: point light shadow cubemap array (fragment shader)
    bindings[10].binding = 10;
    bindings[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[10].descriptorCount = 1;
    bindings[10].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[10].pImmutableSamplers = nullptr;

    // Sampler binding 11: spot light shadow 2D array (fragment shader)
    bindings[11].binding = 11;
    bindings[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[11].descriptorCount = 1;
    bindings[11].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[11].pImmutableSamplers = nullptr;

    // SSBO binding 12: shadow data for point/spot lights (fragment shader)
    bindings[12].binding = 12;
    bindings[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[12].descriptorCount = 1;
    bindings[12].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[12].pImmutableSamplers = nullptr;

    // SSBO binding 13: per-object data for indirect rendering (vertex + fragment)
    bindings[13].binding = 13;
    bindings[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[13].descriptorCount = 1;
    bindings[13].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[13].pImmutableSamplers = nullptr;

    // SSBO binding 14: clustered lighting grid (fragment shader)
    bindings[14].binding = 14;
    bindings[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[14].descriptorCount = 1;
    bindings[14].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[14].pImmutableSamplers = nullptr;

    // SSBO binding 15: clustered lighting index list (fragment shader)
    bindings[15].binding = 15;
    bindings[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[15].descriptorCount = 1;
    bindings[15].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[15].pImmutableSamplers = nullptr;

    // Sampler binding 16: virtual texturing indirection texture (fragment shader)
    bindings[16].binding = 16;
    bindings[16].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[16].descriptorCount = 1;
    bindings[16].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[16].pImmutableSamplers = nullptr;

    // Sampler binding 17: virtual texturing physical atlas (fragment shader)
    bindings[17].binding = 17;
    bindings[17].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[17].descriptorCount = 1;
    bindings[17].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[17].pImmutableSamplers = nullptr;

    // Sampler binding 18: matcap texture (fragment shader)
    bindings[18].binding = 18;
    bindings[18].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[18].descriptorCount = 1;
    bindings[18].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[18].pImmutableSamplers = nullptr;

    // Sampler binding 19: baked reflection probe cubemap (fragment shader)
    bindings[19].binding = 19;
    bindings[19].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[19].descriptorCount = 1;
    bindings[19].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    bindings[19].pImmutableSamplers = nullptr;

    // SSBO binding 20: morph target deltas + weights (vertex shader)
    bindings[20].binding = 20;
    bindings[20].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[20].descriptorCount = 1;
    bindings[20].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[20].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

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
    // Model matrix used in vertex shader, material used in fragment shader
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    // Support bindless textures via set 1 (optional — if layout provided)
    VkDescriptorSetLayout setLayouts[2] = { m_DescriptorSetLayout, m_BindlessSetLayout };
    u32 setCount = (m_BindlessSetLayout != VK_NULL_HANDLE) ? 2 : 1;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = setCount;
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

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

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertexShader->GetModule();
    vertShaderStageInfo.pName = "main";
    shaderStages.push_back(vertShaderStageInfo);

    // Fragment shader is optional (null for depth-only passes like shadow mapping)
    if (fragmentShader != nullptr) {
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragmentShader->GetModule();
        fragShaderStageInfo.pName = "main";
        shaderStages.push_back(fragShaderStageInfo);
    }

    // Vertex input - position, normal, UV, color, tangent, boneWeights, boneIndices
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(f32) * 24; // vec3 pos + vec3 normal + vec2 uv + vec4 color + vec4 tangent + vec4 boneWeights + uvec4 boneIndices = 96 bytes
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 7> attributeDescriptions{};
    // Position (location 0)
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = 0;
    // Normal (location 1)
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = sizeof(f32) * 3;
    // UV (location 2)
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = sizeof(f32) * 6;
    // Vertex color (location 3)
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[3].offset = sizeof(f32) * 8;
    // Tangent (location 4)
    attributeDescriptions[4].binding = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[4].offset = sizeof(f32) * 12;
    // Bone weights (location 5)
    attributeDescriptions[5].binding = 0;
    attributeDescriptions[5].location = 5;
    attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[5].offset = sizeof(f32) * 16;
    // Bone indices (location 6)
    attributeDescriptions[6].binding = 0;
    attributeDescriptions[6].location = 6;
    attributeDescriptions[6].format = VK_FORMAT_R32G32B32A32_UINT;
    attributeDescriptions[6].offset = sizeof(f32) * 20;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<u32>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = config.topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 0.0f; // Will be set dynamically
    viewport.height = 0.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { 0, 0 }; // Will be set dynamically

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = config.polygonMode;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = config.cullMode;
    rasterizer.frontFace = config.frontFace;
    rasterizer.depthBiasEnable = config.depthBiasEnable ? VK_TRUE : VK_FALSE;
    rasterizer.depthBiasConstantFactor = config.depthBiasConstant;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = config.depthBiasSlope;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = config.msaaSamples;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    // Color blending — supports MRT (multiple color attachments for velocity buffer)
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                           VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT |
                                           VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = config.alphaBlend ? VK_TRUE : VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = config.alphaBlend ? VK_BLEND_FACTOR_SRC_ALPHA : VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = config.alphaBlend ? VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    // Velocity attachment: no blending, write RG only
    VkPipelineColorBlendAttachmentState velocityBlendAttachment{};
    velocityBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
    velocityBlendAttachment.blendEnable = VK_FALSE;

    // Build attachment array: [0]=color, [1]=velocity (if MRT)
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
    u32 actualColorCount = config.hasColorAttachment ? config.colorAttachmentCount : 0;
    if (actualColorCount >= 1) blendAttachments.push_back(colorBlendAttachment);
    if (actualColorCount >= 2) blendAttachments.push_back(velocityBlendAttachment);

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = static_cast<u32>(blendAttachments.size());
    colorBlending.pAttachments = blendAttachments.empty() ? nullptr : blendAttachments.data();
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // Dynamic state (viewport and scissor can be changed at runtime)
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<u32>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // Depth stencil
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = config.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = config.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // Create pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<u32>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = config.customVertexInput ? config.customVertexInput : &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_PipelineLayout;
    pipelineInfo.renderPass = config.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkResult result = vkCreateGraphicsPipelines(
        m_Context->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create graphics pipeline: %d", result);
        return false;
    }

    // Cache template create info for variant pipeline creation
    m_RenderPass = config.renderPass;
    m_CachedStages.assign(shaderStages.begin(), shaderStages.end());
    m_TemplateCICache = pipelineInfo;
    m_TemplateCICache.pStages = m_CachedStages.data();
    m_HasTemplateCI = true;

    ENJIN_LOG_INFO(Renderer, "Graphics pipeline created successfully");
    return true;
}

} // namespace Renderer
} // namespace Enjin
