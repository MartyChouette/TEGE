#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

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

protected:
    /**
     * @brief Get the application window
     * @return Pointer to Window
     */
    Window* GetWindow() const { return m_Window; }

private:
    void InitializeEngine();
    void ShutdownEngine();
    void MainLoop();

    Window* m_Window = nullptr;
    bool m_Running = true;
    bool m_Minimized = false;
    bool m_Focused = true;
    f32 m_LastFrameTime = 0.0f;
};

// User must implement this function
extern Application* CreateApplication();

} // namespace Enjin
