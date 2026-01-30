#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <cmath>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace Enjin {

namespace {
    constexpr i32 GLFW_KEY_FIRST = 32;   // GLFW_KEY_SPACE is the first valid key
    constexpr i32 GLFW_KEY_LAST_VALID = 348;  // GLFW_KEY_LAST
    constexpr i32 MAX_KEYS = 512;        // Array size (must be > GLFW_KEY_LAST_VALID)
    constexpr i32 MAX_MOUSE_BUTTONS = 8;
    constexpr i32 MAX_GAMEPAD_BUTTONS = 15;
    constexpr i32 MAX_GAMEPAD_AXES = 6;
    constexpr i32 MAX_GAMEPADS = 4;

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

    // Gamepad state
    bool s_GamepadConnected[MAX_GAMEPADS] = {};
    bool s_GamepadButtons[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS] = {};
    bool s_GamepadButtonsPrev[MAX_GAMEPADS][MAX_GAMEPAD_BUTTONS] = {};
    f32 s_GamepadAxes[MAX_GAMEPADS][MAX_GAMEPAD_AXES] = {};
    f32 s_GamepadDeadZone = 0.15f;
    bool s_GamepadActiveThisFrame[MAX_GAMEPADS] = {};

    // Apply dead zone to a single axis value
    f32 ApplyDeadZone(f32 value, f32 deadZone) {
        if (std::abs(value) < deadZone) return 0.0f;
        // Remap remaining range to 0..1
        f32 sign = value > 0.0f ? 1.0f : -1.0f;
        return sign * (std::abs(value) - deadZone) / (1.0f - deadZone);
    }

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

    // Poll gamepad state
    for (i32 gp = 0; gp < MAX_GAMEPADS; ++gp) {
        // Copy previous button state
        std::memcpy(s_GamepadButtonsPrev[gp], s_GamepadButtons[gp], sizeof(s_GamepadButtons[gp]));
        s_GamepadActiveThisFrame[gp] = false;

        i32 joyId = GLFW_JOYSTICK_1 + gp;
        if (glfwJoystickPresent(joyId) && glfwJoystickIsGamepad(joyId)) {
            s_GamepadConnected[gp] = true;

            GLFWgamepadstate state;
            if (glfwGetGamepadState(joyId, &state)) {
                // Buttons
                for (i32 b = 0; b < MAX_GAMEPAD_BUTTONS; ++b) {
                    s_GamepadButtons[gp][b] = (state.buttons[b] == GLFW_PRESS);
                    if (s_GamepadButtons[gp][b]) {
                        s_GamepadActiveThisFrame[gp] = true;
                    }
                }

                // Axes
                for (i32 a = 0; a < MAX_GAMEPAD_AXES; ++a) {
                    s_GamepadAxes[gp][a] = state.axes[a];
                    if (std::abs(state.axes[a]) > s_GamepadDeadZone) {
                        s_GamepadActiveThisFrame[gp] = true;
                    }
                }
            }
        } else {
            s_GamepadConnected[gp] = false;
            std::memset(s_GamepadButtons[gp], 0, sizeof(s_GamepadButtons[gp]));
            std::memset(s_GamepadAxes[gp], 0, sizeof(s_GamepadAxes[gp]));
        }
    }
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

// --- Gamepad implementation ---

bool Input::IsGamepadConnected(i32 gamepadIndex) {
    if (gamepadIndex < 0 || gamepadIndex >= MAX_GAMEPADS) return false;
    return s_GamepadConnected[gamepadIndex];
}

const char* Input::GetGamepadName(i32 gamepadIndex) {
    if (gamepadIndex < 0 || gamepadIndex >= MAX_GAMEPADS) return "None";
    i32 joyId = GLFW_JOYSTICK_1 + gamepadIndex;
    if (glfwJoystickIsGamepad(joyId)) {
        const char* name = glfwGetGamepadName(joyId);
        return name ? name : "Unknown Gamepad";
    }
    return "None";
}

bool Input::IsGamepadButtonDown(GamepadButton button, i32 gamepadIndex) {
    if (gamepadIndex < 0 || gamepadIndex >= MAX_GAMEPADS) return false;
    i32 b = static_cast<i32>(button);
    if (b < 0 || b >= MAX_GAMEPAD_BUTTONS) return false;
    return s_GamepadButtons[gamepadIndex][b];
}

bool Input::IsGamepadButtonPressed(GamepadButton button, i32 gamepadIndex) {
    if (gamepadIndex < 0 || gamepadIndex >= MAX_GAMEPADS) return false;
    i32 b = static_cast<i32>(button);
    if (b < 0 || b >= MAX_GAMEPAD_BUTTONS) return false;
    return s_GamepadButtons[gamepadIndex][b] && !s_GamepadButtonsPrev[gamepadIndex][b];
}

bool Input::IsGamepadButtonReleased(GamepadButton button, i32 gamepadIndex) {
    if (gamepadIndex < 0 || gamepadIndex >= MAX_GAMEPADS) return false;
    i32 b = static_cast<i32>(button);
    if (b < 0 || b >= MAX_GAMEPAD_BUTTONS) return false;
    return !s_GamepadButtons[gamepadIndex][b] && s_GamepadButtonsPrev[gamepadIndex][b];
}

f32 Input::GetGamepadAxis(GamepadAxis axis, i32 gamepadIndex) {
    if (gamepadIndex < 0 || gamepadIndex >= MAX_GAMEPADS) return 0.0f;
    i32 a = static_cast<i32>(axis);
    if (a < 0 || a >= MAX_GAMEPAD_AXES) return 0.0f;
    return ApplyDeadZone(s_GamepadAxes[gamepadIndex][a], s_GamepadDeadZone);
}

Math::Vector2 Input::GetGamepadLeftStick(i32 gamepadIndex) {
    f32 x = GetGamepadAxis(GamepadAxis::LeftX, gamepadIndex);
    f32 y = GetGamepadAxis(GamepadAxis::LeftY, gamepadIndex);
    // Apply circular dead zone (better than per-axis)
    f32 mag = std::sqrt(x * x + y * y);
    if (mag < s_GamepadDeadZone) return Math::Vector2(0.0f, 0.0f);
    f32 norm = (mag - s_GamepadDeadZone) / (1.0f - s_GamepadDeadZone);
    if (norm > 1.0f) norm = 1.0f;
    return Math::Vector2(x / mag * norm, y / mag * norm);
}

Math::Vector2 Input::GetGamepadRightStick(i32 gamepadIndex) {
    f32 x = GetGamepadAxis(GamepadAxis::RightX, gamepadIndex);
    f32 y = GetGamepadAxis(GamepadAxis::RightY, gamepadIndex);
    f32 mag = std::sqrt(x * x + y * y);
    if (mag < s_GamepadDeadZone) return Math::Vector2(0.0f, 0.0f);
    f32 norm = (mag - s_GamepadDeadZone) / (1.0f - s_GamepadDeadZone);
    if (norm > 1.0f) norm = 1.0f;
    return Math::Vector2(x / mag * norm, y / mag * norm);
}

f32 Input::GetGamepadLeftTrigger(i32 gamepadIndex) {
    // GLFW triggers: -1 released, +1 pressed. Remap to 0..1.
    f32 raw = GetGamepadAxis(GamepadAxis::LeftTrigger, gamepadIndex);
    return (raw + 1.0f) * 0.5f;
}

f32 Input::GetGamepadRightTrigger(i32 gamepadIndex) {
    f32 raw = GetGamepadAxis(GamepadAxis::RightTrigger, gamepadIndex);
    return (raw + 1.0f) * 0.5f;
}

void Input::SetGamepadDeadZone(f32 deadZone) {
    s_GamepadDeadZone = deadZone;
}

f32 Input::GetGamepadDeadZone() {
    return s_GamepadDeadZone;
}

bool Input::IsGamepadActive(i32 gamepadIndex) {
    if (gamepadIndex < 0 || gamepadIndex >= MAX_GAMEPADS) return false;
    return s_GamepadActiveThisFrame[gamepadIndex];
}

} // namespace Enjin
