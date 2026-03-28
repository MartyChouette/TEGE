#include "Enjin/Core/Application.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Debug/CrashHandler.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Platform/Paths.h"
#include "Enjin/Platform/Input.h"
#include <chrono>
#include <thread>
#include <iostream>
#undef CreateWindow

#if ENJIN_PLATFORM_WEB
    #include <emscripten.h>
#elif defined(ENJIN_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
    #include <timeapi.h>  // timeBeginPeriod/timeEndPeriod (requires winmm.lib)
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
#if defined(ENJIN_PLATFORM_WINDOWS) && !ENJIN_PLATFORM_WEB
    // Set Windows timer resolution to 1ms for precise sleep_for() and frame pacing.
    // Without this, sleep granularity defaults to ~15.6ms causing severe frame jitter.
    timeBeginPeriod(1);
#endif

#if !ENJIN_PLATFORM_WEB
    // Make relative paths (like "enjin.log" or shader/assets folders) resolve
    // next to the executable, even when launched via double-click.
    Platform::SetWorkingDirectoryToExecutableDirectory();
#endif

    Logger::Get().Initialize();
    Debug::InstallCrashHandler();
    ENJIN_LOG_INFO(Core, "Initializing Enjin Engine...");
    
    // Create window (windowed mode)
    WindowDesc windowDesc;
    windowDesc.width = 1600;
    windowDesc.height = 900;
    windowDesc.title = "TEGE v" ENJIN_VERSION_STRING;
    // Try multiple icon paths — the .exe icon (from .rc) handles taskbar,
    // but GLFW window icon needs a PNG at runtime
    static const char* iconPaths[] = {
        "icon.png",
        "enjin_icon.png",
        "../installer/TEGE_ICON_64.png",
        "../../installer/TEGE_ICON_64.png",
        "../../../installer/TEGE_ICON_64.png",
        nullptr
    };
    windowDesc.iconPath = nullptr;
    for (int i = 0; iconPaths[i]; ++i) {
        FILE* f = fopen(iconPaths[i], "rb");
        if (f) { fclose(f); windowDesc.iconPath = iconPaths[i]; break; }
    }
    windowDesc.fullscreen = false;
    // Parentheses prevent potential macro substitution as well.
    m_Window = (CreateWindow)(windowDesc);
    
    if (!m_Window) {
        ENJIN_LOG_FATAL(Core, "Failed to create window");
        std::cerr << "CRITICAL ERROR: Failed to create window!" << std::endl;
#if defined(ENJIN_PLATFORM_WINDOWS) && !ENJIN_PLATFORM_WEB
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

    // Initialize input system
    Input::Initialize(m_Window);

    // Register focus callback: clear input on focus loss to prevent stuck keys
    m_Window->SetFocusCallback([this](bool focused) {
        m_Focused = focused;
        if (!focused) {
            Input::ClearAllState();
        }
    });

    // Register iconify callback: track minimized state
    m_Window->SetIconifyCallback([this](bool iconified) {
        m_Minimized = iconified;
    });

    ENJIN_LOG_INFO(Core, "Engine initialized successfully");
}

void Application::ShutdownEngine() {
    ENJIN_LOG_INFO(Core, "Shutting down Enjin Engine...");

    if (m_Window) {
        DestroyWindow(m_Window);
        m_Window = nullptr;
    }

    Debug::UninstallCrashHandler();
    Logger::Get().Shutdown();

#if defined(ENJIN_PLATFORM_WINDOWS) && !ENJIN_PLATFORM_WEB
    timeEndPeriod(1);
#endif
}

void Application::MainLoop() {
    m_LastTime = std::chrono::high_resolution_clock::now();

#if ENJIN_PLATFORM_WEB
    // On web, hand control to the browser's requestAnimationFrame loop.
    // RunOneFrame is called once per frame; emscripten_set_main_loop_arg
    // does not return until emscripten_cancel_main_loop() is called.
    emscripten_set_main_loop_arg([](void* arg) {
        static_cast<Application*>(arg)->RunOneFrame();
    }, this, 0, true);
#else
    while (m_Running) {
        RunOneFrame();
    }
#endif
}

void Application::RunOneFrame() {
    if (!m_Running) {
#if ENJIN_PLATFORM_WEB
        emscripten_cancel_main_loop();
#endif
        return;
    }

    m_FrameStart = std::chrono::high_resolution_clock::now();

    // When minimized, wait for events instead of busy-spinning
    if (m_Minimized && m_Window) {
#if !ENJIN_PLATFORM_WEB
        m_Window->WaitEvents();
#endif
        // Reset m_LastTime so we don't get a huge delta spike on restore
        m_LastTime = std::chrono::high_resolution_clock::now();
        m_FrameStart = m_LastTime;
        if (m_Window->ShouldClose()) {
            m_Running = false;
        }
        return;
    }

    auto currentTime = std::chrono::high_resolution_clock::now();
    auto deltaTimeNs = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - m_LastTime);
    f32 deltaTime = static_cast<f32>(deltaTimeNs.count()) / 1'000'000'000.0f;
    m_LastTime = currentTime;

    // Clamp delta time to prevent physics explosion after long pause/resume
    if (deltaTime > 0.1f) {
        deltaTime = 0.1f;
    }

    // Update window events
    if (m_Window) {
        m_Window->PollEvents();
        if (m_Window->ShouldClose()) {
            m_Running = false;
            return;
        }
    }

    // Update input system (must be after PollEvents)
    Input::Update();

    // Update idle state based on input activity
    UpdateIdleState(deltaTime);

    Update(deltaTime);
    Render();

    // Frame rate limiting (skipped on web — browser controls frame pacing)
#if !ENJIN_PLATFORM_WEB
    if (m_TargetFPSCallback) {
        f32 targetFPS = m_TargetFPSCallback();
        if (targetFPS > 0.0f) {
            LimitFrameRate(targetFPS);
        }
    }
#endif
}

void Application::LimitFrameRate(f32 targetFPS) {
    f32 targetFrameTime = 1.0f / targetFPS;

    auto now = std::chrono::high_resolution_clock::now();
    f32 elapsed = std::chrono::duration<f32>(now - m_FrameStart).count();

    if (elapsed < targetFrameTime) {
        f32 remaining = targetFrameTime - elapsed;

        // Sleep for most of the remaining time (saves CPU), leave ~2ms for precision spin
        if (remaining > 0.003f) {
            auto sleepDuration = std::chrono::microseconds(static_cast<i64>((remaining - 0.002f) * 1'000'000));
            std::this_thread::sleep_for(sleepDuration);
        }

        // Busy-wait for final precision to hit the exact target
        while (std::chrono::duration<f32>(
                   std::chrono::high_resolution_clock::now() - m_FrameStart).count()
               < targetFrameTime) {
            std::this_thread::yield();
        }
    }
}

void Application::UpdateIdleState(f32 deltaTime) {
    // Check if there's any input activity
    bool hasInput = false;

    // Check keyboard
    // We can't easily check "any key" so we check common keys
    if (Input::IsKeyDown(KeyCode::W) || Input::IsKeyDown(KeyCode::A) ||
        Input::IsKeyDown(KeyCode::S) || Input::IsKeyDown(KeyCode::D) ||
        Input::IsKeyDown(KeyCode::Space) || Input::IsKeyDown(KeyCode::LeftShift) ||
        Input::IsKeyDown(KeyCode::Escape) || Input::IsKeyDown(KeyCode::Tab)) {
        hasInput = true;
    }

    // Check mouse buttons
    if (Input::IsMouseButtonDown(MouseButton::Left) ||
        Input::IsMouseButtonDown(MouseButton::Right) ||
        Input::IsMouseButtonDown(MouseButton::Middle)) {
        hasInput = true;
    }

    // Check mouse movement (with a small threshold to ignore noise)
    Math::Vector2 mouseDelta = Input::GetMouseDelta();
    if (std::abs(mouseDelta.x) > 0.5f || std::abs(mouseDelta.y) > 0.5f) {
        hasInput = true;
    }

    // Check scroll
    Math::Vector2 scroll = Input::GetScrollDelta();
    if (std::abs(scroll.y) > 0.01f || std::abs(scroll.x) > 0.01f) {
        hasInput = true;
    }

    // Check gamepad
    for (i32 gp = 0; gp < 4; ++gp) {
        if (Input::IsGamepadConnected(gp)) {
            Math::Vector2 leftStick = Input::GetGamepadLeftStick(gp);
            Math::Vector2 rightStick = Input::GetGamepadRightStick(gp);
            if (leftStick.Length() > 0.1f || rightStick.Length() > 0.1f) {
                hasInput = true;
                break;
            }
        }
    }

    if (hasInput) {
        m_IdleTimer = 0.0f;
        m_IsIdle = false;
    } else {
        m_IdleTimer += deltaTime;
        // m_IsIdle is set by the callback logic in EditorLayer based on timeout
    }
}

} // namespace Enjin
