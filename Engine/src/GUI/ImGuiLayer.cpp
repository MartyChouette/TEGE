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
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

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

    // Wait for GPU to finish before cleanup (fence-based when renderer is active)
    if (m_Renderer && m_Renderer->GetContext()) {
        m_Renderer->GetContext()->WaitForGPU();
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

    // If no body font loaded, add the default font at the configured size
    if (!m_BodyFont) {
        ImFontConfig cfg;
        cfg.SizePixels = fontConfig.bodyFontSize;
        m_BodyFont = io.Fonts->AddFontDefault(&cfg);
    }

    // Load heading font (H1)
    if (!fontConfig.headingFontPath.empty()) {
        m_HeadingFont = io.Fonts->AddFontFromFileTTF(fontConfig.headingFontPath.c_str(), fontConfig.headingFontSize);
        if (!m_HeadingFont) {
            ENJIN_LOG_WARN(Editor, "Failed to load heading font: %s, falling back to body font", fontConfig.headingFontPath.c_str());
            m_HeadingFont = m_BodyFont;
        }
    }

    // Fallback: if heading font is still null, use body font
    if (!m_HeadingFont) {
        m_HeadingFont = m_BodyFont;
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

        case Editor::EditorTheme::Glass: {
            // Frosted glass theme — same teal/olive palette with translucent panels
            // and subtle prismatic border tints. Panels feel like frosted glass
            // hovering over the workspace.
            colors[ImGuiCol_WindowBg]             = ImVec4(0.08f, 0.09f, 0.11f, 0.82f);
            colors[ImGuiCol_ChildBg]              = ImVec4(0.06f, 0.07f, 0.09f, 0.75f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.10f, 0.11f, 0.14f, 0.92f);
            colors[ImGuiCol_Border]               = ImVec4(0.40f, 0.55f, 0.50f, 0.35f); // Teal-tinted border
            colors[ImGuiCol_FrameBg]              = ImVec4(0.12f, 0.13f, 0.16f, 0.70f);
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.22f, 0.25f, 0.80f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.22f, 0.28f, 0.30f, 0.90f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.06f, 0.07f, 0.09f, 0.88f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.10f, 0.14f, 0.13f, 0.92f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.08f, 0.09f, 0.11f, 0.88f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.06f, 0.07f, 0.09f, 0.50f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.30f, 0.40f, 0.35f, 0.60f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.50f, 0.42f, 0.75f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.45f, 0.60f, 0.50f, 0.90f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.50f, 0.82f, 0.65f, 1.00f); // Brighter teal
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.45f, 0.72f, 0.55f, 0.90f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.55f, 0.85f, 0.65f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.18f, 0.25f, 0.22f, 0.70f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.25f, 0.38f, 0.30f, 0.82f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.30f, 0.45f, 0.35f, 0.95f);
            colors[ImGuiCol_Header]               = ImVec4(0.18f, 0.25f, 0.22f, 0.65f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.25f, 0.38f, 0.30f, 0.78f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.30f, 0.45f, 0.35f, 0.90f);
            colors[ImGuiCol_Separator]            = ImVec4(0.35f, 0.50f, 0.45f, 0.30f); // Subtle teal separator
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.35f, 0.55f, 0.42f, 0.30f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.45f, 0.65f, 0.50f, 0.55f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.55f, 0.78f, 0.60f, 0.80f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.12f, 0.14f, 0.16f, 0.70f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.28f, 0.42f, 0.35f, 0.85f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.20f, 0.32f, 0.26f, 0.90f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.30f, 0.55f, 0.40f, 0.40f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.50f, 0.82f, 0.60f, 0.85f);
            colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.95f, 0.93f, 1.00f); // Slightly warm white
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.45f, 0.50f, 0.48f, 1.00f);

            // Glass style adjustments — more rounding, thinner borders
            style.WindowRounding = 8.0f;
            style.FrameRounding = 4.0f;
            style.PopupRounding = 6.0f;
            style.TabRounding = 5.0f;
            style.WindowBorderSize = 1.0f;
            style.FrameBorderSize = 0.0f;
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
            // Super Nintendo — authentic SNES shell: deep purple-grey body, warm grey panels,
            // purple accent buttons (#6B5B95), gold highlights (#C9B037). Soft, rounded, 16-bit era.
            colors[ImGuiCol_WindowBg]             = ImVec4(0.176f, 0.125f, 0.251f, 1.00f); // #2D2040
            colors[ImGuiCol_ChildBg]              = ImVec4(0.145f, 0.102f, 0.216f, 1.00f); // slightly darker
            colors[ImGuiCol_PopupBg]              = ImVec4(0.227f, 0.208f, 0.282f, 0.97f); // #3A3548
            colors[ImGuiCol_Border]               = ImVec4(0.420f, 0.357f, 0.584f, 0.50f); // purple tint
            colors[ImGuiCol_FrameBg]              = ImVec4(0.227f, 0.208f, 0.282f, 1.00f); // #3A3548
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.310f, 0.270f, 0.400f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.420f, 0.357f, 0.584f, 1.00f); // #6B5B95
            colors[ImGuiCol_TitleBg]              = ImVec4(0.145f, 0.102f, 0.216f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.227f, 0.208f, 0.282f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.160f, 0.114f, 0.235f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.145f, 0.102f, 0.216f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.420f, 0.357f, 0.584f, 0.80f); // #6B5B95
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.500f, 0.430f, 0.680f, 0.90f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.580f, 0.500f, 0.760f, 1.00f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.788f, 0.690f, 0.216f, 1.00f); // #C9B037 gold
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.420f, 0.357f, 0.584f, 1.00f); // #6B5B95
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.788f, 0.690f, 0.216f, 1.00f); // gold on active
            colors[ImGuiCol_Button]               = ImVec4(0.420f, 0.357f, 0.584f, 0.85f); // #6B5B95
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.520f, 0.450f, 0.700f, 1.00f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.788f, 0.690f, 0.216f, 1.00f); // gold flash
            colors[ImGuiCol_Header]               = ImVec4(0.310f, 0.270f, 0.420f, 0.80f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.420f, 0.357f, 0.584f, 1.00f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.520f, 0.450f, 0.700f, 1.00f);
            colors[ImGuiCol_Separator]            = ImVec4(0.420f, 0.357f, 0.584f, 0.40f);
            colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.788f, 0.690f, 0.216f, 0.60f);
            colors[ImGuiCol_SeparatorActive]      = ImVec4(0.788f, 0.690f, 0.216f, 0.90f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.420f, 0.357f, 0.584f, 0.40f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.520f, 0.450f, 0.700f, 0.65f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.788f, 0.690f, 0.216f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.227f, 0.208f, 0.282f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.420f, 0.357f, 0.584f, 1.00f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.350f, 0.300f, 0.500f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.420f, 0.357f, 0.584f, 0.45f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.788f, 0.690f, 0.216f, 0.90f);
            colors[ImGuiCol_NavHighlight]         = ImVec4(0.788f, 0.690f, 0.216f, 1.00f);
            colors[ImGuiCol_Text]                 = ImVec4(0.920f, 0.900f, 0.940f, 1.00f); // warm white
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.500f, 0.460f, 0.560f, 1.00f);

            // SNES era: soft, rounded, friendly feel — the plastic shell
            style.WindowRounding    = 8.0f;
            style.FrameRounding     = 6.0f;
            style.PopupRounding     = 6.0f;
            style.ScrollbarRounding = 8.0f;
            style.GrabRounding      = 6.0f;
            style.TabRounding       = 5.0f;
            style.FrameBorderSize   = 1.0f;
            style.WindowBorderSize  = 1.0f;
            style.WindowPadding     = ImVec2(10.0f, 10.0f);
            style.FramePadding      = ImVec2(8.0f, 5.0f);
            break;
        }

        case Editor::EditorTheme::PS2: {
            // PlayStation 2 — dark blue-black void (#0A0E2A), deep blue panels (#141B4D),
            // bright blue accents (#2979FF), silver text. The iconic PS2 boot screen feel.
            colors[ImGuiCol_WindowBg]             = ImVec4(0.039f, 0.055f, 0.165f, 1.00f); // #0A0E2A
            colors[ImGuiCol_ChildBg]              = ImVec4(0.030f, 0.042f, 0.130f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.078f, 0.106f, 0.302f, 0.97f); // #141B4D
            colors[ImGuiCol_Border]               = ImVec4(0.161f, 0.475f, 1.000f, 0.30f); // #2979FF glow
            colors[ImGuiCol_FrameBg]              = ImVec4(0.078f, 0.106f, 0.302f, 1.00f); // #141B4D
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.120f, 0.160f, 0.420f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.161f, 0.475f, 1.000f, 0.60f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.025f, 0.035f, 0.110f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.060f, 0.085f, 0.260f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.035f, 0.048f, 0.150f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.025f, 0.035f, 0.110f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.161f, 0.475f, 1.000f, 0.50f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.161f, 0.475f, 1.000f, 0.70f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.161f, 0.475f, 1.000f, 0.90f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.161f, 0.475f, 1.000f, 1.00f); // #2979FF
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.161f, 0.475f, 1.000f, 0.85f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.350f, 0.600f, 1.000f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.100f, 0.140f, 0.380f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.161f, 0.475f, 1.000f, 0.70f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.161f, 0.475f, 1.000f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.100f, 0.140f, 0.380f, 0.80f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.161f, 0.475f, 1.000f, 0.60f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.161f, 0.475f, 1.000f, 0.85f);
            colors[ImGuiCol_Separator]            = ImVec4(0.161f, 0.475f, 1.000f, 0.25f);
            colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.161f, 0.475f, 1.000f, 0.55f);
            colors[ImGuiCol_SeparatorActive]      = ImVec4(0.161f, 0.475f, 1.000f, 0.85f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.161f, 0.475f, 1.000f, 0.30f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.161f, 0.475f, 1.000f, 0.55f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.161f, 0.475f, 1.000f, 0.85f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.078f, 0.106f, 0.302f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.161f, 0.475f, 1.000f, 0.65f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.120f, 0.300f, 0.700f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.161f, 0.475f, 1.000f, 0.35f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.161f, 0.475f, 1.000f, 0.90f);
            colors[ImGuiCol_NavHighlight]         = ImVec4(0.161f, 0.475f, 1.000f, 1.00f);
            colors[ImGuiCol_Text]                 = ImVec4(0.780f, 0.810f, 0.880f, 1.00f); // silver
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.350f, 0.380f, 0.500f, 1.00f);

            // PS2 era: sharp, modern, precise — the DVD-era sleekness
            style.WindowRounding    = 2.0f;
            style.FrameRounding     = 2.0f;
            style.PopupRounding     = 2.0f;
            style.ScrollbarRounding = 2.0f;
            style.GrabRounding      = 1.0f;
            style.TabRounding       = 2.0f;
            style.FrameBorderSize   = 1.0f;
            style.WindowBorderSize  = 1.0f;
            style.WindowPadding     = ImVec2(10.0f, 10.0f);
            style.FramePadding      = ImVec2(8.0f, 4.0f);
            break;
        }

        case Editor::EditorTheme::Xbox: {
            // Xbox — dark green-black (#0A1A0A), forest green panels (#1B3A1B),
            // bright Xbox green accents (#107C10), white text. Bold, angular, American muscle.
            colors[ImGuiCol_WindowBg]             = ImVec4(0.039f, 0.102f, 0.039f, 1.00f); // #0A1A0A
            colors[ImGuiCol_ChildBg]              = ImVec4(0.030f, 0.080f, 0.030f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.106f, 0.227f, 0.106f, 0.97f); // #1B3A1B
            colors[ImGuiCol_Border]               = ImVec4(0.063f, 0.486f, 0.063f, 0.40f); // #107C10 glow
            colors[ImGuiCol_FrameBg]              = ImVec4(0.106f, 0.227f, 0.106f, 1.00f); // #1B3A1B
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.140f, 0.320f, 0.140f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.063f, 0.486f, 0.063f, 0.70f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.030f, 0.075f, 0.030f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.063f, 0.200f, 0.063f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.035f, 0.090f, 0.035f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.030f, 0.075f, 0.030f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.063f, 0.486f, 0.063f, 0.55f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.063f, 0.486f, 0.063f, 0.75f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.063f, 0.486f, 0.063f, 0.95f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.063f, 0.486f, 0.063f, 1.00f); // #107C10
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.063f, 0.486f, 0.063f, 0.90f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.120f, 0.650f, 0.120f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.106f, 0.227f, 0.106f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.063f, 0.486f, 0.063f, 0.80f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.063f, 0.486f, 0.063f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.106f, 0.227f, 0.106f, 0.80f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.063f, 0.486f, 0.063f, 0.65f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.063f, 0.486f, 0.063f, 0.90f);
            colors[ImGuiCol_Separator]            = ImVec4(0.063f, 0.486f, 0.063f, 0.30f);
            colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.063f, 0.486f, 0.063f, 0.55f);
            colors[ImGuiCol_SeparatorActive]      = ImVec4(0.063f, 0.486f, 0.063f, 0.85f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.063f, 0.486f, 0.063f, 0.35f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.063f, 0.486f, 0.063f, 0.60f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.063f, 0.486f, 0.063f, 0.90f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.106f, 0.227f, 0.106f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.063f, 0.486f, 0.063f, 0.75f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.063f, 0.380f, 0.063f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.063f, 0.486f, 0.063f, 0.35f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.063f, 0.486f, 0.063f, 0.90f);
            colors[ImGuiCol_NavHighlight]         = ImVec4(0.063f, 0.486f, 0.063f, 1.00f);
            colors[ImGuiCol_Text]                 = ImVec4(0.950f, 0.960f, 0.950f, 1.00f); // clean white
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.400f, 0.500f, 0.400f, 1.00f);

            // Xbox era: bold, angular, no-nonsense — the big black box
            style.WindowRounding    = 1.0f;
            style.FrameRounding     = 1.0f;
            style.PopupRounding     = 1.0f;
            style.ScrollbarRounding = 1.0f;
            style.GrabRounding      = 0.0f;
            style.TabRounding       = 1.0f;
            style.FrameBorderSize   = 1.0f;
            style.WindowBorderSize  = 2.0f;
            style.WindowPadding     = ImVec2(12.0f, 12.0f);
            style.FramePadding      = ImVec2(10.0f, 5.0f);
            break;
        }

        case Editor::EditorTheme::Dreamcast: {
            // Dreamcast — clean white (#F5F5F0), light blue-grey panels (#E0E8F0),
            // Sega blue accents (#0066CC), orange highlights (#FF6600). Bright, optimistic, year 2000.
            colors[ImGuiCol_WindowBg]             = ImVec4(0.961f, 0.961f, 0.941f, 1.00f); // #F5F5F0
            colors[ImGuiCol_ChildBg]              = ImVec4(0.940f, 0.945f, 0.930f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.878f, 0.910f, 0.941f, 0.98f); // #E0E8F0
            colors[ImGuiCol_Border]               = ImVec4(0.000f, 0.400f, 0.800f, 0.35f); // #0066CC
            colors[ImGuiCol_FrameBg]              = ImVec4(0.878f, 0.910f, 0.941f, 1.00f); // #E0E8F0
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.820f, 0.870f, 0.930f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.000f, 0.400f, 0.800f, 0.35f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.878f, 0.910f, 0.941f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.000f, 0.400f, 0.800f, 0.85f); // blue title bar
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.920f, 0.935f, 0.950f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.920f, 0.930f, 0.940f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.000f, 0.400f, 0.800f, 0.50f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.000f, 0.400f, 0.800f, 0.70f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(1.000f, 0.400f, 0.000f, 0.90f); // orange on active
            colors[ImGuiCol_CheckMark]            = ImVec4(1.000f, 0.400f, 0.000f, 1.00f); // #FF6600 orange
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.000f, 0.400f, 0.800f, 0.80f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(1.000f, 0.400f, 0.000f, 1.00f); // orange
            colors[ImGuiCol_Button]               = ImVec4(0.000f, 0.400f, 0.800f, 0.80f); // Sega blue
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.000f, 0.460f, 0.900f, 0.90f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(1.000f, 0.400f, 0.000f, 1.00f); // orange flash
            colors[ImGuiCol_Header]               = ImVec4(0.000f, 0.400f, 0.800f, 0.25f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.000f, 0.400f, 0.800f, 0.45f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.000f, 0.400f, 0.800f, 0.65f);
            colors[ImGuiCol_Separator]            = ImVec4(0.000f, 0.400f, 0.800f, 0.25f);
            colors[ImGuiCol_SeparatorHovered]     = ImVec4(1.000f, 0.400f, 0.000f, 0.50f);
            colors[ImGuiCol_SeparatorActive]      = ImVec4(1.000f, 0.400f, 0.000f, 0.80f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.000f, 0.400f, 0.800f, 0.30f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(1.000f, 0.400f, 0.000f, 0.55f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(1.000f, 0.400f, 0.000f, 0.85f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.878f, 0.910f, 0.941f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.000f, 0.400f, 0.800f, 0.55f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.000f, 0.400f, 0.800f, 0.35f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.000f, 0.400f, 0.800f, 0.25f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(1.000f, 0.400f, 0.000f, 0.90f);
            colors[ImGuiCol_NavHighlight]         = ImVec4(1.000f, 0.400f, 0.000f, 1.00f);
            colors[ImGuiCol_Text]                 = ImVec4(0.100f, 0.120f, 0.160f, 1.00f); // dark text
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.450f, 0.470f, 0.500f, 1.00f);

            // Dreamcast era: clean, friendly, slightly rounded — the white console
            style.WindowRounding    = 6.0f;
            style.FrameRounding     = 5.0f;
            style.PopupRounding     = 5.0f;
            style.ScrollbarRounding = 6.0f;
            style.GrabRounding      = 5.0f;
            style.TabRounding       = 4.0f;
            style.FrameBorderSize   = 1.0f;
            style.WindowBorderSize  = 1.0f;
            style.WindowPadding     = ImVec2(10.0f, 10.0f);
            style.FramePadding      = ImVec2(8.0f, 5.0f);
            break;
        }

        case Editor::EditorTheme::SegaSaturn: {
            // Sega Saturn — dark charcoal (#1A1A2E), blue-grey panels (#252545),
            // Saturn blue accents (#3366AA), grey text. Understated, premium, 32-bit era.
            colors[ImGuiCol_WindowBg]             = ImVec4(0.102f, 0.102f, 0.180f, 1.00f); // #1A1A2E
            colors[ImGuiCol_ChildBg]              = ImVec4(0.082f, 0.082f, 0.150f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.145f, 0.145f, 0.271f, 0.97f); // #252545
            colors[ImGuiCol_Border]               = ImVec4(0.200f, 0.400f, 0.667f, 0.40f); // #3366AA
            colors[ImGuiCol_FrameBg]              = ImVec4(0.145f, 0.145f, 0.271f, 1.00f); // #252545
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.190f, 0.200f, 0.360f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.200f, 0.400f, 0.667f, 0.55f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.082f, 0.082f, 0.150f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.145f, 0.145f, 0.271f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.094f, 0.094f, 0.165f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.082f, 0.082f, 0.150f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.200f, 0.400f, 0.667f, 0.50f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.200f, 0.400f, 0.667f, 0.70f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.200f, 0.400f, 0.667f, 0.90f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.200f, 0.400f, 0.667f, 1.00f); // #3366AA
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.200f, 0.400f, 0.667f, 0.80f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.300f, 0.520f, 0.800f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.180f, 0.200f, 0.360f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.200f, 0.400f, 0.667f, 0.70f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.200f, 0.400f, 0.667f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.180f, 0.200f, 0.360f, 0.80f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.200f, 0.400f, 0.667f, 0.55f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.200f, 0.400f, 0.667f, 0.80f);
            colors[ImGuiCol_Separator]            = ImVec4(0.200f, 0.400f, 0.667f, 0.30f);
            colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.200f, 0.400f, 0.667f, 0.55f);
            colors[ImGuiCol_SeparatorActive]      = ImVec4(0.200f, 0.400f, 0.667f, 0.80f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.200f, 0.400f, 0.667f, 0.30f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.200f, 0.400f, 0.667f, 0.55f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.200f, 0.400f, 0.667f, 0.85f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.145f, 0.145f, 0.271f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.200f, 0.400f, 0.667f, 0.60f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.170f, 0.300f, 0.500f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.200f, 0.400f, 0.667f, 0.35f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.200f, 0.400f, 0.667f, 0.90f);
            colors[ImGuiCol_NavHighlight]         = ImVec4(0.200f, 0.400f, 0.667f, 1.00f);
            colors[ImGuiCol_Text]                 = ImVec4(0.700f, 0.720f, 0.780f, 1.00f); // grey-silver
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.400f, 0.410f, 0.460f, 1.00f);

            // Saturn era: understated, slightly angular, premium feel
            style.WindowRounding    = 3.0f;
            style.FrameRounding     = 3.0f;
            style.PopupRounding     = 3.0f;
            style.ScrollbarRounding = 4.0f;
            style.GrabRounding      = 2.0f;
            style.TabRounding       = 3.0f;
            style.FrameBorderSize   = 1.0f;
            style.WindowBorderSize  = 1.0f;
            style.WindowPadding     = ImVec2(10.0f, 10.0f);
            style.FramePadding      = ImVec2(8.0f, 4.0f);
            break;
        }

        case Editor::EditorTheme::GBA: {
            // Game Boy Advance — indigo GBA shell (#2E1A47), lighter purple panels (#3D2A5C),
            // GBA screen green accents (#8BBB26), warm white text. Compact, handheld.
            colors[ImGuiCol_WindowBg]             = ImVec4(0.180f, 0.102f, 0.278f, 1.00f); // #2E1A47
            colors[ImGuiCol_ChildBg]              = ImVec4(0.150f, 0.082f, 0.240f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.239f, 0.165f, 0.361f, 0.97f); // #3D2A5C
            colors[ImGuiCol_Border]               = ImVec4(0.545f, 0.733f, 0.149f, 0.40f); // #8BBB26 glow
            colors[ImGuiCol_FrameBg]              = ImVec4(0.239f, 0.165f, 0.361f, 1.00f); // #3D2A5C
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.300f, 0.220f, 0.440f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.545f, 0.733f, 0.149f, 0.45f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.150f, 0.082f, 0.240f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.239f, 0.165f, 0.361f, 1.00f);
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.165f, 0.094f, 0.260f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.150f, 0.082f, 0.240f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.545f, 0.733f, 0.149f, 0.50f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.545f, 0.733f, 0.149f, 0.70f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.545f, 0.733f, 0.149f, 0.90f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.545f, 0.733f, 0.149f, 1.00f); // #8BBB26
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.545f, 0.733f, 0.149f, 0.80f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.650f, 0.840f, 0.250f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.300f, 0.210f, 0.450f, 1.00f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.545f, 0.733f, 0.149f, 0.65f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.545f, 0.733f, 0.149f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.300f, 0.210f, 0.450f, 0.80f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.545f, 0.733f, 0.149f, 0.50f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.545f, 0.733f, 0.149f, 0.80f);
            colors[ImGuiCol_Separator]            = ImVec4(0.545f, 0.733f, 0.149f, 0.25f);
            colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.545f, 0.733f, 0.149f, 0.50f);
            colors[ImGuiCol_SeparatorActive]      = ImVec4(0.545f, 0.733f, 0.149f, 0.80f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.545f, 0.733f, 0.149f, 0.30f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.545f, 0.733f, 0.149f, 0.55f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.545f, 0.733f, 0.149f, 0.85f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.239f, 0.165f, 0.361f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.545f, 0.733f, 0.149f, 0.60f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.380f, 0.520f, 0.120f, 1.00f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.545f, 0.733f, 0.149f, 0.30f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.545f, 0.733f, 0.149f, 0.90f);
            colors[ImGuiCol_NavHighlight]         = ImVec4(0.545f, 0.733f, 0.149f, 1.00f);
            colors[ImGuiCol_Text]                 = ImVec4(0.920f, 0.910f, 0.880f, 1.00f); // warm white
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.480f, 0.440f, 0.540f, 1.00f);

            // GBA era: compact, handheld, slightly rounded — the indigo shell
            style.WindowRounding    = 5.0f;
            style.FrameRounding     = 4.0f;
            style.PopupRounding     = 4.0f;
            style.ScrollbarRounding = 5.0f;
            style.GrabRounding      = 3.0f;
            style.TabRounding       = 3.0f;
            style.FrameBorderSize   = 1.0f;
            style.WindowBorderSize  = 1.0f;
            style.WindowPadding     = ImVec2(8.0f, 8.0f);
            style.FramePadding      = ImVec2(6.0f, 4.0f);
            break;
        }

        case Editor::EditorTheme::DS: {
            // Nintendo DS — silver (#C0C8D0), light grey panels (#D8DDE3),
            // Nintendo blue accents (#0055BF), dark text. Dual-screen era clean design.
            colors[ImGuiCol_WindowBg]             = ImVec4(0.753f, 0.784f, 0.816f, 1.00f); // #C0C8D0
            colors[ImGuiCol_ChildBg]              = ImVec4(0.730f, 0.760f, 0.790f, 1.00f);
            colors[ImGuiCol_PopupBg]              = ImVec4(0.847f, 0.867f, 0.890f, 0.98f); // #D8DDE3
            colors[ImGuiCol_Border]               = ImVec4(0.000f, 0.333f, 0.749f, 0.35f); // #0055BF
            colors[ImGuiCol_FrameBg]              = ImVec4(0.847f, 0.867f, 0.890f, 1.00f); // #D8DDE3
            colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.800f, 0.830f, 0.870f, 1.00f);
            colors[ImGuiCol_FrameBgActive]        = ImVec4(0.000f, 0.333f, 0.749f, 0.35f);
            colors[ImGuiCol_TitleBg]              = ImVec4(0.800f, 0.820f, 0.850f, 1.00f);
            colors[ImGuiCol_TitleBgActive]        = ImVec4(0.000f, 0.333f, 0.749f, 0.80f); // blue title
            colors[ImGuiCol_MenuBarBg]            = ImVec4(0.780f, 0.800f, 0.830f, 1.00f);
            colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.800f, 0.820f, 0.850f, 1.00f);
            colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.000f, 0.333f, 0.749f, 0.50f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.000f, 0.333f, 0.749f, 0.70f);
            colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.000f, 0.333f, 0.749f, 0.90f);
            colors[ImGuiCol_CheckMark]            = ImVec4(0.000f, 0.333f, 0.749f, 1.00f); // #0055BF
            colors[ImGuiCol_SliderGrab]           = ImVec4(0.000f, 0.333f, 0.749f, 0.80f);
            colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.000f, 0.420f, 0.880f, 1.00f);
            colors[ImGuiCol_Button]               = ImVec4(0.000f, 0.333f, 0.749f, 0.75f);
            colors[ImGuiCol_ButtonHovered]        = ImVec4(0.000f, 0.380f, 0.850f, 0.85f);
            colors[ImGuiCol_ButtonActive]         = ImVec4(0.000f, 0.420f, 0.920f, 1.00f);
            colors[ImGuiCol_Header]               = ImVec4(0.000f, 0.333f, 0.749f, 0.22f);
            colors[ImGuiCol_HeaderHovered]        = ImVec4(0.000f, 0.333f, 0.749f, 0.40f);
            colors[ImGuiCol_HeaderActive]         = ImVec4(0.000f, 0.333f, 0.749f, 0.60f);
            colors[ImGuiCol_Separator]            = ImVec4(0.000f, 0.333f, 0.749f, 0.25f);
            colors[ImGuiCol_SeparatorHovered]     = ImVec4(0.000f, 0.333f, 0.749f, 0.50f);
            colors[ImGuiCol_SeparatorActive]      = ImVec4(0.000f, 0.333f, 0.749f, 0.80f);
            colors[ImGuiCol_ResizeGrip]           = ImVec4(0.000f, 0.333f, 0.749f, 0.25f);
            colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.000f, 0.333f, 0.749f, 0.50f);
            colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.000f, 0.333f, 0.749f, 0.80f);
            colors[ImGuiCol_Tab]                  = ImVec4(0.847f, 0.867f, 0.890f, 1.00f);
            colors[ImGuiCol_TabHovered]           = ImVec4(0.000f, 0.333f, 0.749f, 0.50f);
            colors[ImGuiCol_TabActive]            = ImVec4(0.000f, 0.333f, 0.749f, 0.30f);
            colors[ImGuiCol_TextSelectedBg]       = ImVec4(0.000f, 0.333f, 0.749f, 0.25f);
            colors[ImGuiCol_DragDropTarget]       = ImVec4(0.000f, 0.333f, 0.749f, 0.90f);
            colors[ImGuiCol_NavHighlight]         = ImVec4(0.000f, 0.333f, 0.749f, 1.00f);
            colors[ImGuiCol_Text]                 = ImVec4(0.120f, 0.140f, 0.180f, 1.00f); // dark text
            colors[ImGuiCol_TextDisabled]         = ImVec4(0.420f, 0.440f, 0.480f, 1.00f);

            // DS era: clean, modern, compact — the silver clamshell
            style.WindowRounding    = 4.0f;
            style.FrameRounding     = 4.0f;
            style.PopupRounding     = 4.0f;
            style.ScrollbarRounding = 5.0f;
            style.GrabRounding      = 3.0f;
            style.TabRounding       = 3.0f;
            style.FrameBorderSize   = 1.0f;
            style.WindowBorderSize  = 1.0f;
            style.WindowPadding     = ImVec2(8.0f, 8.0f);
            style.FramePadding      = ImVec2(6.0f, 4.0f);
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
