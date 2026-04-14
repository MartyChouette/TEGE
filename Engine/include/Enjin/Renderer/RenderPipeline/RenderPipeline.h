#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <functional>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace Enjin {
namespace Renderer {

// Forward declarations
class VulkanRenderer;
class VulkanPipeline;
class VulkanBuffer;

// Render pipeline event types - hooks for customization
enum class RenderEventType {
    PreFrame,           // Before frame begins
    PostFrame,          // After frame ends
    PreRenderPass,      // Before render pass
    PostRenderPass,     // After render pass
    PreDraw,            // Before draw call
    PostDraw,           // After draw call
    PreShaderBind,      // Before shader bind
    PostShaderBind,     // After shader bind
    MaterialOverride,   // Material selection override
    CullingOverride,    // Culling decision override
    Custom              // Custom user events
};

// Render event data
struct RenderEvent {
    RenderEventType type;
    void* data = nullptr;           // Event-specific data
    bool handled = false;           // Set to true to prevent default behavior
    bool cancelled = false;          // Set to true to skip this step
};

// Render hook callback
using RenderHook = std::function<void(RenderEvent&)>;

// Render pipeline - extensible, scriptable rendering system
// DESIGN: Easy to extend, hook, and script for future graphics tweaking
class ENJIN_API RenderPipeline {
public:
    RenderPipeline(VulkanRenderer* renderer);
    ~RenderPipeline();

    // Hook registration - add custom behavior at any point
    void RegisterHook(RenderEventType eventType, RenderHook hook, const std::string& name = "");
    void UnregisterHook(RenderEventType eventType, const std::string& name);
    void ClearHooks(RenderEventType eventType);

    // Event dispatching - trigger hooks
    void DispatchEvent(RenderEvent& event);

    // Pipeline stages - override these for custom behavior
    virtual void BeginFrame();
    virtual void EndFrame();
    virtual void BeginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer);
    virtual void EndRenderPass();
    virtual void BindPipeline(VulkanPipeline* pipeline);
    virtual void Draw(VkCommandBuffer cmd, u32 indexCount, u32 instanceCount = 1);

    // Material system - data-driven materials
    struct Material {
        std::string name;
        std::string shaderPath;
        std::unordered_map<std::string, f32> floatParams;
        std::unordered_map<std::string, Math::Vector4> vectorParams;
        std::unordered_map<std::string, std::string> textureParams;
        bool enabled = true;
    };

    // Material management
    u32 RegisterMaterial(const Material& material);
    Material* GetMaterial(u32 id);
    Material* GetMaterial(const std::string& name);
    void ReloadMaterial(u32 id); // Hot-reload material

    // Shader hot-reload
    void ReloadShader(const std::string& shaderPath);
    void ReloadAllShaders();

    // Pipeline state override - runtime tweaking
    struct PipelineState {
        f32 lineWidth = 1.0f;
        bool depthTest = true;
        bool depthWrite = true;
        VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
        VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
        // Add more as needed
    };
    
    void SetPipelineState(const PipelineState& state);
    PipelineState GetPipelineState() const { return m_PipelineState; }

    // Script injection points
    void RegisterScriptCallback(const std::string& name, std::function<void()> callback);
    void CallScriptCallback(const std::string& name);

    // Debug/Visualization
    void EnableDebugVisualization(bool enable) { m_DebugVisualization = enable; }
    bool IsDebugVisualizationEnabled() const { return m_DebugVisualization; }

    // Get renderer
    VulkanRenderer* GetRenderer() const { return m_Renderer; }

private:
    VulkanRenderer* m_Renderer = nullptr;
    bool m_FrameActive = false;
    
    // Hooks - organized by event type
    std::unordered_map<RenderEventType, std::vector<std::pair<std::string, RenderHook>>> m_Hooks;
    
    // Materials
    std::vector<Material> m_Materials;
    std::unordered_map<std::string, u32> m_MaterialNameMap;
    
    // Pipeline state
    PipelineState m_PipelineState;
    
    // Script callbacks
    std::unordered_map<std::string, std::function<void()>> m_ScriptCallbacks;
    
    // Debug
    bool m_DebugVisualization = false;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
