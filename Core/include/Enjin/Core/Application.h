#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <chrono>
#include <functional>

/**
 * @file Application.h
 * @brief Base application class for Enjin Engine
 * @author Enjin Engine Team
 * @date 2025
 */

namespace Enjin {

// Forward declarations
class Window;

/**
 * @brief Application base class
 * 
 * The Engine owns the entry point and manages the application lifecycle.
 * Users should derive from this class and implement the virtual methods.
 */
class ENJIN_API Application {
public:
    Application();
    virtual ~Application();

    /**
     * @brief Main entry point (called by engine)
     * @return Exit code
     */
    int Run();

    /**
     * @brief Initialize application-specific logic
     * Called after engine initialization
     */
    virtual void Initialize() {}

    /**
     * @brief Shutdown application-specific logic
     * Called before engine shutdown
     */
    virtual void Shutdown() {}

    /**
     * @brief Update loop
     * @param deltaTime Time elapsed since last frame in seconds
     */
    virtual void Update(f32 deltaTime) {}

    /**
     * @brief Render loop
     */
    virtual void Render() {}

    /**
     * @brief Headless frame limit (0 = run until the window closes).
     *
     * When set to N > 0, the main loop renders exactly N frames and then
     * requests a clean shutdown (exit code 0). This is the standalone player's
     * equivalent of the editor's --golden-frames: it lets CI boot an exported
     * game, prove it survives a real render loop (not just init), and exit,
     * without needing a window manager or a human to close the window.
     */
    static u32 s_HeadlessFrameLimit;

protected:
    /**
     * @brief Request application shutdown (sets m_Running = false)
     */
    void RequestShutdown() { m_Running = false; }

    /**
     * @brief Get the application window
     * @return Pointer to Window
     */
    Window* GetWindow() const { return m_Window; }

    /**
     * @brief Check if window is focused
     */
    bool IsFocused() const { return m_Focused; }

    /**
     * @brief Check if application is idle (no input for a while)
     */
    bool IsIdle() const { return m_IsIdle; }

    /**
     * @brief Get the idle timer value in seconds
     */
    f32 GetIdleTime() const { return m_IdleTimer; }

    /**
     * @brief Reset the idle timer (called when input is detected)
     */
    void ResetIdleTimer() { m_IdleTimer = 0.0f; m_IsIdle = false; }

    // Frame rate limiting callbacks - set by EditorLayer
    using FrameSettingsCallback = std::function<f32()>;
    void SetTargetFPSCallback(FrameSettingsCallback cb) { m_TargetFPSCallback = std::move(cb); }

    /**
     * @brief Execute a single frame (update + render + frame limiting).
     * Used directly by the Emscripten main loop callback, and called
     * internally by MainLoop() on desktop platforms.
     */
    void RunOneFrame();

private:
    void InitializeEngine();
    void ShutdownEngine();
    void MainLoop();
    void LimitFrameRate(f32 targetFPS);
    void UpdateIdleState(f32 deltaTime);

    Window* m_Window = nullptr;
    bool m_Running = true;
    bool m_Minimized = false;
    bool m_Focused = true;
    f32 m_LastFrameTime = 0.0f;
    std::chrono::high_resolution_clock::time_point m_LastTime;

    // Frame rate limiting
    std::chrono::high_resolution_clock::time_point m_FrameStart;
    f32 m_IdleTimer = 0.0f;
    bool m_IsIdle = false;
    FrameSettingsCallback m_TargetFPSCallback;
    u32 m_FramesRendered = 0;
};

// User must implement this function
extern Application* CreateApplication();

} // namespace Enjin
