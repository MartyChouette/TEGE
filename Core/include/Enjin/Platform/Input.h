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

// Input system - provides keyboard and mouse input state
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

    // Mouse capture (hide cursor and lock to window)
    static void SetMouseCaptured(bool captured);
    static bool IsMouseCaptured();

    // Cursor visibility
    static void SetCursorVisible(bool visible);
    static bool IsCursorVisible();

private:
    Input() = default;
};

} // namespace Enjin
