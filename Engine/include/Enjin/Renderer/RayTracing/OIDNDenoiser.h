#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/RayTracing/IDenoiser.h"
#include <vulkan/vulkan.h>
#include <vector>

#ifdef ENJIN_RAYTRACING_OIDN
#include <OpenImageDenoise/oidn.h>
#endif

namespace Enjin {
namespace Renderer {

class VulkanContext;

// OIDN denoiser quality presets
enum class OIDNQuality : u32 {
    Fast = 0,      // Fastest, lower quality
    Default = 1,   // Balanced quality/performance
    High = 2       // Best quality, slower
};

// OIDN denoiser configuration
struct OIDNConfig {
    OIDNQuality quality = OIDNQuality::Default;
    f32 inputScale = 1.0f;          // Scale factor for noisy input (0 = auto)
    bool useAlbedo = false;         // Use albedo auxiliary image (when available)
    bool useNormals = false;        // Use normals auxiliary image (when available)
    bool cleanAux = false;          // Whether auxiliary images are noise-free
};

// Intel Open Image Denoise backend
// Uses CPU-side denoising with GPU↔CPU staging buffers for Vulkan interop.
// OIDN provides ML-trained denoising that handles very noisy (1 spp) RT output
// better than hand-tuned spatial filters like SVGF.
class ENJIN_API OIDNDenoiser : public IDenoiser {
public:
    OIDNDenoiser(VulkanContext* context);
    ~OIDNDenoiser() override;

    bool Initialize(u32 width, u32 height) override;
    void Resize(u32 width, u32 height) override;

    void DenoiseSingleChannel(VkCommandBuffer cmd, VkImageView noisyInput,
                               VkImageView depthView, VkImageView normalView,
                               VkImageView motionView, VkImageView output) override;

    void DenoiseColor(VkCommandBuffer cmd, VkImageView noisyInput,
                       VkImageView depthView, VkImageView normalView,
                       VkImageView motionView, VkImageView output) override;

    void ResetHistory() override;
    void Shutdown() override;

    OIDNConfig& GetConfig() { return m_Config; }
    const OIDNConfig& GetConfig() const { return m_Config; }

    // Query availability at compile time
    static bool IsAvailable();

private:
    void CreateStagingBuffers();
    void DestroyStagingBuffers();

    // Copy GPU image to CPU staging buffer, denoise, copy back
    void DenoiseImpl(VkCommandBuffer cmd, VkImageView input, VkImageView output,
                     VkImageView normalView, u32 channels);

    VulkanContext* m_Context = nullptr;
    OIDNConfig m_Config;
    u32 m_Width = 0;
    u32 m_Height = 0;

#ifdef ENJIN_RAYTRACING_OIDN
    OIDNDevice m_Device = nullptr;
    OIDNFilter m_ColorFilter = nullptr;
    OIDNFilter m_SingleFilter = nullptr;
#endif

    // CPU-side buffers for OIDN processing
    std::vector<f32> m_InputBuffer;
    std::vector<f32> m_OutputBuffer;
    std::vector<f32> m_NormalBuffer;

    // Vulkan staging buffers for GPU↔CPU transfer
    VkBuffer m_StagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_StagingMemory = VK_NULL_HANDLE;
    VkBuffer m_OutputStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_OutputStagingMemory = VK_NULL_HANDLE;
    usize m_StagingSize = 0;

    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
