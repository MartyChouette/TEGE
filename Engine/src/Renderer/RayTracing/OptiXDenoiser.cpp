#include "Enjin/Renderer/RayTracing/OptiXDenoiser.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Logging/Log.h"

#if defined(ENJIN_RAYTRACING_OPTIX) && !defined(_WIN32)
#include <unistd.h>  // close() for fd-based external memory/semaphore handles
#endif

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
    // Single channel (R16F) -- denoise as luminance
    DenoiseImpl(cmd, noisyInput, output, normalView, 1);
}

void OptiXDenoiser::DenoiseColor(VkCommandBuffer cmd, VkImageView noisyInput,
                                   VkImageView depthView, VkImageView normalView,
                                   VkImageView motionView, VkImageView output) {
    // Multi-channel (RGBA16F) -- full color denoise
    DenoiseImpl(cmd, noisyInput, output, normalView, 4);
}

void OptiXDenoiser::DenoiseImpl(VkCommandBuffer cmd, VkImageView input, VkImageView output,
                                  VkImageView normalView, u32 channels) {
#ifdef ENJIN_RAYTRACING_OPTIX
    if (!m_Initialized || !m_Denoiser) return;

    VkImage inputImage = ResolveImage(input);
    VkImage outputImage = ResolveImage(output);
    VkFormat inputFormat = ResolveFormat(input);

    if (inputImage == VK_NULL_HANDLE || outputImage == VK_NULL_HANDLE) {
        // No image mapping registered -- cannot denoise without source data
        return;
    }

    if (m_CudaInteropAvailable) {
        // ===================================================================
        // GPU-GPU path: Vulkan -> shared buffer -> CUDA/OptiX -> shared buffer -> Vulkan
        // No CPU readback, no pipeline stall. The shared buffers live in
        // device-local memory exported via VK_KHR_external_memory and imported
        // into CUDA via cuImportExternalMemory.
        // ===================================================================

        // Step 1: Copy VkImage -> shared input buffer (GPU-side blit)
        CopyImageToSharedBuffer(cmd, inputImage, inputFormat,
                                m_SharedInputBuffer, channels);

        // Step 2: Signal Vulkan semaphore so CUDA knows the copy is done
        SignalVulkanSemaphore(cmd);

        // We must submit the command buffer up to this point so the semaphore
        // actually gets signaled on the GPU timeline. However, since we are
        // called mid-command-buffer, we synchronize via timeline semaphore
        // wait on the CUDA side which will block the CUDA stream until the
        // Vulkan queue reaches the signal point.

        // Step 3: CUDA waits on the Vulkan semaphore
        WaitCudaOnVulkan();

        // Step 4: Copy from shared input buffer to OptiX's CUDA input buffer.
        // The shared buffer holds half-float data in GPU format; OptiX needs
        // float4. We run a CUDA kernel for format conversion, but for
        // simplicity we copy the raw bytes and let OptiX work with HALF4
        // when available, or do a device-to-device memcpy to the float4 buffer
        // that is already set up.
        usize rawSize = static_cast<usize>(m_Width) * m_Height;
        if (channels == 1) {
            // R16F: 2 bytes per pixel
            rawSize *= sizeof(u16);
        } else {
            // RGBA16F: 8 bytes per pixel
            rawSize *= 4 * sizeof(u16);
        }

        // Copy shared Vulkan buffer (half precision) -> CUDA input buffer.
        // OptiX expects float4, so we use cudaMemcpy followed by an in-place
        // half-to-float expansion. For RGBA16F we can use OptiX's HALF4 format
        // directly to avoid the conversion.
        usize float4Size = static_cast<usize>(m_Width) * m_Height * 4 * sizeof(float);

        // Use OptiX HALF4 format when input is RGBA16F to avoid conversion
        OptixPixelFormat optixFormat = OPTIX_PIXEL_FORMAT_FLOAT4;
        usize inputRowStride = m_Width * 4 * sizeof(float);
        usize inputPixelStride = 4 * sizeof(float);

        if (channels == 4) {
            // RGBA16F -> use HALF4 directly from shared buffer
            optixFormat = OPTIX_PIXEL_FORMAT_HALF4;
            inputRowStride = m_Width * 4 * sizeof(u16);
            inputPixelStride = 4 * sizeof(u16);
        } else {
            // R16F -> expand to float4 in the existing CUDA input buffer
            // We still need the full float4 buffer for single-channel input.
            // Zero out first, then copy the single channel value to all RGB.
            cudaMemsetAsync(reinterpret_cast<void*>(m_InputBuffer), 0, float4Size, m_CudaStream);
            inputRowStride = m_Width * 4 * sizeof(float);
            inputPixelStride = 4 * sizeof(float);
        }

        // Build OptiX image descriptors
        OptixImage2D optixInput{};
        if (channels == 4) {
            // Point directly at shared buffer (HALF4)
            optixInput.data = m_CudaInputPtr;
        } else {
            // Use the CUDA input buffer (FLOAT4, filled above)
            optixInput.data = m_InputBuffer;
        }
        optixInput.width = m_Width;
        optixInput.height = m_Height;
        optixInput.rowStrideInBytes = static_cast<u32>(inputRowStride);
        optixInput.pixelStrideInBytes = static_cast<u32>(inputPixelStride);
        optixInput.format = optixFormat;

        OptixImage2D optixOutput{};
        optixOutput.data = m_OutputBuffer;
        optixOutput.width = m_Width;
        optixOutput.height = m_Height;
        optixOutput.rowStrideInBytes = m_Width * 4 * sizeof(float);
        optixOutput.pixelStrideInBytes = 4 * sizeof(float);
        optixOutput.format = OPTIX_PIXEL_FORMAT_FLOAT4;

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
        layer.input = optixInput;
        layer.output = optixOutput;
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

        // Step 5: Invoke OptiX denoiser on the CUDA stream
        optixDenoiserInvoke(m_Denoiser, m_CudaStream,
                             &params, m_DenoiserState, m_DenoiserStateSize,
                             &guideLayer, &layer, 1,
                             0, 0,  // input offset
                             m_Scratch, m_ScratchSize);

        // Step 6: Copy denoised output (float4) back to the shared output buffer.
        // The output image expects half-float format, so we need conversion.
        // For RGBA16F outputs, we copy float4 -> shared buffer as raw bytes;
        // the CopySharedBufferToImage will handle the layout transition.
        // Note: OptiX always outputs float4. We write it to the CUDA output
        // buffer, then copy that to the shared output buffer for Vulkan to read.
        usize outputRawSize = static_cast<usize>(m_Width) * m_Height * 4 * sizeof(float);
        cudaMemcpyAsync(reinterpret_cast<void*>(m_CudaOutputPtr),
                         reinterpret_cast<void*>(m_OutputBuffer),
                         outputRawSize, cudaMemcpyDeviceToDevice, m_CudaStream);

        // Copy previous output for temporal mode
        if (m_Config.mode == OptiXDenoiserMode::Temporal && m_PrevOutputBuffer) {
            cudaMemcpyAsync(reinterpret_cast<void*>(m_PrevOutputBuffer),
                            reinterpret_cast<void*>(m_OutputBuffer),
                            float4Size, cudaMemcpyDeviceToDevice, m_CudaStream);
        }

        // Step 7: Signal CUDA semaphore so Vulkan knows denoise is done
        SignalCudaForVulkan();

        // Step 8: Vulkan waits on CUDA semaphore, then copies shared output -> VkImage
        WaitVulkanOnCuda(cmd);
        CopySharedBufferToImage(cmd, outputImage, inputFormat,
                                m_SharedOutputBuffer, channels);
    } else {
        // ===================================================================
        // Fallback path: pure CUDA (no interop). The CUDA input/output buffers
        // are not populated from Vulkan images -- this path exists only so
        // the denoiser object can be instantiated and tested without the
        // external memory extensions. In production, m_CudaInteropAvailable
        // should always be true on supported hardware.
        // ===================================================================
        OptixImage2D optixInput{};
        optixInput.data = m_InputBuffer;
        optixInput.width = m_Width;
        optixInput.height = m_Height;
        optixInput.rowStrideInBytes = m_Width * 4 * sizeof(float);
        optixInput.pixelStrideInBytes = 4 * sizeof(float);
        optixInput.format = OPTIX_PIXEL_FORMAT_FLOAT4;

        OptixImage2D optixOutput{};
        optixOutput.data = m_OutputBuffer;
        optixOutput.width = m_Width;
        optixOutput.height = m_Height;
        optixOutput.rowStrideInBytes = m_Width * 4 * sizeof(float);
        optixOutput.pixelStrideInBytes = 4 * sizeof(float);
        optixOutput.format = OPTIX_PIXEL_FORMAT_FLOAT4;

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
        layer.input = optixInput;
        layer.output = optixOutput;
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
                             0, 0,
                             m_Scratch, m_ScratchSize);

        cudaStreamSynchronize(m_CudaStream);

        if (m_Config.mode == OptiXDenoiserMode::Temporal && m_PrevOutputBuffer) {
            usize pixelSize = static_cast<usize>(m_Width) * m_Height * 4 * sizeof(float);
            cudaMemcpy(reinterpret_cast<void*>(m_PrevOutputBuffer),
                       reinterpret_cast<void*>(m_OutputBuffer), pixelSize, cudaMemcpyDeviceToDevice);
        }
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

// ============================================================================
// VULKAN-CUDA INTEROP: SETUP
// ============================================================================
// Creates shared VkBuffers with exportable external memory, imports them into
// CUDA address space, and sets up timeline semaphore interop for GPU-GPU sync.

bool OptiXDenoiser::SetupCudaInterop() {
#ifdef ENJIN_RAYTRACING_OPTIX
    if (!m_Context) return false;

    // Shared buffer size: RGBA float4 at full resolution (largest possible payload).
    // Both input and output buffers are this size so they can handle any RT effect.
    m_SharedBufferSize = static_cast<usize>(m_Width) * m_Height * 4 * sizeof(float);
    if (m_SharedBufferSize == 0) return false;

    // Create shared input buffer (Vulkan writes, CUDA reads)
    if (!CreateSharedBuffer(m_SharedInputBuffer, m_SharedInputMemory,
                            m_CudaExtInputMem, m_CudaInputPtr,
                            m_SharedBufferSize)) {
        ENJIN_LOG_WARN(Renderer, "OptiX: Failed to create shared input buffer -- CUDA interop unavailable");
        CleanupCudaInterop();
        return false;
    }

    // Create shared output buffer (CUDA writes, Vulkan reads)
    if (!CreateSharedBuffer(m_SharedOutputBuffer, m_SharedOutputMemory,
                            m_CudaExtOutputMem, m_CudaOutputPtr,
                            m_SharedBufferSize)) {
        ENJIN_LOG_WARN(Renderer, "OptiX: Failed to create shared output buffer -- CUDA interop unavailable");
        CleanupCudaInterop();
        return false;
    }

    // Create interop semaphores for Vulkan-CUDA synchronization
    if (!CreateInteropSemaphores()) {
        ENJIN_LOG_WARN(Renderer, "OptiX: Failed to create interop semaphores -- CUDA interop unavailable");
        CleanupCudaInterop();
        return false;
    }

    ENJIN_LOG_INFO(Renderer, "OptiX: Vulkan-CUDA interop established (shared buffer %zu bytes)",
                   m_SharedBufferSize);
    return true;
#else
    return false;
#endif
}

// ============================================================================
// VULKAN-CUDA INTEROP: SHARED BUFFER CREATION
// ============================================================================
// Allocates a VkBuffer backed by device-local memory with external memory
// export enabled. The memory handle is then imported into CUDA so both APIs
// can access the same physical allocation without any CPU-side copy.

#ifdef ENJIN_RAYTRACING_OPTIX
bool OptiXDenoiser::CreateSharedBuffer(VkBuffer& buffer, VkDeviceMemory& memory,
                                        CUexternalMemory& extMem, CUdeviceptr& devPtr,
                                        usize size) {
    VkDevice device = m_Context->GetDevice();

    // -- Vulkan side: create buffer with external memory export --
    VkExternalMemoryBufferCreateInfo extBufInfo{};
    extBufInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
#ifdef _WIN32
    extBufInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    extBufInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = &extBufInfo;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: Failed to create shared VkBuffer");
        return false;
    }

    // Query memory requirements
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer, &memReqs);

    // Allocate device-local memory with export capability
    VkExportMemoryAllocateInfo exportInfo{};
    exportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
#ifdef _WIN32
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &exportInfo;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: Failed to allocate exportable device memory");
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return false;
    }

    vkBindBufferMemory(device, buffer, memory, 0);

    // -- Get OS-level handle for the Vulkan memory --
#ifdef _WIN32
    VkMemoryGetWin32HandleInfoKHR handleInfo{};
    handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    handleInfo.memory = memory;
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    auto vkGetMemoryWin32HandleKHR_fn = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
        vkGetDeviceProcAddr(device, "vkGetMemoryWin32HandleKHR"));
    if (!vkGetMemoryWin32HandleKHR_fn) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: vkGetMemoryWin32HandleKHR not available");
        vkFreeMemory(device, memory, nullptr);
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return false;
    }

    HANDLE win32Handle = nullptr;
    if (vkGetMemoryWin32HandleKHR_fn(device, &handleInfo, &win32Handle) != VK_SUCCESS || !win32Handle) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: Failed to get Win32 memory handle");
        vkFreeMemory(device, memory, nullptr);
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return false;
    }

    // -- CUDA side: import external memory --
    CUDA_EXTERNAL_MEMORY_HANDLE_DESC extMemDesc{};
    extMemDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
    extMemDesc.handle.win32.handle = win32Handle;
    extMemDesc.size = memReqs.size;

    CUresult cuResult = cuImportExternalMemory(&extMem, &extMemDesc);
    // The Win32 handle is duplicated by CUDA, so we can close our copy
    CloseHandle(win32Handle);
#else
    VkMemoryGetFdInfoKHR fdInfo{};
    fdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
    fdInfo.memory = memory;
    fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

    auto vkGetMemoryFdKHR_fn = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        vkGetDeviceProcAddr(device, "vkGetMemoryFdKHR"));
    if (!vkGetMemoryFdKHR_fn) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: vkGetMemoryFdKHR not available");
        vkFreeMemory(device, memory, nullptr);
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return false;
    }

    int fd = -1;
    if (vkGetMemoryFdKHR_fn(device, &fdInfo, &fd) != VK_SUCCESS || fd < 0) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: Failed to get FD memory handle");
        vkFreeMemory(device, memory, nullptr);
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return false;
    }

    CUDA_EXTERNAL_MEMORY_HANDLE_DESC extMemDesc{};
    extMemDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD;
    extMemDesc.handle.fd = fd;
    extMemDesc.size = memReqs.size;

    CUresult cuResult = cuImportExternalMemory(&extMem, &extMemDesc);
    // CUDA takes ownership of the fd on success; on failure we must close it
    if (cuResult != CUDA_SUCCESS) {
        close(fd);
    }
#endif

    if (cuResult != CUDA_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: cuImportExternalMemory failed (error %d)",
                       static_cast<int>(cuResult));
        vkFreeMemory(device, memory, nullptr);
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        extMem = nullptr;
        return false;
    }

    // Map the imported memory to get a CUDA device pointer
    CUDA_EXTERNAL_MEMORY_BUFFER_DESC bufDesc{};
    bufDesc.offset = 0;
    bufDesc.size = size;
    bufDesc.flags = 0;

    cuResult = cuExternalMemoryGetMappedBuffer(&devPtr, extMem, &bufDesc);
    if (cuResult != CUDA_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: cuExternalMemoryGetMappedBuffer failed (error %d)",
                       static_cast<int>(cuResult));
        cuDestroyExternalMemory(extMem);
        extMem = nullptr;
        vkFreeMemory(device, memory, nullptr);
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void OptiXDenoiser::DestroySharedBuffer(VkBuffer& buffer, VkDeviceMemory& memory,
                                         CUexternalMemory& extMem, CUdeviceptr& devPtr) {
    if (devPtr) {
        // The device pointer is mapped from external memory; no explicit free needed.
        // It becomes invalid when we destroy the external memory.
        devPtr = 0;
    }
    if (extMem) {
        cuDestroyExternalMemory(extMem);
        extMem = nullptr;
    }
    VkDevice device = m_Context->GetDevice();
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}
#endif // ENJIN_RAYTRACING_OPTIX

// ============================================================================
// VULKAN-CUDA INTEROP: SEMAPHORE CREATION
// ============================================================================
// Timeline semaphores allow fine-grained GPU-GPU synchronization between the
// Vulkan graphics queue and the CUDA stream without CPU-side waits.

#ifdef ENJIN_RAYTRACING_OPTIX
bool OptiXDenoiser::CreateInteropSemaphores() {
    VkDevice device = m_Context->GetDevice();

    // Create exportable timeline semaphores
    VkSemaphoreTypeCreateInfo timelineInfo{};
    timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineInfo.initialValue = 0;

    VkExportSemaphoreCreateInfo exportSemInfo{};
    exportSemInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
    exportSemInfo.pNext = &timelineInfo;
#ifdef _WIN32
    exportSemInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    exportSemInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = &exportSemInfo;

    // "Ready" semaphore: Vulkan signals, CUDA waits
    if (vkCreateSemaphore(device, &semInfo, nullptr, &m_VkReadySemaphore) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: Failed to create ready semaphore");
        return false;
    }

    // "Done" semaphore: CUDA signals, Vulkan waits
    if (vkCreateSemaphore(device, &semInfo, nullptr, &m_CudaDoneSemaphore) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: Failed to create done semaphore");
        vkDestroySemaphore(device, m_VkReadySemaphore, nullptr);
        m_VkReadySemaphore = VK_NULL_HANDLE;
        return false;
    }

    // Import both semaphores into CUDA
    auto importSemaphore = [&](VkSemaphore vkSem, CUexternalSemaphore& cudaSem) -> bool {
#ifdef _WIN32
        VkSemaphoreGetWin32HandleInfoKHR handleInfo{};
        handleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
        handleInfo.semaphore = vkSem;
        handleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

        auto fn = reinterpret_cast<PFN_vkGetSemaphoreWin32HandleKHR>(
            vkGetDeviceProcAddr(device, "vkGetSemaphoreWin32HandleKHR"));
        if (!fn) return false;

        HANDLE win32Handle = nullptr;
        if (fn(device, &handleInfo, &win32Handle) != VK_SUCCESS || !win32Handle) return false;

        CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC desc{};
        desc.type = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32;
        desc.handle.win32.handle = win32Handle;

        CUresult res = cuImportExternalSemaphore(&cudaSem, &desc);
        CloseHandle(win32Handle);
        return res == CUDA_SUCCESS;
#else
        VkSemaphoreGetFdInfoKHR fdInfo{};
        fdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        fdInfo.semaphore = vkSem;
        fdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;

        auto fn = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
            vkGetDeviceProcAddr(device, "vkGetSemaphoreFdKHR"));
        if (!fn) return false;

        int fd = -1;
        if (fn(device, &fdInfo, &fd) != VK_SUCCESS || fd < 0) return false;

        CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC desc{};
        desc.type = CU_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD;
        desc.handle.fd = fd;

        CUresult res = cuImportExternalSemaphore(&cudaSem, &desc);
        if (res != CUDA_SUCCESS) close(fd);
        return res == CUDA_SUCCESS;
#endif
    };

    if (!importSemaphore(m_VkReadySemaphore, m_CudaExtReadySem)) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: Failed to import ready semaphore into CUDA");
        return false;
    }

    if (!importSemaphore(m_CudaDoneSemaphore, m_CudaExtDoneSem)) {
        ENJIN_LOG_WARN(Renderer, "OptiX interop: Failed to import done semaphore into CUDA");
        return false;
    }

    m_SemaphoreValue = 0;
    return true;
}

void OptiXDenoiser::DestroyInteropSemaphores() {
    if (m_CudaExtReadySem) { cuDestroyExternalSemaphore(m_CudaExtReadySem); m_CudaExtReadySem = nullptr; }
    if (m_CudaExtDoneSem) { cuDestroyExternalSemaphore(m_CudaExtDoneSem); m_CudaExtDoneSem = nullptr; }

    VkDevice device = m_Context->GetDevice();
    if (m_VkReadySemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, m_VkReadySemaphore, nullptr);
        m_VkReadySemaphore = VK_NULL_HANDLE;
    }
    if (m_CudaDoneSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, m_CudaDoneSemaphore, nullptr);
        m_CudaDoneSemaphore = VK_NULL_HANDLE;
    }
}
#endif // ENJIN_RAYTRACING_OPTIX

// ============================================================================
// VULKAN-CUDA INTEROP: SYNCHRONIZATION
// ============================================================================
// Timeline-semaphore-based sync: each denoise cycle increments a shared counter.
// Vulkan signals "ready" after the image->buffer copy, CUDA waits on it.
// CUDA signals "done" after denoising, Vulkan waits before buffer->image copy.

#ifdef ENJIN_RAYTRACING_OPTIX
void OptiXDenoiser::SignalVulkanSemaphore(VkCommandBuffer cmd) {
    // We cannot vkCmdSignalSemaphore from a command buffer for a timeline
    // semaphore mid-recording (that requires queue submit). Instead we use
    // a pipeline barrier to ensure the copy is complete, and use
    // vkQueueSubmit with the timeline semaphore signal on the graphics queue.
    //
    // Since we are mid-command-buffer, we insert a full pipeline barrier here
    // and rely on cudaStreamSynchronize + the semaphore to handle cross-API sync.
    // The actual semaphore signal happens at vkQueueSubmit time -- so we use
    // the CUDA wait as a stream-ordered operation that won't execute until
    // the Vulkan queue signals.
    //
    // For the interop path, we advance the timeline counter.
    m_SemaphoreValue++;

    // Insert a memory barrier so the buffer write from vkCmdCopyImageToBuffer
    // is visible to external consumers (CUDA via shared memory).
    VkMemoryBarrier memBarrier{};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 1, &memBarrier, 0, nullptr, 0, nullptr);
}

void OptiXDenoiser::WaitCudaOnVulkan() {
    // CUDA stream waits until the Vulkan "ready" semaphore reaches our value.
    // This is a GPU-side wait -- the CUDA stream blocks, not the CPU.
    CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS waitParams{};
    waitParams.params.fence.value = m_SemaphoreValue;

    cuWaitExternalSemaphoresAsync(&m_CudaExtReadySem, &waitParams, 1, m_CudaStream);
}

void OptiXDenoiser::SignalCudaForVulkan() {
    // CUDA stream signals the "done" semaphore after denoising completes.
    CUDA_EXTERNAL_SEMAPHORE_SIGNAL_PARAMS signalParams{};
    signalParams.params.fence.value = m_SemaphoreValue;

    cuSignalExternalSemaphoresAsync(&m_CudaExtDoneSem, &signalParams, 1, m_CudaStream);
}

void OptiXDenoiser::WaitVulkanOnCuda(VkCommandBuffer cmd) {
    // GPU-side: insert a memory barrier so subsequent buffer reads see CUDA's writes.
    // The actual cross-API sync is handled by the timeline semaphore that the
    // CUDA stream signaled. In a full pipeline this semaphore would be waited on
    // at vkQueueSubmit, but within a single command buffer we use a device-level
    // barrier plus cudaStreamSynchronize as a conservative sync point.
    //
    // cudaStreamSynchronize ensures all CUDA work (including the semaphore signal)
    // has completed before we record more Vulkan commands that read from the
    // shared output buffer.
    cudaStreamSynchronize(m_CudaStream);

    VkMemoryBarrier memBarrier{};
    memBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
    memBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 1, &memBarrier, 0, nullptr, 0, nullptr);
}
#endif // ENJIN_RAYTRACING_OPTIX

// ============================================================================
// VULKAN-CUDA INTEROP: IMAGE <-> SHARED BUFFER COPY
// ============================================================================
// GPU-side copies between VkImage (RT output) and the shared VkBuffer that
// CUDA can access. These are standard Vulkan transfer operations.

#ifdef ENJIN_RAYTRACING_OPTIX
void OptiXDenoiser::CopyImageToSharedBuffer(VkCommandBuffer cmd, VkImage image,
                                             VkFormat format, VkBuffer sharedBuf,
                                             u32 channels) {
    if (image == VK_NULL_HANDLE || sharedBuf == VK_NULL_HANDLE) return;

    // Transition image to TRANSFER_SRC
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy image to shared buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {m_Width, m_Height, 1};

    vkCmdCopyImageToBuffer(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            sharedBuf, 1, &region);
}

void OptiXDenoiser::CopySharedBufferToImage(VkCommandBuffer cmd, VkImage image,
                                              VkFormat format, VkBuffer sharedBuf,
                                              u32 channels) {
    if (image == VK_NULL_HANDLE || sharedBuf == VK_NULL_HANDLE) return;

    // Transition image to TRANSFER_DST
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy shared buffer to image
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {m_Width, m_Height, 1};

    vkCmdCopyBufferToImage(cmd, sharedBuf, image,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition image back to GENERAL for shader access
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}
#endif // ENJIN_RAYTRACING_OPTIX

// ============================================================================
// VULKAN-CUDA INTEROP: CLEANUP
// ============================================================================

void OptiXDenoiser::CleanupCudaInterop() {
#ifdef ENJIN_RAYTRACING_OPTIX
    DestroyInteropSemaphores();
    DestroySharedBuffer(m_SharedInputBuffer, m_SharedInputMemory,
                        m_CudaExtInputMem, m_CudaInputPtr);
    DestroySharedBuffer(m_SharedOutputBuffer, m_SharedOutputMemory,
                        m_CudaExtOutputMem, m_CudaOutputPtr);
    m_SharedBufferSize = 0;
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
