#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {

// Forward declaration
class Window;

// Key codes (matching GLFW key codes for easy mapping)
enum class KeyCode : i32 {
    Unknown = -1,

    // Printable keys
    Space = 32,
    Apostrophe = 39,
    Comma = 44,
    Minus = 45,
    Period = 46,
    Slash = 47,

    Num0 = 48, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    Semicolon = 59,
    Equal = 61,

    A = 65, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    LeftBracket = 91,
    Backslash = 92,
    RightBracket = 93,
    GraveAccent = 96,

    // Function keys
    Escape = 256,
    Enter = 257,
    Tab = 258,
    Backspace = 259,
    Insert = 260,
    Delete = 261,
    Right = 262,
    Left = 263,
    Down = 264,
    Up = 265,
    PageUp = 266,
    PageDown = 267,
    Home = 268,
    End = 269,
    CapsLock = 280,
    ScrollLock = 281,
    NumLock = 282,
    PrintScreen = 283,
    Pause = 284,

    F1 = 290, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Keypad
    KP0 = 320, KP1, KP2, KP3, KP4, KP5, KP6, KP7, KP8, KP9,
    KPDecimal = 330,
    KPDivide = 331,
    KPMultiply = 332,
    KPSubtract = 333,
    KPAdd = 334,
    KPEnter = 335,
    KPEqual = 336,

    // Modifiers
    LeftShift = 340,
    LeftControl = 341,
    LeftAlt = 342,
    LeftSuper = 343,
    RightShift = 344,
    RightControl = 345,
    RightAlt = 346,
    RightSuper = 347,
    Menu = 348
};

enum class MouseButton : i32 {
    Left = 0,
    Right = 1,
    Middle = 2,
    Button4 = 3,
    Button5 = 4,
    Button6 = 5,
    Button7 = 6,
    Button8 = 7
};

// Gamepad buttons (matching GLFW gamepad buttons - Xbox layout)
enum class GamepadButton : i32 {
    A = 0,              // Cross (PlayStation)
    B = 1,              // Circle
    X = 2,              // Square
    Y = 3,              // Triangle
    LeftBumper = 4,     // L1
    RightBumper = 5,    // R1
    Back = 6,           // Select / Share
    Start = 7,          // Options
    Guide = 8,          // PS / Xbox button
    LeftStick = 9,      // L3
    RightStick = 10,    // R3
    DPadUp = 11,
    DPadRight = 12,
    DPadDown = 13,
    DPadLeft = 14,
    Count = 15
};

// Gamepad axes (matching GLFW gamepad axes)
enum class GamepadAxis : i32 {
    LeftX = 0,
    LeftY = 1,
    RightX = 2,
    RightY = 3,
    LeftTrigger = 4,    // L2 (-1 released, +1 pressed)
    RightTrigger = 5,   // R2
    Count = 6
};

// Input system - provides keyboard, mouse, and gamepad input state
class ENJIN_API Input {
public:
    // Initialize the input system with a window
    static void Initialize(Window* window);

    // Call once per frame to update input state (tracks pressed/released)
    static void Update();

    // Keyboard queries
    static bool IsKeyDown(KeyCode key);      // True while key is held
    static bool IsKeyPressed(KeyCode key);   // True only on frame key was pressed
    static bool IsKeyReleased(KeyCode key);  // True only on frame key was released

    // Mouse button queries
    static bool IsMouseButtonDown(MouseButton button);
    static bool IsMouseButtonPressed(MouseButton button);
    static bool IsMouseButtonReleased(MouseButton button);

    // Mouse position
    static Math::Vector2 GetMousePosition();
    static f32 GetMouseX();
    static f32 GetMouseY();

    // Mouse delta (movement since last frame)
    static Math::Vector2 GetMouseDelta();

    // Mouse scroll
    static Math::Vector2 GetScrollDelta();

    // --- Replay injection (deterministic playback) ---
    // When enabled, Update() keeps its previous-frame bookkeeping (so
    // pressed/released edges work) but the CURRENT frame's key/mouse state is
    // forced from the last injected snapshot instead of the hardware. Feed one
    // snapshot per fixed-step frame to replay a recorded session exactly.
    static void SetReplayInjection(bool enabled);
    static bool IsReplayInjectionActive();
    // keysDown: 512 bools indexed by KeyCode; mouseDown: 8 bools.
    static void InjectFrameState(const bool* keysDown, const bool* mouseDown,
                                 Math::Vector2 mousePos);
    // Snapshot the live state (for the recorder). Buffers sized as above.
    static void CaptureFrameState(bool* keysDown, bool* mouseDown,
                                  Math::Vector2& mousePos);

    // Mouse capture (hide cursor and lock to window)
    static void SetMouseCaptured(bool captured);
    static bool IsMouseCaptured();

    // Cursor visibility
    static void SetCursorVisible(bool visible);
    static bool IsCursorVisible();

    // --- Gamepad ---

    // Connection state (checks joystick slot 0 by default, supports up to 4)
    static bool IsGamepadConnected(i32 gamepadIndex = 0);
    static const char* GetGamepadName(i32 gamepadIndex = 0);

    // Button queries (gamepad index 0 by default)
    static bool IsGamepadButtonDown(GamepadButton button, i32 gamepadIndex = 0);
    static bool IsGamepadButtonPressed(GamepadButton button, i32 gamepadIndex = 0);
    static bool IsGamepadButtonReleased(GamepadButton button, i32 gamepadIndex = 0);

    // Axis queries (-1.0 to +1.0, triggers: -1.0 released, +1.0 pressed)
    // Dead zone is applied automatically
    static f32 GetGamepadAxis(GamepadAxis axis, i32 gamepadIndex = 0);

    // Convenience: get stick as Vector2 with dead zone applied
    static Math::Vector2 GetGamepadLeftStick(i32 gamepadIndex = 0);
    static Math::Vector2 GetGamepadRightStick(i32 gamepadIndex = 0);

    // Convenience: get trigger as 0-1 range (remapped from -1..+1)
    static f32 GetGamepadLeftTrigger(i32 gamepadIndex = 0);
    static f32 GetGamepadRightTrigger(i32 gamepadIndex = 0);

    // Dead zone (default 0.15)
    static void SetGamepadDeadZone(f32 deadZone);
    static f32 GetGamepadDeadZone();

    // Any gamepad input detected this frame (for auto-switching UI prompts)
    static bool IsGamepadActive(i32 gamepadIndex = 0);

    // Raw mouse input (bypasses OS acceleration/smoothing)
    static void SetRawMouseInput(bool enabled);
    static bool IsRawMouseInput();

    // Mouse smoothing (0.0 = none, 1.0 = heavy temporal smoothing)
    static void SetMouseSmoothing(f32 amount);
    static f32 GetMouseSmoothing();

    // Clear all input state (call on focus loss to prevent stuck keys)
    static void ClearAllState();

private:
    Input() = default;
};

} // namespace Enjin
