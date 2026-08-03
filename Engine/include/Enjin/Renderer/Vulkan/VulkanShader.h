#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

// Shader module wrapper
class ENJIN_API VulkanShader {
public:
    VulkanShader(VulkanContext* context);
    ~VulkanShader();

    // Load shader from SPIR-V binary
    bool LoadFromSPIRV(const u8* data, usize size);
    
    // Compile GLSL to SPIR-V (requires shaderc)
    bool CompileFromGLSL(const std::string& source, VkShaderStageFlagBits stage);
    
    // Load from file. Pass logMissing=false when probing a list of candidate
    // paths (the caller logs the final outcome) so a not-found file doesn't spam
    // a red ERROR per failed candidate.
    bool LoadFromFile(const std::string& filepath, bool logMissing = true);

    VkShaderModule GetModule() const { return m_Module; }
    VkShaderStageFlagBits GetStage() const { return m_Stage; }

    void Destroy();

private:
    VulkanContext* m_Context = nullptr;
    VkShaderModule m_Module = VK_NULL_HANDLE;
    VkShaderStageFlagBits m_Stage = VK_SHADER_STAGE_VERTEX_BIT;
};

// Shader compilation utilities
namespace ShaderCompiler {
    // Compile GLSL source to SPIR-V
    bool CompileGLSL(const std::string& source, VkShaderStageFlagBits stage, std::vector<u32>& spirv);
    
    // Load SPIR-V from file. logMissing=false suppresses the "failed to open"
    // ERROR for a not-found file (used by multi-path fallback searches).
    bool LoadSPIRV(const std::string& filepath, std::vector<u32>& spirv, bool logMissing = true);
    
    // Save SPIR-V to file
    bool SaveSPIRV(const std::string& filepath, const std::vector<u32>& spirv);
}

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
