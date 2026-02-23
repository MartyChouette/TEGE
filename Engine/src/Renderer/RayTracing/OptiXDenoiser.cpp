#include "Enjin/Renderer/RayTracing/OptiXDenoiser.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Renderer {

OptiXDenoiser::OptiXDenoiser(VulkanContext* context) : m_Context(context) {}
OptiXDenoiser::~OptiXDenoiser() { Shutdown(); }

bool OptiXDenoiser::IsAvailable() {
#ifdef ENJIN_RAYTRACING_OPTIX
    // Check if CUDA + OptiX runtime are present
    int deviceCount = 0;
    if (cudaGetDeviceCount(&deviceCount) != cudaSuccess || deviceCount == 0) {
        return false;
    }
    if (optixInit() != OPTIX_SUCCESS) {
        return false;
    }
    return true;
#else
    return false;
#endif
}

bool OptiXDenoiser::Initialize(u32 width, u32 height) {
    m_Width = width;
    m_Height = height;

#ifdef ENJIN_RAYTRACING_OPTIX
    // Initialize CUDA
    cudaFree(nullptr);  // Force CUDA context init

    // Create OptiX context from CUDA context
    CUcontext cuCtx;
    cuCtxGetCurrent(&cuCtx);

    OptixDeviceContextOptions options{};
    options.logCallbackLevel = 3;
    if (optixDeviceContextCreate(cuCtx, &options, &m_OptixContext) != OPTIX_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "OptiX: Failed to create device context");
        return false;
    }

    // Configure denoiser based on mode
    OptixDenoiserOptions denoiserOptions{};
    denoiserOptions.guideAlbedo = m_Config.useAlbedo ? 1 : 0;
    denoiserOptions.guideNormal = m_Config.useNormals ? 1 : 0;

    OptixDenoiserModelKind modelKind = OPTIX_DENOISER_MODEL_KIND_HDR;
    if (m_Config.mode == OptiXDenoiserMode::LDR) {
        modelKind = OPTIX_DENOISER_MODEL_KIND_LDR;
    } else if (m_Config.mode == OptiXDenoiserMode::Temporal) {
        modelKind = OPTIX_DENOISER_MODEL_KIND_TEMPORAL;
    }

    if (optixDenoiserCreate(m_OptixContext, modelKind, &denoiserOptions, &m_Denoiser) != OPTIX_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "OptiX: Failed to create denoiser");
        return false;
    }

    // Query memory requirements
    OptixDenoiserSizes sizes{};
    if (optixDenoiserComputeMemoryResources(m_Denoiser, width, height, &sizes) != OPTIX_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "OptiX: Failed to compute denoiser memory requirements");
        return false;
    }

    m_DenoiserStateSize = sizes.stateSizeInBytes;
    m_ScratchSize = sizes.withoutOverlapScratchSizeInBytes;

    // Allocate CUDA buffers
    cudaMalloc(reinterpret_cast<void**>(&m_DenoiserState), m_DenoiserStateSize);
    cudaMalloc(reinterpret_cast<void**>(&m_Scratch), m_ScratchSize);

    usize pixelSize = static_cast<usize>(width) * height * 4 * sizeof(float);
    cudaMalloc(reinterpret_cast<void**>(&m_InputBuffer), pixelSize);
    cudaMalloc(reinterpret_cast<void**>(&m_OutputBuffer), pixelSize);

    if (m_Config.useNormals)
        cudaMalloc(reinterpret_cast<void**>(&m_NormalBuffer), pixelSize);
    if (m_Config.useAlbedo)
        cudaMalloc(reinterpret_cast<void**>(&m_AlbedoBuffer), pixelSize);
    if (m_Config.mode == OptiXDenoiserMode::Temporal) {
        cudaMalloc(reinterpret_cast<void**>(&m_FlowBuffer), static_cast<usize>(width) * height * 2 * sizeof(float));
        cudaMalloc(reinterpret_cast<void**>(&m_PrevOutputBuffer), pixelSize);
    }

    cudaStreamCreate(&m_CudaStream);

    // Setup denoiser
    if (optixDenoiserSetup(m_Denoiser, m_CudaStream,
                            width, height,
                            m_DenoiserState, m_DenoiserStateSize,
                            m_Scratch, m_ScratchSize) != OPTIX_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "OptiX: Failed to setup denoiser");
        return false;
    }

    // Try to setup Vulkan-CUDA interop for zero-copy
    m_CudaInteropAvailable = SetupCudaInterop();

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "OptiX AI Denoiser initialized (%ux%u, mode=%u, CUDA interop=%s)",
                   width, height, static_cast<u32>(m_Config.mode),
                   m_CudaInteropAvailable ? "yes" : "no");
    return true;
#else
    ENJIN_LOG_WARN(Renderer, "OptiX denoiser not available (compiled without ENJIN_RAYTRACING_OPTIX)");
    return false;
#endif
}

void OptiXDenoiser::Resize(u32 width, u32 height) {
    if (width == m_Width && height == m_Height) return;
    Shutdown();
    m_Width = width;
    m_Height = height;
    Initialize(width, height);
}

void OptiXDenoiser::DenoiseSingleChannel(VkCommandBuffer cmd, VkImageView noisyInput,
                                           VkImageView depthView, VkImageView normalView,
                                           VkImageView motionView, VkImageView output) {
    // Single channel (R16F) — denoise as luminance
    DenoiseImpl(cmd, noisyInput, output, normalView, 1);
}

void OptiXDenoiser::DenoiseColor(VkCommandBuffer cmd, VkImageView noisyInput,
                                   VkImageView depthView, VkImageView normalView,
                                   VkImageView motionView, VkImageView output) {
    // Multi-channel (RGBA16F) — full color denoise
    DenoiseImpl(cmd, noisyInput, output, normalView, 4);
}

void OptiXDenoiser::DenoiseImpl(VkCommandBuffer cmd, VkImageView input, VkImageView output,
                                  VkImageView normalView, u32 channels) {
#ifdef ENJIN_RAYTRACING_OPTIX
    if (!m_Initialized || !m_Denoiser) return;

    // TODO: Full Vulkan→CUDA copy, OptiX denoise, CUDA→Vulkan copy
    // For now, the denoiser is wired up but the actual GPU-GPU transfer path
    // requires Vulkan external memory (VK_KHR_external_memory) + CUDA import.
    // This will be populated once we verify the OptiX libs link correctly.

    OptixImage2D inputImage{};
    inputImage.data = m_InputBuffer;
    inputImage.width = m_Width;
    inputImage.height = m_Height;
    inputImage.rowStrideInBytes = m_Width * 4 * sizeof(float);
    inputImage.pixelStrideInBytes = 4 * sizeof(float);
    inputImage.format = OPTIX_PIXEL_FORMAT_FLOAT4;

    OptixImage2D outputImage{};
    outputImage.data = m_OutputBuffer;
    outputImage.width = m_Width;
    outputImage.height = m_Height;
    outputImage.rowStrideInBytes = m_Width * 4 * sizeof(float);
    outputImage.pixelStrideInBytes = 4 * sizeof(float);
    outputImage.format = OPTIX_PIXEL_FORMAT_FLOAT4;

    OptixDenoiserGuideLayer guideLayer{};
    if (m_Config.useNormals && m_NormalBuffer) {
        guideLayer.normal.data = m_NormalBuffer;
        guideLayer.normal.width = m_Width;
        guideLayer.normal.height = m_Height;
        guideLayer.normal.rowStrideInBytes = m_Width * 4 * sizeof(float);
        guideLayer.normal.pixelStrideInBytes = 4 * sizeof(float);
        guideLayer.normal.format = OPTIX_PIXEL_FORMAT_FLOAT4;
    }

    OptixDenoiserLayer layer{};
    layer.input = inputImage;
    layer.output = outputImage;
    if (m_Config.mode == OptiXDenoiserMode::Temporal && m_PrevOutputBuffer) {
        layer.previousOutput.data = m_PrevOutputBuffer;
        layer.previousOutput.width = m_Width;
        layer.previousOutput.height = m_Height;
        layer.previousOutput.rowStrideInBytes = m_Width * 4 * sizeof(float);
        layer.previousOutput.pixelStrideInBytes = 4 * sizeof(float);
        layer.previousOutput.format = OPTIX_PIXEL_FORMAT_FLOAT4;
    }

    OptixDenoiserParams params{};
    params.blendFactor = m_Config.blendFactor;

    optixDenoiserInvoke(m_Denoiser, m_CudaStream,
                         &params, m_DenoiserState, m_DenoiserStateSize,
                         &guideLayer, &layer, 1,
                         0, 0,  // input offset
                         m_Scratch, m_ScratchSize);

    cudaStreamSynchronize(m_CudaStream);

    // Copy previous output for temporal mode
    if (m_Config.mode == OptiXDenoiserMode::Temporal && m_PrevOutputBuffer) {
        usize pixelSize = static_cast<usize>(m_Width) * m_Height * 4 * sizeof(float);
        cudaMemcpy(reinterpret_cast<void*>(m_PrevOutputBuffer),
                   reinterpret_cast<void*>(m_OutputBuffer), pixelSize, cudaMemcpyDeviceToDevice);
    }

    m_FrameIndex++;
#endif
}

void OptiXDenoiser::ResetHistory() {
    m_FrameIndex = 0;
#ifdef ENJIN_RAYTRACING_OPTIX
    // Clear temporal buffers
    if (m_PrevOutputBuffer) {
        usize pixelSize = static_cast<usize>(m_Width) * m_Height * 4 * sizeof(float);
        cudaMemset(reinterpret_cast<void*>(m_PrevOutputBuffer), 0, pixelSize);
    }
#endif
}

bool OptiXDenoiser::SetupCudaInterop() {
#ifdef ENJIN_RAYTRACING_OPTIX
    // Vulkan-CUDA interop requires VK_KHR_external_memory + VK_KHR_external_semaphore
    // Check if the Vulkan device supports these extensions
    // Full implementation deferred — staging buffer fallback used for now
    return false;
#else
    return false;
#endif
}

void OptiXDenoiser::CleanupCudaInterop() {
#ifdef ENJIN_RAYTRACING_OPTIX
    if (m_SharedBuffer) {
        vkDestroyBuffer(m_Context->GetDevice(), m_SharedBuffer, nullptr);
        m_SharedBuffer = VK_NULL_HANDLE;
    }
    if (m_SharedMemory) {
        vkFreeMemory(m_Context->GetDevice(), m_SharedMemory, nullptr);
        m_SharedMemory = VK_NULL_HANDLE;
    }
#endif
}

void OptiXDenoiser::RegisterImageMapping(VkImageView view, VkImage image, VkFormat format) {
    m_ImageMap[view] = { image, format };
}

VkImage OptiXDenoiser::ResolveImage(VkImageView view) const {
    auto it = m_ImageMap.find(view);
    return it != m_ImageMap.end() ? it->second.image : VK_NULL_HANDLE;
}

VkFormat OptiXDenoiser::ResolveFormat(VkImageView view) const {
    auto it = m_ImageMap.find(view);
    return it != m_ImageMap.end() ? it->second.format : VK_FORMAT_UNDEFINED;
}

void OptiXDenoiser::Shutdown() {
    if (!m_Initialized) return;

#ifdef ENJIN_RAYTRACING_OPTIX
    if (m_CudaStream) { cudaStreamDestroy(m_CudaStream); m_CudaStream = nullptr; }
    if (m_InputBuffer) { cudaFree(reinterpret_cast<void*>(m_InputBuffer)); m_InputBuffer = 0; }
    if (m_OutputBuffer) { cudaFree(reinterpret_cast<void*>(m_OutputBuffer)); m_OutputBuffer = 0; }
    if (m_NormalBuffer) { cudaFree(reinterpret_cast<void*>(m_NormalBuffer)); m_NormalBuffer = 0; }
    if (m_AlbedoBuffer) { cudaFree(reinterpret_cast<void*>(m_AlbedoBuffer)); m_AlbedoBuffer = 0; }
    if (m_FlowBuffer) { cudaFree(reinterpret_cast<void*>(m_FlowBuffer)); m_FlowBuffer = 0; }
    if (m_PrevOutputBuffer) { cudaFree(reinterpret_cast<void*>(m_PrevOutputBuffer)); m_PrevOutputBuffer = 0; }
    if (m_DenoiserState) { cudaFree(reinterpret_cast<void*>(m_DenoiserState)); m_DenoiserState = 0; }
    if (m_Scratch) { cudaFree(reinterpret_cast<void*>(m_Scratch)); m_Scratch = 0; }
    if (m_Denoiser) { optixDenoiserDestroy(m_Denoiser); m_Denoiser = nullptr; }
    if (m_OptixContext) { optixDeviceContextDestroy(m_OptixContext); m_OptixContext = nullptr; }

    CleanupCudaInterop();
#endif

    m_Initialized = false;
}

} // namespace Renderer
} // namespace Enjin
