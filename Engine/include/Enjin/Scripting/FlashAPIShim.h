#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

class asIScriptEngine;

namespace Enjin {

namespace ECS { class World; }
namespace Audio { class SimpleAudio; }
namespace Gameplay { class TieredSaveSystem; }
namespace Scripting {

// ============================================================================
// Flash API Shim for AngelScript
// ============================================================================
// Provides familiar Flash/ActionScript APIs as an AngelScript compatibility
// layer. Allows ported Flash games to use recognizable APIs that delegate
// to native Enjin engine systems.
//
// Supported Flash APIs:
//   Stage       → Window/viewport management
//   MovieClip   → Entity + AnimatedSprite2D + Timeline control
//   TextField   → UICanvas label creation
//   Mouse       → Input cursor state
//   Keyboard    → Input key polling
//   Sound       → AudioSystem playback
//   SharedObject → SaveSystem persistence
//   Math        → Standard math utilities
//   Timer       → setTimeout/setInterval via coroutines

// --- MovieClip API ---
// Wraps an entity with sprite/timeline animation
struct FlashMovieClip {
    u64 entityId = 0;
    f32 x = 0, y = 0;
    f32 scaleX = 1.0f, scaleY = 1.0f;
    f32 rotation = 0.0f;
    f32 alpha = 1.0f;
    bool visible = true;
    std::string name;
    i32 currentFrame = 1;  // 1-based like Flash
    i32 totalFrames = 1;

    // Timeline control
    void play();
    void stop();
    void gotoAndPlay(i32 frame);
    void gotoAndStop(i32 frame);
    void nextFrame();
    void prevFrame();

    // Display list
    void addChild(FlashMovieClip& child);
    void removeChild(FlashMovieClip& child);

    // Transform sync (entity ↔ MovieClip)
    void syncToEntity();
    void syncFromEntity();
};

// --- TextField API ---
struct FlashTextField {
    u64 entityId = 0;
    std::string text;
    f32 x = 0, y = 0;
    f32 width = 200, height = 30;
    f32 textSize = 14.0f;
    u32 textColor = 0x000000;
    std::string fontName;
    bool selectable = false;
    bool multiline = false;
    bool wordWrap = false;

    void syncToEntity();
};

// --- Sound API ---
struct FlashSound {
    std::string url;
    f32 volume = 1.0f;
    f32 pan = 0.0f;
    i32 loops = 0;
    bool isPlaying = false;

    void play(i32 startTime = 0, i32 loops = 0);
    void stop();
    void setVolume(f32 vol);
    void setPan(f32 p);
};

// --- SharedObject API ---
// Maps to the engine's SaveSystem
struct FlashSharedObject {
    std::string name;
    std::unordered_map<std::string, std::string> data;  // Key-value string store

    void flush();  // Save to disk
    void clear();
    void setProperty(const std::string& key, const std::string& value);
    std::string getProperty(const std::string& key) const;
    bool hasProperty(const std::string& key) const;

    static FlashSharedObject getLocal(const std::string& name);
};

// --- Stage API ---
struct FlashStage {
    static f32 stageWidth;
    static f32 stageHeight;
    static f32 frameRate;
    static i32 quality;  // 0=LOW, 1=MEDIUM, 2=HIGH, 3=BEST
    static bool showDefaultContextMenu;

    static void setFrameRate(f32 fps);
    static void setQuality(i32 q);
};

// --- Mouse API ---
struct FlashMouse {
    static void hide();
    static void show();
    static f32 getX();
    static f32 getY();
    static bool isDown();
};

// --- Keyboard constants ---
struct FlashKeyboard {
    // Common key codes matching Flash's Keyboard class
    static const i32 LEFT = 37;
    static const i32 UP = 38;
    static const i32 RIGHT = 39;
    static const i32 DOWN = 40;
    static const i32 SPACE = 32;
    static const i32 ENTER = 13;
    static const i32 ESCAPE = 27;
    static const i32 SHIFT = 16;
    static const i32 CONTROL = 17;
    static const i32 TAB = 9;
    static const i32 BACKSPACE = 8;
    static const i32 DELETE_KEY = 46;

    // Letter keys (A-Z: 65-90)
    static bool isDown(i32 keyCode);
};

// --- Timer API ---
struct FlashTimer {
    u32 id = 0;
    f32 delay = 0;
    i32 repeatCount = 0;
    i32 currentCount = 0;
    bool running = false;

    void start();
    void stop();
    void reset();
};

// --- Event constants ---
struct FlashEvent {
    static const std::string ENTER_FRAME;
    static const std::string MOUSE_DOWN;
    static const std::string MOUSE_UP;
    static const std::string MOUSE_MOVE;
    static const std::string KEY_DOWN;
    static const std::string KEY_UP;
    static const std::string ADDED_TO_STAGE;
    static const std::string REMOVED_FROM_STAGE;
    static const std::string COMPLETE;
    static const std::string TIMER;
    static const std::string TIMER_COMPLETE;
};

// ============================================================================
// Global Flash API Functions (for AngelScript bindings)
// ============================================================================
// These are the flat function-style bindings registered into AngelScript,
// mapping Flash DisplayObject/MovieClip/Stage/Math/Sound/Timer APIs to
// engine systems via the shared s_BindingsWorld pointer.

namespace FlashAPI {

// --- DisplayObject / MovieClip ---
f32 Flash_GetX(u64 entity);
void Flash_SetX(u64 entity, f32 x);
f32 Flash_GetY(u64 entity);
void Flash_SetY(u64 entity, f32 y);
f32 Flash_GetScaleX(u64 entity);
void Flash_SetScaleX(u64 entity, f32 sx);
f32 Flash_GetScaleY(u64 entity);
void Flash_SetScaleY(u64 entity, f32 sy);
f32 Flash_GetRotation(u64 entity);
void Flash_SetRotation(u64 entity, f32 degrees);
f32 Flash_GetAlpha(u64 entity);
void Flash_SetAlpha(u64 entity, f32 alpha);
bool Flash_GetVisible(u64 entity);
void Flash_SetVisible(u64 entity, bool visible);
void Flash_GotoAndPlay(u64 entity, i32 frame);
void Flash_GotoAndStop(u64 entity, i32 frame);
void Flash_Play(u64 entity);
void Flash_Stop(u64 entity);
i32 Flash_GetCurrentFrame(u64 entity);
i32 Flash_GetTotalFrames(u64 entity);
u64 Flash_GetChildByName(u64 entity, const std::string& name);

// --- Stage ---
f32 Flash_GetStageWidth();
f32 Flash_GetStageHeight();
f32 Flash_GetFrameRate();

// --- Mouse / Keyboard ---
f32 Flash_GetMouseX();
f32 Flash_GetMouseY();
bool Flash_IsKeyDown(i32 keyCode);

// --- TextField ---
void Flash_SetText(u64 entity, const std::string& text);
std::string Flash_GetText(u64 entity);

// --- Math ---
f32 Flash_MathRandom();
f32 Flash_MathFloor(f32 x);
f32 Flash_MathCeil(f32 x);
f32 Flash_MathRound(f32 x);
f32 Flash_MathAbs(f32 x);
f32 Flash_MathSqrt(f32 x);
f32 Flash_MathSin(f32 x);
f32 Flash_MathCos(f32 x);
f32 Flash_MathAtan2(f32 y, f32 x);

// --- Sound ---
void Flash_PlaySound(const std::string& name);
void Flash_StopSound(const std::string& name);
void Flash_SetVolume(const std::string& name, f32 vol);

// --- Timer ---
u32 Flash_SetTimeout(f32 ms);
u32 Flash_SetInterval(f32 ms);
void Flash_ClearInterval(u32 id);

} // namespace FlashAPI

// ============================================================================
// Registration function — call during engine startup
// ============================================================================

// Register all Flash API shim types and functions into AngelScript
ENJIN_API void RegisterFlashAPIBindings(asIScriptEngine* engine);

// Set the world, audio, and save system pointers for the shim to use
void SetFlashShimWorld(ECS::World* world);
void SetFlashShimAudio(Audio::SimpleAudio* audio);
void SetFlashShimSaveSystem(Gameplay::TieredSaveSystem* sys);

} // namespace Scripting
} // namespace Enjin
