#include "Enjin/Renderer/VisibilityBuffer/VisibilityBuffer.h"
#include "Enjin/Renderer/VisibilityBuffer/MaterialResolve.h"
#include "Enjin/Renderer/Vulkan/VulkanBuffer.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

namespace Enjin {
namespace Renderer {

VisibilityBufferRenderer::VisibilityBufferRenderer(VulkanContext* context)
    : m_Context(context) {}

VisibilityBufferRenderer::~VisibilityBufferRenderer() {
    Shutdown();
}

bool VisibilityBufferRenderer::Initialize(u32 width, u32 height, VkRenderPass compatibleRenderPass) {
    (void)compatibleRenderPass;
    if (m_Initialized) return true;

    m_Width = width;
    m_Height = height;

    if (!CreateVisibilityBuffer()) {
        ENJIN_LOG_ERROR(Renderer, "VisibilityBuffer: Failed to create visibility buffer");
        return false;
    }

    if (!CreateVisibilityRenderPass()) {
        ENJIN_LOG_ERROR(Renderer, "VisibilityBuffer: Failed to create render pass");
        return false;
    }

    if (!CreateVisibilityFramebuffer()) {
        ENJIN_LOG_ERROR(Renderer, "VisibilityBuffer: Failed to create framebuffer");
        return false;
    }

    if (!CreateVisibilityPipeline()) {
        ENJIN_LOG_WARN(Renderer, "VisibilityBuffer: Visibility pipeline creation failed");
    }

    if (!CreateResolvePipeline()) {
        ENJIN_LOG_WARN(Renderer, "VisibilityBuffer: Resolve pipeline creation failed");
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "VisibilityBuffer initialized: %ux%u", width, height);
    return true;
}

void VisibilityBufferRenderer::Shutdown() {
    DestroyResources();
    m_Initialized = false;
}

void VisibilityBufferRenderer::DestroyResources() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (m_ResolvePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_ResolvePipeline, nullptr);
        m_ResolvePipeline = VK_NULL_HANDLE;
    }
    if (m_ResolvePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_ResolvePipelineLayout, nullptr);
        m_ResolvePipelineLayout = VK_NULL_HANDLE;
    }
    if (m_ResolveDescSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_ResolveDescSetLayout, nullptr);
        m_ResolveDescSetLayout = VK_NULL_HANDLE;
    }
    if (m_ResolveDescPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_ResolveDescPool, nullptr);
        m_ResolveDescPool = VK_NULL_HANDLE;
    }

    if (m_VisPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_VisPipeline, nullptr);
        m_VisPipeline = VK_NULL_HANDLE;
    }
    if (m_VisPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_VisPipelineLayout, nullptr);
        m_VisPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_VisFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_VisFramebuffer, nullptr);
        m_VisFramebuffer = VK_NULL_HANDLE;
    }
    if (m_VisRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_VisRenderPass, nullptr);
        m_VisRenderPass = VK_NULL_HANDLE;
    }

    auto destroyImage = [&](VkImage& img, VkDeviceMemory& mem, VkImageView& view) {
        if (view != VK_NULL_HANDLE) { vkDestroyImageView(device, view, nullptr); view = VK_NULL_HANDLE; }
        if (img != VK_NULL_HANDLE) { vkDestroyImage(device, img, nullptr); img = VK_NULL_HANDLE; }
        if (mem != VK_NULL_HANDLE) { vkFreeMemory(device, mem, nullptr); mem = VK_NULL_HANDLE; }
    };

    destroyImage(m_VisBuffer, m_VisBufferMemory, m_VisBufferView);
    destroyImage(m_DepthImage, m_DepthMemory, m_DepthView);
}

bool VisibilityBufferRenderer::CreateVisibilityBuffer() {
    VkDevice device = m_Context->GetDevice();

    // Visibility buffer: R32G32_UINT (triangleID, instanceID)
    VkImageCreateInfo visInfo{};
    visInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    visInfo.imageType = VK_IMAGE_TYPE_2D;
    visInfo.format = VK_FORMAT_R32G32_UINT;
    visInfo.extent = { m_Width, m_Height, 1 };
    visInfo.mipLevels = 1;
    visInfo.arrayLayers = 1;
    visInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    visInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    visInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    visInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    visInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &visInfo, nullptr, &m_VisBuffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_VisBuffer, &memReqs);

    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(m_Context->GetPhysicalDevice(), &memProps);

    u32 memTypeIndex = 0;
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((memReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            memTypeIndex = i;
            break;
        }
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memTypeIndex;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_VisBufferMemory) != VK_SUCCESS) {
        return false;
    }
    vkBindImageMemory(device, m_VisBuffer, m_VisBufferMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_VisBuffer;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32_UINT;
    viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_VisBufferView) != VK_SUCCESS) {
        return false;
    }

    // Depth buffer for visibility pass
    VkImageCreateInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType = VK_IMAGE_TYPE_2D;
    depthInfo.format = VK_FORMAT_D32_SFLOAT;
    depthInfo.extent = { m_Width, m_Height, 1 };
    depthInfo.mipLevels = 1;
    depthInfo.arrayLayers = 1;
    depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depthInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(device, &depthInfo, nullptr, &m_DepthImage) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements depthReqs;
    vkGetImageMemoryRequirements(device, m_DepthImage, &depthReqs);
    VkMemoryAllocateInfo depthAlloc{};
    depthAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    depthAlloc.allocationSize = depthReqs.size;
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((depthReqs.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) {
            depthAlloc.memoryTypeIndex = i;
            break;
        }
    }
    if (vkAllocateMemory(device, &depthAlloc, nullptr, &m_DepthMemory) != VK_SUCCESS) {
        return false;
    }
    vkBindImageMemory(device, m_DepthImage, m_DepthMemory, 0);

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = m_DepthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
    depthViewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device, &depthViewInfo, nullptr, &m_DepthView) != VK_SUCCESS) {
        return false;
    }

    return true;
}

bool VisibilityBufferRenderer::CreateVisibilityRenderPass() {
    VkDevice device = m_Context->GetDevice();

    VkAttachmentDescription attachments[2] = {};

    // Color attachment (visibility buffer — R32G32_UINT)
    attachments[0].format = VK_FORMAT_R32G32_UINT;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Depth attachment
    attachments[1].format = VK_FORMAT_D32_SFLOAT;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = attachments;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    return vkCreateRenderPass(device, &rpInfo, nullptr, &m_VisRenderPass) == VK_SUCCESS;
}

bool VisibilityBufferRenderer::CreateVisibilityFramebuffer() {
    VkDevice device = m_Context->GetDevice();

    VkImageView fbAttachments[] = { m_VisBufferView, m_DepthView };
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_VisRenderPass;
    fbInfo.attachmentCount = 2;
    fbInfo.pAttachments = fbAttachments;
    fbInfo.width = m_Width;
    fbInfo.height = m_Height;
    fbInfo.layers = 1;

    return vkCreateFramebuffer(device, &fbInfo, nullptr, &m_VisFramebuffer) == VK_SUCCESS;
}

bool VisibilityBufferRenderer::CreateVisibilityPipeline() {
    VkDevice device = m_Context->GetDevice();

    // --- Load shaders ---
    VulkanShader vertShader(m_Context);
    VulkanShader fragShader(m_Context);

    const char* vertPaths[] = {
        "shaders/visibility.vert.spv",
        "Engine/shaders/visibility.vert.spv",
        "../Engine/shaders/visibility.vert.spv",
        "../../Engine/shaders/visibility.vert.spv"
    };
    const char* fragPaths[] = {
        "shaders/visibility.frag.spv",
        "Engine/shaders/visibility.frag.spv",
        "../Engine/shaders/visibility.frag.spv",
        "../../Engine/shaders/visibility.frag.spv"
    };

    bool vertLoaded = false;
    for (const char* path : vertPaths) {
        if (vertShader.LoadFromFile(path, false) && vertShader.GetModule() != VK_NULL_HANDLE) {
            vertLoaded = true;
            break;
        }
    }
    if (!vertLoaded) {
        ENJIN_LOG_WARN(Renderer, "VisibilityBuffer: visibility.vert.spv not found");
        return false;
    }

    bool fragLoaded = false;
    for (const char* path : fragPaths) {
        if (fragShader.LoadFromFile(path, false) && fragShader.GetModule() != VK_NULL_HANDLE) {
            fragLoaded = true;
            break;
        }
    }
    if (!fragLoaded) {
        ENJIN_LOG_WARN(Renderer, "VisibilityBuffer: visibility.frag.spv not found");
        return false;
    }

    // --- Shader stages ---
    VkPipelineShaderStageCreateInfo shaderStages[2] = {};

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShader.GetModule();
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShader.GetModule();
    shaderStages[1].pName = "main";

    // --- Vertex input ---
    // Must match the engine vertex layout (same buffer as the main pipeline).
    // The visibility vertex shader only reads location 0-2 (position, normal, UV)
    // but we bind the full stride so the same vertex buffers work.
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(f32) * 34; // Full engine vertex: 136 bytes (pos..boneIndices + uv1 + boneWeights2 + boneIndices2)
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribDescs[3] = {};
    // Position (location 0)
    attribDescs[0].binding = 0;
    attribDescs[0].location = 0;
    attribDescs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribDescs[0].offset = 0;
    // Normal (location 1)
    attribDescs[1].binding = 0;
    attribDescs[1].location = 1;
    attribDescs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribDescs[1].offset = sizeof(f32) * 3;
    // UV (location 2)
    attribDescs[2].binding = 0;
    attribDescs[2].location = 2;
    attribDescs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attribDescs[2].offset = sizeof(f32) * 6;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 3;
    vertexInputInfo.pVertexAttributeDescriptions = attribDescs;

    // --- Input assembly ---
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // --- Viewport / scissor (dynamic) ---
    VkViewport viewport = { 0, 0, 0, 0, 0, 1 };
    VkRect2D scissor = { { 0, 0 }, { 0, 0 } };

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // --- Rasterization ---
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // --- Multisampling (disabled — single-sample visibility buffer) ---
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // --- Depth-stencil ---
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // --- Color blend (no blending — writing integer IDs) ---
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // --- Dynamic state ---
    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // --- Pipeline layout (push constants only, no descriptor sets) ---
    // Push constants: mat4 mvp (64 bytes) + uint objectID + 3x uint padding = 80 bytes
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = 80;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pSetLayouts = nullptr;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_VisPipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "VisibilityBuffer: Failed to create pipeline layout");
        return false;
    }

    // --- Create graphics pipeline ---
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_VisPipelineLayout;
    pipelineInfo.renderPass = m_VisRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkResult result = vkCreateGraphicsPipelines(
        device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_VisPipeline);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "VisibilityBuffer: Failed to create graphics pipeline: %d", result);
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "VisibilityBuffer: Graphics pipeline created");
    return true;
}

bool VisibilityBufferRenderer::CreateResolvePipeline() {
    VkDevice device = m_Context->GetDevice();

    // Descriptor set layout for resolve pass
    VkDescriptorSetLayoutBinding bindings[5] = {};
    // 0: Visibility buffer (storage image)
    bindings[0] = { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    // 1: Depth buffer (sampled)
    bindings[1] = { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    // 2: Output color (storage image)
    bindings[2] = { 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    // 3: Vertex buffer (SSBO)
    bindings[3] = { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    // 4: Object data SSBO
    bindings[4] = { 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 5;
    layoutInfo.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_ResolveDescSetLayout) != VK_SUCCESS) {
        return false;
    }

    // Push constants
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = 128; // Max push constant size (use first 128 of MaterialResolvePushConstants)

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_ResolveDescSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_ResolvePipelineLayout) != VK_SUCCESS) {
        return false;
    }

    // Descriptor pool and set
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 }
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_ResolveDescPool) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_ResolveDescPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_ResolveDescSetLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &m_ResolveDescSet) != VK_SUCCESS) {
        return false;
    }

    // Load resolve compute shader
    VulkanShader shader(m_Context);
    const char* shaderPaths[] = {
        "shaders/material_resolve.comp.spv",
        "Engine/shaders/material_resolve.comp.spv",
        "../Engine/shaders/material_resolve.comp.spv",
        "../../Engine/shaders/material_resolve.comp.spv"
    };
    bool loaded = false;
    for (const char* path : shaderPaths) {
        if (shader.LoadFromFile(path, false) && shader.GetModule() != VK_NULL_HANDLE) {
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        ENJIN_LOG_WARN(Renderer, "VisibilityBuffer: material_resolve.comp.spv not found");
        return false;
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = m_ResolvePipelineLayout;
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shader.GetModule();
    stage.pName = "main";
    pipelineInfo.stage = stage;

    return vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_ResolvePipeline) == VK_SUCCESS;
}

void VisibilityBufferRenderer::BeginVisibilityPass(VkCommandBuffer commandBuffer) {
    if (!m_Initialized || m_VisRenderPass == VK_NULL_HANDLE) return;

    VkClearValue clearValues[2] = {};
    clearValues[0].color = { { 0, 0, 0, 0 } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = m_VisRenderPass;
    rpBegin.framebuffer = m_VisFramebuffer;
    rpBegin.renderArea = { { 0, 0 }, { m_Width, m_Height } };
    rpBegin.clearValueCount = 2;
    rpBegin.pClearValues = clearValues;

    vkCmdBeginRenderPass(commandBuffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = { 0, 0, (f32)m_Width, (f32)m_Height, 0, 1 };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    VkRect2D scissor = { { 0, 0 }, { m_Width, m_Height } };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void VisibilityBufferRenderer::EndVisibilityPass(VkCommandBuffer commandBuffer) {
    vkCmdEndRenderPass(commandBuffer);
}

void VisibilityBufferRenderer::ResolvePass(
    VkCommandBuffer commandBuffer,
    VkImageView outputColor,
    VkBuffer vertexBuffer,
    VkBuffer indexBuffer,
    VkBuffer objectDataBuffer,
    const Math::Matrix4& viewProj,
    const Math::Matrix4& invViewProj
) {
    if (!m_Initialized || m_ResolvePipeline == VK_NULL_HANDLE) return;
    (void)indexBuffer; // Index buffer access through object data

    // Transition visibility buffer for compute read
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.image = m_VisBuffer;
    barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ResolvePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
        m_ResolvePipelineLayout, 0, 1, &m_ResolveDescSet, 0, nullptr);

    // Push constants (first 128 bytes — viewProj + invViewProj)
    struct ResolvePC {
        Math::Matrix4 vp;
        Math::Matrix4 ivp;
    } pc;
    pc.vp = viewProj;
    pc.ivp = invViewProj;
    vkCmdPushConstants(commandBuffer, m_ResolvePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 128, &pc);

    u32 groupsX = (m_Width + 7) / 8;
    u32 groupsY = (m_Height + 7) / 8;
    vkCmdDispatch(commandBuffer, groupsX, groupsY, 1);
}

void VisibilityBufferRenderer::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    DestroyResources();
    m_Width = width;
    m_Height = height;
    if (m_Initialized) {
        CreateVisibilityBuffer();
        CreateVisibilityFramebuffer();
    }
}

} // namespace Renderer
} // namespace Enjin
