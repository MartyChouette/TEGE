#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

// Abstract denoiser interface for RT output denoising
class ENJIN_API IDenoiser {
public:
    virtual ~IDenoiser() = default;

    // Initialize denoiser resources for given resolution
    virtual bool Initialize(u32 width, u32 height) = 0;

    // Resize internal buffers when resolution changes
    virtual void Resize(u32 width, u32 height) = 0;

    // Denoise a single-channel (R16F) input image (shadows, AO)
    virtual void DenoiseSingleChannel(VkCommandBuffer cmd, VkImageView noisyInput,
                                       VkImageView depthView, VkImageView normalView,
                                       VkImageView motionView, VkImageView output) = 0;

    // Denoise a multi-channel (RGBA16F) input image (reflections, GI)
    virtual void DenoiseColor(VkCommandBuffer cmd, VkImageView noisyInput,
                               VkImageView depthView, VkImageView normalView,
                               VkImageView motionView, VkImageView output) = 0;

    // Reset temporal history (call on camera cut / scene change)
    virtual void ResetHistory() = 0;

    virtual void Shutdown() = 0;
};

} // namespace Renderer
} // namespace Enjin
