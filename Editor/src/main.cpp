#include "Enjin/Core/Application.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Debug/CrashHandler.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Audio/AudioSystem.h"
#include <iostream>
#include <memory>
#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #ifdef CreateWindow
        #undef CreateWindow
    #endif
#else
    #include <unistd.h>
    #include <cstdio>
#endif

// Crash context — file-scope pointers for function-pointer providers
static Enjin::Renderer::VulkanRenderer* s_CrashRenderer = nullptr;
static Enjin::ECS::World* s_CrashWorld = nullptr;
static Enjin::Editor::EditorLayer* s_CrashEditor = nullptr;
static char s_GPUNameBuf[256] = {};

// Editor application
class EditorApplication : public Enjin::Application {
public:
    void Initialize() override {
        ENJIN_LOG_INFO(Editor, "Enjin Editor starting...");

        // Initialize Vulkan renderer
        m_Renderer = std::make_unique<Enjin::Renderer::VulkanRenderer>();
        if (!m_Renderer->Initialize(GetWindow())) {
            ENJIN_LOG_FATAL(Editor, "Failed to initialize Vulkan renderer");
            m_Renderer.reset();
            return;
        }

        // Register window resize callback to proactively trigger swapchain recreation
        GetWindow()->SetResizeCallback([this](Enjin::u32, Enjin::u32) {
            if (m_Renderer) {
                m_Renderer->SetFramebufferResized(true);
            }
        });

        // Setup camera
        m_Camera = std::make_unique<Enjin::Renderer::Camera>();
        m_Camera->SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
        m_Camera->SetPosition(Enjin::Math::Vector3(0.0f, 2.5f, 7.0f));

        // Setup camera controller
        m_CameraController = std::make_unique<Enjin::Renderer::CameraController>(m_Camera.get());
        m_CameraController->SetMoveSpeed(5.0f);
        m_CameraController->SetLookSensitivity(0.15f);

        // Create ECS world
        m_World = std::make_unique<Enjin::ECS::World>();

        // Setup render system
        m_RenderSystem = m_World->RegisterSystem<Enjin::ECS::RenderSystem>(m_World.get(), m_Renderer.get());
        m_RenderSystem->SetCamera(m_Camera.get());
        m_RenderSystem->SetEditorMode(true);
        m_RenderSystem->Initialize();

        // Setup editor UI
        m_EditorLayer = std::make_unique<Enjin::Editor::EditorLayer>();
        if (!m_EditorLayer->Initialize(GetWindow(), m_Renderer.get())) {
            ENJIN_LOG_ERROR(Editor, "Failed to initialize editor layer");
            m_EditorLayer.reset();
        } else {
            m_EditorLayer->SetWorld(m_World.get());
            m_EditorLayer->SetCamera(m_Camera.get());
            m_EditorLayer->SetCameraController(m_CameraController.get());
            m_EditorLayer->SetRenderSystem(m_RenderSystem);
        }

        // Initialize audio system
        Enjin::Audio::AudioManager::Get().Initialize();

        // Set up frame rate limiting callback
        // This callback is called each frame to determine the target FPS
        SetTargetFPSCallback([this]() -> Enjin::f32 {
            if (!m_EditorLayer) return 0.0f;

            auto& settings = m_EditorLayer->GetEditorSettings();
            auto& playMode = m_EditorLayer->GetPlayMode();
            auto& sceneManager = m_EditorLayer->GetSceneManager();

            // Check if in play mode
            // NOTE: During play mode, the main loop runs uncapped.
            // Game View frame rate is controlled separately via RenderOffscreen().
            // The GameFrameSettings.targetFrameRate is only used for exported builds.
            if (playMode.IsPlaying()) {
                auto& gameSettings = sceneManager.GetGameFrameSettings();

                // Handle background behavior when unfocused during play mode
                if (!IsFocused()) {
                    switch (gameSettings.backgroundBehavior) {
                        case Enjin::Scene::BackgroundBehavior::Pause:
                            // Return a very low FPS when paused
                            return 5.0f;
                        case Enjin::Scene::BackgroundBehavior::ReduceTo30:
                            return 30.0f;
                        case Enjin::Scene::BackgroundBehavior::RunNormally:
                        default:
                            break;
                    }
                }

                // Run uncapped during play mode - Game View FPS is controlled
                // by the dropdown in the Game View panel via RenderOffscreen()
                return 0.0f;
            }

            // Editor mode - use editor frame settings
            // Check unfocused reduction
            if (!IsFocused() && settings.reduceFrameRateWhenUnfocused) {
                return static_cast<Enjin::f32>(settings.unfocusedFrameRate);
            }

            // Check idle reduction
            if (settings.reduceFrameRateWhenIdle && GetIdleTime() > settings.idleTimeoutSeconds) {
                return static_cast<Enjin::f32>(settings.idleFrameRate);
            }

            // Return editor frame rate limit (0 = uncapped)
            Enjin::u32 limitVal = static_cast<Enjin::u32>(settings.editorFrameRateLimit);
            return limitVal > 0 ? static_cast<Enjin::f32>(limitVal) : 0.0f;
        });

        // Register crash context providers for enriched crash reports
        s_CrashRenderer = m_Renderer.get();
        s_CrashWorld = m_World.get();
        s_CrashEditor = m_EditorLayer.get();

        // Cache GPU name once (queried from Vulkan physical device)
        if (m_Renderer && m_Renderer->GetContext()) {
            VkPhysicalDeviceProperties props = {};
            vkGetPhysicalDeviceProperties(m_Renderer->GetContext()->GetPhysicalDevice(), &props);
            snprintf(s_GPUNameBuf, sizeof(s_GPUNameBuf), "%s", props.deviceName);
        }

        Enjin::Debug::CrashContext ctx;
        ctx.engineVersion = []() -> const char* { return ENJIN_VERSION_STRING; };
        ctx.gpuName = []() -> const char* { return s_GPUNameBuf; };
        ctx.sceneName = []() -> const char* {
            if (s_CrashEditor) {
                auto& sm = s_CrashEditor->GetSceneManager();
                const auto& name = sm.GetCurrentSceneName();
                return name.empty() ? "(unsaved)" : name.c_str();
            }
            return "unknown";
        };
        ctx.entityCount = []() -> Enjin::u32 {
            if (s_CrashWorld) {
                return static_cast<Enjin::u32>(s_CrashWorld->GetEntityCount());
            }
            return 0;
        };
        Enjin::Debug::SetCrashContext(ctx);

        ENJIN_LOG_INFO(Editor, "Editor initialized - Use RMB + WASD to fly, scroll to adjust speed");
    }

    void Shutdown() override {
        ENJIN_LOG_INFO(Editor, "Enjin Editor shutting down...");

        // Clear crash context pointers before teardown
        s_CrashRenderer = nullptr;
        s_CrashWorld = nullptr;
        s_CrashEditor = nullptr;
        Enjin::Debug::SetCrashContext({});

        Enjin::Audio::AudioManager::Get().Shutdown();

        if (m_EditorLayer) {
            m_EditorLayer->Shutdown();
            m_EditorLayer.reset();
        }

        if (m_RenderSystem) {
            m_RenderSystem->Shutdown();
            m_RenderSystem = nullptr;
        }
        m_World.reset();
        m_CameraController.reset();
        m_Camera.reset();
        m_Renderer.reset();
    }

    void Update(Enjin::f32 deltaTime) override {
        // Update editor layer first (drives splash/hub state)
        if (m_EditorLayer) {
            m_EditorLayer->Update(deltaTime);
        }

        // Skip camera input when splash screen or project hub covers the screen
        if (m_EditorLayer && m_EditorLayer->IsShowingFullscreenOverlay()) {
            return;
        }

        // Update camera controller only when the editor viewport is hovered
        // (or when already captured from a prior RMB press in the viewport)
        if (m_CameraController) {
            bool viewportHovered = m_EditorLayer && m_EditorLayer->IsEditorViewportHovered();
            bool alreadyCaptured = m_CameraController->IsMouseCaptured();
            if (viewportHovered || alreadyCaptured) {
                m_CameraController->Update(deltaTime);
            }
        }
    }

    void Render() override {
        if (!m_Renderer) {
            return;
        }

        if (!m_Renderer->BeginFrame()) {
            m_FrameFailCount++;
            if (m_Renderer->IsDeviceLost()) {
                ENJIN_LOG_FATAL(Editor, "GPU device lost — shutting down. Please restart the application.");
                RequestShutdown();
                return;
            }
            if (m_FrameFailCount % 60 == 1) {
                ENJIN_LOG_WARN(Editor, "BeginFrame failed (total failures: %u)", m_FrameFailCount);
            }
            return;
        }

        m_FrameFailCount = 0;

        // Update editor camera aspect ratio.
        // The editor/scene view camera is always perspective — it's the editor's
        // navigation camera, not the game camera. 2D game cameras are separate
        // entities with CameraComponent (used by Game View, not Scene View).
        auto extent = m_Renderer->GetSwapchainExtent();
        if (extent.width > 0 && extent.height > 0 && m_Camera) {
            Enjin::f32 aspect = static_cast<Enjin::f32>(extent.width) / static_cast<Enjin::f32>(extent.height);
            m_Camera->SetPerspective(45.0f, aspect, 0.1f, 1000.0f);
        }

        // Flush deferred changes (skybox config, pipeline recreation) before
        // any rendering commands that might reference old GPU resources
        if (m_RenderSystem) {
            m_RenderSystem->FlushPendingChanges();
        }

        // Resize render targets BEFORE command buffer recording.
        // This avoids destroying/recreating Vulkan resources while a command
        // buffer is in recording state, which crashes with Vulkan layer hooks
        // (OBS game capture, RenderDoc, etc.) that hold resource references.
        if (m_EditorLayer) {
            m_EditorLayer->PrepareRenderTargets();
        }

        VkCommandBuffer cmd = m_Renderer->GetCurrentCommandBuffer();

        // Render offscreen targets (before main render pass)
        if (m_EditorLayer && cmd != VK_NULL_HANDLE) {
            m_EditorLayer->RenderOffscreen(cmd);
        }

        // Render scene (starts main render pass)
        if (m_World) {
            m_World->Update(0.0f);
        }

        // Render editor UI (within main render pass)
        if (m_EditorLayer && cmd != VK_NULL_HANDLE) {
            m_EditorLayer->Render(cmd);
        }

        m_Renderer->EndFrame();
    }

private:
    std::unique_ptr<Enjin::Renderer::VulkanRenderer> m_Renderer;
    std::unique_ptr<Enjin::Renderer::Camera> m_Camera;
    std::unique_ptr<Enjin::Renderer::CameraController> m_CameraController;
    std::unique_ptr<Enjin::ECS::World> m_World;
    std::unique_ptr<Enjin::Editor::EditorLayer> m_EditorLayer;
    Enjin::ECS::RenderSystem* m_RenderSystem = nullptr;
    Enjin::u32 m_FrameFailCount = 0;
};

Enjin::Application* CreateApplication() {
    return new EditorApplication();
}

// Entry point - Engine owns this
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Enjin::Application* app = CreateApplication();
    int result = app->Run();
    delete app;

    std::cout << "Application exited with code " << result << "." << std::endl;
#if !defined(_WIN32)
    // Only pause when launched from an interactive terminal.
    if (isatty(fileno(stdin))) {
        std::cout << "Press Enter to close..." << std::endl;
        std::cin.get();
    }
#endif

    return result;
}

// Windows GUI subsystem entry point — forwards to main().
// This prevents the black console window from appearing on launch.
// Linux/Mac don't need this — they don't spawn consoles for GUI apps.
#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(0, nullptr);
}
#endif
