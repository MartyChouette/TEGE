#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <functional>

namespace Enjin {
namespace Renderer {

// Forward declarations
class VulkanPipeline;
class VulkanShader;
class VulkanContext;

// Material parameter types
enum class MaterialParamType {
    Float,
    Vector2,
    Vector3,
    Vector4,
    Texture,
    Sampler
};

// Material parameter value
struct MaterialParam {
    MaterialParamType type;
    union {
        f32 floatValue;
        Math::Vector2 vec2Value;
        Math::Vector3 vec3Value;
        Math::Vector4 vec4Value;
    };
    std::string texturePath;
    u32 textureHandle = 0;
    
    MaterialParam() : type(MaterialParamType::Float), floatValue(0.0f) {}
};

// Material definition - data-driven, JSON-loadable
struct MaterialDefinition {
    std::string name;
    std::string vertexShader;
    std::string fragmentShader;
    std::unordered_map<std::string, MaterialParam> parameters;
    std::unordered_map<std::string, std::string> textures;
    
    // Pipeline state
    bool depthTest = true;
    bool depthWrite = true;
    bool blendEnable = false;
    VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    
    // Hot-reload
    bool hotReloadEnabled = true;
    std::string filePath; // Path to material file
};

// Material instance - runtime material with compiled shaders
class ENJIN_API MaterialInstance {
public:
    MaterialInstance();
    ~MaterialInstance();

    bool Load(VulkanContext* context, VkRenderPass renderPass, const MaterialDefinition& definition);
    void Reload(); // Hot-reload material
    void Unload();

    // Parameter access
    void SetParameter(const std::string& name, f32 value);
    void SetParameter(const std::string& name, const Math::Vector4& value);
    void SetTexture(const std::string& name, const std::string& path);
    
    MaterialParam* GetParameter(const std::string& name);
    const MaterialParam* GetParameter(const std::string& name) const;

    // Pipeline access
    VulkanPipeline* GetPipeline() const { return m_Pipeline.get(); }
    const MaterialDefinition& GetDefinition() const { return m_Definition; }

    // Update uniform buffers
    void UpdateUniforms(VkCommandBuffer cmd, u32 frameIndex);

private:
    VulkanContext* m_Context = nullptr;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;
    MaterialDefinition m_Definition;
    std::unique_ptr<VulkanPipeline> m_Pipeline;
    std::unique_ptr<VulkanShader> m_VertexShader;
    std::unique_ptr<VulkanShader> m_FragmentShader;
    std::unordered_map<std::string, MaterialParam> m_Parameters;
    bool m_Dirty = true; // Needs uniform update
};

// Material system - manages all materials
// DESIGN: Data-driven, hot-reloadable, scriptable
class ENJIN_API MaterialSystem {
public:
    MaterialSystem();
    ~MaterialSystem();

    void Shutdown();

    // Load material from file (JSON)
    u32 LoadMaterial(VulkanContext* context, VkRenderPass renderPass, const std::string& filepath);
    u32 LoadMaterial(VulkanContext* context, VkRenderPass renderPass, const MaterialDefinition& definition);
    
    // Get material
    MaterialInstance* GetMaterial(u32 id);
    MaterialInstance* GetMaterial(const std::string& name);
    
    // Hot-reload
    void ReloadMaterial(u32 id);
    void ReloadMaterial(const std::string& name);
    void ReloadAllMaterials();
    
    // Watch for file changes (for hot-reload)
    void WatchMaterialFiles(bool enable);
    
    // Material iteration
    void ForEachMaterial(std::function<void(MaterialInstance*)> callback);

private:
    std::vector<std::unique_ptr<MaterialInstance>> m_Materials;
    std::unordered_map<std::string, u32> m_MaterialNameMap;
    bool m_FileWatching = false;

    // File modification time tracking for hot-reload
    struct WatchedFile {
        std::string path;
        u32 materialId = 0;
        i64 lastModTime = 0; // Filesystem modification time
    };
    std::vector<WatchedFile> m_WatchedFiles;

    void PollFileChanges();
};

} // namespace Renderer
} // namespace Enjin
