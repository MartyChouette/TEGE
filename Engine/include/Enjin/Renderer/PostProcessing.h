#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>

namespace Enjin {
namespace Renderer {

// Forward declarations
class VulkanContext;
class VulkanBuffer;
class VulkanImage;

// Post-processing effect types
enum class PostProcessEffect : u32 {
    None = 0,
    ToneMapping = 1 << 0,    // HDR to LDR conversion
    Bloom = 1 << 1,          // Bright area glow
    Vignette = 1 << 2,       // Dark corners
    ChromaticAberration = 1 << 3,
    FXAA = 1 << 4,           // Fast approximate anti-aliasing
    ColorGrading = 1 << 5,   // Color correction
    FilmGrain = 1 << 6,      // Film grain noise
    All = 0xFFFFFFFF
};

inline PostProcessEffect operator|(PostProcessEffect a, PostProcessEffect b) {
    return static_cast<PostProcessEffect>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline PostProcessEffect operator&(PostProcessEffect a, PostProcessEffect b) {
    return static_cast<PostProcessEffect>(static_cast<u32>(a) & static_cast<u32>(b));
}

inline bool HasEffect(PostProcessEffect flags, PostProcessEffect effect) {
    return (static_cast<u32>(flags) & static_cast<u32>(effect)) != 0;
}

// Tone mapping modes
enum class ToneMappingMode : u32 {
    None = 0,           // No tone mapping (linear)
    Reinhard,           // Simple Reinhard
    ReinhardExtended,   // Extended Reinhard with white point
    ACES,               // ACES filmic
    Uncharted2,         // Uncharted 2 filmic
    AgX                 // AgX filmic
};

// Post-processing settings (GPU-aligned)
struct alignas(16) PostProcessSettings {
    // Tone mapping
    alignas(4) u32 toneMappingMode = static_cast<u32>(ToneMappingMode::ACES);
    alignas(4) f32 exposure = 1.0f;
    alignas(4) f32 gamma = 2.2f;
    alignas(4) f32 whitePoint = 4.0f;

    // Bloom
    alignas(4) u32 bloomEnabled = 0;
    alignas(4) f32 bloomThreshold = 1.0f;
    alignas(4) f32 bloomIntensity = 0.5f;
    alignas(4) f32 bloomRadius = 0.005f;

    // Vignette
    alignas(4) u32 vignetteEnabled = 0;
    alignas(4) f32 vignetteIntensity = 0.3f;
    alignas(4) f32 vignetteSmoothness = 0.5f;
    alignas(4) f32 _pad0;

    // Chromatic aberration
    alignas(4) u32 chromaticAberrationEnabled = 0;
    alignas(4) f32 chromaticAberrationIntensity = 0.005f;
    alignas(4) f32 _pad1;
    alignas(4) f32 _pad2;

    // Color grading
    alignas(16) Math::Vector3 colorFilter = Math::Vector3(1.0f, 1.0f, 1.0f);
    alignas(4) f32 saturation = 1.0f;
    alignas(4) f32 contrast = 1.0f;
    alignas(4) f32 brightness = 0.0f;
    alignas(4) f32 _pad3;
    alignas(4) f32 _pad4;

    // Film grain
    alignas(4) u32 filmGrainEnabled = 0;
    alignas(4) f32 filmGrainIntensity = 0.05f;
    alignas(4) f32 time = 0.0f;
    alignas(4) f32 _pad5;

    // FXAA
    alignas(4) u32 fxaaEnabled = 1;
    alignas(4) f32 fxaaSpanMax = 8.0f;
    alignas(4) f32 fxaaReduceMin = 1.0f / 128.0f;
    alignas(4) f32 fxaaReduceMul = 1.0f / 8.0f;

    // Screen resolution for effects
    alignas(4) u32 screenWidth = 1920;
    alignas(4) u32 screenHeight = 1080;
    alignas(4) f32 _pad6;
    alignas(4) f32 _pad7;
};

// Post-processing manager
class ENJIN_API PostProcessing {
public:
    PostProcessing();
    ~PostProcessing();

    // Initialize/Shutdown
    bool Initialize(VulkanContext* context, VkRenderPass renderPass, u32 width, u32 height);
    void Shutdown();

    // Resize handling
    void OnResize(u32 width, u32 height);

    // Apply post-processing to a rendered image
    void Apply(VkCommandBuffer cmd, VkImageView sourceImage, VkFramebuffer targetFramebuffer);

    // Settings
    PostProcessSettings& GetSettings() { return m_Settings; }
    const PostProcessSettings& GetSettings() const { return m_Settings; }
    void SetSettings(const PostProcessSettings& settings) { m_Settings = settings; }

    // Individual effect toggles
    void SetEffectEnabled(PostProcessEffect effect, bool enabled);
    bool IsEffectEnabled(PostProcessEffect effect) const;

    // Update time for animated effects
    void Update(f32 deltaTime) { m_Settings.time += deltaTime; }

    // Get scene render target (render to this instead of swapchain)
    VkImage GetSceneImage() const;
    VkImageView GetSceneImageView() const;
    VkFramebuffer GetSceneFramebuffer() const;
    VkRenderPass GetSceneRenderPass() const { return m_SceneRenderPass; }

    // Check if post-processing is initialized
    bool IsInitialized() const { return m_Initialized; }

private:
    bool CreateSceneRenderTarget(u32 width, u32 height);
    bool CreatePipeline();
    bool CreateDescriptorSets();
    bool CreateUniformBuffer();
    void DestroySceneRenderTarget();
    void UpdateUniformBuffer();

    VulkanContext* m_Context = nullptr;
    VkRenderPass m_RenderPass = VK_NULL_HANDLE;  // Final output render pass
    VkRenderPass m_SceneRenderPass = VK_NULL_HANDLE;  // Scene render pass (HDR)

    // Scene render target (HDR floating-point texture)
    VkImage m_SceneImage = VK_NULL_HANDLE;
    VkDeviceMemory m_SceneImageMemory = VK_NULL_HANDLE;
    VkImageView m_SceneImageView = VK_NULL_HANDLE;
    VkFramebuffer m_SceneFramebuffer = VK_NULL_HANDLE;
    VkSampler m_SceneSampler = VK_NULL_HANDLE;

    // Depth buffer for scene rendering
    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;

    // Post-processing pipeline
    VkPipeline m_Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;

    // Uniform buffer for settings
    std::unique_ptr<VulkanBuffer> m_UniformBuffer;

    PostProcessSettings m_Settings;
    PostProcessEffect m_EnabledEffects = PostProcessEffect::ToneMapping;

    u32 m_Width = 0;
    u32 m_Height = 0;
    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
