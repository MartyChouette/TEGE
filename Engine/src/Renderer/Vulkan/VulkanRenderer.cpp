#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanBufferManager.h"
#include "Enjin/Renderer/Vulkan/VulkanTextureManager.h"
#include "Enjin/Renderer/Vulkan/VulkanShaderManager.h"
#include "Enjin/Renderer/Vulkan/VulkanPipelineManager.h"
#include "Enjin/Renderer/Vulkan/VulkanBindGroupManager.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderEncoder.h"
#include "Enjin/Logging/Log.h"
#include <chrono>
#include "Enjin/Core/Assert.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #ifdef CreateWindow
        #undef CreateWindow
    #endif
#endif

/**
 * @file VulkanRenderer.cpp
 * @brief Implementation of VulkanRenderer class
 * @author Enjin Engine Team
 * @date 2025
 */

namespace Enjin {
namespace Renderer {

VulkanRenderer::VulkanRenderer() {
}

VulkanRenderer::~VulkanRenderer() {
    Shutdown();
}

bool VulkanRenderer::Initialize(Window* window) {
    m_Window = window;
    ENJIN_LOG_INFO(Renderer, "Initializing Vulkan renderer...");

    // Create Vulkan context
    m_Context = std::make_unique<VulkanContext>();
    if (!m_Context->Initialize()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to initialize Vulkan context");
        // VulkanContext::Initialize already shows a MessageBox for specific failures
        m_Context.reset();
        return false;
    }

    // Create surface
    if (!CreateSurface()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create Vulkan surface");
#ifdef _WIN32
        MessageBoxA(nullptr,
            "Failed to create a Vulkan rendering surface.\n\n"
            "This may indicate a GPU driver issue.\n\n"
            "Check 'enjin.log' for details.",
            "TEGE - Startup Error",
            MB_OK | MB_ICONERROR);
#endif
        return false;
    }

    // Find present queue family
    u32 presentQueueFamily = m_Context->FindPresentQueueFamily(m_Surface);
    if (presentQueueFamily == UINT32_MAX) {
        ENJIN_LOG_ERROR(Renderer, "No present queue family found");
#ifdef _WIN32
        MessageBoxA(nullptr,
            "Your GPU does not support presenting to a window.\n\n"
            "Please update your GPU drivers and try again.\n\n"
            "Check 'enjin.log' for details.",
            "TEGE - Startup Error",
            MB_OK | MB_ICONERROR);
#endif
        return false;
    }

    // Update present queue if different from graphics queue
    if (presentQueueFamily != m_Context->GetGraphicsQueueFamily()) {
        m_Context->SetPresentQueueFamily(presentQueueFamily);
    }

    // Create swapchain
    u32 width = m_Window->GetWidth();
    u32 height = m_Window->GetHeight();
    m_Swapchain = std::make_unique<VulkanSwapchain>(m_Context.get());
    if (!m_Swapchain->Initialize(m_Surface, width, height)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to initialize swapchain");
        return false;
    }

    // Create render pass
    if (!CreateRenderPass()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render pass");
        return false;
    }

    // Create framebuffers
    m_Swapchain->SetRenderPass(m_RenderPass);
    m_Swapchain->RecreateFramebuffers();

    // Create command pool and buffers
    if (!CreateCommandPool() || !CreateCommandBuffers()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create command buffers");
        return false;
    }

    // Create sync objects
    if (!CreateSyncObjects()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create sync objects");
        return false;
    }

    // Create async compute resources (optional — falls back to graphics queue)
    CreateComputeResources();

    // Register fence-based wait so subsystems use WaitForGPU() instead of vkDeviceWaitIdle
    m_Context->SetFenceWaitFunction([this]() { WaitForAllFrames(); });

    // Create abstract sub-managers for IRenderBackend interface
    m_BufferMgr = std::make_unique<VulkanBufferManager>(m_Context.get());
    m_TextureMgr = std::make_unique<VulkanTextureManager>(m_Context.get());
    m_ShaderMgr = std::make_unique<VulkanShaderManager>(m_Context.get());
    m_PipelineMgr = std::make_unique<VulkanPipelineManager>(m_Context.get(), m_ShaderMgr.get());
    m_PipelineMgr->SetRenderPass(m_RenderPass);
    m_PipelineMgr->SetMSAASamples(m_MSAASamples);
    m_BindGroupMgr = std::make_unique<VulkanBindGroupManager>(m_Context.get(), m_BufferMgr.get(), m_TextureMgr.get());

    ENJIN_LOG_INFO(Renderer, "Vulkan renderer initialized successfully");
    return true;
}

void VulkanRenderer::Shutdown() {
    if (m_Context) {
        // Clear fence-wait registration before destroying fences
        m_Context->SetFenceWaitFunction(nullptr);
    }
    if (m_Context && m_Context->GetDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_Context->GetDevice());
    }

    // Destroy abstract sub-managers before Vulkan resources
    m_ActiveEncoder.reset();
    m_BindGroupMgr.reset();
    m_PipelineMgr.reset();
    m_ShaderMgr.reset();
    m_TextureMgr.reset();
    m_BufferMgr.reset();

    DestroyComputeResources();
    DestroySyncObjects();

    if (m_CommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_Context->GetDevice(), m_CommandPool, nullptr);
        m_CommandPool = VK_NULL_HANDLE;
    }

    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_Context->GetDevice(), m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }

    m_Swapchain.reset();

    if (m_Surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_Context->GetInstance(), m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }

    m_Context.reset();
}

bool VulkanRenderer::CreateSurface() {
    GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(m_Window->GetNativeHandle());
    VkResult result = glfwCreateWindowSurface(
        m_Context->GetInstance(),
        glfwWindow,
        nullptr,
        &m_Surface
    );

    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create window surface: %d", result);
        return false;
    }

    return true;
}

bool VulkanRenderer::CreateRenderPass() {
    const bool msaaEnabled = m_MSAASamples > VK_SAMPLE_COUNT_1_BIT;

    if (!msaaEnabled) {
        // ---- Non-MSAA render pass (original path) ----
        // Attachment 0: Color (swapchain format)
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_Swapchain->GetImageFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Attachment 1: Velocity buffer (RG16F per-pixel motion vectors for TAA)
        VkAttachmentDescription velocityAttachment{};
        velocityAttachment.format = VulkanSwapchain::VELOCITY_FORMAT;
        velocityAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        velocityAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        velocityAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        velocityAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        velocityAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        velocityAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        velocityAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Attachment 2: Depth
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = m_Swapchain->GetDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Color attachment references (MRT: color + velocity)
        std::array<VkAttachmentReference, 2> colorAttachmentRefs{};
        colorAttachmentRefs[0].attachment = 0;
        colorAttachmentRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentRefs[1].attachment = 1;
        colorAttachmentRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 2;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<u32>(colorAttachmentRefs.size());
        subpass.pColorAttachments = colorAttachmentRefs.data();
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        // Prior frame's depth writes complete at LATE_FRAGMENT_TESTS and must
        // be made available (srcAccessMask) before this frame's implicit depth
        // layout transition — EARLY-only + srcAccess 0 was a WRITE_AFTER_WRITE
        // hazard on the depth attachment (sync-validation probe, 2026-08-04).
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, velocityAttachment, depthAttachment };

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VkResult result = vkCreateRenderPass(m_Context->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass);
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create render pass: %d", result);
            return false;
        }

        ENJIN_LOG_INFO(Renderer, "Render pass created with velocity + depth attachments");
    } else {
        // ---- MSAA render pass ----
        // Attachment layout:
        //   0: MSAA color (multisampled, rendered into)
        //   1: MSAA velocity (multisampled, rendered into)
        //   2: MSAA depth (multisampled, rendered into)
        //   3: Resolve color (single-sample swapchain image)
        //   4: Resolve velocity (single-sample, for post-processing readback)

        // Attachment 0: MSAA color
        VkAttachmentDescription msaaColor{};
        msaaColor.format = m_Swapchain->GetImageFormat();
        msaaColor.samples = m_MSAASamples;
        msaaColor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        msaaColor.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Resolved, not stored directly
        msaaColor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        msaaColor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        msaaColor.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        msaaColor.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Attachment 1: MSAA velocity
        VkAttachmentDescription msaaVelocity{};
        msaaVelocity.format = VulkanSwapchain::VELOCITY_FORMAT;
        msaaVelocity.samples = m_MSAASamples;
        msaaVelocity.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        msaaVelocity.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        msaaVelocity.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        msaaVelocity.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        msaaVelocity.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        msaaVelocity.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Attachment 2: MSAA depth
        VkAttachmentDescription msaaDepth{};
        msaaDepth.format = m_Swapchain->GetDepthFormat();
        msaaDepth.samples = m_MSAASamples;
        msaaDepth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        msaaDepth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        msaaDepth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        msaaDepth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        msaaDepth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        msaaDepth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Attachment 3: Resolve color (single-sample swapchain image)
        VkAttachmentDescription resolveColor{};
        resolveColor.format = m_Swapchain->GetImageFormat();
        resolveColor.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveColor.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveColor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveColor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveColor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveColor.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveColor.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Attachment 4: Resolve velocity (single-sample, for post-processing)
        VkAttachmentDescription resolveVelocity{};
        resolveVelocity.format = VulkanSwapchain::VELOCITY_FORMAT;
        resolveVelocity.samples = VK_SAMPLE_COUNT_1_BIT;
        resolveVelocity.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveVelocity.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        resolveVelocity.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolveVelocity.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolveVelocity.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolveVelocity.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Subpass: render to MSAA attachments, auto-resolve to single-sample targets
        std::array<VkAttachmentReference, 2> colorRefs{};
        colorRefs[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };  // MSAA color
        colorRefs[1] = { 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };  // MSAA velocity

        VkAttachmentReference depthRef = { 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

        std::array<VkAttachmentReference, 2> resolveRefs{};
        resolveRefs[0] = { 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };  // Resolve color
        resolveRefs[1] = { 4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };  // Resolve velocity

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<u32>(colorRefs.size());
        subpass.pColorAttachments = colorRefs.data();
        subpass.pDepthStencilAttachment = &depthRef;
        subpass.pResolveAttachments = resolveRefs.data();

        // Same depth WAW fix as the non-MSAA pass above (LATE stage + srcAccess)
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 5> attachments = {
            msaaColor, msaaVelocity, msaaDepth, resolveColor, resolveVelocity
        };

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VkResult result = vkCreateRenderPass(m_Context->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass);
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create MSAA render pass: %d", result);
            return false;
        }

        ENJIN_LOG_INFO(Renderer, "MSAA render pass created: %dx samples with color + velocity resolve",
                       static_cast<int>(m_MSAASamples));
    }

    return true;
}

bool VulkanRenderer::CreateCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult result = vkCreateCommandPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_CommandPool);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create command pool: %d", result);
        return false;
    }

    return true;
}

bool VulkanRenderer::CreateCommandBuffers() {
    m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<u32>(m_CommandBuffers.size());

    VkResult result = vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, m_CommandBuffers.data());
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate command buffers: %d", result);
        return false;
    }

    return true;
}

bool VulkanRenderer::CreateSyncObjects() {
    // The render-finished semaphore must be PER SWAPCHAIN IMAGE, not per frame in
    // flight: the present operation keeps waiting on it until the image is actually
    // shown, so a per-frame semaphore can be re-signaled while a present that used
    // it is still pending (validation vkQueueSubmit-pSignalSemaphores-00067). The
    // image-in-flight fence wait already guarantees the prior present of a given
    // image index has completed before we reuse that index's semaphore.
    const u32 imageCount = m_Swapchain->GetImageCount();
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(imageCount);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_ImagesInFlight.resize(imageCount, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_Context->GetDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create sync objects for frame %zu", i);
            return false;
        }
    }
    for (usize i = 0; i < imageCount; ++i) {
        if (vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create render-finished semaphore %zu", i);
            return false;
        }
    }

    // Create GPU timestamp query pools (one per frame in flight)
    if (m_Context->GetTimestampPeriod() > 0.0f) {
        VkQueryPoolCreateInfo qpInfo{};
        qpInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qpInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpInfo.queryCount = GPU_TS_COUNT;
        for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateQueryPool(m_Context->GetDevice(), &qpInfo, nullptr, &m_TimestampPools[i]) != VK_SUCCESS) {
                ENJIN_LOG_WARN(Renderer, "Failed to create timestamp query pool %u — GPU timing disabled", i);
                m_TimestampPools[i] = VK_NULL_HANDLE;
            }
        }
    }

    return true;
}

void VulkanRenderer::DestroySyncObjects() {
    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (m_TimestampPools[i] != VK_NULL_HANDLE) {
            vkDestroyQueryPool(m_Context->GetDevice(), m_TimestampPools[i], nullptr);
            m_TimestampPools[i] = VK_NULL_HANDLE;
        }
        if (m_ImageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_Context->GetDevice(), m_ImageAvailableSemaphores[i], nullptr);
        }
        if (m_InFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(m_Context->GetDevice(), m_InFlightFences[i], nullptr);
        }
    }
    // Render-finished semaphores are per swapchain image, so their count differs
    // from MAX_FRAMES_IN_FLIGHT.
    for (auto& sem : m_RenderFinishedSemaphores) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_Context->GetDevice(), sem, nullptr);
        }
    }
}

bool VulkanRenderer::AcquireNextImage() {
    auto t0 = std::chrono::high_resolution_clock::now();
    vkWaitForFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);
    auto t1 = std::chrono::high_resolution_clock::now();
    m_FenceWaitMs = m_FenceWaitMs * 0.9f + std::chrono::duration<f32, std::milli>(t1 - t0).count() * 0.1f;

    // Proactive resize: callback set the flag before this frame
    if (m_FramebufferResized) {
        m_FramebufferResized = false;
        OnWindowResize(m_Window->GetWidth(), m_Window->GetHeight());
        return false;
    }

    VkResult result = vkAcquireNextImageKHR(
        m_Context->GetDevice(),
        m_Swapchain->GetSwapchain(),
        UINT64_MAX,
        m_ImageAvailableSemaphores[m_CurrentFrame],
        VK_NULL_HANDLE,
        &m_CurrentImageIndex
    );
    auto t2 = std::chrono::high_resolution_clock::now();
    m_AcquireMs = m_AcquireMs * 0.9f + std::chrono::duration<f32, std::milli>(t2 - t1).count() * 0.1f;

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        OnWindowResize(m_Window->GetWidth(), m_Window->GetHeight());
        return false;
    } else if (result == VK_ERROR_DEVICE_LOST) {
        ENJIN_LOG_FATAL(Renderer, "Vulkan device lost during image acquisition");
        m_DeviceLost = true;
        return false;
    } else if (result == VK_ERROR_SURFACE_LOST_KHR) {
        ENJIN_LOG_FATAL(Renderer, "Vulkan surface lost (recovery requires surface recreation)");
        m_DeviceLost = true;
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        ENJIN_LOG_ERROR(Renderer, "Failed to acquire swapchain image: %d", result);
        return false;
    }

    // Bounds check after potential swapchain image count change
    if (m_CurrentImageIndex >= static_cast<u32>(m_ImagesInFlight.size())) {
        ENJIN_LOG_WARN(Renderer, "Image index %u out of bounds, triggering resize", m_CurrentImageIndex);
        OnWindowResize(m_Window->GetWidth(), m_Window->GetHeight());
        return false;
    }

    // Check if a previous frame is using this image
    if (m_ImagesInFlight[m_CurrentImageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(m_Context->GetDevice(), 1, &m_ImagesInFlight[m_CurrentImageIndex], VK_TRUE, UINT64_MAX);
    }
    m_ImagesInFlight[m_CurrentImageIndex] = m_InFlightFences[m_CurrentFrame];

    return true;
}

void VulkanRenderer::SubmitCommandBuffer() {
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Wait on image available, compute semaphore (if submitted), and any external semaphores.
    // Stack-allocated arrays sized for typical usage (image + compute + up to 6 external).
    constexpr u32 MAX_WAIT_SEMAPHORES = 8;
    VkSemaphore waitSemaphores[MAX_WAIT_SEMAPHORES];
    VkPipelineStageFlags waitStages[MAX_WAIT_SEMAPHORES];

    waitSemaphores[0] = m_ImageAvailableSemaphores[m_CurrentFrame];
    waitStages[0] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    u32 waitCount = 1;

    if (m_ComputeSubmittedThisFrame) {
        waitSemaphores[waitCount] = m_ComputeFinishedSemaphores[m_CurrentFrame];
        waitStages[waitCount] = VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        waitCount++;
        m_ComputeSubmittedThisFrame = false;
    }

    // Append external wait semaphores (from AsyncComputeScheduler or other systems)
    for (const auto& ext : m_ExternalWaitSemaphores) {
        if (waitCount >= MAX_WAIT_SEMAPHORES) break;
        waitSemaphores[waitCount] = ext.semaphore;
        waitStages[waitCount] = ext.waitStage;
        waitCount++;
    }
    m_ExternalWaitSemaphores.clear();

    submitInfo.waitSemaphoreCount = waitCount;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

    // Signal the per-image render-finished semaphore (present waits on it until the
    // image is shown). Indexed by image, not frame in flight (validation 00067).
    VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentImageIndex] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);

    VkResult submitResult = vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]);
    if (submitResult == VK_ERROR_DEVICE_LOST) {
        ENJIN_LOG_FATAL(Renderer, "Vulkan device lost during queue submit");
        m_DeviceLost = true;
        return;
    } else if (submitResult != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to submit draw command buffer: %d", submitResult);
        // Submit failed but fence was already reset - must not leave it unsignaled
        // or vkWaitForFences will deadlock on the next use of this frame slot
        m_DeviceLost = true;
        return;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapchains[] = { m_Swapchain->GetSwapchain() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &m_CurrentImageIndex;

    VkResult result = vkQueuePresentKHR(m_Context->GetPresentQueue(), &presentInfo);
    if (result == VK_ERROR_DEVICE_LOST) {
        ENJIN_LOG_FATAL(Renderer, "Vulkan device lost during present");
        m_DeviceLost = true;
    } else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized) {
        m_FramebufferResized = false;
        OnWindowResize(m_Window->GetWidth(), m_Window->GetHeight());
    } else if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to present swapchain image: %d", result);
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool VulkanRenderer::BeginFrameVulkan() {
    // Skip rendering if device is lost
    if (m_DeviceLost) return false;

    // Skip rendering while window is minimized (0x0 framebuffer)
    if (m_Window->GetWidth() == 0 || m_Window->GetHeight() == 0) {
        return false;
    }

    if (m_IsFrameStarted) {
        ENJIN_LOG_WARN(Renderer, "BeginFrame called while frame already in progress");
        return false;
    }

    if (!AcquireNextImage()) {
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to begin recording command buffer");
        return false;
    }

    // Reset timestamp query pool for this frame (must happen after command buffer begin)
    if (m_TimestampPools[m_CurrentFrame] != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(m_CommandBuffers[m_CurrentFrame], m_TimestampPools[m_CurrentFrame], 0, GPU_TS_COUNT);
    }

    m_IsFrameStarted = true;
    m_IsMainRenderPassActive = false;

    // Note: Main render pass is NOT started here anymore.
    // Call BeginMainRenderPass() after any pre-render passes (shadow, etc.)
    return true;
}

void VulkanRenderer::BeginMainRenderPass() {
    if (!m_IsFrameStarted) {
        ENJIN_LOG_WARN(Renderer, "BeginMainRenderPass called without BeginFrame");
        return;
    }

    if (m_IsMainRenderPassActive) {
        ENJIN_LOG_WARN(Renderer, "BeginMainRenderPass called while already active");
        return;
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_RenderPass;
    renderPassInfo.framebuffer = m_Swapchain->GetFramebuffer(m_CurrentImageIndex);
    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = m_Swapchain->GetExtent();

    // Clear values must match render pass attachment count and order.
    // Non-MSAA: [0]=color, [1]=velocity, [2]=depth  (3 attachments)
    // MSAA:     [0]=MSAAcolor, [1]=MSAAvelocity, [2]=MSAAdepth, [3]=resolveColor, [4]=resolveVelocity (5 attachments)
    const bool msaaActive = m_MSAASamples > VK_SAMPLE_COUNT_1_BIT;
    std::array<VkClearValue, 5> clearValues{};
    clearValues[0].color = { { 0.06f, 0.06f, 0.06f, 1.0f } };  // Near-black — blends with ImGui theme backgrounds
    clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };     // Velocity: zero motion
    clearValues[2].depthStencil = { 1.0f, 0 };
    if (msaaActive) {
        clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // Resolve color (don't-care but spec requires entry)
        clearValues[4].color = { { 0.0f, 0.0f, 0.0f, 0.0f } }; // Resolve velocity
    }

    renderPassInfo.clearValueCount = msaaActive ? 5u : 3u;
    renderPassInfo.pClearValues = clearValues.data();

    // Attach VRS shading rate image when available (VK_KHR_fragment_shading_rate)
#ifdef ENJIN_VRS
    VkRenderingFragmentShadingRateAttachmentInfoKHR vrsAttachment{};
    if (m_ShadingRateImageView != VK_NULL_HANDLE) {
        vrsAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR;
        vrsAttachment.imageView = m_ShadingRateImageView;
        vrsAttachment.imageLayout = VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR;
        vrsAttachment.shadingRateAttachmentTexelSize = m_ShadingRateTileSize;
        renderPassInfo.pNext = &vrsAttachment;
    }
#endif

    vkCmdBeginRenderPass(m_CommandBuffers[m_CurrentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    m_IsMainRenderPassActive = true;
}

void VulkanRenderer::EndFrame() {
    if (!m_IsFrameStarted) {
        ENJIN_LOG_WARN(Renderer, "EndFrame called without matching BeginFrame");
        return;
    }

    if (m_IsMainRenderPassActive) {
        vkCmdEndRenderPass(m_CommandBuffers[m_CurrentFrame]);
        m_IsMainRenderPassActive = false;
    }

    if (vkEndCommandBuffer(m_CommandBuffers[m_CurrentFrame]) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to record command buffer");
        return;
    }

    SubmitCommandBuffer();
    m_IsFrameStarted = false;

    // Read previous frame's GPU timestamp results (fence already waited in AcquireNextImage)
    {
        u32 prevFrame = (m_CurrentFrame + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT;
        if (m_TimestampPools[prevFrame] != VK_NULL_HANDLE) {
            VkResult tsResult = vkGetQueryPoolResults(
                m_Context->GetDevice(), m_TimestampPools[prevFrame], 0, GPU_TS_COUNT,
                sizeof(m_TimestampResults), m_TimestampResults, sizeof(u64),
                VK_QUERY_RESULT_64_BIT);
            if (tsResult == VK_SUCCESS) {
                f32 period = m_Context->GetTimestampPeriod();
                for (u32 i = 0; i < 4; ++i) {
                    u64 begin = m_TimestampResults[i * 2];
                    u64 end = m_TimestampResults[i * 2 + 1];
                    m_GPUPassTimes[i] = (end > begin) ? static_cast<f32>(end - begin) * period / 1e6f : 0.0f;
                }
            }
            // VK_NOT_READY is expected for the first few frames — silently ignore
        }
    }

    // Process deferred VSync change (safe: frame fully submitted+presented)
    if (m_VSyncChangeRequested) {
        m_VSyncChangeRequested = false;
        if (m_Swapchain) {
            m_Swapchain->SetVSyncEnabled(m_VSyncDesiredState);
        }
    }
}

VkCommandBuffer VulkanRenderer::GetCurrentCommandBuffer() const {
    if (m_CurrentFrame >= m_CommandBuffers.size()) return VK_NULL_HANDLE;
    return m_CommandBuffers[m_CurrentFrame];
}

void VulkanRenderer::WaitForAllFrames() {
    if (!m_Context || m_Context->GetDevice() == VK_NULL_HANDLE) return;
    if (m_InFlightFences.empty()) return;
    vkWaitForFences(m_Context->GetDevice(),
                    static_cast<u32>(m_InFlightFences.size()),
                    m_InFlightFences.data(),
                    VK_TRUE, UINT64_MAX);
}

void VulkanRenderer::OnWindowResize(u32 width, u32 height) {
    if (width == 0 || height == 0) {
        // Window is minimized; mark for resize when restored
        m_FramebufferResized = true;
        return;
    }

    WaitForAllFrames();

    // Reset frame state - any in-progress frame is now invalid
    m_IsFrameStarted = false;
    m_IsMainRenderPassActive = false;

    // Recreate swapchain (Recreate() handles framebuffers internally)
    m_Swapchain->Recreate(width, height, true);

    // Resize images-in-flight tracking to match new swapchain image count
    m_ImagesInFlight.assign(m_Swapchain->GetImageCount(), VK_NULL_HANDLE);

    // Command buffers do not depend on swapchain dimensions — reuse existing ones
    // (they are reset implicitly on vkBeginCommandBuffer each frame)

    m_FramebufferResized = false;

    // Notify external systems (post-processing, etc.)
    for (auto& callback : m_ResizeCallbacks) {
        callback(width, height);
    }

    ENJIN_LOG_INFO(Renderer, "Window resized to %ux%u", width, height);
}

bool VulkanRenderer::CreateComputeResources() {
    if (!m_Context->HasDedicatedComputeQueue()) {
        ENJIN_LOG_INFO(Renderer, "No dedicated compute queue — compute work uses graphics queue");
        return false;
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_Context->GetComputeQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_Context->GetDevice(), &poolInfo, nullptr, &m_ComputeCommandPool) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to create compute command pool");
        return false;
    }

    m_ComputeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_ComputeCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    if (vkAllocateCommandBuffers(m_Context->GetDevice(), &allocInfo, m_ComputeCommandBuffers.data()) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to allocate compute command buffers");
        vkDestroyCommandPool(m_Context->GetDevice(), m_ComputeCommandPool, nullptr);
        m_ComputeCommandPool = VK_NULL_HANDLE;
        return false;
    }

    m_ComputeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_Context->GetDevice(), &semInfo, nullptr, &m_ComputeFinishedSemaphores[i]) != VK_SUCCESS) {
            ENJIN_LOG_WARN(Renderer, "Failed to create compute semaphore %u", i);
            DestroyComputeResources();
            return false;
        }
    }

    // Graphics→Compute semaphores (allows compute to wait on graphics)
    m_GraphicsToComputeSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_Context->GetDevice(), &semInfo, nullptr, &m_GraphicsToComputeSemaphores[i]) != VK_SUCCESS) {
            ENJIN_LOG_WARN(Renderer, "Failed to create graphics→compute semaphore %u", i);
            DestroyComputeResources();
            return false;
        }
    }

    ENJIN_LOG_INFO(Renderer, "Async compute resources initialized (queue family %u)",
                   m_Context->GetComputeQueueFamily());
    return true;
}

void VulkanRenderer::DestroyComputeResources() {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();
    for (auto sem : m_ComputeFinishedSemaphores) {
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(device, sem, nullptr);
    }
    m_ComputeFinishedSemaphores.clear();
    for (auto sem : m_GraphicsToComputeSemaphores) {
        if (sem != VK_NULL_HANDLE) vkDestroySemaphore(device, sem, nullptr);
    }
    m_GraphicsToComputeSemaphores.clear();
    m_ComputeCommandBuffers.clear();
    if (m_ComputeCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, m_ComputeCommandPool, nullptr);
        m_ComputeCommandPool = VK_NULL_HANDLE;
    }
}

VkCommandBuffer VulkanRenderer::GetCurrentComputeCommandBuffer() const {
    if (m_ComputeCommandBuffers.empty() || m_CurrentFrame >= m_ComputeCommandBuffers.size()) return VK_NULL_HANDLE;
    return m_ComputeCommandBuffers[m_CurrentFrame];
}

bool VulkanRenderer::BeginComputeCommandBuffer() {
    if (m_ComputeCommandPool == VK_NULL_HANDLE) return false;
    if (m_CurrentFrame >= m_ComputeCommandBuffers.size()) return false;
    VkCommandBuffer cmd = m_ComputeCommandBuffers[m_CurrentFrame];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) return false;
    m_ComputeRecording = true;
    return true;
}

void VulkanRenderer::EndComputeCommandBuffer() {
    if (!m_ComputeRecording) return;
    vkEndCommandBuffer(m_ComputeCommandBuffers[m_CurrentFrame]);
    m_ComputeRecording = false;
}

void VulkanRenderer::SubmitCompute() {
    if (m_ComputeCommandPool == VK_NULL_HANDLE) return;
    VkCommandBuffer cmd = m_ComputeCommandBuffers[m_CurrentFrame];

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_ComputeFinishedSemaphores[m_CurrentFrame];

    // If graphics signaled us, wait on that semaphore before compute begins
    VkSemaphore waitSem = VK_NULL_HANDLE;
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    if (m_GraphicsToComputeSignaled) {
        waitSem = m_GraphicsToComputeSemaphores[m_CurrentFrame];
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &waitSem;
        submitInfo.pWaitDstStageMask = &waitStage;
        m_GraphicsToComputeSignaled = false;
    }

    VkQueue computeQueue = m_Context->GetComputeQueue();
    VkResult submitResult = vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (submitResult == VK_ERROR_DEVICE_LOST) {
        ENJIN_LOG_FATAL(Renderer, "Vulkan device lost during compute submit");
        m_DeviceLost = true;
        return;
    } else if (submitResult != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to submit compute command buffer: %d", submitResult);
        return;
    }
    m_ComputeSubmittedThisFrame = true;
}

void VulkanRenderer::InsertComputeToGraphicsBarrier(VkCommandBuffer graphicsCmd, VkPipelineStageFlags dstStage) {
    // This is a memory barrier on the graphics command buffer.
    // The actual semaphore wait happens at SubmitCommandBuffer() time.
    // This barrier ensures proper memory visibility for compute→graphics data.
    if (!m_ComputeSubmittedThisFrame) return;

    VkMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    vkCmdPipelineBarrier(graphicsCmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, dstStage,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void VulkanRenderer::SignalGraphicsToCompute(VkCommandBuffer graphicsCmd) {
    // This method is called to signal the graphics→compute semaphore.
    // The semaphore is signaled during graphics queue submit (added to signal list).
    // For now, we set a flag that SubmitCompute will check.
    (void)graphicsCmd;
    m_GraphicsToComputeSignaled = true;
}

void VulkanRenderer::AddExternalWaitSemaphore(VkSemaphore semaphore, VkPipelineStageFlags waitStage) {
    if (semaphore == VK_NULL_HANDLE) return;
    m_ExternalWaitSemaphores.push_back({ semaphore, waitStage });
}

bool VulkanRenderer::SetMSAASamples(VkSampleCountFlagBits samples) {
    if (samples == m_MSAASamples) return true;

    // Validate that the hardware supports the requested sample count
    if (samples > VK_SAMPLE_COUNT_1_BIT) {
        VkSampleCountFlagBits maxSamples = m_Context->GetMaxUsableSampleCount();
        if (samples > maxSamples) {
            ENJIN_LOG_WARN(Renderer, "MSAA %dx requested but hardware only supports up to %dx",
                           static_cast<int>(samples), static_cast<int>(maxSamples));
            return false;
        }
    }

    WaitForAllFrames();
    m_IsFrameStarted = false;
    m_IsMainRenderPassActive = false;

    m_MSAASamples = samples;

    // Propagate to swapchain so it creates/destroys MSAA images on recreation
    m_Swapchain->SetMSAASamples(samples);

    // Clear render pass before framebuffer recreation
    m_Swapchain->SetRenderPass(VK_NULL_HANDLE);

    // Recreate MSAA images (destroy old ones, create new if samples > 1)
    auto extent = m_Swapchain->GetExtent();
    if (!m_Swapchain->Recreate(extent.width, extent.height, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to recreate swapchain for MSAA change");
        m_MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        m_Swapchain->SetMSAASamples(VK_SAMPLE_COUNT_1_BIT);
        m_Swapchain->Recreate(extent.width, extent.height, true);
        RecreateRenderPass();
        m_ImagesInFlight.assign(m_Swapchain->GetImageCount(), VK_NULL_HANDLE);
        return false;
    }

    // Recreate render pass with new sample count, then rebuild framebuffers
    RecreateRenderPass();

    m_ImagesInFlight.assign(m_Swapchain->GetImageCount(), VK_NULL_HANDLE);

    // Reset frame state so the next BeginFrame starts clean
    m_CurrentFrame = 0;
    m_IsFrameStarted = false;
    m_IsMainRenderPassActive = false;

    // Notify external systems (post-processing, RenderSystem pipelines, etc.)
    for (auto& callback : m_ResizeCallbacks) {
        if (callback) callback(extent.width, extent.height);
    }

    ENJIN_LOG_INFO(Renderer, "MSAA %s (%dx samples)",
                   samples > VK_SAMPLE_COUNT_1_BIT ? "enabled" : "disabled",
                   static_cast<int>(samples));
    return true;
}

void VulkanRenderer::RequestVSyncChange(bool enabled) {
    m_VSyncChangeRequested = true;
    m_VSyncDesiredState = enabled;
}

bool VulkanRenderer::IsVSyncEnabled() const {
    return m_Swapchain ? m_Swapchain->IsVSyncEnabled() : false;
}

bool VulkanRenderer::RecreateRenderPass() {
    if (m_RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_Context->GetDevice(), m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }
    if (!CreateRenderPass()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to recreate render pass");
        return false;
    }
    // Framebuffers reference the render pass — rebuild them
    m_Swapchain->SetRenderPass(m_RenderPass);
    m_Swapchain->RecreateFramebuffers();
    return true;
}

void VulkanRenderer::SetHDREnabled(bool enabled) {
    if (!m_Swapchain) return;
    if (m_Swapchain->IsHDREnabled() == enabled) return;

    // Check HDR format availability before attempting the switch
    if (enabled && !m_Swapchain->IsHDRFormatAvailable()) {
        ENJIN_LOG_WARN(Renderer, "HDR requested but no HDR surface format available (enable Windows HDR in Display Settings)");
        return;
    }

    WaitForAllFrames();
    m_IsFrameStarted = false;
    m_IsMainRenderPassActive = false;

    // Save old format for rollback on failure
    VkFormat oldFormat = m_Swapchain->GetImageFormat();

    // Clear render pass BEFORE recreating swapchain: Recreate() would otherwise
    // build framebuffers against the OLD render pass whose attachment format no
    // longer matches the new swapchain image format (SRGB vs FP16/HDR10).
    m_Swapchain->SetRenderPass(VK_NULL_HANDLE);

    // Update swapchain HDR preference and recreate with new format
    m_Swapchain->SetHDREnabled(enabled);
    auto extent = m_Swapchain->GetExtent();
    if (!m_Swapchain->Recreate(extent.width, extent.height, true)) {
        // Swapchain recreation failed — revert HDR flag and try to recover
        ENJIN_LOG_ERROR(Renderer, "HDR swapchain recreation failed, reverting to previous mode");
        m_Swapchain->SetHDREnabled(!enabled);
        m_Swapchain->Recreate(extent.width, extent.height, true);
        RecreateRenderPass();
        m_ImagesInFlight.assign(m_Swapchain->GetImageCount(), VK_NULL_HANDLE);
        return;
    }

    // Render pass attachment 0 format must match the new swapchain format.
    // This also sets the new render pass on the swapchain and rebuilds framebuffers.
    RecreateRenderPass();

    // Reset image tracking
    m_ImagesInFlight.assign(m_Swapchain->GetImageCount(), VK_NULL_HANDLE);

    // Notify external systems (post-processing, RenderSystem pipelines, etc.)
    for (auto& callback : m_ResizeCallbacks) {
        callback(extent.width, extent.height);
    }

    const char* modeNames[] = { "SDR", "scRGB", "HDR10" };
    u32 mode = m_Swapchain->GetHDROutputMode();
    ENJIN_LOG_INFO(Renderer, "HDR output %s (mode: %s)",
        enabled ? "enabled" : "disabled",
        mode < 3 ? modeNames[mode] : "Unknown");
}

bool VulkanRenderer::IsHDREnabled() const {
    return m_Swapchain ? m_Swapchain->IsHDREnabled() : false;
}

u32 VulkanRenderer::GetHDROutputMode() const {
    return m_Swapchain ? m_Swapchain->GetHDROutputMode() : 0;
}

// ============================================================================
// IRenderBackend overrides
// ============================================================================

bool VulkanRenderer::Initialize(u32 /*width*/, u32 /*height*/) {
    // Use Initialize(Window*) for Vulkan. This override exists only to satisfy the interface.
    ENJIN_LOG_ERROR(Renderer, "VulkanRenderer::Initialize(u32,u32) is not supported — use Initialize(Window*)");
    return false;
}

void VulkanRenderer::BeginFrame() {
    BeginFrameVulkan();
}

void VulkanRenderer::Present() {
    // Vulkan presents inside EndFrame/SubmitCommandBuffer — no-op here
}

void VulkanRenderer::Resize(u32 width, u32 height) {
    OnWindowResize(width, height);
}

PlatformCapabilities VulkanRenderer::GetCapabilities() const {
    PlatformCapabilities caps;
    caps.hasVulkan = true;
    caps.maxTextureSize = 16384;
    caps.preferredCompression = TextureCompression::BC7;
    return caps;
}

GPUCapabilities VulkanRenderer::GetGPUCapabilities() const {
    return MakeVulkanCapabilities();
}

u32 VulkanRenderer::GetSwapchainWidth() const {
    auto ext = GetSwapchainExtent();
    return ext.width;
}

u32 VulkanRenderer::GetSwapchainHeight() const {
    auto ext = GetSwapchainExtent();
    return ext.height;
}

IGPUBufferManager* VulkanRenderer::GetBufferManager() { return m_BufferMgr.get(); }
IGPUTextureManager* VulkanRenderer::GetTextureManager() { return m_TextureMgr.get(); }
IGPUPipelineManager* VulkanRenderer::GetPipelineManager() { return m_PipelineMgr.get(); }
IGPUShaderManager* VulkanRenderer::GetShaderManager() { return m_ShaderMgr.get(); }
IGPUBindGroupManager* VulkanRenderer::GetBindGroupManager() { return m_BindGroupMgr.get(); }

IRenderEncoder* VulkanRenderer::BeginRenderPass(const GPURenderPassDesc& desc) {
    // Default desc (width=0) = main swapchain render pass
    if (desc.width == 0 && desc.height == 0) {
        if (!m_IsMainRenderPassActive) {
            BeginMainRenderPass();
        }
        m_ActiveEncoder = std::make_unique<VulkanRenderEncoder>(
            GetCurrentCommandBuffer(), VK_NULL_HANDLE,
            m_PipelineMgr.get(), m_BufferMgr.get(), m_BindGroupMgr.get());
        return m_ActiveEncoder.get();
    }
    // Custom render passes not yet supported through abstract interface
    return nullptr;
}

void VulkanRenderer::EndRenderPass(IRenderEncoder* encoder) {
    // The Vulkan main render pass is ended in EndFrame, not here.
    // Just release the encoder.
    if (m_ActiveEncoder.get() == encoder) {
        m_ActiveEncoder.reset();
    }
}

} // namespace Renderer
} // namespace Enjin
