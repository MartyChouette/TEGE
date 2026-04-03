#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

// Upscaler backend type
enum class UpscalerType : u8 {
    None = 0,
    FSR2 = 1,
    DLSS = 2,
    XeSS = 3
};

// Upscaler quality preset — maps to render resolution as a fraction of display resolution
enum class UpscalerQuality : u8 {
    Performance   = 0,   // 50% resolution (4x pixel gain)
    Balanced      = 1,   // 58% resolution (~3x pixel gain)
    Quality       = 2,   // 67% resolution (~2.25x pixel gain)
    UltraQuality  = 3    // 77% resolution (~1.7x pixel gain)
};

// Input data for an upscaler dispatch
struct UpscalerInput {
    VkImageView colorInput;       // Low-res jittered color (RGBA16F)
    VkImageView depthInput;       // Low-res depth (D32F)
    VkImageView velocityInput;    // Low-res velocity (RG16F, NDC space)
    VkImageView output;           // Full-res output (RGBA16F)
    f32 jitterX, jitterY;         // Current frame jitter in pixel space
    f32 deltaTime;                // Frame delta time in seconds
    f32 cameraNear, cameraFar;    // Camera clip planes
    f32 sharpness;                // Additional sharpening (0 = default)
    bool cameraCut;               // True on scene transitions / camera cuts
};

// Abstract upscaler interface — implemented by FSR2, DLSS, XeSS backends
class ENJIN_API IUpscaler {
public:
    virtual ~IUpscaler() = default;

    // Initialize upscaler resources for given render and display resolutions
    virtual bool Initialize(u32 renderWidth, u32 renderHeight,
                           u32 displayWidth, u32 displayHeight,
                           UpscalerQuality quality) = 0;

    // Resize when render or display resolution changes
    virtual void Resize(u32 renderWidth, u32 renderHeight,
                       u32 displayWidth, u32 displayHeight) = 0;

    // Dispatch the upscaler (compute pass, must be called outside a render pass)
    virtual void Dispatch(VkCommandBuffer cmd, const UpscalerInput& input) = 0;

    // Reset temporal history (call on camera cut / scene change)
    virtual void ResetHistory() = 0;

    // Release all resources
    virtual void Shutdown() = 0;

    // Backend identification
    virtual UpscalerType GetType() const = 0;
    virtual const char* GetName() const = 0;

    // Query whether this backend is available (SDK compiled in + hardware support)
    virtual bool IsAvailable() const = 0;

    // Get the upscaled output image view (for post-processing input)
    virtual VkImageView GetOutputImageView() const { return VK_NULL_HANDLE; }

    // Standard render scale ratios for each quality preset
    static f32 GetRenderScaleForQuality(UpscalerQuality quality) {
        switch (quality) {
            case UpscalerQuality::Performance:  return 0.50f;
            case UpscalerQuality::Balanced:     return 0.58f;
            case UpscalerQuality::Quality:      return 0.67f;
            case UpscalerQuality::UltraQuality: return 0.77f;
            default:                            return 0.67f;
        }
    }

    // Compute optimal render resolution for a given display resolution and quality
    static void GetRenderResolution(u32 displayWidth, u32 displayHeight,
                                    UpscalerQuality quality,
                                    u32& outRenderWidth, u32& outRenderHeight) {
        f32 scale = GetRenderScaleForQuality(quality);
        // Round to even dimensions (required by most upscalers)
        outRenderWidth  = (static_cast<u32>(displayWidth  * scale) + 1u) & ~1u;
        outRenderHeight = (static_cast<u32>(displayHeight * scale) + 1u) & ~1u;
    }
};

} // namespace Renderer
} // namespace Enjin
