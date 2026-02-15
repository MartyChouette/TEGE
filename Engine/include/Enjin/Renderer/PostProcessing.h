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
class VulkanRenderer;
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
    LUT = 1 << 7,            // LUT color grading
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
    alignas(4) u32 toneMappingMode = static_cast<u32>(ToneMappingMode::None);
    alignas(4) f32 exposure = 1.0f;
    alignas(4) f32 gamma = 1.0f;  // Main shader already applies gamma; 1.0 = no double correction
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
    alignas(4) u32 colorblindMode = 0;       // 0=off, 1=protanopia, 2=deuteranopia, 3=tritanopia, 4=protanomaly, 5=deuteranomaly, 6=tritanomaly, 7=achromatopsia
    alignas(4) f32 colorblindStrength = 1.0f;

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

    // Retro: Dithering
    alignas(4) u32 ditherEnabled = 0;
    alignas(4) u32 ditherPattern = 0;     // 0=Bayer2x2, 1=Bayer4x4, 2=Bayer8x8
    alignas(4) f32 ditherStrength = 1.0f;
    alignas(4) f32 _retroPad0 = 0.0f;

    // Retro: Color quantization
    alignas(4) u32 colorQuantEnabled = 0;
    alignas(4) u32 colorBitDepth = 8;     // bits per channel (5=PS1, 8=modern)
    alignas(4) f32 _retroPad1 = 0.0f;
    alignas(4) f32 _retroPad2 = 0.0f;

    // Retro: Resolution downscaling
    alignas(4) u32 resDownscaleEnabled = 0;
    alignas(4) u32 internalWidth = 320;
    alignas(4) u32 internalHeight = 240;
    alignas(4) u32 usePointFiltering = 1;

    // CRT scanlines
    alignas(4) u32 crtEnabled = 0;
    alignas(4) f32 scanlineIntensity = 0.3f;
    alignas(4) f32 scanlineWidth = 1.0f;
    alignas(4) f32 crtCurvature = 0.0f;

    // LUT color grading
    alignas(4) u32 lutEnabled = 0;
    alignas(4) f32 lutStrength = 1.0f;
    alignas(4) u32 lutSize = 32;
    alignas(4) f32 _lutPad0 = 0.0f;

    // CRT Phosphor subpixel blending
    alignas(4) u32 crtPhosphorEnabled = 0;
    alignas(4) u32 crtMaskType = 0;        // 0=aperture grille, 1=shadow mask, 2=slot mask
    alignas(4) f32 crtMaskPitch = 1.0f;
    alignas(4) f32 crtBloomRadius = 1.5f;
    alignas(4) f32 crtBloomStrength = 0.3f;
    alignas(4) f32 _crtPad0 = 0.0f;
    alignas(4) f32 _crtPad1 = 0.0f;
    alignas(4) f32 _crtPad2 = 0.0f;

    // VHS filter
    alignas(4) u32 vhsEnabled = 0;
    alignas(4) f32 vhsTrackingIntensity = 0.3f;
    alignas(4) f32 vhsTrackingSpeed = 1.0f;
    alignas(4) f32 vhsWobbleIntensity = 0.002f;
    alignas(4) f32 vhsWobbleSpeed = 2.0f;
    alignas(4) f32 vhsColorBleed = 0.003f;
    alignas(4) f32 vhsNoiseIntensity = 0.05f;
    alignas(4) f32 vhsBlueShift = 0.05f;
    alignas(4) u32 vhsScreenTear = 0;
    alignas(4) f32 vhsTearOffset = 0.0f;
    alignas(4) u32 vhsInterlacing = 0;
    alignas(4) f32 _vhsPad0 = 0.0f;

    // Color palette lock
    alignas(4) u32 paletteEnabled = 0;
    alignas(4) u32 paletteColors = 16;
    alignas(4) f32 _palPad0 = 0.0f;
    alignas(4) f32 _palPad1 = 0.0f;

    // Depth of Field
    alignas(4) u32 dofEnabled = 0;
    alignas(4) f32 dofFocalDistance = 10.0f;    // World units
    alignas(4) f32 dofFocalRange = 5.0f;        // Transition zone width
    alignas(4) f32 dofNearBlurStrength = 1.0f;  // 0-1
    alignas(4) f32 dofFarBlurStrength = 1.0f;   // 0-1
    alignas(4) f32 dofBokehSize = 4.0f;         // Pixel radius
    alignas(4) u32 dofApertureShape = 0;        // 0=Circle, 1=Hexagon, 2=Octagon
    alignas(4) u32 dofDebugCoC = 0;             // Visualize circle of confusion

    // Camera planes (for depth linearization)
    alignas(4) f32 cameraNearPlane = 0.1f;
    alignas(4) f32 cameraFarPlane = 100.0f;
    alignas(4) f32 _cameraPad0 = 0.0f;
    alignas(4) f32 _cameraPad1 = 0.0f;

    // Tilt-Shift
    alignas(4) u32 tiltShiftEnabled = 0;
    alignas(4) f32 tiltShiftFocusY = 0.5f;      // Normalized screen Y (0-1)
    alignas(4) f32 tiltShiftBandWidth = 0.3f;   // Focus band width
    alignas(4) f32 tiltShiftBlurAmount = 3.0f;  // Max blur

    // Cel shading outlines (Sobel edge detection on depth)
    alignas(4) u32 celOutlineEnabled = 0;
    alignas(4) f32 celOutlineThickness = 1.0f;
    alignas(4) f32 celOutlineThreshold = 0.1f;    // Depth edge sensitivity
    alignas(4) f32 _celPad0 = 0.0f;
    alignas(16) Math::Vector3 celOutlineColor = Math::Vector3(0.0f, 0.0f, 0.0f);
    alignas(4) f32 _celPad1 = 0.0f;

    // Full-screen stipple / dither
    alignas(4) u32 stippleEnabled = 0;
    alignas(4) u32 stipplePatternMask = 1;   // Bitmask: bit0=Bayer4x4, bit1=Bayer8x8, bit2=BlueNoise, bit3=Halftone, bit4=Crosshatch, bit5=Overlook, bit6=Ordered2x2, bit7=FloydSteinberg
    alignas(4) u32 stippleColorMode = 0;     // 0=Monochrome, 1=DuoTone, 2=FullColor
    alignas(4) f32 stippleScale = 1.0f;      // Pattern pixel scale (1.0=native, 2.0=bigger)
    alignas(4) f32 stippleDensity = 0.5f;    // Threshold bias 0.0-1.0
    alignas(4) f32 stippleStrength = 1.0f;   // Blend 0.0-1.0 with original
    alignas(4) f32 _stipplePad0 = 0.0f;
    alignas(4) f32 _stipplePad1 = 0.0f;
    alignas(16) Math::Vector3 stippleFgColor = Math::Vector3(0.0f, 0.0f, 0.0f);  // Foreground/ink (black)
    alignas(4) f32 _stipplePad2 = 0.0f;
    alignas(16) Math::Vector3 stippleBgColor = Math::Vector3(1.0f, 1.0f, 1.0f);  // Background/paper (white)
    alignas(4) f32 _stipplePad3 = 0.0f;

    // Returns true if any post-processing effect is actually enabled or non-identity.
    // When false, the entire post-processing pass can be skipped (render direct to target).
    bool HasAnyActiveEffects() const {
        if (toneMappingMode != 0) return true;
        if (bloomEnabled) return true;
        if (vignetteEnabled) return true;
        if (chromaticAberrationEnabled) return true;
        if (filmGrainEnabled) return true;
        if (fxaaEnabled) return true;
        if (ditherEnabled) return true;
        if (colorQuantEnabled) return true;
        if (resDownscaleEnabled) return true;
        if (crtEnabled) return true;
        if (lutEnabled) return true;
        if (crtPhosphorEnabled) return true;
        if (vhsEnabled) return true;
        if (paletteEnabled) return true;
        if (dofEnabled) return true;
        if (tiltShiftEnabled) return true;
        if (celOutlineEnabled) return true;
        if (stippleEnabled) return true;
        if (colorblindMode != 0) return true;
        // Non-identity color grading
        if (brightness != 0.0f || contrast != 1.0f || saturation != 1.0f) return true;
        if (colorFilter.x != 1.0f || colorFilter.y != 1.0f || colorFilter.z != 1.0f) return true;
        // Non-identity gamma
        if (gamma != 1.0f) return true;
        return false;
    }

    // Returns true if any effect needs to read the depth buffer (DoF, TiltShift, CelOutline).
    bool NeedsDepthBuffer() const {
        return dofEnabled || tiltShiftEnabled || celOutlineEnabled;
    }
};

// Post-processing manager
class ENJIN_API PostProcessing {
public:
    PostProcessing();
    ~PostProcessing();

    // Initialize/Shutdown
    bool Initialize(VulkanContext* context, VkRenderPass renderPass, u32 width, u32 height,
                     VulkanRenderer* renderer = nullptr);
    void Shutdown();

    // Resize handling
    void OnResize(u32 width, u32 height);

    // Apply post-processing to a rendered image (uses internal scene render target)
    void Apply(VkCommandBuffer cmd, VkImageView sourceImage, VkFramebuffer targetFramebuffer);

    // Apply post-processing within an already-active render pass using an external source image.
    // Call UpdateSourceImage() first to bind the source texture, then call this inside a render pass.
    void ApplyToCurrentPass(VkCommandBuffer cmd, u32 width, u32 height);

    // Update the source image that post-processing reads from (rebinds descriptor set binding 0)
    void UpdateSourceImage(VkImageView imageView, VkSampler sampler);

    // Settings
    PostProcessSettings& GetSettings() { return m_Settings; }
    const PostProcessSettings& GetSettings() const { return m_Settings; }
    void SetSettings(const PostProcessSettings& settings) { m_Settings = settings; }

    // LUT color grading
    bool LoadLUT(const std::string& filepath);
    void ClearLUT();
    std::string GetLUTPath() const { return m_LUTPath; }
    bool IsLUTLoaded() const { return m_LUTLoaded; }

    // Individual effect toggles
    void SetEffectEnabled(PostProcessEffect effect, bool enabled);
    bool IsEffectEnabled(PostProcessEffect effect) const;

    // Update time for animated effects
    void Update(f32 deltaTime) { m_Settings.time += deltaTime; }

    // Set camera planes for depth linearization (DoF/Tilt-Shift)
    void SetCameraPlanes(f32 nearPlane, f32 farPlane) {
        m_Settings.cameraNearPlane = nearPlane;
        m_Settings.cameraFarPlane = farPlane;
    }

    // Get scene render target (render to this instead of swapchain)
    VkImage GetSceneImage() const;
    VkImageView GetSceneImageView() const;
    VkFramebuffer GetSceneFramebuffer() const;
    VkRenderPass GetSceneRenderPass() const { return m_SceneRenderPass; }

    // Check if post-processing is initialized
    bool IsInitialized() const { return m_Initialized; }

    // Apply depth-of-field blur to the scene image (CPU-side fallback).
    // Reads depth buffer + color, computes CoC, applies separable Gaussian blur.
    // Called automatically by Apply() when dofEnabled is set.
    void ApplyDepthOfField(VkCommandBuffer cmd);

    // Apply tilt-shift miniature effect to the scene image (CPU-side fallback).
    // Screen-space Y-driven blur band.
    // Called automatically by Apply() when tiltShiftEnabled is set.
    void ApplyTiltShift(VkCommandBuffer cmd);

private:
    bool CreateSceneRenderTarget(u32 width, u32 height);
    bool CreatePipeline();
    bool CreateDescriptorSets();
    bool CreateUniformBuffer();
    void DestroySceneRenderTarget();
    void UpdateUniformBuffer();

    // CPU-side blur helpers for DoF and Tilt-Shift
    bool CreateDofStagingBuffers();
    void DestroyDofStagingBuffers();
    void GaussianBlurCPU(std::vector<f32>& pixels, u32 w, u32 h, u32 channels,
                          const std::vector<f32>& blurWeights, std::vector<f32>& temp);
    void SeparableWeightedBlur(std::vector<f32>& color, const std::vector<f32>& coc,
                                u32 w, u32 h, u32 channels, f32 maxRadius,
                                std::vector<f32>& temp);

    VulkanContext* m_Context = nullptr;
    VulkanRenderer* m_Renderer = nullptr;
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

    // LUT texture resources
    std::string m_LUTPath;
    VkImage m_LUTImage = VK_NULL_HANDLE;
    VkDeviceMemory m_LUTImageMemory = VK_NULL_HANDLE;
    VkImageView m_LUTImageView = VK_NULL_HANDLE;
    VkSampler m_LUTSampler = VK_NULL_HANDLE;
    bool m_LUTLoaded = false;
    void DestroyLUTResources();

    // DoF/Tilt-Shift CPU staging buffers (GPU<->CPU roundtrip for blur)
    VkBuffer m_DofColorStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_DofColorStagingMemory = VK_NULL_HANDLE;
    VkBuffer m_DofDepthStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_DofDepthStagingMemory = VK_NULL_HANDLE;
    VkBuffer m_DofUploadBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_DofUploadMemory = VK_NULL_HANDLE;
    usize m_DofColorStagingSize = 0;
    usize m_DofDepthStagingSize = 0;
    bool m_DofStagingReady = false;
};

} // namespace Renderer
} // namespace Enjin
