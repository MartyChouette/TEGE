#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <array>

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
        return false;
    }

    // Create surface
    if (!CreateSurface()) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create Vulkan surface");
        return false;
    }

    // Find present queue family
    u32 presentQueueFamily = m_Context->FindPresentQueueFamily(m_Surface);
    if (presentQueueFamily == UINT32_MAX) {
        ENJIN_LOG_ERROR(Renderer, "No present queue family found");
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

    ENJIN_LOG_INFO(Renderer, "Vulkan renderer initialized successfully");
    return true;
}

void VulkanRenderer::Shutdown() {
    if (m_Context && m_Context->GetDevice() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_Context->GetDevice());
    }

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
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_Swapchain->GetImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_Swapchain->GetDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

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

    ENJIN_LOG_INFO(Renderer, "Render pass created with depth attachment");
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
    m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    m_ImagesInFlight.resize(m_Swapchain->GetImageCount(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_Context->GetDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_Context->GetDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create sync objects for frame %zu", i);
            return false;
        }
    }

    return true;
}

void VulkanRenderer::DestroySyncObjects() {
    for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (m_ImageAvailableSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_Context->GetDevice(), m_ImageAvailableSemaphores[i], nullptr);
        }
        if (m_RenderFinishedSemaphores[i] != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_Context->GetDevice(), m_RenderFinishedSemaphores[i], nullptr);
        }
        if (m_InFlightFences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(m_Context->GetDevice(), m_InFlightFences[i], nullptr);
        }
    }
}

bool VulkanRenderer::AcquireNextImage() {
    vkWaitForFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

    VkResult result = vkAcquireNextImageKHR(
        m_Context->GetDevice(),
        m_Swapchain->GetSwapchain(),
        UINT64_MAX,
        m_ImageAvailableSemaphores[m_CurrentFrame],
        VK_NULL_HANDLE,
        &m_CurrentImageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain needs to be recreated
        OnWindowResize(m_Window->GetWidth(), m_Window->GetHeight());
        return false;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        ENJIN_LOG_ERROR(Renderer, "Failed to acquire swapchain image: %d", result);
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

    VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

    VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(m_Context->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);

    if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to submit draw command buffer");
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
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        OnWindowResize(m_Window->GetWidth(), m_Window->GetHeight());
    } else if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to present swapchain image: %d", result);
    }

    m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool VulkanRenderer::BeginFrame() {
    if (m_IsFrameStarted) {
        ENJIN_LOG_WARN(Renderer, "BeginFrame called while frame already in progress");
        return false;
    }

    if (!AcquireNextImage()) {
        ENJIN_LOG_WARN(Renderer, "BeginFrame: AcquireNextImage failed");
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(m_CommandBuffers[m_CurrentFrame], &beginInfo) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to begin recording command buffer");
        return false;
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

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { { 0.1f, 0.1f, 0.2f, 1.0f } };  // Dark blue to confirm clearing works
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassInfo.clearValueCount = static_cast<u32>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

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
}

VkCommandBuffer VulkanRenderer::GetCurrentCommandBuffer() const {
    return m_CommandBuffers[m_CurrentFrame];
}

void VulkanRenderer::OnWindowResize(u32 width, u32 height) {
    if (width == 0 || height == 0) {
        return;
    }

    vkDeviceWaitIdle(m_Context->GetDevice());

    m_Swapchain->Recreate(width, height);
    m_Swapchain->RecreateFramebuffers();

    // Recreate command buffers if needed
    vkFreeCommandBuffers(m_Context->GetDevice(), m_CommandPool,
        static_cast<u32>(m_CommandBuffers.size()), m_CommandBuffers.data());
    CreateCommandBuffers();
}

} // namespace Renderer
} // namespace Enjin
