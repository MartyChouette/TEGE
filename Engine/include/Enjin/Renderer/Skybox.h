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

    // Cloud shape controls (2D sky; 3D compositor follow-up)
    f32 cloudSoftness = 0.5f;     // 0 = crisp puffy edges, 1 = soft wispy haze
    // Optional custom cloud texture: when set, its luminance drives the cloud
    // mask (tiled + wind-drifted) instead of the procedural FBM, so authors can
    // supply their own cloud shapes. Empty = procedural.
    std::string cloudTexturePath;
    i32 cachedCloudTexIndex = -2;  // runtime bindless index (-2 unresolved), not serialized
};

// Water2DConfig — a scene-level water surface for 2D scenes, the water cousin of
// the 2D sky. It is drawn as a full-screen overlay AFTER the 2D sprites, so
// everything below the world-space waterline reads as submerged: a wavy surface
// line with foam, a translucent tint that deepens with distance below the line,
// and a gentle caustic shimmer. Off by default (enabled = false), so existing 2D
// scenes are pixel-identical. Lives here beside SkyboxConfig because both are
// scene-environment config the SceneSerializer round-trips on every platform.
struct Water2DConfig {
    bool enabled = false;
    f32 waterLineY = 0.0f;                                  // world Y of the surface
    Math::Vector3 surfaceColor = Math::Vector3(0.25f, 0.55f, 0.72f); // tint just under the line
    Math::Vector3 deepColor = Math::Vector3(0.04f, 0.12f, 0.26f);    // tint far below
    f32 opacity = 0.72f;                                    // max tint strength at depth
    f32 depthFalloff = 6.0f;                                // world units to reach deep color/opacity
    f32 waveAmplitude = 0.35f;                              // world-unit height of the surface waves
    f32 waveLength = 6.0f;                                  // world-unit wavelength
    f32 waveSpeed = 1.0f;                                   // drift rate of the surface
    Math::Vector3 foamColor = Math::Vector3(0.9f, 0.96f, 1.0f);
    f32 foamWidth = 0.35f;                                  // world-unit band of foam at the line
    f32 causticStrength = 0.25f;                            // 0 = flat tint, 1 = strong shimmer
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
