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

    // Synthesize a one-frame key press (e.g. a touch button standing in for a
    // hardware key). On web it sets the same down-latch the browser keydown uses,
    // so the next Update raises IsKeyPressed for exactly one frame.
    static void InjectKeyPress(KeyCode key);

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
    // While injection is active the REAL hardware state is kept alongside the
    // injected stream. Inside a real-input scope, IsKeyDown / mouse queries
    // answer from the hardware instead - this is how the editor's free camera
    // flies during replay playback without the recorded WASD steering it.
    // (Edge queries - pressed/released - stay on the injected stream.)
    static void BeginRealInputScope();
    static void EndRealInputScope();

    // ---- Mobile touch controls ---------------------------------------------
    // The on-screen overlay is a left-side floating move stick, an optional
    // right-side look-drag region, and a set of anchored action buttons. The
    // scheme is built by Engine (InputSystem::ApplyTouchPreset, from the
    // GameAction table) or authored by a game (SetTouchScheme); Core owns only
    // hit-zones, geometry and per-frame state. Real touches come from the
    // browser; desktop and the editor can SIMULATE one touch from the mouse
    // (SetTouchSimulation) inside a registered surface (SetTouchSurface), so a
    // layout is testable without a phone. Safe-area insets keep it clear of
    // notches/rounded corners on web.
    static constexpr int kMaxTouchButtons = 6;

    // One anchored on-screen button. Position is a grid slot measured from the
    // bottom-right of the SAFE area, in multiples of the button spacing, so a
    // cluster scales together with the button size.
    struct TouchButtonDef {
        f32 radiusFrac = 0.075f;   // radius as a fraction of safe-area height
        f32 colFromRight = 0.0f;   // 0 = rightmost column, grows leftward
        f32 rowFromBottom = 0.0f;  // 0 = bottom row, grows upward
        int keyCode = 0;           // key held while pressed; negative = mouse
                                   // button (-1 => button 0, i.e. fire/click)
        int action = -1;           // InputSystem::GameAction id, or -1 for none.
                                   // When >= 0 and an action resolver is set, the
                                   // button presses the action's CURRENT binding
                                   // (so rebinding updates touch), falling back to
                                   // keyCode if the action has no key/mouse bind.
        char label[8] = {0};
    };

    struct TouchScheme {
        bool moveStick = true;
        int  stickKeys[4] = {65, 68, 87, 83};   // Left,Right,Up,Down (A D W S)
        int  stickActions[4] = {-1,-1,-1,-1};    // MoveLeft/Right/Forward/Back action
                                                 // ids, or -1; resolved like buttons.
        bool lookRegion = true;                  // right side drags the camera
        f32  moveZoneSplit = 0.45f;              // x fraction owned by the stick
        // Accessibility: mirror the whole layout for left-handed play (stick on
        // the RIGHT, buttons growing from the bottom-LEFT), and scale every
        // button for reach or low vision. Both are player/project settings.
        bool leftHanded = false;
        f32  buttonScale = 1.0f;                 // 0.5 - 2.0
        TouchButtonDef buttons[kMaxTouchButtons];
        int  buttonCount = 0;
    };

    static void SetTouchScheme(const TouchScheme& scheme);
    static const TouchScheme& GetTouchScheme();

    // Desktop / editor: let the mouse stand in for one touch (LMB down = touch
    // start at the cursor, drag = move, up = end). A touch claimed by the stick
    // or an action button is swallowed (never reaches the game as LMB), like
    // the browser's preventDefault; a look/tap touch passes LMB through. No-op
    // on web, where real touches drive the overlay.
    static void SetTouchSimulation(bool enabled);
    static bool IsTouchSimulation();
    // The rect (window pixels, same space as GetMousePosition) the game view
    // occupies: whole window in the player, the Game View image in the editor.
    // Touch geometry and the overlay live inside it. Ignored on web (canvas).
    static void SetTouchSurface(f32 x0, f32 y0, f32 w, f32 h);

    // Binding reflection: the touch overlay lives in this platform layer but the
    // action-binding map (InputActionMap) lives in Engine, which depends on Core
    // (not the reverse). So Engine INJECTS resolvers here: given a GameAction id,
    // return its current key code (negative = mouse button, Core's encoding) or
    // kTouchNoBinding to fall back to the button's static keyCode; and a short
    // label for the current binding (nullptr/empty = keep the static label).
    // With these set, touch buttons/stick reflect live rebinds automatically.
    static constexpr int kTouchNoBinding = (-2147483647 - 1);
    using ActionKeyResolver = int (*)(int action);
    using ActionLabelResolver = const char* (*)(int action);
    static void SetActionKeyResolver(ActionKeyResolver resolver);
    static void SetActionLabelResolver(ActionLabelResolver resolver);

    // Resolved, per-frame overlay geometry for the host to draw (Engine's
    // InputSystem::DrawTouchOverlay does it). Positions are in canvas BACKING
    // pixels on web, window pixels elsewhere. Active on web once a touch is
    // seen OR a coarse pointer (phone/tablet) is detected; on desktop while
    // simulation is on and a surface is registered.
    struct TouchOverlayState {
        bool active = false;
        bool showStick = true;
        bool stickHeld = false;
        f32 stickBaseX = 0, stickBaseY = 0;
        f32 stickNubX = 0, stickNubY = 0;
        f32 stickRadius = 0;
        struct Button { f32 x = 0, y = 0, r = 0; bool held = false; char label[8] = {0}; };
        Button buttons[kMaxTouchButtons];
        int buttonCount = 0;
    };
    static TouchOverlayState GetTouchOverlay();

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
