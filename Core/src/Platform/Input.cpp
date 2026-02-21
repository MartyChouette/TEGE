#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Platform/Window.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <cmath>

#if ENJIN_PLATFORM_WEB
#include <emscripten.h>
#include <emscripten/html5.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

namespace Enjin {

namespace {
    constexpr i32 MAX_KEYS = 512;
    constexpr i32 MAX_MOUSE_BUTTONS = 8;
    constexpr i32 MAX_GAMEPAD_BUTTONS = 15;
    constexpr i32 MAX_GAMEPAD_AXES = 6;
    constexpr i32 MAX_GAMEPADS = 4;

#if !ENJIN_PLATFORM_WEB
    constexpr i32 GLFW_KEY_FIRST = 32;   // GLFW_KEY_SPACE is the first valid key
    constexpr i32 GLFW_KEY_LAST_VALID = 348;  // GLFW_KEY_LAST
    GLFWwindow* s_Window = nullptr;
#endif

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

    // Raw mouse input and smoothing
    bool s_UseRawInput = true;
    f32 s_MouseSmoothAmount = 0.0f;
    Math::Vector2 s_SmoothedDelta = {};

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

#if !ENJIN_PLATFORM_WEB
    // GLFW scroll callback
    void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        (void)window;
        s_ScrollAccumulator.x += static_cast<f32>(xoffset);
        s_ScrollAccumulator.y += static_cast<f32>(yoffset);
    }
#endif

#if ENJIN_PLATFORM_WEB
    // --- Emscripten event callbacks ---
    EM_BOOL WebKeyCallback(int eventType, const EmscriptenKeyboardEvent* e, void* userData) {
        (void)userData;
        // Map DOM key codes to our key array (DOM codes overlap with GLFW for ASCII range)
        i32 keyCode = static_cast<i32>(e->keyCode);
        if (keyCode >= 0 && keyCode < MAX_KEYS) {
            s_KeysDown[keyCode] = (eventType == EMSCRIPTEN_EVENT_KEYDOWN);
        }
        return EM_TRUE;
    }

    EM_BOOL WebMouseMoveCallback(int eventType, const EmscriptenMouseEvent* e, void* userData) {
        (void)eventType; (void)userData;
        s_MousePosition = Math::Vector2(static_cast<f32>(e->targetX), static_cast<f32>(e->targetY));
        return EM_TRUE;
    }

    EM_BOOL WebMouseButtonCallback(int eventType, const EmscriptenMouseEvent* e, void* userData) {
        (void)userData;
        i32 button = static_cast<i32>(e->button);
        if (button >= 0 && button < MAX_MOUSE_BUTTONS) {
            s_MouseButtonsDown[button] = (eventType == EMSCRIPTEN_EVENT_MOUSEDOWN);
        }
        return EM_TRUE;
    }

    EM_BOOL WebWheelCallback(int eventType, const EmscriptenWheelEvent* e, void* userData) {
        (void)eventType; (void)userData;
        s_ScrollAccumulator.x += static_cast<f32>(e->deltaX) * -0.01f;
        s_ScrollAccumulator.y += static_cast<f32>(e->deltaY) * -0.01f;
        return EM_TRUE;
    }
#endif
}

void Input::Initialize(Window* window) {
    if (!window) {
        ENJIN_LOG_ERROR(Core, "Input::Initialize called with null window");
        return;
    }

    // Clear state
    std::memset(s_KeysDown, 0, sizeof(s_KeysDown));
    std::memset(s_KeysDownPrev, 0, sizeof(s_KeysDownPrev));
    std::memset(s_MouseButtonsDown, 0, sizeof(s_MouseButtonsDown));
    std::memset(s_MouseButtonsDownPrev, 0, sizeof(s_MouseButtonsDownPrev));

#if ENJIN_PLATFORM_WEB
    // Register browser event handlers on the canvas
    const char* target = "#game-canvas";
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, false, WebKeyCallback);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, false, WebKeyCallback);
    emscripten_set_mousemove_callback(target, nullptr, false, WebMouseMoveCallback);
    emscripten_set_mousedown_callback(target, nullptr, false, WebMouseButtonCallback);
    emscripten_set_mouseup_callback(target, nullptr, false, WebMouseButtonCallback);
    emscripten_set_wheel_callback(target, nullptr, false, WebWheelCallback);
    s_FirstMouseMove = true;
#else
    s_Window = static_cast<GLFWwindow*>(window->GetNativeHandle());
    if (!s_Window) {
        ENJIN_LOG_ERROR(Core, "Failed to get GLFW window handle for input");
        return;
    }

    // Get initial mouse position
    double mx, my;
    glfwGetCursorPos(s_Window, &mx, &my);
    s_MousePosition = Math::Vector2(static_cast<f32>(mx), static_cast<f32>(my));
    s_MousePositionPrev = s_MousePosition;
    s_FirstMouseMove = true;

    // Set scroll callback
    glfwSetScrollCallback(s_Window, ScrollCallback);
#endif

    ENJIN_LOG_INFO(Core, "Input system initialized");
}

void Input::Update() {
    // Copy current state to previous
    std::memcpy(s_KeysDownPrev, s_KeysDown, sizeof(s_KeysDown));
    std::memcpy(s_MouseButtonsDownPrev, s_MouseButtonsDown, sizeof(s_MouseButtonsDown));

#if ENJIN_PLATFORM_WEB
    // On web, key/mouse state is updated via Emscripten callbacks (WebKeyCallback, etc.)
    // We just need to compute mouse delta and consume scroll accumulator.
#else
    if (!s_Window) return;

    // Poll keyboard state (GLFW keys are 32-348)
    for (i32 key = GLFW_KEY_FIRST; key <= GLFW_KEY_LAST_VALID; ++key) {
        s_KeysDown[key] = glfwGetKey(s_Window, key) == GLFW_PRESS;
    }

    // Poll mouse button state
    for (i32 button = 0; button < MAX_MOUSE_BUTTONS; ++button) {
        s_MouseButtonsDown[button] = glfwGetMouseButton(s_Window, button) == GLFW_PRESS;
    }

    // Update mouse position
    double mx, my;
    glfwGetCursorPos(s_Window, &mx, &my);
    s_MousePosition = Math::Vector2(static_cast<f32>(mx), static_cast<f32>(my));
#endif

    // Calculate mouse delta
    s_MousePositionPrev = s_MousePosition; // Note: on web, position set by callback
    if (s_FirstMouseMove) {
        s_MouseDelta = Math::Vector2(0.0f, 0.0f);
        s_SmoothedDelta = Math::Vector2(0.0f, 0.0f);
        s_FirstMouseMove = false;
    } else {
        s_MouseDelta = s_MousePosition - s_MousePositionPrev;
    }

    // Apply temporal smoothing if enabled
    if (s_MouseSmoothAmount > 0.0f) {
        f32 t = 1.0f - s_MouseSmoothAmount * 0.9f;
        s_SmoothedDelta = s_SmoothedDelta + (s_MouseDelta - s_SmoothedDelta) * t;
        s_MouseDelta = s_SmoothedDelta;
    }

    // Update scroll delta and reset accumulator
    s_ScrollDelta = s_ScrollAccumulator;
    s_ScrollAccumulator = Math::Vector2(0.0f, 0.0f);

    // Poll gamepad state
    for (i32 gp = 0; gp < MAX_GAMEPADS; ++gp) {
        std::memcpy(s_GamepadButtonsPrev[gp], s_GamepadButtons[gp], sizeof(s_GamepadButtons[gp]));
        s_GamepadActiveThisFrame[gp] = false;

#if !ENJIN_PLATFORM_WEB
        i32 joyId = GLFW_JOYSTICK_1 + gp;
        if (glfwJoystickPresent(joyId) && glfwJoystickIsGamepad(joyId)) {
            s_GamepadConnected[gp] = true;
            GLFWgamepadstate state;
            if (glfwGetGamepadState(joyId, &state)) {
                for (i32 b = 0; b < MAX_GAMEPAD_BUTTONS; ++b) {
                    s_GamepadButtons[gp][b] = (state.buttons[b] == GLFW_PRESS);
                    if (s_GamepadButtons[gp][b]) s_GamepadActiveThisFrame[gp] = true;
                }
                for (i32 a = 0; a < MAX_GAMEPAD_AXES; ++a) {
                    s_GamepadAxes[gp][a] = state.axes[a];
                    if (std::abs(state.axes[a]) > s_GamepadDeadZone) s_GamepadActiveThisFrame[gp] = true;
                }
            }
        } else {
            s_GamepadConnected[gp] = false;
            std::memset(s_GamepadButtons[gp], 0, sizeof(s_GamepadButtons[gp]));
            std::memset(s_GamepadAxes[gp], 0, sizeof(s_GamepadAxes[gp]));
        }
#else
        // Gamepad API: Emscripten supports HTML5 Gamepad API but not yet wired up
        s_GamepadConnected[gp] = false;
#endif
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
    s_MouseCaptured = captured;
#if ENJIN_PLATFORM_WEB
    if (captured) {
        emscripten_request_pointerlock("#game-canvas", true);
        s_FirstMouseMove = true;
    } else {
        emscripten_exit_pointerlock();
    }
#else
    if (!s_Window) return;
    if (captured) {
        glfwSetInputMode(s_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (s_UseRawInput && glfwRawMouseMotionSupported()) {
            glfwSetInputMode(s_Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
        s_FirstMouseMove = true;
    } else {
        glfwSetInputMode(s_Window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        glfwSetInputMode(s_Window, GLFW_CURSOR, s_CursorVisible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
#endif
}

bool Input::IsMouseCaptured() {
    return s_MouseCaptured;
}

void Input::SetCursorVisible(bool visible) {
    s_CursorVisible = visible;
#if !ENJIN_PLATFORM_WEB
    if (!s_Window) return;
    if (!s_MouseCaptured) {
        glfwSetInputMode(s_Window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
#endif
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

void Input::SetRawMouseInput(bool enabled) {
    s_UseRawInput = enabled;
#if !ENJIN_PLATFORM_WEB
    // Apply immediately if currently captured
    if (s_Window && s_MouseCaptured) {
        if (enabled && glfwRawMouseMotionSupported()) {
            glfwSetInputMode(s_Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        } else {
            glfwSetInputMode(s_Window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        }
    }
#endif
}

bool Input::IsRawMouseInput() {
    return s_UseRawInput;
}

void Input::SetMouseSmoothing(f32 amount) {
    s_MouseSmoothAmount = amount;
    if (amount <= 0.0f) {
        s_SmoothedDelta = Math::Vector2(0.0f, 0.0f);
    }
}

f32 Input::GetMouseSmoothing() {
    return s_MouseSmoothAmount;
}

void Input::ClearAllState() {
    std::memset(s_KeysDown, 0, sizeof(s_KeysDown));
    std::memset(s_KeysDownPrev, 0, sizeof(s_KeysDownPrev));
    std::memset(s_MouseButtonsDown, 0, sizeof(s_MouseButtonsDown));
    std::memset(s_MouseButtonsDownPrev, 0, sizeof(s_MouseButtonsDownPrev));
    s_MouseDelta = Math::Vector2(0.0f, 0.0f);
    s_ScrollDelta = Math::Vector2(0.0f, 0.0f);
    s_ScrollAccumulator = Math::Vector2(0.0f, 0.0f);
    s_FirstMouseMove = true;
    for (i32 gp = 0; gp < MAX_GAMEPADS; ++gp) {
        std::memset(s_GamepadButtons[gp], 0, sizeof(s_GamepadButtons[gp]));
        std::memset(s_GamepadButtonsPrev[gp], 0, sizeof(s_GamepadButtonsPrev[gp]));
        std::memset(s_GamepadAxes[gp], 0, sizeof(s_GamepadAxes[gp]));
        s_GamepadActiveThisFrame[gp] = false;
    }
}

} // namespace Enjin
