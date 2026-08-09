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

#if ENJIN_PLATFORM_WEB
    // Browser events land BETWEEN frames, so they must never write s_KeysDown
    // directly: Update() copies current->previous first, and a write that
    // arrived before the copy makes curr==prev, erasing the pressed edge
    // (IsKeyPressed/IsMouseButtonPressed never fire — Tab and UI clicks dead).
    // Callbacks write this pending state; Update() applies it AFTER the copy.
    // The down-latch keeps a press visible for one frame even if the release
    // also arrived within the same frame gap.
    bool s_WebKeysLatest[MAX_KEYS] = {};
    bool s_WebKeysDownLatch[MAX_KEYS] = {};
    bool s_WebMouseLatest[MAX_MOUSE_BUTTONS] = {};
    bool s_WebMouseDownLatch[MAX_MOUSE_BUTTONS] = {};
#endif

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
    // Pointer-lock mouse delta accumulator (movementX/Y is relative)
    Math::Vector2 s_WebMouseMovementAccum = {};

    // Map DOM keyCode → our (GLFW-aligned) KeyCode for special keys.
    // Printable ASCII (A-Z, 0-9, Space) match natively, so they're not in the table.
    i32 MapDomKeyCode(i32 dom) {
        switch (dom) {
            case 8:   return 259; // Backspace
            case 9:   return 258; // Tab
            case 13:  return 257; // Enter
            case 16:  return 340; // Shift  -> LeftShift
            case 17:  return 341; // Ctrl   -> LeftControl
            case 18:  return 342; // Alt    -> LeftAlt
            case 27:  return 256; // Escape
            case 33:  return 266; // PageUp
            case 34:  return 267; // PageDown
            case 35:  return 269; // End
            case 36:  return 268; // Home
            case 37:  return 263; // Left
            case 38:  return 265; // Up
            case 39:  return 262; // Right
            case 40:  return 264; // Down
            case 45:  return 260; // Insert
            case 46:  return 261; // Delete
            case 112: return 290; // F1
            case 113: return 291; // F2
            case 114: return 292; case 115: return 293; case 116: return 294;
            case 117: return 295; case 118: return 296; case 119: return 297;
            case 120: return 298; case 121: return 299; case 122: return 300;
            case 123: return 301;
            default:  return dom; // Letters/digits already match
        }
    }

    // --- Emscripten event callbacks ---
    EM_BOOL WebKeyCallback(int eventType, const EmscriptenKeyboardEvent* e, void* userData) {
        (void)userData;
        i32 keyCode = MapDomKeyCode(static_cast<i32>(e->keyCode));
        if (keyCode >= 0 && keyCode < MAX_KEYS) {
            bool down = (eventType == EMSCRIPTEN_EVENT_KEYDOWN);
            s_WebKeysLatest[keyCode] = down;
            if (down) s_WebKeysDownLatch[keyCode] = true;
        }
        // Prevent browser from scrolling on space/arrows when game has focus
        return EM_TRUE;
    }

    EM_BOOL WebMouseMoveCallback(int eventType, const EmscriptenMouseEvent* e, void* userData) {
        (void)eventType; (void)userData;
        // targetX/Y are CSS pixels but the canvas backing store (and every UI
        // rect the engine tests against) is devicePixelRatio-scaled — without
        // this, clicks land short of the real UI at any display scale != 100%
        f32 dpr = static_cast<f32>(emscripten_get_device_pixel_ratio());
        if (dpr <= 0.0f) dpr = 1.0f;
        s_MousePosition = Math::Vector2(static_cast<f32>(e->targetX) * dpr,
                                        static_cast<f32>(e->targetY) * dpr);
        // Accumulate relative movement for pointer-locked look (clamp to prevent spikes)
        f32 dx = static_cast<f32>(e->movementX);
        f32 dy = static_cast<f32>(e->movementY);
        constexpr f32 MAX_DELTA = 150.0f;
        if (dx > -MAX_DELTA && dx < MAX_DELTA) s_WebMouseMovementAccum.x += dx;
        if (dy > -MAX_DELTA && dy < MAX_DELTA) s_WebMouseMovementAccum.y += dy;
        return EM_TRUE;
    }

    EM_BOOL WebMouseButtonCallback(int eventType, const EmscriptenMouseEvent* e, void* userData) {
        (void)userData;
        i32 button = static_cast<i32>(e->button);
        if (button >= 0 && button < MAX_MOUSE_BUTTONS) {
            bool down = (eventType == EMSCRIPTEN_EVENT_MOUSEDOWN);
            s_WebMouseLatest[button] = down;
            if (down) s_WebMouseDownLatch[button] = true;
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
#if !ENJIN_PLATFORM_WEB
    if (!window) {
        ENJIN_LOG_ERROR(Core, "Input::Initialize called with null window");
        return;
    }
#else
    (void)window;  // Web uses canvas selectors directly, no Window object needed
#endif

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
    // Enable Gamepad API (must be called before polling gamepad state)
    emscripten_sample_gamepad_data();
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
    // Apply the async browser event state now that previous is snapshotted.
    // A latched down forces the key visible for this one frame even if its
    // keyup already arrived; the latch then clears so the release edge
    // follows next frame.
    for (i32 key = 0; key < MAX_KEYS; ++key) {
        s_KeysDown[key] = s_WebKeysLatest[key] || s_WebKeysDownLatch[key];
        s_WebKeysDownLatch[key] = false;
    }
    for (i32 button = 0; button < MAX_MOUSE_BUTTONS; ++button) {
        s_MouseButtonsDown[button] = s_WebMouseLatest[button] || s_WebMouseDownLatch[button];
        s_WebMouseDownLatch[button] = false;
    }
    // For pointer-locked mode, prefer accumulated movementX/Y over position deltas.
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
    if (s_FirstMouseMove) {
        s_MouseDelta = Math::Vector2(0.0f, 0.0f);
        s_SmoothedDelta = Math::Vector2(0.0f, 0.0f);
        s_FirstMouseMove = false;
#if ENJIN_PLATFORM_WEB
        s_WebMouseMovementAccum = Math::Vector2(0.0f, 0.0f);
#endif
    } else {
#if ENJIN_PLATFORM_WEB
        // Use accumulated relative movement (works for both pointer-locked and free cursor)
        s_MouseDelta = s_WebMouseMovementAccum;
        s_WebMouseMovementAccum = Math::Vector2(0.0f, 0.0f);
#else
        s_MouseDelta = s_MousePosition - s_MousePositionPrev;
#endif
    }
    s_MousePositionPrev = s_MousePosition;

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
        GLFWgamepadstate state;
        // Connected ONLY when a full state read succeeds. Marking connected
        // before the read left the axes zeroed on failure — and a raw trigger
        // value of 0.0 remaps to 0.5 "half pulled" in GetGamepad*Trigger
        // (released is -1), which pushed the editor fly camera straight down
        // as if it had gravity.
        if (glfwJoystickPresent(joyId) && glfwJoystickIsGamepad(joyId) &&
            glfwGetGamepadState(joyId, &state)) {
            s_GamepadConnected[gp] = true;
            for (i32 b = 0; b < MAX_GAMEPAD_BUTTONS; ++b) {
                s_GamepadButtons[gp][b] = (state.buttons[b] == GLFW_PRESS);
                if (s_GamepadButtons[gp][b]) s_GamepadActiveThisFrame[gp] = true;
            }
            for (i32 a = 0; a < MAX_GAMEPAD_AXES; ++a) {
                s_GamepadAxes[gp][a] = state.axes[a];
                if (std::abs(state.axes[a]) > s_GamepadDeadZone) s_GamepadActiveThisFrame[gp] = true;
            }
            // Guard against mappings that carry no trigger entries (pads with
            // digital triggers map LT/RT as buttons): an untouched slot reads
            // exactly 0.0, which is "half pulled" in trigger convention. Snap
            // it to released. A real analog trigger never rests at exactly 0.
            if (s_GamepadAxes[gp][4] == 0.0f) s_GamepadAxes[gp][4] = -1.0f;
            if (s_GamepadAxes[gp][5] == 0.0f) s_GamepadAxes[gp][5] = -1.0f;
        } else {
            s_GamepadConnected[gp] = false;
            std::memset(s_GamepadButtons[gp], 0, sizeof(s_GamepadButtons[gp]));
            std::memset(s_GamepadAxes[gp], 0, sizeof(s_GamepadAxes[gp]));
            // Triggers rest at -1, not 0 (see above).
            s_GamepadAxes[gp][4] = -1.0f;
            s_GamepadAxes[gp][5] = -1.0f;
        }
#else
        // HTML5 Gamepad API via Emscripten
        // Note: emscripten_sample_gamepad_data() must be called once per frame before queries
        if (gp == 0) {
            emscripten_sample_gamepad_data();
        }

        EmscriptenGamepadEvent gpEvent;
        if (emscripten_get_gamepad_status(gp, &gpEvent) == EMSCRIPTEN_RESULT_SUCCESS && gpEvent.connected) {
            s_GamepadConnected[gp] = true;

            // Map standard gamepad layout (Chrome/Firefox follow W3C standard mapping)
            // W3C buttons: 0=A 1=B 2=X 3=Y 4=LB 5=RB 6=LT 7=RT 8=Back 9=Start
            //              10=LStick 11=RStick 12=DUp 13=DDown 14=DLeft 15=DRight 16=Guide
            // Our enum:    0=A 1=B 2=X 3=Y 4=LB 5=RB 6=Back 7=Start 8=Guide
            //              9=LStick 10=RStick 11=DUp 12=DRight 13=DDown 14=DLeft
            // W3C standard mapping (17 entries, index 16 = Guide button)
            static const i32 webToOurButton[17] = {
                0, 1, 2, 3,         // A B X Y
                4, 5,               // LB RB
                -1, -1,             // LT RT (handled as axes below)
                6, 7,               // Back Start
                9, 10,              // LStick RStick
                11, 13, 14, 12,     // DUp DDown DLeft DRight (our DRight=12, DDown=13, DLeft=14)
                8                   // Guide
            };
            for (i32 wb = 0; wb < gpEvent.numButtons && wb < 17; ++wb) {
                i32 ourB = webToOurButton[wb];
                if (ourB >= 0 && ourB < MAX_GAMEPAD_BUTTONS) {
                    s_GamepadButtons[gp][ourB] = gpEvent.digitalButton[wb] != 0;
                    if (s_GamepadButtons[gp][ourB]) s_GamepadActiveThisFrame[gp] = true;
                }
            }

            // Axes: W3C 0=LX 1=LY 2=RX 3=RY (no triggers as axes — those are buttons 6,7)
            // Our enum: 0=LX 1=LY 2=RX 3=RY 4=LT 5=RT
            for (i32 a = 0; a < gpEvent.numAxes && a < 4; ++a) {
                s_GamepadAxes[gp][a] = static_cast<f32>(gpEvent.axis[a]);
                if (std::abs(s_GamepadAxes[gp][a]) > s_GamepadDeadZone) s_GamepadActiveThisFrame[gp] = true;
            }
            // Triggers: W3C reports as buttons 6 (LT) and 7 (RT) with analog values 0..1.
            // Our convention: -1 released, +1 pressed. Remap. A pad without those
            // buttons gets explicit released (-1) — a 0.0 slot would remap to
            // "half pulled" downstream.
            s_GamepadAxes[gp][4] = (gpEvent.numButtons > 6)
                ? static_cast<f32>(gpEvent.analogButton[6]) * 2.0f - 1.0f : -1.0f;
            s_GamepadAxes[gp][5] = (gpEvent.numButtons > 7)
                ? static_cast<f32>(gpEvent.analogButton[7]) * 2.0f - 1.0f : -1.0f;
        } else {
            s_GamepadConnected[gp] = false;
            std::memset(s_GamepadButtons[gp], 0, sizeof(s_GamepadButtons[gp]));
            std::memset(s_GamepadAxes[gp], 0, sizeof(s_GamepadAxes[gp]));
            // Triggers rest at -1, not 0.
            s_GamepadAxes[gp][4] = -1.0f;
            s_GamepadAxes[gp][5] = -1.0f;
        }
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
    // Mirror intent to JS: the canvas click handler re-locks the pointer only
    // while the game WANTS capture. Without this, releasing the cursor for
    // on-screen UI (Web Demo's Tab menu mode) re-locked on the first menu
    // click and made the UI unusable.
    EM_ASM({ Module.tegeWantPointerLock = $0 ? true : false; }, captured ? 1 : 0);
    if (captured) {
        emscripten_request_pointerlock("#game-canvas", true);
    } else {
        emscripten_exit_pointerlock();
    }
    s_FirstMouseMove = true;
#else
    if (!s_Window) return;
    if (captured) {
        glfwSetInputMode(s_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (s_UseRawInput && glfwRawMouseMotionSupported()) {
            glfwSetInputMode(s_Window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
        }
    } else {
        glfwSetInputMode(s_Window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
        glfwSetInputMode(s_Window, GLFW_CURSOR, s_CursorVisible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
    // BOTH transitions change the coordinate source (physical cursor vs the
    // unbounded virtual position of GLFW_CURSOR_DISABLED). Releasing without
    // resetting left the next frame's delta = physical - virtual: a massive
    // spike that whipped any camera reading GetMouseDelta that frame.
    s_FirstMouseMove = true;
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
#if !ENJIN_PLATFORM_WEB
    i32 joyId = GLFW_JOYSTICK_1 + gamepadIndex;
    if (glfwJoystickIsGamepad(joyId)) {
        const char* name = glfwGetGamepadName(joyId);
        return name ? name : "Unknown Gamepad";
    }
#endif
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

// Triggers rest at -1 and press toward +1, so the CENTERED deadzone in
// GetGamepadAxis is wrong for them twice over: it does nothing at the resting
// end, and it zeroes a slightly-off-center rest (raw -0.1 -> 0.0), which the
// 0..1 remap then turns into "half pulled" (0.5) — the editor fly camera sank
// as if under gravity from exactly this. Read the raw axis and apply the
// deadzone at the RELEASED end, after remapping.
static f32 RemapTriggerAxis(f32 raw, f32 deadZone) {
    f32 t = (raw + 1.0f) * 0.5f;
    return (t < deadZone) ? 0.0f : t;
}

f32 Input::GetGamepadLeftTrigger(i32 gamepadIndex) {
    if (gamepadIndex < 0 || gamepadIndex >= MAX_GAMEPADS) return 0.0f;
    return RemapTriggerAxis(s_GamepadAxes[gamepadIndex][static_cast<i32>(GamepadAxis::LeftTrigger)], s_GamepadDeadZone);
}

f32 Input::GetGamepadRightTrigger(i32 gamepadIndex) {
    if (gamepadIndex < 0 || gamepadIndex >= MAX_GAMEPADS) return 0.0f;
    return RemapTriggerAxis(s_GamepadAxes[gamepadIndex][static_cast<i32>(GamepadAxis::RightTrigger)], s_GamepadDeadZone);
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
