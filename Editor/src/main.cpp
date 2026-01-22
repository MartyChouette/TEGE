#include "Enjin/Core/Application.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
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

        // Minimal bring-up: render the existing triangle system so the window
        // isn't blank. This will evolve into the full editor renderer later.
        m_Renderer = std::make_unique<Enjin::Renderer::VulkanRenderer>();
        if (!m_Renderer->Initialize(GetWindow())) {
            ENJIN_LOG_FATAL(Editor, "Failed to initialize Vulkan renderer");
            m_Renderer.reset();
            return;
        }

        m_World = std::make_unique<Enjin::ECS::World>();
        m_RenderSystem = m_World->RegisterSystem<Enjin::ECS::RenderSystem>(m_World.get(), m_Renderer.get());
        m_RenderSystem->Initialize();
    }

    void Shutdown() override {
        ENJIN_LOG_INFO(Editor, "Enjin Editor shutting down...");

        if (m_RenderSystem) {
            m_RenderSystem->Shutdown();
            m_RenderSystem = nullptr;
        }
        m_World.reset();
        m_Renderer.reset();
    }

    void Update(Enjin::f32 deltaTime) override {
        (void)deltaTime;
        // Editor update logic
    }

    void Render() override {
        if (!m_Renderer) {
            return;
        }

        // IMPORTANT: record draw commands between BeginFrame/EndFrame.
        // BeginFrame can fail due to swapchain recreation, minimized window, etc.
        if (!m_Renderer->BeginFrame()) {
            return;  // Skip this frame if we couldn't acquire an image
        }

        // RenderSystem::Update records draw calls into the current command buffer.
        if (m_World) {
            m_World->Update(0.0f);
        }

        m_Renderer->EndFrame();
    }

private:
    std::unique_ptr<Enjin::Renderer::VulkanRenderer> m_Renderer;
    std::unique_ptr<Enjin::ECS::World> m_World;
    Enjin::ECS::RenderSystem* m_RenderSystem = nullptr;
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
