// Pre-rendered backgrounds: drawing a finished image of a room, and the depth
// it was rendered at, so live characters occlude against it correctly.
//
// The plate is drawn FIRST in the scene pass and writes depth without testing
// it. Everything after is an ordinary depth-tested draw, which is the whole
// elegance of the technique: nothing else in the renderer needs to know that
// the room is a picture.
//
// Texture loading lives here rather than going through the shared texture cache
// because the two images want opposite treatment. The colour plate is a
// photograph and wants sRGB and filtering; the depth plate is a 24-bit NUMBER
// split across three bytes and must be UNORM and point-sampled, or the hardware
// rescales it on the way in and the room bends.

#include "Enjin/ECS/Systems/RenderSystem.h"

#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/ECS/Components/PreRenderedBackground.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/Renderer/DepthPlate.h"
#include "Enjin/Renderer/Texture.h"
#include "Enjin/Renderer/Vulkan/VulkanSampler.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/BindlessResources.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Assets/MeshAssetCache.h"

#include <filesystem>
#include "Enjin/Logging/Log.h"

#include <array>

// The implementation lives in VulkanImage.cpp; this is a plain declaration
// include, so no STB_IMAGE_IMPLEMENTATION and no STB_IMAGE_STATIC here.
#include "stb_image.h"

namespace Enjin {
namespace ECS {

struct PlatePushData {
    Math::Vector4 tex;       // x colour index, y depth index (-1 = none)
    Math::Vector4 mapping;   // x a, y b, z inverse flag, w world-unit bias
    Math::Vector4 range;     // x plate near, y plate far
};

// --- Loading -----------------------------------------------------------------

u32 RenderSystem::LoadPlateImage(const std::string& path, bool isDepth) {
    if (path.empty()) return UINT32_MAX;

    auto it = m_PlateTextures.find(path);
    if (it != m_PlateTextures.end()) return it->second.bindless;

    // Same rooting as every other texture: a scene stores a project-relative
    // path, and the process CWD is never reliable (editor and player both run
    // from the exe directory), so a bare relative path has to be joined to the
    // asset search root before it will open.
    std::string resolved = path;
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        if (fs::path(path).is_relative() && !fs::exists(path, ec)) {
            const std::string& root = Assets::MeshAssetCache::Get().GetSearchRoot();
            if (!root.empty()) {
                std::string joined = (fs::path(root) / path).string();
                if (fs::exists(joined, ec)) resolved = joined;
            }
        }
    }

    int w = 0, h = 0, channels = 0;
    // Forced to 4 channels: the depth packing reads r, g and b, and a 3-channel
    // upload would need a format this loader does not otherwise use.
    stbi_uc* pixels = stbi_load(resolved.c_str(), &w, &h, &channels, 4);
    if (!pixels || w <= 0 || h <= 0) {
        if (pixels) stbi_image_free(pixels);
        ENJIN_LOG_ERROR(Renderer, "Background plate could not be loaded: %s", resolved.c_str());
        // Remembered as a failure so a missing file is reported once rather
        // than retried every frame for the life of the scene.
        m_PlateTextures[path] = PlateTexture{};
        return UINT32_MAX;
    }

    PlateTexture entry;
    entry.texture = std::make_shared<Renderer::Texture>(m_VulkanRenderer->GetContext());

    // The whole reason this does not use the shared texture loader. A depth
    // plate is data: UNORM so the bytes arrive unchanged, NEAREST so two
    // distances are never averaged, and no mip chain because a smaller version
    // of a depth plate is a room at a distance that does not exist.
    const VkFormat format = isDepth ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
    const Renderer::SamplerConfig sampler = isDepth ? Renderer::VulkanSampler::Nearest()
                                                    : Renderer::SamplerConfig{};
    const bool ok = entry.texture->CreateFromData(pixels, static_cast<u32>(w), static_cast<u32>(h),
                                                  4, format, sampler, /*generateMips=*/!isDepth);
    stbi_image_free(pixels);

    if (!ok) {
        ENJIN_LOG_ERROR(Renderer, "Background plate texture could not be created: %s", resolved.c_str());
        m_PlateTextures[path] = PlateTexture{};
        return UINT32_MAX;
    }

    entry.bindless = m_BindlessManager->RegisterTexture(entry.texture->GetImageView(),
                                                        entry.texture->GetSampler());
    if (entry.bindless == UINT32_MAX) {
        ENJIN_LOG_ERROR(Renderer, "Background plate got no bindless slot: %s", resolved.c_str());
        entry.texture.reset();
    } else {
        ENJIN_LOG_INFO(Renderer, "Background plate loaded: %s (%dx%d, %s, bindless %u)",
                       resolved.c_str(), w, h, isDepth ? "depth" : "colour", entry.bindless);
    }

    const u32 handle = entry.bindless;
    m_PlateTextures[path] = std::move(entry);
    return handle;
}

void RenderSystem::UpdatePreRenderedPlates() {
    if (!m_VulkanRenderer || !m_BindlessManager || !m_World) return;

    // Creating images and registering bindless slots, so this only ever runs
    // from FlushPendingChanges -- the one place with no command buffer open.
    for (Entity e : m_World->GetEntitiesWithComponent<PreRenderedBackgroundComponent>()) {
        auto* bg = m_World->GetComponent<PreRenderedBackgroundComponent>(e);
        if (!bg || !bg->enabled) continue;
        if (!bg->platePath.empty()) LoadPlateImage(bg->platePath, /*isDepth=*/false);
        if (!bg->depthPath.empty()) LoadPlateImage(bg->depthPath, /*isDepth=*/true);
    }
}

void RenderSystem::ClearPreRenderedPlates() {
    m_PlateTextures.clear();
}

// --- Pipeline ----------------------------------------------------------------

void RenderSystem::CreatePlatePipeline(VkRenderPass renderPass) {
    if (!m_Renderer || !m_VulkanRenderer->GetContext()) return;
    VkDevice device = m_VulkanRenderer->GetContext()->GetDevice();

    if (m_PlatePipelineLayout == VK_NULL_HANDLE) {
        VkPushConstantRange pcRange{};
        pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pcRange.offset = 0;
        pcRange.size = sizeof(PlatePushData);   // 48 bytes

        // Set 1 is the bindless table, so an empty set 0 has to precede it to
        // put it at the index the shader declares. Same shape as the 2D sky.
        VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayoutCreateInfo emptyInfo{};
        emptyInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        vkCreateDescriptorSetLayout(device, &emptyInfo, nullptr, &emptyLayout);
        VkDescriptorSetLayout bindless = m_BindlessManager ? m_BindlessManager->GetDescriptorSetLayout()
                                                           : VK_NULL_HANDLE;
        VkDescriptorSetLayout sets[2] = { emptyLayout, bindless };

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = (bindless != VK_NULL_HANDLE) ? 2 : 0;
        layoutInfo.pSetLayouts = (bindless != VK_NULL_HANDLE) ? sets : nullptr;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pcRange;
        VkResult lr = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_PlatePipelineLayout);
        if (emptyLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, emptyLayout, nullptr);
        if (lr != VK_SUCCESS) {
            ENJIN_LOG_WARN(Renderer, "Failed to create background plate pipeline layout");
            return;
        }
    }

    VkRenderPass mainPass = (renderPass != VK_NULL_HANDLE) ? renderPass : m_VulkanRenderer->GetRenderPass();
    if (m_PlatePipeline == VK_NULL_HANDLE)
        CreatePlatePipelineVariant(mainPass, 2, m_VulkanRenderer->GetMSAASamples(), m_PlatePipeline);
    if (m_OffscreenRenderPass != VK_NULL_HANDLE && m_PlatePipelineOffscreen == VK_NULL_HANDLE)
        CreatePlatePipelineVariant(m_OffscreenRenderPass, 1, VK_SAMPLE_COUNT_1_BIT, m_PlatePipelineOffscreen);
}

bool RenderSystem::CreatePlatePipelineVariant(VkRenderPass renderPass, u32 colorAttachmentCount,
                                              VkSampleCountFlagBits samples, VkPipeline& outPipeline) {
    if (!m_Renderer || renderPass == VK_NULL_HANDLE || m_PlatePipelineLayout == VK_NULL_HANDLE)
        return false;
    auto* context = m_VulkanRenderer->GetContext();
    VkDevice device = context->GetDevice();

    Renderer::VulkanShader vert(context);
    if (!vert.LoadFromSPIRV(reinterpret_cast<const u8*>(Renderer::ShaderData::FullscreenVertexShaderData),
                            Renderer::ShaderData::FullscreenVertexShaderDataSize)) return false;
    Renderer::VulkanShader frag(context);
    if (!frag.LoadFromSPIRV(reinterpret_cast<const u8*>(Renderer::ShaderData::PlateFragmentShaderData),
                            Renderer::ShaderData::PlateFragmentShaderDataSize)) return false;

    VkPipelineVertexInputStateCreateInfo vertexInput{};   // fullscreen triangle: no vertex buffer
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dyn;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = samples;

    // The one place this differs from every other fullscreen background: it
    // WRITES depth. That is what makes the room occlude live geometry.
    //
    // The test is ENABLED with COMPARE_OP_ALWAYS rather than disabled, and the
    // difference is not cosmetic: with depthTestEnable VK_FALSE the depth
    // attachment is not written AT ALL, whatever depthWriteEnable says. The
    // plate then draws its colour perfectly and occludes nothing, which looks
    // like a broken depth plate rather than a pipeline flag.
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

    std::array<VkPipelineColorBlendAttachmentState, 2> blend{};
    blend[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // The main pass is MRT (colour + velocity). The plate has no velocity to
    // report -- it never moves -- so the second attachment is masked off.
    blend[1].colorWriteMask = 0;
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = (colorAttachmentCount <= 2) ? colorAttachmentCount : 2;
    colorBlending.pAttachments = blend.data();

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vert.GetModule();
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = frag.GetModule();
    stages[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_PlatePipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &outPipeline) == VK_SUCCESS;
}

// --- Draw --------------------------------------------------------------------

const PreRenderedBackgroundComponent* RenderSystem::ActivePlate() const {
    if (!m_World) return nullptr;

    // The plate belongs to the camera it was rendered from, so the only plate
    // that can be correct is the ACTIVE camera's. Anything else would paint one
    // room's walls over another room's view.
    for (Entity e : m_World->GetEntitiesWithComponent<PreRenderedBackgroundComponent>()) {
        const auto* cam = m_World->GetComponent<CameraComponent>(e);
        if (!cam || !cam->isActive) continue;
        const auto* bg = m_World->GetComponent<PreRenderedBackgroundComponent>(e);
        if (bg && bg->enabled && bg->visible && !bg->platePath.empty()) return bg;
    }
    return nullptr;
}

void RenderSystem::RenderPreRenderedPlate(VkCommandBuffer commandBuffer,
                                          const VkViewport* viewportOverride,
                                          const VkRect2D* scissorOverride,
                                          bool offscreenPass) {
    VkPipeline pipeline = offscreenPass ? m_PlatePipelineOffscreen : m_PlatePipeline;
    if (pipeline == VK_NULL_HANDLE || !m_Camera) return;

    const PreRenderedBackgroundComponent* bg = ActivePlate();
    if (!bg) return;

    auto colorIt = m_PlateTextures.find(bg->platePath);
    if (colorIt == m_PlateTextures.end() || colorIt->second.bindless == UINT32_MAX) return;

    u32 depthIdx = UINT32_MAX;
    if (!bg->depthPath.empty()) {
        auto depthIt = m_PlateTextures.find(bg->depthPath);
        if (depthIt != m_PlateTextures.end()) depthIdx = depthIt->second.bindless;
    }

    VkDescriptorSet bindlessSet = m_BindlessManager ? m_BindlessManager->GetDescriptorSet() : VK_NULL_HANDLE;
    if (bindlessSet == VK_NULL_HANDLE) return;

    // Solved from the camera's CURRENT projection every frame rather than baked
    // into the component. A window resize changes the aspect and a field-of-view
    // change rewrites the whole mapping; a stored one would quietly stop
    // matching the geometry drawn next to it.
    const Renderer::PlateDepthMapping mapping =
        Renderer::SolvePlateDepthMapping(m_Camera->GetProjectionMatrix(),
                                         bg->depthNear, bg->depthFar);

    PlatePushData pc{};
    pc.tex = { static_cast<f32>(colorIt->second.bindless),
               (depthIdx != UINT32_MAX) ? static_cast<f32>(depthIdx) : -1.0f, 0.0f, 0.0f };
    pc.mapping = { mapping.a, mapping.b, mapping.inverse ? 1.0f : 0.0f, bg->depthBias };
    pc.range = { bg->depthNear, bg->depthFar, 0.0f, 0.0f };

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_PlatePipelineLayout, 1, 1, &bindlessSet, 0, nullptr);

    VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
    VkViewport vp{};
    if (viewportOverride) { vp = *viewportOverride; }
    else { vp.width = static_cast<f32>(extent.width); vp.height = static_cast<f32>(extent.height); vp.maxDepth = 1.0f; }
    VkRect2D sc{};
    if (scissorOverride) { sc = *scissorOverride; }
    else { sc.extent = extent; }
    vkCmdSetViewport(commandBuffer, 0, 1, &vp);
    vkCmdSetScissor(commandBuffer, 0, 1, &sc);

    vkCmdPushConstants(commandBuffer, m_PlatePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(pc), &pc);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

} // namespace ECS
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
