#include "Enjin/GUI/ImGuiLayer.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Enjin {
namespace GUI {

ImGuiLayer::~ImGuiLayer() {
    Shutdown();
}

bool ImGuiLayer::Initialize(Window* window, Renderer::VulkanRenderer* renderer) {
    if (m_Initialized) {
        return true;
    }

    if (!window || !renderer) {
        ENJIN_LOG_ERROR(Editor, "ImGuiLayer::Initialize called with null window or renderer");
        return false;
    }

    m_Renderer = renderer;
    Renderer::VulkanContext* context = renderer->GetContext();

    // Create descriptor pool for ImGui
    if (!CreateDescriptorPool()) {
        ENJIN_LOG_ERROR(Editor, "Failed to create ImGui descriptor pool");
        return false;
    }

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup custom Enjin style
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // Rounding
    style.WindowRounding = 6.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    // Padding and spacing
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 20.0f;

    // Borders
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    // Custom colors (dark blue-gray theme)
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_ChildBg]              = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg]              = ImVec4(0.12f, 0.12f, 0.15f, 0.95f);
    colors[ImGuiCol_Border]               = ImVec4(0.25f, 0.25f, 0.30f, 0.60f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_MenuBarBg]            = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.35f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.45f, 0.65f, 0.95f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.40f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.50f, 0.65f, 0.95f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.22f, 0.25f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.30f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.35f, 0.42f, 0.55f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.22f, 0.25f, 0.32f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.30f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.35f, 0.42f, 0.55f, 1.00f);
    colors[ImGuiCol_Separator]            = ImVec4(0.25f, 0.25f, 0.30f, 0.60f);
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.35f, 0.45f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.40f, 0.50f, 0.70f, 0.70f);
    colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.45f, 0.60f, 0.90f, 0.90f);
    colors[ImGuiCol_Tab]                  = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.35f, 0.42f, 0.55f, 1.00f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.25f, 0.30f, 0.40f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.35f, 0.45f, 0.65f, 0.50f);
    colors[ImGuiCol_DragDropTarget]       = ImVec4(0.45f, 0.65f, 0.95f, 0.90f);

    // Initialize GLFW backend
    GLFWwindow* glfwWindow = static_cast<GLFWwindow*>(window->GetNativeHandle());
    ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);

    // Initialize Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = context->GetInstance();
    initInfo.PhysicalDevice = context->GetPhysicalDevice();
    initInfo.Device = context->GetDevice();
    initInfo.QueueFamily = context->GetGraphicsQueueFamily();
    initInfo.Queue = context->GetGraphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_DescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = 2;

    // Set up pipeline info (new API structure)
    initInfo.PipelineInfoMain.RenderPass = renderer->GetRenderPass();
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        ENJIN_LOG_ERROR(Editor, "Failed to initialize ImGui Vulkan backend");
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        DestroyDescriptorPool();
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Editor, "ImGui initialized successfully");
    return true;
}

void ImGuiLayer::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    // Wait for device to be idle before cleanup
    if (m_Renderer && m_Renderer->GetContext()) {
        vkDeviceWaitIdle(m_Renderer->GetContext()->GetDevice());
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    DestroyDescriptorPool();

    m_Initialized = false;
    m_Renderer = nullptr;
    ENJIN_LOG_INFO(Editor, "ImGui shut down");
}

void ImGuiLayer::BeginFrame() {
    if (!m_Initialized || !m_Enabled) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::EndFrame(VkCommandBuffer commandBuffer) {
    if (!m_Initialized || !m_Enabled) {
        return;
    }

    ImGui::Render();
    ImDrawData* drawData = ImGui::GetDrawData();
    if (drawData) {
        ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
    }
}

void ImGuiLayer::ShowDemoWindow(bool* open) {
    if (!m_Initialized || !m_Enabled) {
        return;
    }
    ImGui::ShowDemoWindow(open);
}

bool ImGuiLayer::CreateDescriptorPool() {
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * static_cast<u32>(sizeof(poolSizes) / sizeof(poolSizes[0]));
    poolInfo.poolSizeCount = static_cast<u32>(sizeof(poolSizes) / sizeof(poolSizes[0]));
    poolInfo.pPoolSizes = poolSizes;

    VkResult result = vkCreateDescriptorPool(
        m_Renderer->GetContext()->GetDevice(),
        &poolInfo,
        nullptr,
        &m_DescriptorPool
    );

    return result == VK_SUCCESS;
}

void ImGuiLayer::DestroyDescriptorPool() {
    if (m_DescriptorPool != VK_NULL_HANDLE && m_Renderer && m_Renderer->GetContext()) {
        vkDestroyDescriptorPool(m_Renderer->GetContext()->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }
}

} // namespace GUI
} // namespace Enjin
