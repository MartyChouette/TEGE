#include "Enjin/Renderer/ShaderManager.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>

namespace Enjin {
namespace Renderer {

ShaderManager::ShaderManager(VulkanContext* context)
    : m_Context(context) {
}

ShaderManager::~ShaderManager() {
    UnloadAll();
}

std::string ShaderManager::LoadShader(const std::string& filepath, VkShaderStageFlagBits stage) {
    // Use filepath as the shader ID
    std::string shaderId = filepath;

    // Check if already loaded
    auto it = m_Shaders.find(shaderId);
    if (it != m_Shaders.end()) {
        ENJIN_LOG_WARN(Renderer, "Shader already loaded: %s", filepath.c_str());
        return shaderId;
    }

    // Check if file exists
    if (!std::filesystem::exists(filepath)) {
        ENJIN_LOG_ERROR(Renderer, "Shader file not found: %s", filepath.c_str());
        return "";
    }

    // Create and load shader
    auto shader = std::make_unique<VulkanShader>(m_Context);
    if (!shader->LoadFromFile(filepath)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load shader: %s", filepath.c_str());
        return "";
    }

    // Store shader entry
    ShaderEntry entry;
    entry.shader = std::move(shader);
    entry.filepath = filepath;
    entry.stage = stage;
    entry.lastModified = std::filesystem::last_write_time(filepath);

    m_Shaders[shaderId] = std::move(entry);

    ENJIN_LOG_INFO(Renderer, "Loaded shader: %s", filepath.c_str());
    return shaderId;
}

VulkanShader* ShaderManager::GetShader(const std::string& shaderId) {
    auto it = m_Shaders.find(shaderId);
    if (it != m_Shaders.end()) {
        return it->second.shader.get();
    }
    return nullptr;
}

bool ShaderManager::ReloadShader(const std::string& shaderId) {
    auto it = m_Shaders.find(shaderId);
    if (it == m_Shaders.end()) {
        ENJIN_LOG_ERROR(Renderer, "Cannot reload unknown shader: %s", shaderId.c_str());
        return false;
    }

    ShaderEntry& entry = it->second;

    // Check if file still exists
    if (!std::filesystem::exists(entry.filepath)) {
        ENJIN_LOG_ERROR(Renderer, "Shader file no longer exists: %s", entry.filepath.c_str());
        return false;
    }

    // Wait for GPU to finish using the old shader (fence-based when renderer is active)
    m_Context->WaitForGPU();

    // Destroy old shader
    entry.shader->Destroy();

    // Reload shader
    if (!entry.shader->LoadFromFile(entry.filepath)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to reload shader: %s", entry.filepath.c_str());
        return false;
    }

    // Update last modified time
    entry.lastModified = std::filesystem::last_write_time(entry.filepath);

    ENJIN_LOG_INFO(Renderer, "Reloaded shader: %s", entry.filepath.c_str());

    // Call reload callback
    if (m_ReloadCallback) {
        m_ReloadCallback(shaderId);
    }

    return true;
}

void ShaderManager::Update() {
    if (!m_HotReloadEnabled) {
        return;
    }

    // Check for file changes
    for (auto& [shaderId, entry] : m_Shaders) {
        if (!std::filesystem::exists(entry.filepath)) {
            continue;
        }

        auto currentModTime = std::filesystem::last_write_time(entry.filepath);
        if (currentModTime > entry.lastModified) {
            ENJIN_LOG_INFO(Renderer, "Detected shader change: %s", entry.filepath.c_str());
            ReloadShader(shaderId);
        }
    }
}

void ShaderManager::UnloadShader(const std::string& shaderId) {
    auto it = m_Shaders.find(shaderId);
    if (it != m_Shaders.end()) {
        m_Context->WaitForGPU();
        m_Shaders.erase(it);
        ENJIN_LOG_INFO(Renderer, "Unloaded shader: %s", shaderId.c_str());
    }
}

void ShaderManager::UnloadAll() {
    if (m_Context && m_Context->GetDevice()) {
        m_Context->WaitForGPU();
    }
    m_Shaders.clear();
    ENJIN_LOG_INFO(Renderer, "Unloaded all shaders");
}

} // namespace Renderer
} // namespace Enjin
