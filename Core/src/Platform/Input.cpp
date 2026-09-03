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

    // Injected by Engine (InputActionMap) so touch controls reflect live
    // bindings. Null until a game/editor wires an action map. Compiled on all
    // platforms; only read by the web touch path.
    Input::ActionKeyResolver   s_ActionKeyResolver   = nullptr;
    Input::ActionLabelResolver s_ActionLabelResolver = nullptr;

    // Input focus + UI pointer capture. One flag each, read by InputActionMap
    // and the controllers, so every runtime suppresses gameplay input the same
    // way instead of each keeping its own boolean.
    Input::InputFocus s_InputFocus = Input::InputFocus::Gameplay;
    bool s_UIConsumedPointer = false;
    Input::UIHitTestResolver s_UIHitTestResolver = nullptr;

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

    // Replay injection: forced per-frame state (see Input::SetReplayInjection)
    bool s_ReplayInjection = false;
    // Hardware state preserved during injection + the real-input scope flag
    bool s_RealKeysDown[MAX_KEYS] = {};
    bool s_RealMouseButtons[8] = {};
    Math::Vector2 s_RealMousePos{};
    Math::Vector2 s_RealMousePosPrev{};
    Math::Vector2 s_RealMouseDelta{};
    bool s_RealFirstMove = true;
    bool s_RealScope = false;
    bool s_InjectedKeys[MAX_KEYS] = {};
    bool s_InjectedMouse[MAX_MOUSE_BUTTONS] = {};
    Math::Vector2 s_InjectedMousePos = {};
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

    // ---- Touch controls (all platforms) ------------------------------------
    // The browser feeds real touches (web block below); desktop and the editor
    // can SIMULATE one touch from the mouse (Input::SetTouchSimulation) so the
    // layout is testable without a phone. Layout math is shared: a floating
    // move stick on the left, an optional look-drag region on the right, and
    // anchored action buttons growing up/left from the bottom-right of the safe
    // area. Engine builds the scheme (TouchActionBridge); Core owns only the
    // hit-zones, geometry and per-frame state.
    struct TouchPoint {
        long id = -1;
        f32 startX = 0, startY = 0;   // surface pixels
        f32 curX = 0, curY = 0;
        f32 lastX = 0, lastY = 0;
        double startMs = 0;
        int role = 0;                 // 0 none, 1 stick, 2 look/tap, 100+i = button i
    };
    constexpr int MAX_TOUCHES = 6;
    TouchPoint s_Touches[MAX_TOUCHES];
    bool s_TouchSeen = false;          // web: any touch ever -> show the overlay
    bool s_CoarsePointer = false;      // web: phone/tablet detected -> show at boot
    f32 s_SafeInset[4] = {0, 0, 0, 0}; // top,right,bottom,left in surface pixels (web)
    Input::TouchScheme s_TouchScheme;  // seeded empty; Engine applies a preset
    bool s_TouchSim = false;           // desktop/editor: the mouse is one touch
    f32 s_TouchSurfX = 0, s_TouchSurfY = 0, s_TouchSurfW = 0, s_TouchSurfH = 0;
    constexpr long kSimTouchId = 1000000;

#if ENJIN_PLATFORM_WEB
    void WebCanvasScale(f32& sx, f32& sy, int& bw, int& bh);
    void WebQuerySafeArea(f32 sx, f32 sy);
#endif

    // The rectangle touches live in: the canvas (web) or the rect the host
    // registered via SetTouchSurface (desktop/editor game view).
    void TouchSurfaceRect(f32& x0, f32& y0, f32& w, f32& h) {
#if ENJIN_PLATFORM_WEB
        f32 sx, sy; int bw, bh;
        WebCanvasScale(sx, sy, bw, bh);
        x0 = 0.0f; y0 = 0.0f; w = static_cast<f32>(bw); h = static_cast<f32>(bh);
#else
        x0 = s_TouchSurfX; y0 = s_TouchSurfY; w = s_TouchSurfW; h = s_TouchSurfH;
#endif
    }

    // Safe area = surface minus notch/rounded-corner insets. On web the insets
    // are re-queried whenever the canvas size changes (rotate / fullscreen /
    // on-screen keyboard). Falls back to the full surface when unknown.
    void TouchSafeRect(f32& x0, f32& y0, f32& w, f32& h) {
        f32 sx0, sy0, sw, sh;
        TouchSurfaceRect(sx0, sy0, sw, sh);
#if ENJIN_PLATFORM_WEB
        static int s_SafeForW = -1, s_SafeForH = -1;
        if (static_cast<int>(sw) != s_SafeForW || static_cast<int>(sh) != s_SafeForH) {
            f32 qsx, qsy; int qbw, qbh;
            WebCanvasScale(qsx, qsy, qbw, qbh);
            WebQuerySafeArea(qsx, qsy);
            s_SafeForW = static_cast<int>(sw); s_SafeForH = static_cast<int>(sh);
        }
#endif
        x0 = sx0 + s_SafeInset[3];
        y0 = sy0 + s_SafeInset[0];
        w = sw - s_SafeInset[1] - s_SafeInset[3];
        h = sh - s_SafeInset[0] - s_SafeInset[2];
        if (w < 1.0f) { x0 = sx0; w = sw; }
        if (h < 1.0f) { y0 = sy0; h = sh; }
    }

    // Resolve one scheme button to a surface-pixel center + radius. The cluster
    // grows up/left from the bottom-right corner of the safe area.
    void TouchResolveButton(const Input::TouchButtonDef& b, f32& cx, f32& cy, f32& r) {
        f32 x0, y0, w, h;
        TouchSafeRect(x0, y0, w, h);
        f32 scale = (s_TouchScheme.buttonScale > 0.05f) ? s_TouchScheme.buttonScale : 1.0f;
        r = b.radiusFrac * h * scale;
        f32 spacing = r * 2.4f;
        // colFromRight counts inward from the button-cluster edge: the right
        // edge normally, the LEFT edge when the layout is mirrored for
        // left-handed play (which also moves the stick to the other side).
        f32 inset = r * 1.6f + b.colFromRight * spacing;
        cx = s_TouchScheme.leftHanded ? (x0 + inset) : (x0 + w - inset);
        cy = y0 + h - r * 1.8f - b.rowFromBottom * spacing;
    }

    // Which control a touch landing at (px,py) belongs to: action buttons
    // first (topmost slot wins), then the move-stick zone, else look/tap.
    int AssignTouchRole(f32 px, f32 py) {
        // UI first: a finger on a button or slider must reach the UI as a real
        // pointer, or the stick zone silently swallows the left half of the
        // screen and sliders are undraggable.
        if (s_UIHitTestResolver && s_UIHitTestResolver(px, py)) return 3;
        for (int bi = 0; bi < s_TouchScheme.buttonCount; ++bi) {
            f32 cx, cy, cr;
            TouchResolveButton(s_TouchScheme.buttons[bi], cx, cy, cr);
            f32 ddx = px - cx, ddy = py - cy;
            if (ddx * ddx + ddy * ddy < cr * cr) return 100 + bi;
        }
        f32 x0, y0, zw, zh;
        TouchSafeRect(x0, y0, zw, zh);
        if (s_TouchScheme.moveStick) {
            bool inStickZone = s_TouchScheme.leftHanded
                ? (px > x0 + zw * (1.0f - s_TouchScheme.moveZoneSplit))
                : (px < x0 + zw * s_TouchScheme.moveZoneSplit);
            if (inStickZone) return 1;
        }
        return 2;
    }

    TouchPoint* FindTouch(long id) {
        for (auto& t : s_Touches) if (t.id == id) return &t;
        return nullptr;
    }

    bool TouchOverlayActive() {
#if ENJIN_PLATFORM_WEB
        return s_TouchSeen || s_CoarsePointer;
#else
        return s_TouchSim && s_TouchSurfW > 0.0f && s_TouchSurfH > 0.0f;
#endif
    }

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
        // Let browser shortcuts through: function keys (F1-F12, so F12 DevTools /
        // F5 reload / F11 fullscreen work) and any Ctrl/Cmd combo (DevTools,
        // reload, tab switching). The key is still recorded above, so the game can
        // read it too. preventDefault everything else so game keys (space/arrows/
        // Tab) don't scroll the page or move focus while the game has focus.
        bool isFunctionKey = (e->keyCode >= 112 && e->keyCode <= 123);
        if (isFunctionKey || e->ctrlKey || e->metaKey) {
            return EM_FALSE;
        }
        return EM_TRUE;
    }

    EM_BOOL WebMouseMoveCallback(int eventType, const EmscriptenMouseEvent* e, void* userData) {
        (void)eventType; (void)userData;
        // targetX/Y are CSS pixels relative to the canvas; the UI hit-tests in canvas
        // BACKING-STORE pixels. Scale by backingSize/cssSize — the actual CSS->backing
        // mapping — which is correct windowed AND fullscreen. Fullscreen CSS-stretches the
        // canvas to the screen while the backing store keeps its render resolution, so the
        // old devicePixelRatio scale sent every click to the wrong place and nothing was
        // clickable. Falls back to DPR if the element size can't be read.
        double cssW = 0.0, cssH = 0.0;
        int bw = 0, bh = 0;
        f32 sx, sy;
        if (emscripten_get_element_css_size("#game-canvas", &cssW, &cssH) == EMSCRIPTEN_RESULT_SUCCESS &&
            emscripten_get_canvas_element_size("#game-canvas", &bw, &bh) == EMSCRIPTEN_RESULT_SUCCESS &&
            cssW > 0.0 && cssH > 0.0) {
            sx = static_cast<f32>(bw) / static_cast<f32>(cssW);
            sy = static_cast<f32>(bh) / static_cast<f32>(cssH);
        } else {
            f32 dpr = static_cast<f32>(emscripten_get_device_pixel_ratio());
            if (dpr <= 0.0f) dpr = 1.0f;
            sx = sy = dpr;
        }
        s_MousePosition = Math::Vector2(static_cast<f32>(e->targetX) * sx,
                                        static_cast<f32>(e->targetY) * sy);
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

    // ---- Browser touch source ------------------------------------------------
    // Real touches from the canvas. Shared touch state/geometry lives above.
    // A quick tap anywhere still clicks (UI + actions).
    void WebCanvasScale(f32& sx, f32& sy, int& bw, int& bh) {
        double cssW = 0.0, cssH = 0.0;
        bw = 0; bh = 0;
        if (emscripten_get_element_css_size("#game-canvas", &cssW, &cssH) == EMSCRIPTEN_RESULT_SUCCESS &&
            emscripten_get_canvas_element_size("#game-canvas", &bw, &bh) == EMSCRIPTEN_RESULT_SUCCESS &&
            cssW > 0.0 && cssH > 0.0) {
            sx = static_cast<f32>(bw) / static_cast<f32>(cssW);
            sy = static_cast<f32>(bh) / static_cast<f32>(cssH);
        } else {
            f32 dpr = static_cast<f32>(emscripten_get_device_pixel_ratio());
            if (dpr <= 0.0f) dpr = 1.0f;
            sx = sy = dpr;
        }
    }

    // Query CSS env(safe-area-inset-*) and cache it (backing pixels).
    void WebQuerySafeArea(f32 sx, f32 sy) {
        double vals[4] = {0, 0, 0, 0};
        EM_ASM({
            var d = document.createElement('div');
            d.style.cssText =
                'position:fixed;top:0;left:0;visibility:hidden;pointer-events:none;' +
                'padding-top:env(safe-area-inset-top);' +
                'padding-right:env(safe-area-inset-right);' +
                'padding-bottom:env(safe-area-inset-bottom);' +
                'padding-left:env(safe-area-inset-left);';
            document.body.appendChild(d);
            var cs = getComputedStyle(d);
            HEAPF64[($0>>3)+0] = parseFloat(cs.paddingTop) || 0;
            HEAPF64[($0>>3)+1] = parseFloat(cs.paddingRight) || 0;
            HEAPF64[($0>>3)+2] = parseFloat(cs.paddingBottom) || 0;
            HEAPF64[($0>>3)+3] = parseFloat(cs.paddingLeft) || 0;
            document.body.removeChild(d);
        }, vals);
        s_SafeInset[0] = static_cast<f32>(vals[0]) * sy;
        s_SafeInset[1] = static_cast<f32>(vals[1]) * sx;
        s_SafeInset[2] = static_cast<f32>(vals[2]) * sy;
        s_SafeInset[3] = static_cast<f32>(vals[3]) * sx;
    }

    // W2: on coarse-pointer (mobile) browsers, the FIRST touch is the user
    // gesture we ride to request fullscreen + landscape lock. Attempted once;
    // every failure path is swallowed (iPhone Safari has limited fullscreen -
    // the overlay still works windowed).
    bool s_TriedFullscreen = false;
    void WebRequestMobileFullscreen() {
        if (s_TriedFullscreen) return;
        s_TriedFullscreen = true;
        EM_ASM({
            try {
                if (window.matchMedia && window.matchMedia('(pointer: coarse)').matches &&
                    !document.fullscreenElement && !document.webkitFullscreenElement) {
                    // Prefer the shell helper (handles iPhone pseudo-fullscreen);
                    // bare-shell fallback targets the container, never the canvas.
                    if (window.enjinEnterFullscreen) { window.enjinEnterFullscreen(); return; }
                    var c = document.getElementById('game-container') ||
                            document.getElementById('game-canvas') || document.body;
                    var p = c.requestFullscreen ? c.requestFullscreen()
                          : (c.webkitRequestFullscreen ? c.webkitRequestFullscreen() : null);
                    if (p && p.then) {
                        p.then(function() {
                            try {
                                if (screen.orientation && screen.orientation.lock) {
                                    screen.orientation.lock('landscape').catch(function(){});
                                }
                            } catch (e) {}
                        }).catch(function(){});
                    }
                }
            } catch (e) {}
        });
    }

}

// JS-callable (the on-page "Touch controls" button): force the overlay on as
// if a coarse pointer had been detected. Must be extern "C" + KEEPALIVE so
// Module._enjin_enable_touch_controls survives dead-code elimination.
extern "C" EMSCRIPTEN_KEEPALIVE void enjin_enable_touch_controls() {
    s_CoarsePointer = true;
    s_TouchSeen = true;
}

namespace {

    EM_BOOL WebTouchCallback(int eventType, const EmscriptenTouchEvent* e, void* userData) {
        (void)userData;
        f32 sx, sy; int bw, bh;
        WebCanvasScale(sx, sy, bw, bh);

        for (int i = 0; i < e->numTouches; ++i) {
            const auto& tp = e->touches[i];
            if (!tp.isChanged) continue;
            f32 px = static_cast<f32>(tp.targetX) * sx;
            f32 py = static_cast<f32>(tp.targetY) * sy;

            if (eventType == EMSCRIPTEN_EVENT_TOUCHSTART) {
                s_TouchSeen = true;
                TouchPoint* slot = FindTouch(tp.identifier);
                if (!slot) slot = FindTouch(-1);
                if (!slot) continue;
                slot->id = tp.identifier;
                slot->startX = slot->curX = slot->lastX = px;
                slot->startY = slot->curY = slot->lastY = py;
                slot->startMs = emscripten_get_now();
                WebRequestMobileFullscreen();   // W2: first-touch gesture
                slot->role = AssignTouchRole(px, py);
                if (slot->role == 3) {
                    // UI pointer: a real held press, so drags (sliders, scroll)
                    // work rather than only taps. Latest carries the hold, the
                    // latch guarantees the press edge survives the frame gap.
                    s_MousePosition = Math::Vector2(px, py);
                    s_WebMouseLatest[0] = true;
                    s_WebMouseDownLatch[0] = true;
                }
                // Touch position doubles as the pointer (hover, aim)
                s_MousePosition = Math::Vector2(px, py);
            } else if (eventType == EMSCRIPTEN_EVENT_TOUCHMOVE) {
                TouchPoint* slot = FindTouch(tp.identifier);
                if (!slot) continue;
                if (slot->role == 3) {
                    s_MousePosition = Math::Vector2(px, py);   // UI drag
                } else if (slot->role == 2) {
                    // Only a scheme with a look region drags the camera; 2D
                    // presets (lookRegion=false) still move the pointer so a
                    // tap-click lands where the finger lifted.
                    if (s_TouchScheme.lookRegion) {
                        constexpr f32 MAX_DELTA = 150.0f;
                        f32 dx = px - slot->lastX, dy = py - slot->lastY;
                        if (dx > -MAX_DELTA && dx < MAX_DELTA) s_WebMouseMovementAccum.x += dx;
                        if (dy > -MAX_DELTA && dy < MAX_DELTA) s_WebMouseMovementAccum.y += dy;
                    }
                    s_MousePosition = Math::Vector2(px, py);
                }
                slot->lastX = slot->curX; slot->lastY = slot->curY;
                slot->curX = px; slot->curY = py;
            } else {   // TOUCHEND / TOUCHCANCEL
                TouchPoint* slot = FindTouch(tp.identifier);
                if (!slot) continue;
                if (slot->role == 3) {
                    // UI pointer released. No tap synthesis: it already had a
                    // real press, so a second click here would double-fire.
                    s_MousePosition = Math::Vector2(px, py);
                    s_WebMouseLatest[0] = false;
                }
                if (eventType == EMSCRIPTEN_EVENT_TOUCHEND && (slot->role == 1 || slot->role == 2)) {
                    f32 mx = px - slot->startX, my = py - slot->startY;
                    bool shortTap = (emscripten_get_now() - slot->startMs) < 300.0 &&
                                    (mx * mx + my * my) < 16.0f * 16.0f;
                    if (shortTap) {
                        // Synthesize a click at the tap point (UI + actions)
                        s_MousePosition = Math::Vector2(px, py);
                        s_WebMouseDownLatch[0] = true;
                    }
                }
                slot->id = -1;
                slot->role = 0;
            }
        }
        return EM_TRUE;   // preventDefault: no page scroll/zoom while playing
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
    // Touch: empty scheme (stick + look, no buttons) until Engine applies a
    // preset from the scene's controller (InputSystem::ApplyTouchPresetForWorld).
    s_TouchScheme = TouchScheme{};
    for (auto& t : s_Touches) t = TouchPoint{};

#if ENJIN_PLATFORM_WEB
    // Register browser event handlers on the canvas
    const char* target = "#game-canvas";
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, false, WebKeyCallback);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, nullptr, false, WebKeyCallback);
    emscripten_set_mousemove_callback(target, nullptr, false, WebMouseMoveCallback);
    emscripten_set_mousedown_callback(target, nullptr, false, WebMouseButtonCallback);
    emscripten_set_mouseup_callback(target, nullptr, false, WebMouseButtonCallback);
    emscripten_set_wheel_callback(target, nullptr, false, WebWheelCallback);
    emscripten_set_touchstart_callback(target, nullptr, false, WebTouchCallback);
    emscripten_set_touchmove_callback(target, nullptr, false, WebTouchCallback);
    emscripten_set_touchend_callback(target, nullptr, false, WebTouchCallback);
    emscripten_set_touchcancel_callback(target, nullptr, false, WebTouchCallback);
    // Enable Gamepad API (must be called before polling gamepad state)
    emscripten_sample_gamepad_data();
    s_FirstMouseMove = true;

    // Mobile touch overlay: detect a coarse pointer (phone/tablet) so the
    // controls show before the first tap, cache safe-area insets, and seed the
    // default control scheme (the player refines it per controller).
    s_CoarsePointer = EM_ASM_INT({
        return (window.matchMedia && window.matchMedia('(pointer: coarse)').matches) ? 1 : 0;
    }) != 0;

    // Manual "go mobile": touch-capable laptops, tablets with mice, and DevTools
    // emulation don't always report a coarse pointer — give every player a small
    // on-page toggle that forces the overlay on (and hides itself). Injected
    // from the engine so every export has it without touching the HTML shell.
    EM_ASM({
        try {
            if (document.getElementById('enjin-touch-toggle')) return;
            var b = document.createElement('button');
            b.id = 'enjin-touch-toggle';
            b.textContent = '📱 Touch controls';
            b.style.cssText = 'position:fixed;top:8px;left:8px;z-index:1000;' +
                'padding:6px 10px;border-radius:8px;border:1px solid rgba(255,255,255,0.35);' +
                'background:rgba(0,0,0,0.45);color:#fff;font:13px sans-serif;cursor:pointer;opacity:0.7;';
            b.onclick = function() {
                if (Module && Module._enjin_enable_touch_controls) Module._enjin_enable_touch_controls();
                b.remove();
                try {
                    // Prefer the shell's helper: it fullscreens #game-container
                    // (the ResizeObserver watches it) and falls back to CSS
                    // pseudo-fullscreen on iPhone Safari. Bare-shell fallback
                    // below still targets the container, never the canvas.
                    if (window.enjinEnterFullscreen) { window.enjinEnterFullscreen(); return; }
                    var c = document.getElementById('game-container') ||
                            document.getElementById('game-canvas') || document.body;
                    if (document.fullscreenElement || document.webkitFullscreenElement) return;
                    if (c.requestFullscreen) c.requestFullscreen().catch(function(){});
                    else if (c.webkitRequestFullscreen) c.webkitRequestFullscreen();
                } catch (e) {}
            };
            document.body.appendChild(b);
        } catch (e) {}
    });
    {
        f32 qsx, qsy; int qbw, qbh;
        WebCanvasScale(qsx, qsy, qbw, qbh);
        WebQuerySafeArea(qsx, qsy);
    }
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

#else
    // Poll hardware only when a window exists; replay injection (below) and
    // headless tests still work without one.
    if (s_Window) {
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
    }

    // Touch simulation: the mouse stands in for ONE touch so the overlay layout
    // can be exercised without a phone. Mirrors the browser: a touch claimed by
    // the stick or an action button never reaches the game as a mouse button
    // (preventDefault), while a look/tap touch passes LMB through (tap = click,
    // hold-drag = look).
    if (s_TouchSim && s_TouchSurfW > 0.0f && s_TouchSurfH > 0.0f) {
        TouchPoint* t = FindTouch(kSimTouchId);
        bool lmb = s_MouseButtonsDown[0];
        f32 px = s_MousePosition.x, py = s_MousePosition.y;
        if (lmb && !t) {
            t = FindTouch(-1);
            if (t) {
                t->id = kSimTouchId;
                t->startX = t->curX = t->lastX = px;
                t->startY = t->curY = t->lastY = py;
                t->startMs = glfwGetTime() * 1000.0;
                t->role = AssignTouchRole(px, py);
            }
        } else if (lmb && t) {
            t->lastX = t->curX; t->lastY = t->curY;
            t->curX = px; t->curY = py;
        } else if (!lmb && t) {
            t->id = -1; t->role = 0; t = nullptr;
        }
        if (t && (t->role == 1 || t->role >= 100)) s_MouseButtonsDown[0] = false;
    } else if (TouchPoint* t = FindTouch(kSimTouchId)) {
        t->id = -1; t->role = 0;
    }
#endif

    // Touch controls: the move stick maps onto the scheme's 4 direction keys
    // (8-way, with a dead zone); each held button maps onto its scheme key (or
    // a mouse button for negative key codes, e.g. FPS fire). ORed over the key
    // state so a physical keyboard keeps working alongside.
    {
        constexpr int kKeyCount = static_cast<int>(sizeof(s_KeysDown) / sizeof(s_KeysDown[0]));
        for (const auto& t : s_Touches) {
            if (t.id == -1) continue;
            // Resolve a slot's effective key code: prefer the action's CURRENT
            // binding (so rebinding is reflected live) and fall back to the
            // static key code when there's no action / no bound key/mouse.
            auto effectiveKey = [](int staticKey, int action) -> int {
                if (action >= 0 && s_ActionKeyResolver) {
                    int rk = s_ActionKeyResolver(action);
                    if (rk != Input::kTouchNoBinding) return rk;
                }
                return staticKey;
            };
            if (t.role == 1 && s_TouchScheme.moveStick) {
                f32 dx = t.curX - t.startX;
                f32 dy = t.curY - t.startY;
                constexpr f32 DEAD = 18.0f;
                auto press = [&](int idx) {
                    int k = effectiveKey(s_TouchScheme.stickKeys[idx], s_TouchScheme.stickActions[idx]);
                    if (k >= 0 && k < kKeyCount) s_KeysDown[k] = true;
                };
                if (dx < -DEAD) press(0);   // left
                if (dx >  DEAD) press(1);   // right
                if (dy < -DEAD) press(2);   // up / forward
                if (dy >  DEAD) press(3);   // down / back
            } else if (t.role >= 100) {
                int bi = t.role - 100;
                if (bi < s_TouchScheme.buttonCount) {
                    int k = effectiveKey(s_TouchScheme.buttons[bi].keyCode, s_TouchScheme.buttons[bi].action);
                    if (k < 0) {
                        int mb = -k - 1;                             // -1 => mouse button 0 (fire/click)
                        if (mb >= 0 && mb < MAX_MOUSE_BUTTONS) s_MouseButtonsDown[mb] = true;
                    } else if (k < kKeyCount) {
                        s_KeysDown[k] = true;
                    }
                }
            }
        }
    }

    // Replay injection: the recorded snapshot wins over whatever the hardware
    // said this frame. Previous-frame state was already snapshotted above, so
    // pressed/released edge queries work against the injected stream. The
    // hardware state is preserved to the side for real-input scopes (the
    // replay free camera reads it).
    if (s_ReplayInjection) {
        std::memcpy(s_RealKeysDown, s_KeysDown, sizeof(s_KeysDown));
        std::memcpy(s_RealMouseButtons, s_MouseButtonsDown, sizeof(s_MouseButtonsDown));
        s_RealMouseDelta = s_RealFirstMove ? Math::Vector2(0.0f, 0.0f)
                                           : s_MousePosition - s_RealMousePosPrev;
        s_RealFirstMove = false;
        s_RealMousePosPrev = s_MousePosition;
        s_RealMousePos = s_MousePosition;

        std::memcpy(s_KeysDown, s_InjectedKeys, sizeof(s_KeysDown));
        std::memcpy(s_MouseButtonsDown, s_InjectedMouse, sizeof(s_MouseButtonsDown));
        s_MousePosition = s_InjectedMousePos;
    } else {
        s_RealFirstMove = true;
    }

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
    if (s_RealScope && s_ReplayInjection) return s_RealKeysDown[keyIndex];
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

void Input::InjectKeyPress(KeyCode key) {
    i32 keyIndex = static_cast<i32>(key);
    if (keyIndex < 0 || keyIndex >= MAX_KEYS) return;
#ifdef __EMSCRIPTEN__
    // Ride the same down-latch a browser keydown sets. Update() ORs the latch into
    // s_KeysDown for one frame and clears it, so IsKeyPressed fires exactly once.
    s_WebKeysDownLatch[keyIndex] = true;
#else
    // Desktop rebuilds s_KeysDown from GLFW each Update, so a one-frame press can't
    // be latched the same way; set it directly (edge computed against prev this frame).
    s_KeysDown[keyIndex] = true;
#endif
}

bool Input::IsMouseButtonDown(MouseButton button) {
    i32 buttonIndex = static_cast<i32>(button);
    if (buttonIndex < 0 || buttonIndex >= MAX_MOUSE_BUTTONS) return false;
    if (s_RealScope && s_ReplayInjection) return s_RealMouseButtons[buttonIndex];
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
    if (s_RealScope && s_ReplayInjection) return s_RealMousePos;
    return s_MousePosition;
}

f32 Input::GetMouseX() {
    return s_MousePosition.x;
}

f32 Input::GetMouseY() {
    return s_MousePosition.y;
}

Math::Vector2 Input::GetMouseDelta() {
    if (s_RealScope && s_ReplayInjection) return s_RealMouseDelta;
    return s_MouseDelta;
}

Math::Vector2 Input::GetScrollDelta() {
    return s_ScrollDelta;
}

void Input::SetReplayInjection(bool enabled) {
    s_ReplayInjection = enabled;
    if (!enabled) {
        std::memset(s_InjectedKeys, 0, sizeof(s_InjectedKeys));
        std::memset(s_InjectedMouse, 0, sizeof(s_InjectedMouse));
    }
}

bool Input::IsReplayInjectionActive() { return s_ReplayInjection; }

Input::TouchOverlayState Input::GetTouchOverlay() {
    TouchOverlayState st;
    // Web: any device that has ever touched OR reports a coarse pointer, so the
    // controls are visible before the first tap. Desktop: simulation + surface.
    if (!TouchOverlayActive()) return st;
    st.active = true;
    st.showStick = s_TouchScheme.moveStick;
    // Same reference as the buttons (safe-area height), so stick and buttons
    // scale together on notched devices.
    f32 safeX0, safeY0, safeW, safeH;
    TouchSafeRect(safeX0, safeY0, safeW, safeH);
    st.stickRadius = 0.07f * safeH;
    for (const auto& t : s_Touches) {
        if (t.id == -1) continue;
        if (t.role == 1) {
            st.stickHeld = true;
            st.stickBaseX = t.startX;
            st.stickBaseY = t.startY;
            // Clamp the nub inside the base circle
            f32 dx = t.curX - t.startX, dy = t.curY - t.startY;
            f32 len = std::sqrt(dx * dx + dy * dy);
            if (len > st.stickRadius && len > 0.0f) {
                dx = dx / len * st.stickRadius;
                dy = dy / len * st.stickRadius;
            }
            st.stickNubX = t.startX + dx;
            st.stickNubY = t.startY + dy;
        }
    }
    // Resolve each scheme button's geometry + held state for the player to draw.
    st.buttonCount = s_TouchScheme.buttonCount;
    for (int bi = 0; bi < s_TouchScheme.buttonCount; ++bi) {
        auto& out = st.buttons[bi];
        const auto& def = s_TouchScheme.buttons[bi];
        TouchResolveButton(def, out.x, out.y, out.r);
        // Prefer the current binding's label (so the glyph tracks a rebind);
        // fall back to the static label. Truncated to fit the 8-char field.
        const char* lbl = def.label;
        if (def.action >= 0 && s_ActionLabelResolver) {
            const char* rl = s_ActionLabelResolver(def.action);
            if (rl && rl[0]) lbl = rl;
        }
        int k = 0;
        for (; k < 7 && lbl[k]; ++k) out.label[k] = lbl[k];
        out.label[k] = 0;
        for (const auto& t : s_Touches) {
            if (t.id != -1 && t.role == 100 + bi) { out.held = true; break; }
        }
    }
    return st;
}

void Input::SetTouchScheme(const TouchScheme& scheme) {
    s_TouchScheme = scheme;
    if (s_TouchScheme.buttonCount < 0) s_TouchScheme.buttonCount = 0;
    if (s_TouchScheme.buttonCount > kMaxTouchButtons) s_TouchScheme.buttonCount = kMaxTouchButtons;
}

const Input::TouchScheme& Input::GetTouchScheme() {
    return s_TouchScheme;
}

void Input::SetActionKeyResolver(ActionKeyResolver resolver)   { s_ActionKeyResolver = resolver; }
void Input::SetActionLabelResolver(ActionLabelResolver resolver) { s_ActionLabelResolver = resolver; }

void Input::SetInputFocus(InputFocus focus) { s_InputFocus = focus; }
Input::InputFocus Input::GetInputFocus() { return s_InputFocus; }
bool Input::IsGameplayFocused() { return s_InputFocus == InputFocus::Gameplay; }

void Input::SetUIConsumedPointer(bool consumed) { s_UIConsumedPointer = consumed; }
bool Input::IsUIConsumedPointer() { return s_UIConsumedPointer; }

void Input::SetUIHitTestResolver(UIHitTestResolver resolver) { s_UIHitTestResolver = resolver; }

void Input::SetTouchSimulation(bool enabled) {
#if ENJIN_PLATFORM_WEB
    (void)enabled;   // real touches drive the overlay in the browser
#else
    s_TouchSim = enabled;
#endif
}

bool Input::IsTouchSimulation() { return s_TouchSim; }

void Input::SetTouchSurface(f32 x0, f32 y0, f32 w, f32 h) {
    s_TouchSurfX = x0; s_TouchSurfY = y0; s_TouchSurfW = w; s_TouchSurfH = h;
}

void Input::BeginRealInputScope() { s_RealScope = true; }
void Input::EndRealInputScope() { s_RealScope = false; }

void Input::InjectFrameState(const bool* keysDown, const bool* mouseDown,
                             Math::Vector2 mousePos) {
    if (keysDown) std::memcpy(s_InjectedKeys, keysDown, sizeof(s_InjectedKeys));
    if (mouseDown) std::memcpy(s_InjectedMouse, mouseDown, sizeof(s_InjectedMouse));
    s_InjectedMousePos = mousePos;
}

void Input::CaptureFrameState(bool* keysDown, bool* mouseDown, Math::Vector2& mousePos) {
    if (keysDown) std::memcpy(keysDown, s_KeysDown, sizeof(s_KeysDown));
    if (mouseDown) std::memcpy(mouseDown, s_MouseButtonsDown, sizeof(s_MouseButtonsDown));
    mousePos = s_MousePosition;
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
