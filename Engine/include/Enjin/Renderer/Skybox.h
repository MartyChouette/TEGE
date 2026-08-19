#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include <string>
#include <array>

namespace Enjin {
namespace Renderer {

// SkyboxConfig/SkyboxType are platform-agnostic (used by SceneSerializer on all platforms)
enum class SkyboxType : u32 {
    None = 0,
    Cubemap,
    Procedural,
    SolidColor
};

struct SkyboxConfig {
    SkyboxType type = SkyboxType::None;
    std::array<std::string, 6> cubemapPaths;
    // Shared palette: deep blue zenith, warm light horizon, dark blue-grey ground
    // arc (keeps the sky in the same blue-teal family as water).
    // See docs/art/PROCEDURAL_EFFECTS_DIRECTION.md.
    Math::Vector3 topColor = Math::Vector3(0.05f, 0.12f, 0.52f);
    Math::Vector3 bottomColor = Math::Vector3(0.18f, 0.22f, 0.28f);
    Math::Vector3 horizonColor = Math::Vector3(0.55f, 0.72f, 0.88f);
    Math::Vector3 sunDirection = Math::Vector3(0.5f, 0.8f, 0.3f);
    Math::Vector3 solidColor = Math::Vector3(0.2f, 0.3f, 0.4f);
    f32 rotation = 0.0f;

    // ── Atmosphere: sun disc + cloud layers + haze, composited LIVE by
    //    skybox.frag over any sky type. Defaults = off (classic look). ──
    f32 sunIntensity = 0.0f;      // 0 = no drawn sun disc
    f32 sunSize = 0.045f;         // angular size of the disc
    Math::Vector3 sunColor = Math::Vector3(1.0f, 0.95f, 0.85f);
    f32 cloudCoverage = 0.0f;     // layer 1 (big, low): 0 = clear sky
    f32 cloudScale = 1.0f;
    f32 cloudSpeed = 1.0f;        // drift rate (direction follows the wind)
    Math::Vector3 cloudColor = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 cloud2Coverage = 0.0f;    // layer 2 (fine, high, faster)
    f32 cloud2Scale = 2.5f;
    f32 horizonHaze = 0.0f;       // bright band hugging the horizon
    f32 cloudShadowStrength = 0.6f;  // how much passing clouds darken the ground (0 = off)
};

} // namespace Renderer
} // namespace Enjin

// Vulkan Skybox class — not available on web
#if !ENJIN_RENDERER_WEBGPU

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace Enjin {
namespace Renderer {

class VulkanContext;

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
    void UploadFaces(const std::vector<std::unique_ptr<u8[]>>& faceData, u32 faceSize);
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
#endif // !ENJIN_RENDERER_WEBGPU
