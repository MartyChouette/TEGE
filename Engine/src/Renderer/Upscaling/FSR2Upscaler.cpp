#include "Enjin/Renderer/Upscaling/FSR2Upscaler.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

// ============================================================================
// AVAILABILITY CHECK
// ============================================================================

bool FSR2Upscaler::IsCompiled() {
#ifdef ENJIN_UPSCALING_FSR2
    return true;
#else
    return false;
#endif
}

bool FSR2Upscaler::IsAvailable() const {
#ifdef ENJIN_UPSCALING_FSR2
    return true;
#else
    return false;
#endif
}

// ============================================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================================

FSR2Upscaler::FSR2Upscaler(VulkanContext* context)
    : m_Context(context) {}

FSR2Upscaler::~FSR2Upscaler() {
    Shutdown();
}

// ============================================================================
// INITIALIZE
// ============================================================================

bool FSR2Upscaler::Initialize(u32 renderWidth, u32 renderHeight,
                              u32 displayWidth, u32 displayHeight,
                              UpscalerQuality quality) {
    if (m_Initialized) return true;

#ifdef ENJIN_UPSCALING_FSR2
    m_RenderWidth = renderWidth;
    m_RenderHeight = renderHeight;
    m_DisplayWidth = displayWidth;
    m_DisplayHeight = displayHeight;

    // Get scratch buffer size for Vulkan backend
    VkPhysicalDevice physDevice = m_Context->GetPhysicalDevice();
    usize scratchSize = ffxFsr2GetScratchMemorySizeVK(physDevice);
    m_ScratchBuffer.resize(scratchSize);

    // Create FSR2 context description
    FfxFsr2ContextDescription contextDesc{};
    contextDesc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE
                      | FFX_FSR2_ENABLE_DEPTH_INVERTED
                      | FFX_FSR2_ENABLE_AUTO_EXPOSURE;
    contextDesc.maxRenderSize.width = renderWidth;
    contextDesc.maxRenderSize.height = renderHeight;
    contextDesc.displaySize.width = displayWidth;
    contextDesc.displaySize.height = displayHeight;

    // Get Vulkan backend interface
    FfxErrorCode err = ffxFsr2GetInterfaceVK(
        &contextDesc.callbacks,
        m_ScratchBuffer.data(),
        scratchSize,
        physDevice,
        vkGetDeviceProcAddr
    );

    if (err != FFX_OK) {
        ENJIN_LOG_WARN(Renderer, "FSR 2: Failed to create Vulkan interface (error %d)", static_cast<int>(err));
        return false;
    }

    // Create FSR2 context
    err = ffxFsr2ContextCreate(&m_FSR2Context, &contextDesc);
    if (err != FFX_OK) {
        ENJIN_LOG_WARN(Renderer, "FSR 2: Failed to create context (error %d)", static_cast<int>(err));
        return false;
    }

    m_Initialized = true;
    m_HistoryReset = true;
    ENJIN_LOG_INFO(Renderer, "FSR 2 upscaler initialized (%ux%u -> %ux%u)",
                   renderWidth, renderHeight, displayWidth, displayHeight);
    return true;
#else
    ENJIN_LOG_WARN(Renderer, "FSR 2 upscaler not available (ENJIN_UPSCALING_FSR2=OFF)");
    return false;
#endif
}

// ============================================================================
// RESIZE
// ============================================================================

void FSR2Upscaler::Resize(u32 renderWidth, u32 renderHeight,
                          u32 displayWidth, u32 displayHeight) {
    if (renderWidth == m_RenderWidth && renderHeight == m_RenderHeight &&
        displayWidth == m_DisplayWidth && displayHeight == m_DisplayHeight) return;

#ifdef ENJIN_UPSCALING_FSR2
    // FSR2 requires context recreation on resize
    UpscalerQuality quality = UpscalerQuality::Quality;  // Preserve current quality
    Shutdown();
    Initialize(renderWidth, renderHeight, displayWidth, displayHeight, quality);
#endif
}

// ============================================================================
// DISPATCH
// ============================================================================

void FSR2Upscaler::Dispatch(VkCommandBuffer cmd, const UpscalerInput& input) {
#ifdef ENJIN_UPSCALING_FSR2
    if (!m_Initialized) return;

    FfxFsr2DispatchDescription dispatchDesc{};

    // Convert VkImageViews to FfxResource — FSR2 Vulkan backend handles this
    dispatchDesc.commandList = ffxGetCommandListVK(cmd);
    dispatchDesc.color = ffxGetTextureResourceVK(&m_FSR2Context, input.colorInput,
                                                  m_RenderWidth, m_RenderHeight,
                                                  VK_FORMAT_R16G16B16A16_SFLOAT);
    dispatchDesc.depth = ffxGetTextureResourceVK(&m_FSR2Context, input.depthInput,
                                                  m_RenderWidth, m_RenderHeight,
                                                  VK_FORMAT_D32_SFLOAT);
    dispatchDesc.motionVectors = ffxGetTextureResourceVK(&m_FSR2Context, input.velocityInput,
                                                          m_RenderWidth, m_RenderHeight,
                                                          VK_FORMAT_R16G16_SFLOAT);
    dispatchDesc.output = ffxGetTextureResourceVK(&m_FSR2Context, input.output,
                                                   m_DisplayWidth, m_DisplayHeight,
                                                   VK_FORMAT_R16G16B16A16_SFLOAT);

    dispatchDesc.jitterOffset.x = input.jitterX;
    dispatchDesc.jitterOffset.y = input.jitterY;
    dispatchDesc.motionVectorScale.x = static_cast<f32>(m_RenderWidth);
    dispatchDesc.motionVectorScale.y = static_cast<f32>(m_RenderHeight);
    dispatchDesc.renderSize.width = m_RenderWidth;
    dispatchDesc.renderSize.height = m_RenderHeight;
    dispatchDesc.enableSharpening = (input.sharpness > 0.0f);
    dispatchDesc.sharpness = input.sharpness;
    dispatchDesc.frameTimeDelta = input.deltaTime * 1000.0f;  // FSR2 expects milliseconds
    dispatchDesc.cameraNear = input.cameraNear;
    dispatchDesc.cameraFar = input.cameraFar;
    dispatchDesc.cameraFovAngleVertical = 0.0f;  // Not required when using projection matrix
    dispatchDesc.reset = m_HistoryReset || input.cameraCut;

    FfxErrorCode err = ffxFsr2ContextDispatch(&m_FSR2Context, &dispatchDesc);
    if (err != FFX_OK) {
        ENJIN_LOG_WARN(Renderer, "FSR 2: Dispatch failed (error %d)", static_cast<int>(err));
    }

    m_HistoryReset = false;
#endif
}

// ============================================================================
// RESET HISTORY
// ============================================================================

void FSR2Upscaler::ResetHistory() {
    m_HistoryReset = true;
}

// ============================================================================
// SHUTDOWN
// ============================================================================

void FSR2Upscaler::Shutdown() {
    if (!m_Initialized) return;

#ifdef ENJIN_UPSCALING_FSR2
    ffxFsr2ContextDestroy(&m_FSR2Context);
    m_ScratchBuffer.clear();
#endif

    m_Initialized = false;
    m_RenderWidth = 0;
    m_RenderHeight = 0;
    m_DisplayWidth = 0;
    m_DisplayHeight = 0;
}

} // namespace Renderer
} // namespace Enjin
