#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <functional>

namespace Enjin {
namespace Renderer {

// Callback type for shader reload notifications
using ShaderReloadCallback = std::function<void(const std::string& shaderPath)>;

// Shader manager with hot-reload support
class ENJIN_API ShaderManager {
public:
    ShaderManager(VulkanContext* context);
    ~ShaderManager();

    // Load a shader from file (returns shader ID)
    // The shader will be watched for changes if hot-reload is enabled
    std::string LoadShader(const std::string& filepath, VkShaderStageFlagBits stage);

    // Get a loaded shader by ID
    VulkanShader* GetShader(const std::string& shaderId);

    // Reload a specific shader from disk
    bool ReloadShader(const std::string& shaderId);

    // Check for file changes and reload if necessary
    // Call this periodically (e.g., once per frame) to detect changes
    void Update();

    // Enable/disable hot-reload
    void SetHotReloadEnabled(bool enabled) { m_HotReloadEnabled = enabled; }
    bool IsHotReloadEnabled() const { return m_HotReloadEnabled; }

    // Set reload callback (called when a shader is reloaded)
    void SetReloadCallback(ShaderReloadCallback callback) { m_ReloadCallback = callback; }

    // Unload a shader
    void UnloadShader(const std::string& shaderId);

    // Unload all shaders
    void UnloadAll();

private:
    struct ShaderEntry {
        std::unique_ptr<VulkanShader> shader;
        std::string filepath;
        VkShaderStageFlagBits stage;
        std::filesystem::file_time_type lastModified;
    };

    VulkanContext* m_Context = nullptr;
    std::unordered_map<std::string, ShaderEntry> m_Shaders;
    ShaderReloadCallback m_ReloadCallback;
    bool m_HotReloadEnabled = true;
    f32 m_CheckInterval = 1.0f; // Check every second
    f32 m_TimeSinceLastCheck = 0.0f;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
