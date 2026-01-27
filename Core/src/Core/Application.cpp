#include "Enjin/Core/Application.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Platform/Paths.h"
#include <chrono>
#include <iostream>
#undef CreateWindow

#if defined(ENJIN_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    // Windows headers define CreateWindow/CreateWindowA macros which conflict
    // with Enjin::CreateWindow(). Undefine them so our function name is usable.
    #ifdef CreateWindow
        #undef CreateWindow
    #endif
#endif

namespace Enjin {

extern Window* CreateWindow(const WindowDesc& desc);
extern void DestroyWindow(Window* window);

Application::Application() {
}

Application::~Application() {
}

int Application::Run() {
    int exitCode = 0;
    try {
        InitializeEngine();
        if (!m_Running || !m_Window) {
            // Engine init already logged the failure.
            ShutdownEngine();
            return 1;
        }

        Initialize();
        MainLoop();
        Shutdown();
        ShutdownEngine();
    } catch (const std::exception& e) {
        ENJIN_LOG_FATAL(Core, "Unhandled exception: %s", e.what());
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
        exitCode = 1;
        ShutdownEngine();
    } catch (...) {
        ENJIN_LOG_FATAL(Core, "Unhandled non-standard exception");
        std::cerr << "Unhandled non-standard exception" << std::endl;
        exitCode = 1;
        ShutdownEngine();
    }

    return exitCode;
}

void Application::InitializeEngine() {
    // Make relative paths (like "enjin.log" or shader/assets folders) resolve
    // next to the executable, even when launched via double-click.
    Platform::SetWorkingDirectoryToExecutableDirectory();

    Logger::Get().Initialize();
    ENJIN_LOG_INFO(Core, "Initializing Enjin Engine...");
    
    // Create window
    WindowDesc windowDesc;
    windowDesc.width = 1280;
    windowDesc.height = 720;
    windowDesc.title = "Enjin Engine";
    // Parentheses prevent potential macro substitution as well.
    m_Window = (CreateWindow)(windowDesc);
    
    if (!m_Window) {
        ENJIN_LOG_FATAL(Core, "Failed to create window");
        std::cerr << "CRITICAL ERROR: Failed to create window!" << std::endl;
#if defined(ENJIN_PLATFORM_WINDOWS)
        // If launched as a GUI app (no console), show an error dialog.
        if (GetConsoleWindow() == nullptr) {
            MessageBoxA(nullptr,
                "Failed to create window.\n\nCheck 'enjin.log' next to the executable for details.",
                "Enjin Fatal Error",
                MB_OK | MB_ICONERROR);
        }
#endif
        m_Running = false;
        return;
    }
    
    ENJIN_LOG_INFO(Core, "Engine initialized successfully");
}

void Application::ShutdownEngine() {
    ENJIN_LOG_INFO(Core, "Shutting down Enjin Engine...");
    
    if (m_Window) {
        DestroyWindow(m_Window);
        m_Window = nullptr;
    }
    
    Logger::Get().Shutdown();
}

void Application::MainLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (m_Running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto deltaTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - lastTime);
        f32 deltaTime = static_cast<f32>(deltaTimeNs.count()) / 1'000'000'000.0f;
        lastTime = currentTime;

        // Update window events
        if (m_Window) {
            m_Window->PollEvents();
            if (m_Window->ShouldClose()) {
                m_Running = false;
                break;
            }
        }

        Update(deltaTime);
        Render();
    }
}

} // namespace Enjin
