#include "Enjin/Core/Application.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Audio/AudioSystem.h"
#include <iostream>
#include <memory>
#if !defined(_WIN32)
    #include <unistd.h>
    #include <cstdio>
#endif

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

        ENJIN_LOG_INFO(Editor, "Editor initialized - Use RMB + WASD to fly, scroll to adjust speed");
    }

    void Shutdown() override {
        ENJIN_LOG_INFO(Editor, "Enjin Editor shutting down...");

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
        // Update camera controller
        if (m_CameraController) {
            m_CameraController->Update(deltaTime);
        }

        // Update editor layer
        if (m_EditorLayer) {
            m_EditorLayer->Update(deltaTime);
        }
    }

    void Render() override {
        if (!m_Renderer) {
            return;
        }

        if (!m_Renderer->BeginFrame()) {
            m_FrameFailCount++;
            if (m_FrameFailCount % 60 == 1) {
                ENJIN_LOG_WARN(Editor, "BeginFrame failed (total failures: %u)", m_FrameFailCount);
            }
            return;
        }

        m_FrameFailCount = 0;

        // Update camera aspect ratio
        auto extent = m_Renderer->GetSwapchainExtent();
        if (extent.width > 0 && extent.height > 0 && m_Camera) {
            Enjin::f32 aspect = static_cast<Enjin::f32>(extent.width) / static_cast<Enjin::f32>(extent.height);
            m_Camera->SetPerspective(45.0f, aspect, 0.1f, 1000.0f);
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
