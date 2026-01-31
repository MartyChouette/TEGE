#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include <vulkan/vulkan.h>
#include <string>
#include <array>
#include <vector>

namespace Enjin {
namespace Renderer {

class VulkanContext;

enum class SkyboxType : u32 {
    None = 0,
    Cubemap,
    Procedural,
    SolidColor
};

struct SkyboxConfig {
    SkyboxType type = SkyboxType::None;

    // Cubemap face paths (Right, Left, Top, Bottom, Front, Back)
    std::array<std::string, 6> cubemapPaths;

    // Procedural sky parameters
    Math::Vector3 topColor = Math::Vector3(0.1f, 0.2f, 0.6f);
    Math::Vector3 bottomColor = Math::Vector3(0.8f, 0.6f, 0.3f);
    Math::Vector3 horizonColor = Math::Vector3(0.5f, 0.7f, 0.9f);
    Math::Vector3 sunDirection = Math::Vector3(0.5f, 0.8f, 0.3f);

    // Solid color
    Math::Vector3 solidColor = Math::Vector3(0.2f, 0.3f, 0.4f);

    // Rotation angle (degrees around Y axis)
    f32 rotation = 0.0f;
};

class ENJIN_API Skybox {
public:
    Skybox();
    ~Skybox();

    bool Initialize(VulkanContext* context);
    void Shutdown();

    bool LoadCubemap(const std::array<std::string, 6>& facePaths);
    bool CreateProcedural(const Math::Vector3& topColor, const Math::Vector3& bottomColor,
                          const Math::Vector3& horizonColor);
    bool CreateSolidColor(const Math::Vector3& color);

    void SetConfig(const SkyboxConfig& config);
    const SkyboxConfig& GetConfig() const { return m_Config; }

    VkDescriptorImageInfo GetDescriptorInfo() const;
    bool IsValid() const { return m_CubemapView != VK_NULL_HANDLE; }

private:
    bool CreateCubemapImage(u32 faceSize);
    void UploadFaces(const std::vector<u8*>& faceData, u32 faceSize);
    bool CreateSampler();
    void DestroyImage();

    VulkanContext* m_Context = nullptr;
    SkyboxConfig m_Config;

    VkImage m_CubemapImage = VK_NULL_HANDLE;
    VkDeviceMemory m_CubemapMemory = VK_NULL_HANDLE;
    VkImageView m_CubemapView = VK_NULL_HANDLE;
    VkSampler m_Sampler = VK_NULL_HANDLE;
    u32 m_FaceSize = 0;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
