#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/Upscaling/IUpscaler.h"
#include <vulkan/vulkan.h>

#ifdef ENJIN_UPSCALING_FSR2
#include <FidelityFX/host/ffx_fsr2.h>
#include <FidelityFX/host/backends/vk/ffx_fsr2_vk.h>
#endif

namespace Enjin {
namespace Renderer {

class VulkanContext;

// AMD FidelityFX Super Resolution 2 temporal upscaler
// When ENJIN_UPSCALING_FSR2 is not defined, all methods gracefully return false / no-op.
class ENJIN_API FSR2Upscaler : public IUpscaler {
public:
    FSR2Upscaler(VulkanContext* context);
    ~FSR2Upscaler() override;

    bool Initialize(u32 renderWidth, u32 renderHeight,
                   u32 displayWidth, u32 displayHeight,
                   UpscalerQuality quality) override;
    void Resize(u32 renderWidth, u32 renderHeight,
               u32 displayWidth, u32 displayHeight) override;
    void Dispatch(VkCommandBuffer cmd, const UpscalerInput& input) override;
    void ResetHistory() override;
    void Shutdown() override;

    UpscalerType GetType() const override { return UpscalerType::FSR2; }
    const char* GetName() const override { return "AMD FSR 2"; }
    bool IsAvailable() const override;

    // Compile-time availability check
    static bool IsCompiled();

private:
    VulkanContext* m_Context = nullptr;
    u32 m_RenderWidth = 0;
    u32 m_RenderHeight = 0;
    u32 m_DisplayWidth = 0;
    u32 m_DisplayHeight = 0;
    bool m_Initialized = false;
    bool m_HistoryReset = false;

#ifdef ENJIN_UPSCALING_FSR2
    FfxFsr2Context m_FSR2Context{};
    std::vector<u8> m_ScratchBuffer;
#endif
};

} // namespace Renderer
} // namespace Enjin
