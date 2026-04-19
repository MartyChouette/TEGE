#include "Enjin/Renderer/RenderPipeline/RenderPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace Enjin {
namespace Renderer {

RenderPipeline::RenderPipeline(VulkanRenderer* renderer)
    : m_Renderer(renderer) {
    ENJIN_LOG_INFO(Renderer, "RenderPipeline initialized - extensible rendering system ready");
}

RenderPipeline::~RenderPipeline() {
    m_Hooks.clear();
    m_Materials.clear();
    m_ScriptCallbacks.clear();
}

void RenderPipeline::RegisterHook(RenderEventType eventType, RenderHook hook, const std::string& name) {
    std::string hookName = name.empty() ? "anonymous_" + std::to_string(m_Hooks[eventType].size()) : name;
    m_Hooks[eventType].push_back({hookName, hook});
    ENJIN_LOG_DEBUG(Renderer, "Registered hook '%s' for event type %d", hookName.c_str(), static_cast<int>(eventType));
}

void RenderPipeline::UnregisterHook(RenderEventType eventType, const std::string& name) {
    auto& hooks = m_Hooks[eventType];
    hooks.erase(
        std::remove_if(hooks.begin(), hooks.end(),
            [&name](const auto& pair) { return pair.first == name; }),
        hooks.end()
    );
}

void RenderPipeline::ClearHooks(RenderEventType eventType) {
    m_Hooks[eventType].clear();
}

void RenderPipeline::DispatchEvent(RenderEvent& event) {
    auto it = m_Hooks.find(event.type);
    if (it == m_Hooks.end()) {
        return;
    }

    for (auto& [name, hook] : it->second) {
        if (event.cancelled) {
            break;
        }
        hook(event);
    }
}

void RenderPipeline::BeginFrame() {
    RenderEvent event{RenderEventType::PreFrame};
    DispatchEvent(event);

    if (!event.cancelled && m_Renderer) {
        m_FrameActive = m_Renderer->BeginFrameVulkan();
    }

    event.type = RenderEventType::PostFrame;
    // PostFrame will be called in EndFrame
}

void RenderPipeline::EndFrame() {
    RenderEvent event{RenderEventType::PostFrame};
    DispatchEvent(event);

    if (!event.cancelled && m_Renderer && m_FrameActive) {
        m_Renderer->EndFrame();
    }

    m_FrameActive = false;
}

void RenderPipeline::BeginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer) {
    RenderEvent event{RenderEventType::PreRenderPass};
    event.data = &renderPass;
    DispatchEvent(event);
    
    if (event.cancelled) {
        return;
    }
    
    VkCommandBuffer cmd = m_Renderer->GetCurrentCommandBuffer();
    if (cmd != VK_NULL_HANDLE) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = framebuffer;
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_Renderer->GetSwapchainExtent();
        
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        
        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    
    event.type = RenderEventType::PostRenderPass;
    // PostRenderPass will be called in EndRenderPass
}

void RenderPipeline::EndRenderPass() {
    RenderEvent event{RenderEventType::PostRenderPass};
    DispatchEvent(event);
    
    VkCommandBuffer cmd = m_Renderer->GetCurrentCommandBuffer();
    if (cmd != VK_NULL_HANDLE) {
        vkCmdEndRenderPass(cmd);
    }
}

void RenderPipeline::BindPipeline(VulkanPipeline* pipeline) {
    RenderEvent event{RenderEventType::PreShaderBind};
    event.data = pipeline;
    DispatchEvent(event);
    
    if (event.cancelled) {
        return;
    }
    
    VkCommandBuffer cmd = m_Renderer->GetCurrentCommandBuffer();
    if (cmd != VK_NULL_HANDLE && pipeline) {
        pipeline->Bind(cmd);
    }
    
    event.type = RenderEventType::PostShaderBind;
    DispatchEvent(event);
}

void RenderPipeline::Draw(VkCommandBuffer cmd, u32 indexCount, u32 instanceCount) {
    RenderEvent event{RenderEventType::PreDraw};
    event.data = &indexCount;
    DispatchEvent(event);
    
    if (event.cancelled) {
        return;
    }
    
    if (cmd != VK_NULL_HANDLE) {
        vkCmdDrawIndexed(cmd, indexCount, instanceCount, 0, 0, 0);
    }
    
    event.type = RenderEventType::PostDraw;
    DispatchEvent(event);
}

u32 RenderPipeline::RegisterMaterial(const Material& material) {
    u32 id = static_cast<u32>(m_Materials.size());
    m_Materials.push_back(material);
    m_MaterialNameMap[material.name] = id;
    
    ENJIN_LOG_INFO(Renderer, "Registered material '%s' (ID: %u)", material.name.c_str(), id);
    return id;
}

RenderPipeline::Material* RenderPipeline::GetMaterial(u32 id) {
    if (id >= m_Materials.size()) {
        return nullptr;
    }
    return &m_Materials[id];
}

RenderPipeline::Material* RenderPipeline::GetMaterial(const std::string& name) {
    auto it = m_MaterialNameMap.find(name);
    if (it == m_MaterialNameMap.end()) {
        return nullptr;
    }
    return GetMaterial(it->second);
}

void RenderPipeline::ReloadMaterial(u32 id) {
    Material* material = GetMaterial(id);
    if (!material) {
        ENJIN_LOG_WARN(Renderer, "Cannot reload material %u - not found", id);
        return;
    }
    
    ENJIN_LOG_INFO(Renderer, "Reloading material '%s' (ID: %u)", material->name.c_str(), id);
    
    // Reload shader
    ReloadShader(material->shaderPath);
    
    // Trigger material reload event
    RenderEvent event{RenderEventType::MaterialOverride};
    event.data = material;
    DispatchEvent(event);
}

void RenderPipeline::ReloadShader(const std::string& shaderPath) {
    ENJIN_LOG_INFO(Renderer, "Reloading shader: %s", shaderPath.c_str());

    // Read shader source from file
    std::ifstream file(shaderPath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to open shader file for reload: %s", shaderPath.c_str());
        return;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    if (source.empty()) {
        ENJIN_LOG_ERROR(Renderer, "Shader file is empty: %s", shaderPath.c_str());
        return;
    }

    // Determine shader stage from file extension
    VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
    if (shaderPath.find(".frag") != std::string::npos) {
        stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    } else if (shaderPath.find(".comp") != std::string::npos) {
        stage = VK_SHADER_STAGE_COMPUTE_BIT;
    } else if (shaderPath.find(".geom") != std::string::npos) {
        stage = VK_SHADER_STAGE_GEOMETRY_BIT;
    }

    // Compile GLSL to SPIR-V using the shader compiler
    std::vector<u32> spirv;
    if (!ShaderCompiler::CompileGLSL(source, stage, spirv)) {
        ENJIN_LOG_ERROR(Renderer, "Shader compilation failed during hot-reload: %s", shaderPath.c_str());
        return;
    }

    ENJIN_LOG_INFO(Renderer, "Shader recompiled successfully: %s (%zu SPIR-V words)", shaderPath.c_str(), spirv.size());

    // Trigger shader reload event for systems to pick up the new SPIR-V
    RenderEvent event{RenderEventType::Custom};
    event.data = const_cast<char*>(shaderPath.c_str());
    DispatchEvent(event);
}

void RenderPipeline::ReloadAllShaders() {
    ENJIN_LOG_INFO(Renderer, "Reloading all shaders...");
    
    for (auto& material : m_Materials) {
        ReloadShader(material.shaderPath);
    }
}

void RenderPipeline::SetPipelineState(const PipelineState& state) {
    m_PipelineState = state;
    
    // Trigger pipeline state change event
    RenderEvent event{RenderEventType::Custom};
    event.data = const_cast<PipelineState*>(&state);
    DispatchEvent(event);
}

void RenderPipeline::RegisterScriptCallback(const std::string& name, std::function<void()> callback) {
    m_ScriptCallbacks[name] = callback;
    ENJIN_LOG_DEBUG(Renderer, "Registered script callback: %s", name.c_str());
}

void RenderPipeline::CallScriptCallback(const std::string& name) {
    auto it = m_ScriptCallbacks.find(name);
    if (it != m_ScriptCallbacks.end()) {
        it->second();
    } else {
        ENJIN_LOG_WARN(Renderer, "Script callback '%s' not found", name.c_str());
    }
}

} // namespace Renderer
} // namespace Enjin
