#include "Enjin/Renderer/PipelineVariantCache.h"
#include "Enjin/Logging/Log.h"

#if !ENJIN_RENDERER_WEBGPU

#include <array>

namespace Enjin {
namespace Renderer {

// VkSpecializationMapEntry table — maps constant_id to byte offsets in SpecConstantData.
// Must match the layout(constant_id=N) declarations in the fragment shader.
static const std::array<VkSpecializationMapEntry, 8> s_SpecMapEntries = {{
    { 0, offsetof(SpecConstantData, hasBaseColorTex), sizeof(u32) },
    { 1, offsetof(SpecConstantData, hasNormalTex),    sizeof(u32) },
    { 2, offsetof(SpecConstantData, hasMetallicTex),  sizeof(u32) },
    { 3, offsetof(SpecConstantData, hasEmissiveTex),  sizeof(u32) },
    { 4, offsetof(SpecConstantData, hasHeightTex),    sizeof(u32) },
    { 5, offsetof(SpecConstantData, doubleSided),     sizeof(u32) },
    { 6, offsetof(SpecConstantData, flatShading),     sizeof(u32) },
    { 7, offsetof(SpecConstantData, alphaMode),       sizeof(u32) },
}};

PipelineVariantCache::~PipelineVariantCache() {
    // Pipelines must be destroyed externally via Destroy(device) before destructor
}

VkPipeline PipelineVariantCache::GetOrCreate(
    VkDevice device,
    VkPipelineLayout layout,
    VkRenderPass renderPass,
    const VkGraphicsPipelineCreateInfo& templateCI,
    const MaterialSpecKey& key
) {
    auto it = m_Cache.find(key);
    if (it != m_Cache.end()) return it->second;

    // Build specialization data from key
    SpecConstantData specData = MaterialSpecKeyToData(key);

    VkSpecializationInfo specInfo{};
    specInfo.mapEntryCount = static_cast<u32>(s_SpecMapEntries.size());
    specInfo.pMapEntries = s_SpecMapEntries.data();
    specInfo.dataSize = sizeof(SpecConstantData);
    specInfo.pData = &specData;

    // Clone the template create info and inject specialization into fragment stage
    VkGraphicsPipelineCreateInfo ci = templateCI;

    // We need to clone the shader stages to inject specialization info
    std::vector<VkPipelineShaderStageCreateInfo> stages(
        ci.pStages, ci.pStages + ci.stageCount);

    for (auto& stage : stages) {
        if (stage.stage == VK_SHADER_STAGE_FRAGMENT_BIT) {
            stage.pSpecializationInfo = &specInfo;
        }
    }
    ci.pStages = stages.data();
    ci.layout = layout;
    ci.renderPass = renderPass;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "PipelineVariantCache: failed to create variant (key=0x%X, err=%d)",
                        key.bits, result);
        return VK_NULL_HANDLE;
    }

    m_Cache[key] = pipeline;
    ENJIN_LOG_INFO(Renderer, "PipelineVariantCache: created variant 0x%X (total: %zu)",
                   key.bits, m_Cache.size());
    return pipeline;
}

void PipelineVariantCache::Destroy(VkDevice device) {
    for (auto& [key, pipeline] : m_Cache) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
        }
    }
    m_Cache.clear();
}

} // namespace Renderer
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
