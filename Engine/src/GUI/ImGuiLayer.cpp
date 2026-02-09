#include "Enjin/GUI/ImGuiLayer.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
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

bool ImGuiLayer::Initialize(Window* window, Renderer::VulkanRenderer* renderer,
                            const EditorFontConfig& fontConfig) {
    if (m_Initialized) {
        return true;
    }

    if (!window || !renderer) {
        ENJIN_LOG_ERROR(Editor, "ImGuiLayer::Initialize called with null window or renderer");
        return false;
    }

    m_Window = window;
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
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(10.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
    style.IndentSpacing = 22.0f;
    style.ScrollbarSize = 16.0f;
    style.GrabMinSize = 12.0f;

    // Borders
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.PopupBorderSize = 1.0f;

    // Custom colors (dark sage-gray theme)
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
    colors[ImGuiCol_CheckMark]            = ImVec4(0.55f, 0.78f, 0.58f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.50f, 0.70f, 0.52f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.60f, 0.82f, 0.63f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.22f, 0.27f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.30f, 0.38f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.35f, 0.45f, 0.38f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.22f, 0.27f, 0.24f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.30f, 0.38f, 0.32f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.35f, 0.45f, 0.38f, 1.00f);
    colors[ImGuiCol_Separator]            = ImVec4(0.25f, 0.25f, 0.30f, 0.60f);
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.40f, 0.33f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.40f, 0.55f, 0.43f, 0.70f);
    colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.50f, 0.70f, 0.53f, 0.90f);
    colors[ImGuiCol_Tab]                  = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.35f, 0.45f, 0.38f, 1.00f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.25f, 0.33f, 0.28f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.35f, 0.50f, 0.38f, 0.50f);
    colors[ImGuiCol_DragDropTarget]       = ImVec4(0.55f, 0.78f, 0.58f, 0.90f);

    // Load custom fonts (before backend init so atlas is ready)
    LoadFonts(fontConfig);

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

    m_BodyFont = nullptr;
    m_HeadingFont = nullptr;
    m_H2Font = nullptr;
    m_SmallFont = nullptr;
    m_MonoFont = nullptr;
    m_Initialized = false;
    m_Renderer = nullptr;
    m_Window = nullptr;
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

void ImGuiLayer::LoadFonts(const EditorFontConfig& fontConfig) {
    ImGuiIO& io = ImGui::GetIO();

    m_BodyFont = nullptr;
    m_HeadingFont = nullptr;
    m_H2Font = nullptr;
    m_SmallFont = nullptr;
    m_MonoFont = nullptr;

    // Load body font (becomes the default ImGui font if loaded first)
    if (!fontConfig.bodyFontPath.empty()) {
        m_BodyFont = io.Fonts->AddFontFromFileTTF(fontConfig.bodyFontPath.c_str(), fontConfig.bodyFontSize);
        if (!m_BodyFont) {
            ENJIN_LOG_WARN(Editor, "Failed to load body font: %s, using default", fontConfig.bodyFontPath.c_str());
        }
    }

    // If no body font loaded, add the default font so heading/mono are separate
    if (!m_BodyFont) {
        m_BodyFont = io.Fonts->AddFontDefault();
    }

    // Load heading font (H1)
    if (!fontConfig.headingFontPath.empty()) {
        m_HeadingFont = io.Fonts->AddFontFromFileTTF(fontConfig.headingFontPath.c_str(), fontConfig.headingFontSize);
        if (!m_HeadingFont) {
            ENJIN_LOG_WARN(Editor, "Failed to load heading font: %s", fontConfig.headingFontPath.c_str());
        }
    }

    // Load H2 font (section titles — uses heading font at smaller size)
    const std::string& h2Path = !fontConfig.headingFontPath.empty() ? fontConfig.headingFontPath : fontConfig.bodyFontPath;
    if (!h2Path.empty()) {
        m_H2Font = io.Fonts->AddFontFromFileTTF(h2Path.c_str(), fontConfig.h2FontSize);
        if (!m_H2Font) {
            ENJIN_LOG_WARN(Editor, "Failed to load H2 font from: %s", h2Path.c_str());
        }
    }

    // Load small font (labels/hints — uses body font at smaller size)
    if (!fontConfig.bodyFontPath.empty()) {
        m_SmallFont = io.Fonts->AddFontFromFileTTF(fontConfig.bodyFontPath.c_str(), fontConfig.smallFontSize);
        if (!m_SmallFont) {
            ENJIN_LOG_WARN(Editor, "Failed to load small font from: %s", fontConfig.bodyFontPath.c_str());
        }
    }

    // Load monospace font
    if (!fontConfig.monoFontPath.empty()) {
        m_MonoFont = io.Fonts->AddFontFromFileTTF(fontConfig.monoFontPath.c_str(), fontConfig.monoFontSize);
        if (!m_MonoFont) {
            ENJIN_LOG_WARN(Editor, "Failed to load monospace font: %s", fontConfig.monoFontPath.c_str());
        }
    }

    // Atlas builds automatically on first ImGui_ImplVulkan_NewFrame() call
}

static ImVec4 ToImVec4(const Editor::AccentColor& c) {
    return ImVec4(c.r, c.g, c.b, c.a);
}

void ImGuiLayer::ApplyTheme(Editor::EditorTheme theme, const Editor::AccentColorConfig* accentColors) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    switch (theme) {
        case Editor::EditorTheme::Dark:
        default: {
            // Dark sage-gray theme (TEGE brand)
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
            colors[ImGuiCol_CheckMark]            = ImVec4(0.55f, 0.78f, 0.58f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.50f, 0.70f, 0.52f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.60f, 0.82f, 0.63f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.22f, 0.27f, 0.24f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.30f, 0.38f, 0.32f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.35f, 0.45f, 0.38f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.22f, 0.27f, 0.24f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.30f, 0.38f, 0.32f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.35f, 0.45f, 0.38f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.25f, 0.25f, 0.30f, 0.60f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.40f, 0.33f, 0.50f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.40f, 0.55f, 0.43f, 0.70f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.50f, 0.70f, 0.53f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.35f, 0.45f, 0.38f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.25f, 0.33f, 0.28f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.35f, 0.50f, 0.38f, 0.50f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.55f, 0.78f, 0.58f, 0.90f);
            colors[ImGuiCol_Text]                 = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
            break;
        }

        case Editor::EditorTheme::Light: {
            // Light theme with dark text
            colors[ImGuiCol_WindowBg]             = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.96f, 0.96f, 0.96f, 0.98f);
            colors[ImGuiCol_Border]               = ImVec4(0.70f, 0.70f, 0.70f, 0.50f);
            colors[ImGuiCol_FrameBg]              = ImVec4(0.86f, 0.86f, 0.86f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.78f, 0.82f, 0.90f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.70f, 0.76f, 0.86f, 1.00f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.76f, 0.82f, 0.78f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.20f, 0.50f, 0.30f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.28f, 0.55f, 0.35f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.20f, 0.48f, 0.28f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.78f, 0.84f, 0.79f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.68f, 0.76f, 0.70f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.58f, 0.68f, 0.61f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.78f, 0.84f, 0.79f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.68f, 0.76f, 0.70f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.58f, 0.68f, 0.61f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.70f, 0.70f, 0.70f, 0.50f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.55f, 0.65f, 0.58f, 0.50f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.40f, 0.55f, 0.43f, 0.70f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.30f, 0.48f, 0.35f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.82f, 0.82f, 0.86f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.60f, 0.72f, 0.63f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.70f, 0.80f, 0.73f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.50f, 0.65f, 0.53f, 0.50f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.30f, 0.60f, 0.35f, 0.90f);
            colors[ImGuiCol_Text]                 = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
            break;
        }

        case Editor::EditorTheme::HighContrastDark: {
            // High-contrast dark: near-black bg, pure white text, bright borders (7:1+ ratios)
            colors[ImGuiCol_WindowBg]             = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.06f, 0.06f, 0.06f, 0.98f);
            colors[ImGuiCol_Border]               = ImVec4(0.80f, 0.80f, 0.80f, 0.90f);
            colors[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.18f, 0.25f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.25f, 0.25f, 0.35f, 1.00f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.10f, 0.14f, 0.11f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.50f, 0.90f, 0.55f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.50f, 0.90f, 0.55f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.60f, 0.95f, 0.65f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.15f, 0.18f, 0.16f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.25f, 0.35f, 0.28f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.35f, 0.48f, 0.38f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.15f, 0.18f, 0.16f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.25f, 0.35f, 0.28f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.35f, 0.48f, 0.38f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.80f, 0.80f, 0.80f, 0.70f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.45f, 0.65f, 0.48f, 0.60f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.55f, 0.80f, 0.58f, 0.80f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.60f, 0.90f, 0.65f, 1.00f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.10f, 0.12f, 0.11f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.30f, 0.40f, 0.33f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.20f, 0.30f, 0.23f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.30f, 0.55f, 0.35f, 0.60f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.50f, 0.90f, 0.55f, 1.00f);
            colors[ImGuiCol_Text]                 = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);

            // Thicker borders for visibility
            style.FrameBorderSize = 1.0f;
            style.PopupBorderSize = 2.0f;
            style.WindowBorderSize = 2.0f;
            break;
        }

        case Editor::EditorTheme::HighContrastLight: {
            // High-contrast light: white bg, black text, bold borders (7:1+ ratios)
            colors[ImGuiCol_WindowBg]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
            colors[ImGuiCol_Border]               = ImVec4(0.00f, 0.00f, 0.00f, 0.80f);
            colors[ImGuiCol_FrameBg]              = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.82f, 0.85f, 0.92f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.72f, 0.78f, 0.88f, 1.00f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.80f, 0.86f, 0.82f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.10f, 0.45f, 0.18f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.10f, 0.45f, 0.18f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.08f, 0.38f, 0.15f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.82f, 0.87f, 0.83f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.68f, 0.76f, 0.70f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.55f, 0.65f, 0.58f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.82f, 0.87f, 0.83f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.68f, 0.76f, 0.70f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.55f, 0.65f, 0.58f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.00f, 0.00f, 0.00f, 0.60f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.28f, 0.42f, 0.32f, 0.60f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.18f, 0.38f, 0.22f, 0.80f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.08f, 0.32f, 0.15f, 1.00f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.88f, 0.91f, 0.89f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.65f, 0.74f, 0.67f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.75f, 0.84f, 0.78f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.38f, 0.55f, 0.42f, 0.50f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.15f, 0.55f, 0.25f, 0.90f);
            colors[ImGuiCol_Text]                 = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

            // Thicker borders for visibility
            style.FrameBorderSize = 1.0f;
            style.PopupBorderSize = 2.0f;
            style.WindowBorderSize = 2.0f;
            break;
        }

        case Editor::EditorTheme::SNES: {
            // Super Nintendo — gray-purple shell, colorful button accents
            colors[ImGuiCol_WindowBg]             = ImVec4(0.12f, 0.10f, 0.16f, 1.00f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.09f, 0.08f, 0.13f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.14f, 0.12f, 0.19f, 0.95f);
            colors[ImGuiCol_Border]               = ImVec4(0.35f, 0.30f, 0.45f, 0.60f);
            colors[ImGuiCol_FrameBg]              = ImVec4(0.17f, 0.15f, 0.22f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.25f, 0.22f, 0.33f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.32f, 0.28f, 0.42f, 1.00f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.09f, 0.08f, 0.13f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.16f, 0.13f, 0.22f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.11f, 0.10f, 0.15f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.09f, 0.08f, 0.13f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.35f, 0.30f, 0.48f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.38f, 0.58f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.55f, 0.48f, 0.68f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.68f, 0.55f, 0.85f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.58f, 0.48f, 0.75f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.70f, 0.58f, 0.88f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.25f, 0.20f, 0.35f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.35f, 0.28f, 0.50f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.45f, 0.38f, 0.62f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.25f, 0.20f, 0.35f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.35f, 0.28f, 0.50f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.45f, 0.38f, 0.62f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.35f, 0.30f, 0.45f, 0.60f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.40f, 0.32f, 0.55f, 0.50f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.50f, 0.42f, 0.68f, 0.70f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.60f, 0.52f, 0.78f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.17f, 0.15f, 0.22f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.40f, 0.32f, 0.55f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.30f, 0.25f, 0.42f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.45f, 0.35f, 0.60f, 0.50f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.68f, 0.55f, 0.85f, 0.90f);
            colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.90f, 0.96f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.50f, 0.45f, 0.58f, 1.00f);
            break;
        }

        case Editor::EditorTheme::PS2: {
            // PlayStation 2 — deep navy blue, iconic blue glow
            colors[ImGuiCol_WindowBg]             = ImVec4(0.06f, 0.07f, 0.14f, 1.00f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.04f, 0.05f, 0.11f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.08f, 0.09f, 0.18f, 0.95f);
            colors[ImGuiCol_Border]               = ImVec4(0.15f, 0.22f, 0.45f, 0.60f);
            colors[ImGuiCol_FrameBg]              = ImVec4(0.10f, 0.12f, 0.22f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.15f, 0.18f, 0.35f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.25f, 0.45f, 1.00f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.04f, 0.05f, 0.11f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.08f, 0.10f, 0.22f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.06f, 0.07f, 0.14f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.04f, 0.05f, 0.11f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.18f, 0.25f, 0.50f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.35f, 0.62f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.30f, 0.42f, 0.72f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.35f, 0.55f, 0.95f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.28f, 0.45f, 0.85f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.38f, 0.58f, 0.98f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.12f, 0.18f, 0.35f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.18f, 0.28f, 0.52f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.24f, 0.38f, 0.65f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.12f, 0.18f, 0.35f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.18f, 0.28f, 0.52f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.24f, 0.38f, 0.65f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.15f, 0.22f, 0.45f, 0.60f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.20f, 0.32f, 0.60f, 0.50f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.28f, 0.42f, 0.72f, 0.70f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.35f, 0.52f, 0.85f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.10f, 0.12f, 0.22f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.22f, 0.32f, 0.58f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.16f, 0.24f, 0.45f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.20f, 0.35f, 0.65f, 0.50f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.35f, 0.55f, 0.95f, 0.90f);
            colors[ImGuiCol_Text]                 = ImVec4(0.85f, 0.90f, 1.00f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.35f, 0.42f, 0.58f, 1.00f);
            break;
        }

        case Editor::EditorTheme::Xbox: {
            // Xbox — black and green, bold and vivid
            colors[ImGuiCol_WindowBg]             = ImVec4(0.07f, 0.09f, 0.07f, 1.00f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.05f, 0.07f, 0.05f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.09f, 0.12f, 0.09f, 0.95f);
            colors[ImGuiCol_Border]               = ImVec4(0.15f, 0.30f, 0.15f, 0.60f);
            colors[ImGuiCol_FrameBg]              = ImVec4(0.12f, 0.15f, 0.12f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.25f, 0.18f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.22f, 0.35f, 0.22f, 1.00f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.05f, 0.07f, 0.05f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.08f, 0.14f, 0.08f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.07f, 0.09f, 0.07f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.05f, 0.07f, 0.05f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.18f, 0.38f, 0.18f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.50f, 0.25f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.30f, 0.60f, 0.30f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.30f, 0.78f, 0.30f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.25f, 0.65f, 0.25f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.35f, 0.82f, 0.35f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.12f, 0.22f, 0.12f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.18f, 0.38f, 0.18f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.22f, 0.50f, 0.22f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.12f, 0.22f, 0.12f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.18f, 0.38f, 0.18f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.22f, 0.50f, 0.22f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.15f, 0.30f, 0.15f, 0.60f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.20f, 0.42f, 0.20f, 0.50f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.28f, 0.55f, 0.28f, 0.70f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.35f, 0.68f, 0.35f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.12f, 0.15f, 0.12f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.20f, 0.42f, 0.20f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.15f, 0.30f, 0.15f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.18f, 0.45f, 0.18f, 0.50f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.30f, 0.78f, 0.30f, 0.90f);
            colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.95f, 0.92f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.40f, 0.50f, 0.40f, 1.00f);
            break;
        }

        case Editor::EditorTheme::Dreamcast: {
            // Dreamcast — warm dark gray with orange swirl accents
            colors[ImGuiCol_WindowBg]             = ImVec4(0.12f, 0.11f, 0.10f, 1.00f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.09f, 0.08f, 0.07f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.15f, 0.13f, 0.12f, 0.95f);
            colors[ImGuiCol_Border]               = ImVec4(0.40f, 0.28f, 0.15f, 0.60f);
            colors[ImGuiCol_FrameBg]              = ImVec4(0.18f, 0.15f, 0.13f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.28f, 0.22f, 0.16f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.38f, 0.28f, 0.18f, 1.00f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.09f, 0.08f, 0.07f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.18f, 0.14f, 0.10f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.12f, 0.11f, 0.09f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.09f, 0.08f, 0.07f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.45f, 0.30f, 0.15f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.58f, 0.38f, 0.18f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.68f, 0.45f, 0.20f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.90f, 0.55f, 0.18f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.80f, 0.48f, 0.15f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.95f, 0.58f, 0.20f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.28f, 0.20f, 0.13f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.42f, 0.28f, 0.15f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.55f, 0.35f, 0.18f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.28f, 0.20f, 0.13f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.42f, 0.28f, 0.15f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.55f, 0.35f, 0.18f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.40f, 0.28f, 0.15f, 0.60f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.50f, 0.32f, 0.15f, 0.50f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.65f, 0.42f, 0.18f, 0.70f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.80f, 0.50f, 0.20f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.18f, 0.15f, 0.13f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.48f, 0.32f, 0.16f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.35f, 0.24f, 0.14f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.50f, 0.32f, 0.15f, 0.50f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.90f, 0.55f, 0.18f, 0.90f);
            colors[ImGuiCol_Text]                 = ImVec4(0.95f, 0.92f, 0.88f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.55f, 0.48f, 0.40f, 1.00f);
            break;
        }

        case Editor::EditorTheme::SegaSaturn: {
            // Sega Saturn — dark blue-gray with blue and subtle red accents
            colors[ImGuiCol_WindowBg]             = ImVec4(0.08f, 0.09f, 0.14f, 1.00f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.06f, 0.07f, 0.11f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.11f, 0.18f, 0.95f);
            colors[ImGuiCol_Border]               = ImVec4(0.22f, 0.25f, 0.40f, 0.60f);
            colors[ImGuiCol_FrameBg]              = ImVec4(0.13f, 0.14f, 0.22f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.20f, 0.22f, 0.35f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.30f, 0.45f, 1.00f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.06f, 0.07f, 0.11f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.14f, 0.24f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.09f, 0.14f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.07f, 0.11f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.25f, 0.28f, 0.48f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.36f, 0.58f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.40f, 0.44f, 0.68f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.45f, 0.52f, 0.88f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.38f, 0.44f, 0.78f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.50f, 0.56f, 0.92f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.18f, 0.20f, 0.35f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.28f, 0.32f, 0.52f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.35f, 0.40f, 0.62f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.18f, 0.20f, 0.35f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.28f, 0.32f, 0.52f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.35f, 0.40f, 0.62f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.22f, 0.25f, 0.40f, 0.60f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.30f, 0.35f, 0.58f, 0.50f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.40f, 0.45f, 0.70f, 0.70f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.48f, 0.54f, 0.82f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.13f, 0.14f, 0.22f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.32f, 0.36f, 0.55f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.22f, 0.25f, 0.42f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.28f, 0.32f, 0.55f, 0.50f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.45f, 0.52f, 0.88f, 0.90f);
            colors[ImGuiCol_Text]                 = ImVec4(0.88f, 0.90f, 0.98f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.40f, 0.42f, 0.55f, 1.00f);
            break;
        }

        case Editor::EditorTheme::GBA: {
            // Game Boy Advance — deep indigo/purple shell
            colors[ImGuiCol_WindowBg]             = ImVec4(0.10f, 0.08f, 0.16f, 1.00f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.07f, 0.06f, 0.13f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.12f, 0.10f, 0.20f, 0.95f);
            colors[ImGuiCol_Border]               = ImVec4(0.28f, 0.22f, 0.48f, 0.60f);
            colors[ImGuiCol_FrameBg]              = ImVec4(0.15f, 0.12f, 0.25f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.18f, 0.38f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.30f, 0.25f, 0.48f, 1.00f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.07f, 0.06f, 0.13f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.14f, 0.11f, 0.25f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.10f, 0.08f, 0.16f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.07f, 0.06f, 0.13f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.22f, 0.52f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.30f, 0.65f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.48f, 0.38f, 0.75f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.58f, 0.42f, 0.92f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.48f, 0.35f, 0.82f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.62f, 0.48f, 0.95f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.20f, 0.16f, 0.35f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.30f, 0.24f, 0.52f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.40f, 0.32f, 0.65f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.20f, 0.16f, 0.35f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.30f, 0.24f, 0.52f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.40f, 0.32f, 0.65f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.28f, 0.22f, 0.48f, 0.60f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.35f, 0.26f, 0.58f, 0.50f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.45f, 0.35f, 0.72f, 0.70f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.55f, 0.44f, 0.85f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.15f, 0.12f, 0.25f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.35f, 0.28f, 0.58f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.25f, 0.20f, 0.42f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.35f, 0.26f, 0.58f, 0.50f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.58f, 0.42f, 0.92f, 0.90f);
            colors[ImGuiCol_Text]                 = ImVec4(0.90f, 0.88f, 0.98f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.40f, 0.58f, 1.00f);
            break;
        }

        case Editor::EditorTheme::DS: {
            // Nintendo DS — silver-gray with cool blue accents
            colors[ImGuiCol_WindowBg]             = ImVec4(0.13f, 0.13f, 0.15f, 1.00f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.16f, 0.16f, 0.19f, 0.95f);
            colors[ImGuiCol_Border]               = ImVec4(0.32f, 0.35f, 0.42f, 0.60f);
            colors[ImGuiCol_FrameBg]              = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.22f, 0.25f, 0.32f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.28f, 0.32f, 0.42f, 1.00f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.15f, 0.17f, 0.22f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.13f, 0.13f, 0.15f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.35f, 0.48f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.44f, 0.58f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.45f, 0.52f, 0.68f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.35f, 0.58f, 0.85f, 1.00f);
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.30f, 0.50f, 0.75f, 1.00f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.40f, 0.62f, 0.90f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.20f, 0.22f, 0.30f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.26f, 0.32f, 0.45f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.32f, 0.42f, 0.58f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.20f, 0.22f, 0.30f, 1.00f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.26f, 0.32f, 0.45f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.32f, 0.42f, 0.58f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.32f, 0.35f, 0.42f, 0.60f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.28f, 0.38f, 0.55f, 0.50f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.35f, 0.48f, 0.68f, 0.70f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.42f, 0.58f, 0.80f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.28f, 0.36f, 0.52f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.22f, 0.28f, 0.40f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.25f, 0.38f, 0.58f, 0.50f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.35f, 0.58f, 0.85f, 0.90f);
            colors[ImGuiCol_Text]                 = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.48f, 0.55f, 1.00f);
            break;
        }
    }

    // Apply custom accent colors if enabled
    if (accentColors && accentColors->useCustom) {
        colors[ImGuiCol_Button]          = ToImVec4(accentColors->button);
        colors[ImGuiCol_ButtonHovered]   = ToImVec4(accentColors->buttonHover);
        colors[ImGuiCol_ButtonActive]    = ToImVec4(accentColors->buttonActive);
        colors[ImGuiCol_Header]          = ToImVec4(accentColors->button);
        colors[ImGuiCol_HeaderHovered]   = ToImVec4(accentColors->buttonHover);
        colors[ImGuiCol_HeaderActive]    = ToImVec4(accentColors->buttonActive);
        colors[ImGuiCol_CheckMark]       = ToImVec4(accentColors->checkMark);
        colors[ImGuiCol_SliderGrab]      = ToImVec4(accentColors->sliderGrab);
        colors[ImGuiCol_SliderGrabActive] = ToImVec4(accentColors->sliderGrabActive);
        colors[ImGuiCol_ResizeGrip]      = ToImVec4(accentColors->resizeGrip);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(
            accentColors->resizeGrip.r + 0.10f,
            accentColors->resizeGrip.g + 0.15f,
            accentColors->resizeGrip.b + 0.10f,
            std::min(accentColors->resizeGrip.a + 0.20f, 1.0f));
        colors[ImGuiCol_ResizeGripActive] = ImVec4(
            accentColors->resizeGrip.r + 0.20f,
            accentColors->resizeGrip.g + 0.30f,
            accentColors->resizeGrip.b + 0.20f,
            std::min(accentColors->resizeGrip.a + 0.40f, 1.0f));
        colors[ImGuiCol_TextSelectedBg]  = ToImVec4(accentColors->textSelected);
        colors[ImGuiCol_DragDropTarget]  = ToImVec4(accentColors->dragDropTarget);
        colors[ImGuiCol_TabActive]       = ToImVec4(accentColors->tabActive);
        colors[ImGuiCol_TabHovered]      = ToImVec4(accentColors->tabHovered);
    }
}

void ImGuiLayer::SetGlobalScale(f32 scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = scale;
}

void ImGuiLayer::ReloadFonts(const EditorFontConfig& fontConfig) {
    if (!m_Initialized || !m_Renderer || !m_Renderer->GetContext()) {
        return;
    }

    // Wait for GPU to finish before rebuilding font atlas
    m_Renderer->WaitForAllFrames();

    // Clear existing fonts and reload
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    LoadFonts(fontConfig);

    // Build the font atlas - backend auto-uploads on next NewFrame()
    io.Fonts->Build();

    ENJIN_LOG_INFO(Editor, "Reloaded editor fonts");
}

// ============================================================================
// UIAnimationState singleton
// ============================================================================

UIAnimationState& UIAnimationState::Get() {
    static UIAnimationState instance;
    return instance;
}

HoverAnimation& UIAnimationState::GetHoverAnim(u32 widgetId) {
    auto it = m_HoverAnims.find(widgetId);
    if (it == m_HoverAnims.end()) {
        HoverAnimation anim;
        anim.pressSpring.value = 1.0f;
        anim.pressSpring.target = 1.0f;
        m_HoverAnims[widgetId] = anim;
        return m_HoverAnims[widgetId];
    }
    return it->second;
}

void UIAnimationState::Cleanup() {
    // Remove entries that haven't been active (settled at rest state)
    for (auto it = m_HoverAnims.begin(); it != m_HoverAnims.end(); ) {
        auto& anim = it->second;
        if (anim.hoverAlpha == 0.0f && anim.pressScale == 1.0f) {
            it = m_HoverAnims.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace GUI
} // namespace Enjin
