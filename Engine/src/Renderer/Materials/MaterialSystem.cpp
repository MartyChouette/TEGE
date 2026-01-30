#include "Enjin/Renderer/Materials/MaterialSystem.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/Vulkan/VulkanShader.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace Enjin {
namespace Renderer {

MaterialInstance::MaterialInstance() {
}

MaterialInstance::~MaterialInstance() {
    Unload();
}

bool MaterialInstance::Load(VulkanContext* context, VkRenderPass renderPass, const MaterialDefinition& definition) {
    m_Context = context;
    m_RenderPass = renderPass;
    m_Definition = definition;
    
    ENJIN_LOG_INFO(Renderer, "Loading material: %s", definition.name.c_str());
    
    // Copy parameters
    m_Parameters = definition.parameters;
    
    // Load vertex shader
    if (!definition.vertexShader.empty()) {
        m_VertexShader = std::make_unique<VulkanShader>(context);
        if (!m_VertexShader->LoadFromFile(definition.vertexShader)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to load vertex shader: %s", definition.vertexShader.c_str());
            return false;
        }
    } else {
        ENJIN_LOG_ERROR(Renderer, "Material '%s' has no vertex shader specified", definition.name.c_str());
        return false;
    }
    
    // Load fragment shader
    if (!definition.fragmentShader.empty()) {
        m_FragmentShader = std::make_unique<VulkanShader>(context);
        if (!m_FragmentShader->LoadFromFile(definition.fragmentShader)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to load fragment shader: %s", definition.fragmentShader.c_str());
            return false;
        }
    } else {
        ENJIN_LOG_ERROR(Renderer, "Material '%s' has no fragment shader specified", definition.name.c_str());
        return false;
    }
    
    // Create pipeline
    PipelineConfig config;
    config.renderPass = renderPass;
    config.depthTest = definition.depthTest;
    config.depthWrite = definition.depthWrite;
    config.cullMode = definition.cullMode;
    config.polygonMode = definition.polygonMode;
    
    m_Pipeline = std::make_unique<VulkanPipeline>(context);
    if (!m_Pipeline->Create(config, m_VertexShader.get(), m_FragmentShader.get())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create pipeline for material: %s", definition.name.c_str());
        return false;
    }
    
    m_Dirty = true;
    ENJIN_LOG_INFO(Renderer, "Material '%s' loaded successfully", definition.name.c_str());
    return true;
}

void MaterialInstance::Reload() {
    ENJIN_LOG_INFO(Renderer, "Reloading material: %s", m_Definition.name.c_str());
    // Reload shaders and recreate pipeline
    if (m_Context && m_RenderPass != VK_NULL_HANDLE) {
        Load(m_Context, m_RenderPass, m_Definition);
    }
}

void MaterialInstance::Unload() {
    m_Pipeline.reset();
    m_FragmentShader.reset();
    m_VertexShader.reset();
    m_Parameters.clear();
}

void MaterialInstance::SetParameter(const std::string& name, f32 value) {
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end()) {
        it->second.floatValue = value;
        m_Dirty = true;
    } else {
        MaterialParam param;
        param.type = MaterialParamType::Float;
        param.floatValue = value;
        m_Parameters[name] = param;
        m_Dirty = true;
    }
}

void MaterialInstance::SetParameter(const std::string& name, const Math::Vector4& value) {
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end()) {
        it->second.vec4Value = value;
        m_Dirty = true;
    } else {
        MaterialParam param;
        param.type = MaterialParamType::Vector4;
        param.vec4Value = value;
        m_Parameters[name] = param;
        m_Dirty = true;
    }
}

void MaterialInstance::SetTexture(const std::string& name, const std::string& path) {
    m_Definition.textures[name] = path;
    m_Dirty = true;
}

MaterialParam* MaterialInstance::GetParameter(const std::string& name) {
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end()) {
        return &it->second;
    }
    return nullptr;
}

const MaterialParam* MaterialInstance::GetParameter(const std::string& name) const {
    auto it = m_Parameters.find(name);
    if (it != m_Parameters.end()) {
        return &it->second;
    }
    return nullptr;
}

void MaterialInstance::UpdateUniforms(VkCommandBuffer cmd, u32 frameIndex) {
    (void)cmd;
    (void)frameIndex;
    // Update uniform buffers with material parameters
    // This would be implemented when integrated with renderer
    if (m_Dirty) {
        m_Dirty = false;
    }
}

MaterialSystem::MaterialSystem() {
}

MaterialSystem::~MaterialSystem() {
    Shutdown();
}

// Simple JSON parsing helpers
namespace {
    std::string Trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }
    
    std::string ExtractString(const std::string& json, const std::string& key) {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return "";
        
        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";
        pos++;
        
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        
        if (pos >= json.length() || json[pos] != '"') return "";
        pos++;
        
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        
        return json.substr(pos, end - pos);
    }
    
    f32 ExtractFloat(const std::string& json, const std::string& key, f32 defaultValue = 0.0f) {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultValue;
        
        pos = json.find(':', pos);
        if (pos == std::string::npos) return defaultValue;
        pos++;
        
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        
        size_t end = pos;
        while (end < json.length() && (std::isdigit(json[end]) || json[end] == '.' || json[end] == '-' || json[end] == '+')) end++;
        
        if (end == pos) return defaultValue;
        
        try {
            return std::stof(json.substr(pos, end - pos));
        } catch (...) {
            return defaultValue;
        }
    }
    
    bool ExtractBool(const std::string& json, const std::string& key, bool defaultValue = false) {
        std::string searchKey = "\"" + key + "\"";
        size_t pos = json.find(searchKey);
        if (pos == std::string::npos) return defaultValue;
        
        pos = json.find(':', pos);
        if (pos == std::string::npos) return defaultValue;
        pos++;
        
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        
        if (json.substr(pos, 4) == "true") return true;
        if (json.substr(pos, 5) == "false") return false;
        return defaultValue;
    }
}

u32 MaterialSystem::LoadMaterial(VulkanContext* context, VkRenderPass renderPass, const std::string& filepath) {
    ENJIN_LOG_INFO(Renderer, "Loading material from file: %s", filepath.c_str());
    
    std::ifstream file(filepath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to open material file: %s", filepath.c_str());
        return UINT32_MAX;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string json = buffer.str();
    file.close();
    
    MaterialDefinition definition;
    definition.filePath = filepath;
    
    // Extract basic fields
    definition.name = ExtractString(json, "name");
    if (definition.name.empty()) {
        // Use filename as name
        size_t lastSlash = filepath.find_last_of("/\\");
        size_t lastDot = filepath.find_last_of('.');
        if (lastDot != std::string::npos && (lastSlash == std::string::npos || lastDot > lastSlash)) {
            definition.name = filepath.substr(lastSlash == std::string::npos ? 0 : lastSlash + 1, lastDot - (lastSlash == std::string::npos ? 0 : lastSlash + 1));
        } else {
            definition.name = "UnnamedMaterial";
        }
    }
    
    definition.vertexShader = ExtractString(json, "vertexShader");
    definition.fragmentShader = ExtractString(json, "fragmentShader");
    
    // Extract pipeline state
    definition.depthTest = ExtractBool(json, "depthTest", true);
    definition.depthWrite = ExtractBool(json, "depthWrite", true);
    definition.blendEnable = ExtractBool(json, "blendEnable", false);
    
    std::string cullModeStr = ExtractString(json, "cullMode");
    if (cullModeStr == "none" || cullModeStr == "NONE") {
        definition.cullMode = VK_CULL_MODE_NONE;
    } else if (cullModeStr == "front" || cullModeStr == "FRONT") {
        definition.cullMode = VK_CULL_MODE_FRONT_BIT;
    } else {
        definition.cullMode = VK_CULL_MODE_BACK_BIT; // default
    }
    
    // Extract parameters (simplified - only float for now)
    // Look for "parameters" object
    size_t paramsStart = json.find("\"parameters\"");
    if (paramsStart != std::string::npos) {
        paramsStart = json.find('{', paramsStart);
        if (paramsStart != std::string::npos) {
            size_t paramsEnd = json.find('}', paramsStart);
            if (paramsEnd != std::string::npos) {
                std::string paramsJson = json.substr(paramsStart, paramsEnd - paramsStart);
                // Simple extraction - look for key-value pairs
                size_t pos = 0;
                while ((pos = paramsJson.find('"', pos)) != std::string::npos) {
                    size_t keyStart = pos + 1;
                    size_t keyEnd = paramsJson.find('"', keyStart);
                    if (keyEnd == std::string::npos) break;
                    
                    std::string key = paramsJson.substr(keyStart, keyEnd - keyStart);
                    pos = paramsJson.find(':', keyEnd);
                    if (pos == std::string::npos) break;
                    pos++;
                    
                    while (pos < paramsJson.length() && (paramsJson[pos] == ' ' || paramsJson[pos] == '\t')) pos++;
                    
                    MaterialParam param;
                    param.type = MaterialParamType::Float;
                    param.floatValue = ExtractFloat(paramsJson, key, 0.0f);
                    definition.parameters[key] = param;
                    
                    pos = paramsJson.find(',', pos);
                    if (pos == std::string::npos) break;
                }
            }
        }
    }
    
    // Extract textures
    size_t texturesStart = json.find("\"textures\"");
    if (texturesStart != std::string::npos) {
        texturesStart = json.find('{', texturesStart);
        if (texturesStart != std::string::npos) {
            size_t texturesEnd = json.find('}', texturesStart);
            if (texturesEnd != std::string::npos) {
                std::string texturesJson = json.substr(texturesStart, texturesEnd - texturesStart);
                size_t pos = 0;
                while ((pos = texturesJson.find('"', pos)) != std::string::npos) {
                    size_t keyStart = pos + 1;
                    size_t keyEnd = texturesJson.find('"', keyStart);
                    if (keyEnd == std::string::npos) break;
                    
                    std::string key = texturesJson.substr(keyStart, keyEnd - keyStart);
                    pos = texturesJson.find(':', keyEnd);
                    if (pos == std::string::npos) break;
                    pos++;
                    
                    while (pos < texturesJson.length() && (texturesJson[pos] == ' ' || texturesJson[pos] == '\t')) pos++;
                    
                    if (pos < texturesJson.length() && texturesJson[pos] == '"') {
                        pos++;
                        size_t valueStart = pos;
                        size_t valueEnd = texturesJson.find('"', valueStart);
                        if (valueEnd != std::string::npos) {
                            std::string value = texturesJson.substr(valueStart, valueEnd - valueStart);
                            definition.textures[key] = value;
                        }
                    }
                    
                    pos = texturesJson.find(',', pos);
                    if (pos == std::string::npos) break;
                }
            }
        }
    }
    
    return LoadMaterial(context, renderPass, definition);
}

u32 MaterialSystem::LoadMaterial(VulkanContext* context, VkRenderPass renderPass, const MaterialDefinition& definition) {
    u32 id = static_cast<u32>(m_Materials.size());
    
    auto material = std::make_unique<MaterialInstance>();
    if (!material->Load(context, renderPass, definition)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load material: %s", definition.name.c_str());
        return UINT32_MAX;
    }
    
    m_Materials.push_back(std::move(material));
    m_MaterialNameMap[definition.name] = id;
    
    ENJIN_LOG_INFO(Renderer, "Loaded material '%s' with ID %u", definition.name.c_str(), id);
    return id;
}

MaterialInstance* MaterialSystem::GetMaterial(u32 id) {
    if (id >= m_Materials.size()) {
        return nullptr;
    }
    return m_Materials[id].get();
}

MaterialInstance* MaterialSystem::GetMaterial(const std::string& name) {
    auto it = m_MaterialNameMap.find(name);
    if (it != m_MaterialNameMap.end()) {
        return GetMaterial(it->second);
    }
    return nullptr;
}

void MaterialSystem::ReloadMaterial(u32 id) {
    MaterialInstance* material = GetMaterial(id);
    if (material) {
        material->Reload();
    }
}

void MaterialSystem::ReloadMaterial(const std::string& name) {
    MaterialInstance* material = GetMaterial(name);
    if (material) {
        material->Reload();
    }
}

void MaterialSystem::ReloadAllMaterials() {
    ENJIN_LOG_INFO(Renderer, "Reloading all materials...");
    for (auto& material : m_Materials) {
        if (material) {
            material->Reload();
        }
    }
}

void MaterialSystem::WatchMaterialFiles(bool enable) {
    m_FileWatching = enable;
    ENJIN_LOG_INFO(Renderer, "Material file watching: %s", enable ? "enabled" : "disabled");

    if (enable) {
        // Build list of files to watch from all materials
        m_WatchedFiles.clear();
        for (u32 i = 0; i < static_cast<u32>(m_Materials.size()); ++i) {
            if (!m_Materials[i]) continue;
            const auto& def = m_Materials[i]->GetDefinition();
            if (def.filePath.empty()) continue;

            WatchedFile wf;
            wf.path = def.filePath;
            wf.materialId = i;
            wf.lastModTime = 0;

            // Get initial modification time
            std::error_code ec;
            auto modTime = std::filesystem::last_write_time(def.filePath, ec);
            if (!ec) {
                wf.lastModTime = modTime.time_since_epoch().count();
            }

            m_WatchedFiles.push_back(wf);
        }
        ENJIN_LOG_INFO(Renderer, "Watching %zu material files", m_WatchedFiles.size());
    } else {
        m_WatchedFiles.clear();
    }
}

void MaterialSystem::PollFileChanges() {
    if (!m_FileWatching) return;

    for (auto& wf : m_WatchedFiles) {
        std::error_code ec;
        auto modTime = std::filesystem::last_write_time(wf.path, ec);
        if (ec) continue;

        i64 currentModTime = modTime.time_since_epoch().count();
        if (currentModTime != wf.lastModTime && wf.lastModTime != 0) {
            wf.lastModTime = currentModTime;
            ENJIN_LOG_INFO(Renderer, "Material file changed: %s, reloading...", wf.path.c_str());
            ReloadMaterial(wf.materialId);
        } else {
            wf.lastModTime = currentModTime;
        }
    }
}

void MaterialSystem::ForEachMaterial(std::function<void(MaterialInstance*)> callback) {
    for (auto& material : m_Materials) {
        if (material) {
            callback(material.get());
        }
    }
}

void MaterialSystem::Shutdown() {
    m_Materials.clear();
    m_MaterialNameMap.clear();
}

} // namespace Renderer
} // namespace Enjin
