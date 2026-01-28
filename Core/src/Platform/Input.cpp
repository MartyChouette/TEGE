#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Enjin {

namespace {
    constexpr i32 GLFW_KEY_FIRST = 32;   // GLFW_KEY_SPACE is the first valid key
    constexpr i32 GLFW_KEY_LAST_VALID = 348;  // GLFW_KEY_LAST
    constexpr i32 MAX_KEYS = 512;        // Array size (must be > GLFW_KEY_LAST_VALID)
    constexpr i32 MAX_MOUSE_BUTTONS = 8;

    GLFWwindow* s_Window = nullptr;

    // Current frame state
    bool s_KeysDown[MAX_KEYS] = {};
    bool s_MouseButtonsDown[MAX_MOUSE_BUTTONS] = {};

    // Previous frame state (for pressed/released detection)
    bool s_KeysDownPrev[MAX_KEYS] = {};
    bool s_MouseButtonsDownPrev[MAX_MOUSE_BUTTONS] = {};

    // Mouse position
    Math::Vector2 s_MousePosition = {};
    Math::Vector2 s_MousePositionPrev = {};
    Math::Vector2 s_MouseDelta = {};

    // Scroll delta (accumulated between frames)
    Math::Vector2 s_ScrollDelta = {};
    Math::Vector2 s_ScrollAccumulator = {};

    bool s_MouseCaptured = false;
    bool s_CursorVisible = true;
    bool s_FirstMouseMove = true;

    // GLFW scroll callback
    void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        (void)window;
        s_ScrollAccumulator.x += static_cast<f32>(xoffset);
        s_ScrollAccumulator.y += static_cast<f32>(yoffset);
    }
}

void Input::Initialize(Window* window) {
    if (!window) {
        ENJIN_LOG_ERROR(Core, "Input::Initialize called with null window");
        return;
    }

    s_Window = static_cast<GLFWwindow*>(window->GetNativeHandle());
    if (!s_Window) {
        ENJIN_LOG_ERROR(Core, "Failed to get GLFW window handle for input");
        return;
    }

    // Clear state
    std::memset(s_KeysDown, 0, sizeof(s_KeysDown));
    std::memset(s_KeysDownPrev, 0, sizeof(s_KeysDownPrev));
    std::memset(s_MouseButtonsDown, 0, sizeof(s_MouseButtonsDown));
    std::memset(s_MouseButtonsDownPrev, 0, sizeof(s_MouseButtonsDownPrev));

    // Get initial mouse position
    double mx, my;
    glfwGetCursorPos(s_Window, &mx, &my);
    s_MousePosition = Math::Vector2(static_cast<f32>(mx), static_cast<f32>(my));
    s_MousePositionPrev = s_MousePosition;
    s_FirstMouseMove = true;

    // Set scroll callback
    glfwSetScrollCallback(s_Window, ScrollCallback);

    ENJIN_LOG_INFO(Core, "Input system initialized");
}

void Input::Update() {
    if (!s_Window) return;

    // Copy current state to previous
    std::memcpy(s_KeysDownPrev, s_KeysDown, sizeof(s_KeysDown));
    std::memcpy(s_MouseButtonsDownPrev, s_MouseButtonsDown, sizeof(s_MouseButtonsDown));

    // Poll keyboard state (GLFW keys are 32-348)
    for (i32 key = GLFW_KEY_FIRST; key <= GLFW_KEY_LAST_VALID; ++key) {
        s_KeysDown[key] = glfwGetKey(s_Window, key) == GLFW_PRESS;
    }

    // Poll mouse button state
    for (i32 button = 0; button < MAX_MOUSE_BUTTONS; ++button) {
        s_MouseButtonsDown[button] = glfwGetMouseButton(s_Window, button) == GLFW_PRESS;
    }

    // Update mouse position
    s_MousePositionPrev = s_MousePosition;
    double mx, my;
    glfwGetCursorPos(s_Window, &mx, &my);
    s_MousePosition = Math::Vector2(static_cast<f32>(mx), static_cast<f32>(my));

    // Calculate mouse delta
    if (s_FirstMouseMove) {
        s_MouseDelta = Math::Vector2(0.0f, 0.0f);
        s_FirstMouseMove = false;
    } else {
        s_MouseDelta = s_MousePosition - s_MousePositionPrev;
    }

    // Update scroll delta and reset accumulator
    s_ScrollDelta = s_ScrollAccumulator;
    s_ScrollAccumulator = Math::Vector2(0.0f, 0.0f);
}

bool Input::IsKeyDown(KeyCode key) {
    i32 keyIndex = static_cast<i32>(key);
    if (keyIndex < 0 || keyIndex >= MAX_KEYS) return false;
    return s_KeysDown[keyIndex];
}

bool Input::IsKeyPressed(KeyCode key) {
    i32 keyIndex = static_cast<i32>(key);
    if (keyIndex < 0 || keyIndex >= MAX_KEYS) return false;
    return s_KeysDown[keyIndex] && !s_KeysDownPrev[keyIndex];
}

bool Input::IsKeyReleased(KeyCode key) {
    i32 keyIndex = static_cast<i32>(key);
    if (keyIndex < 0 || keyIndex >= MAX_KEYS) return false;
    return !s_KeysDown[keyIndex] && s_KeysDownPrev[keyIndex];
}

bool Input::IsMouseButtonDown(MouseButton button) {
    i32 buttonIndex = static_cast<i32>(button);
    if (buttonIndex < 0 || buttonIndex >= MAX_MOUSE_BUTTONS) return false;
    return s_MouseButtonsDown[buttonIndex];
}

bool Input::IsMouseButtonPressed(MouseButton button) {
    i32 buttonIndex = static_cast<i32>(button);
    if (buttonIndex < 0 || buttonIndex >= MAX_MOUSE_BUTTONS) return false;
    return s_MouseButtonsDown[buttonIndex] && !s_MouseButtonsDownPrev[buttonIndex];
}

bool Input::IsMouseButtonReleased(MouseButton button) {
    i32 buttonIndex = static_cast<i32>(button);
    if (buttonIndex < 0 || buttonIndex >= MAX_MOUSE_BUTTONS) return false;
    return !s_MouseButtonsDown[buttonIndex] && s_MouseButtonsDownPrev[buttonIndex];
}

Math::Vector2 Input::GetMousePosition() {
    return s_MousePosition;
}

f32 Input::GetMouseX() {
    return s_MousePosition.x;
}

f32 Input::GetMouseY() {
    return s_MousePosition.y;
}

Math::Vector2 Input::GetMouseDelta() {
    return s_MouseDelta;
}

Math::Vector2 Input::GetScrollDelta() {
    return s_ScrollDelta;
}

void Input::SetMouseCaptured(bool captured) {
    if (!s_Window) return;

    s_MouseCaptured = captured;
    if (captured) {
        glfwSetInputMode(s_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        s_FirstMouseMove = true; // Reset to avoid large delta on capture
    } else {
        glfwSetInputMode(s_Window, GLFW_CURSOR, s_CursorVisible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
}

bool Input::IsMouseCaptured() {
    return s_MouseCaptured;
}

void Input::SetCursorVisible(bool visible) {
    if (!s_Window) return;

    s_CursorVisible = visible;
    if (!s_MouseCaptured) {
        glfwSetInputMode(s_Window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
}

bool Input::IsCursorVisible() {
    return s_CursorVisible;
}

} // namespace Enjin
