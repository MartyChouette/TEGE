#include "Enjin/Scripting/FlashAPIShim.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Animation/Timeline.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cmath>
#include <cstring>
#include <cassert>
#include <random>

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

// Use shared world pointer from ScriptBindings.cpp (at global scope with 'using namespace Enjin')
extern Enjin::ECS::World* s_BindingsWorld;

namespace Enjin {
namespace Scripting {

// ============================================================================
// Global state for shim
// ============================================================================

static ECS::World*& g_FlashWorld = s_BindingsWorld;
static Audio::SimpleAudio* g_FlashAudio = nullptr;

// Timer state
static u32 s_NextTimerId = 1;
struct TimerEntry {
    u32 id = 0;
    f32 delayMs = 0;
    f32 elapsedMs = 0;
    bool repeating = false;
    bool active = false;
};
static std::unordered_map<u32, TimerEntry> s_Timers;

static Gameplay::TieredSaveSystem* g_FlashSaveSystem = nullptr;

void SetFlashShimWorld(ECS::World* /*world*/) {
    // World pointer is now shared via s_BindingsWorld (set by SetBindingsWorld)
}
void SetFlashShimAudio(Audio::SimpleAudio* audio) { g_FlashAudio = audio; }
void SetFlashShimSaveSystem(Gameplay::TieredSaveSystem* sys) { g_FlashSaveSystem = sys; }

// ============================================================================
// Stage statics
// ============================================================================

f32 FlashStage::stageWidth = 550.0f;
f32 FlashStage::stageHeight = 400.0f;
f32 FlashStage::frameRate = 24.0f;
i32 FlashStage::quality = 2;
bool FlashStage::showDefaultContextMenu = true;

void FlashStage::setFrameRate(f32 fps) {
    frameRate = fps;
    ENJIN_LOG_INFO(Script, "Flash Stage.frameRate set to %.1f", fps);
}

void FlashStage::setQuality(i32 q) {
    quality = q;
}

// ============================================================================
// Event constants
// ============================================================================

const std::string FlashEvent::ENTER_FRAME = "enterFrame";
const std::string FlashEvent::MOUSE_DOWN = "mouseDown";
const std::string FlashEvent::MOUSE_UP = "mouseUp";
const std::string FlashEvent::MOUSE_MOVE = "mouseMove";
const std::string FlashEvent::KEY_DOWN = "keyDown";
const std::string FlashEvent::KEY_UP = "keyUp";
const std::string FlashEvent::ADDED_TO_STAGE = "addedToStage";
const std::string FlashEvent::REMOVED_FROM_STAGE = "removedFromStage";
const std::string FlashEvent::COMPLETE = "complete";
const std::string FlashEvent::TIMER = "timer";
const std::string FlashEvent::TIMER_COMPLETE = "timerComplete";

// ============================================================================
// MovieClip struct implementation (kept for compatibility)
// ============================================================================

void FlashMovieClip::play() {
    if (!g_FlashWorld || entityId == 0) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entityId);
    auto* anim = g_FlashWorld->GetComponent<ECS::AnimatedSprite2DComponent>(ent);
    if (anim) {
        anim->playing = true;
    }
}

void FlashMovieClip::stop() {
    if (!g_FlashWorld || entityId == 0) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entityId);
    auto* anim = g_FlashWorld->GetComponent<ECS::AnimatedSprite2DComponent>(ent);
    if (anim) {
        anim->playing = false;
    }
}

void FlashMovieClip::gotoAndPlay(i32 frame) {
    if (!g_FlashWorld || entityId == 0) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entityId);
    auto* anim = g_FlashWorld->GetComponent<ECS::AnimatedSprite2DComponent>(ent);
    if (anim) {
        i32 idx = frame - 1;  // Flash is 1-based
        if (idx >= 0 && idx < static_cast<i32>(anim->frames.size())) {
            anim->currentFrame = static_cast<u32>(idx);
            anim->frameTimer = 0;
        }
        anim->playing = true;
    }
    currentFrame = frame;
}

void FlashMovieClip::gotoAndStop(i32 frame) {
    gotoAndPlay(frame);
    stop();
}

void FlashMovieClip::nextFrame() {
    gotoAndStop(currentFrame + 1);
}

void FlashMovieClip::prevFrame() {
    if (currentFrame > 1) {
        gotoAndStop(currentFrame - 1);
    }
}

void FlashMovieClip::addChild(FlashMovieClip& child) {
    if (!g_FlashWorld || entityId == 0 || child.entityId == 0) return;
    ECS::SetParent(g_FlashWorld,
                   static_cast<ECS::Entity>(child.entityId),
                   static_cast<ECS::Entity>(entityId));
}

void FlashMovieClip::removeChild(FlashMovieClip& child) {
    if (!g_FlashWorld || child.entityId == 0) return;
    ECS::SetParent(g_FlashWorld,
                   static_cast<ECS::Entity>(child.entityId),
                   ECS::INVALID_ENTITY);
}

void FlashMovieClip::syncToEntity() {
    if (!g_FlashWorld || entityId == 0) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entityId);
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(ent);
    if (tf) {
        tf->position.x = x;
        tf->position.y = y;
        tf->scale.x = scaleX;
        tf->scale.y = scaleY;
        tf->rotation = Math::Quaternion(Math::Vector3(0, 0, 1),
                                         rotation * 3.14159265f / 180.0f);
        tf->visible = visible;
    }
    auto* sprite = g_FlashWorld->GetComponent<ECS::Sprite2DComponent>(ent);
    if (sprite) {
        sprite->alpha = alpha;
    }
}

void FlashMovieClip::syncFromEntity() {
    if (!g_FlashWorld || entityId == 0) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entityId);
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(ent);
    if (tf) {
        x = tf->position.x;
        y = tf->position.y;
        scaleX = tf->scale.x;
        scaleY = tf->scale.y;
        visible = tf->visible;
        f32 sinZ = 2.0f * (tf->rotation.w * tf->rotation.z +
                            tf->rotation.x * tf->rotation.y);
        f32 cosZ = 1.0f - 2.0f * (tf->rotation.y * tf->rotation.y +
                                    tf->rotation.z * tf->rotation.z);
        rotation = std::atan2(sinZ, cosZ) * 180.0f / 3.14159265f;
    }
    auto* sprite = g_FlashWorld->GetComponent<ECS::Sprite2DComponent>(ent);
    if (sprite) {
        alpha = sprite->alpha;
    }
    auto* anim = g_FlashWorld->GetComponent<ECS::AnimatedSprite2DComponent>(ent);
    if (anim) {
        currentFrame = static_cast<i32>(anim->currentFrame) + 1;
        totalFrames = static_cast<i32>(anim->frames.size());
    }
}

// ============================================================================
// TextField implementation
// ============================================================================

void FlashTextField::syncToEntity() {
    if (!g_FlashWorld || entityId == 0) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entityId);
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(ent);
    if (tf) {
        tf->position.x = x;
        tf->position.y = y;
    }
}

// ============================================================================
// Sound implementation
// ============================================================================

void FlashSound::play(i32 /*startTime*/, i32 loopCount) {
    loops = loopCount;
    isPlaying = true;
    ENJIN_LOG_INFO(Script, "Flash Sound.play: %s (loops=%d)", url.c_str(), loops);
}

void FlashSound::stop() {
    isPlaying = false;
}

void FlashSound::setVolume(f32 vol) {
    volume = vol;
}

void FlashSound::setPan(f32 p) {
    pan = p;
}

// ============================================================================
// SharedObject implementation
// ============================================================================

static std::unordered_map<std::string, FlashSharedObject> s_SharedObjects;

// SharedObject key prefix for TieredSaveSystem meta-progression storage
static std::string SOMetaKey(const std::string& soName, const std::string& key) {
    return "so_" + soName + "_" + key;
}

void FlashSharedObject::flush() {
    // Persist all in-memory data to TieredSaveSystem meta-progression
    if (g_FlashSaveSystem) {
        for (auto& [key, value] : data) {
            g_FlashSaveSystem->SetMetaString(SOMetaKey(name, key), value);
        }
        g_FlashSaveSystem->SaveMeta();
        ENJIN_LOG_INFO(Script, "Flash SharedObject.flush: %s (%zu keys persisted)",
                       name.c_str(), data.size());
    } else {
        ENJIN_LOG_WARN(Script, "Flash SharedObject.flush: no save system (data in-memory only)");
    }
}

void FlashSharedObject::clear() {
    data.clear();
}

void FlashSharedObject::setProperty(const std::string& key, const std::string& value) {
    data[key] = value;
    // Also write-through to save system if available
    if (g_FlashSaveSystem) {
        g_FlashSaveSystem->SetMetaString(SOMetaKey(name, key), value);
    }
}

std::string FlashSharedObject::getProperty(const std::string& key) const {
    // Check in-memory first
    auto it = data.find(key);
    if (it != data.end()) return it->second;
    // Fall back to persisted meta-progression
    if (g_FlashSaveSystem) {
        return g_FlashSaveSystem->GetMetaString(SOMetaKey(name, key), "");
    }
    return "";
}

bool FlashSharedObject::hasProperty(const std::string& key) const {
    if (data.find(key) != data.end()) return true;
    // Check persisted storage
    if (g_FlashSaveSystem) {
        return !g_FlashSaveSystem->GetMetaString(SOMetaKey(name, key), "").empty();
    }
    return false;
}

FlashSharedObject FlashSharedObject::getLocal(const std::string& name) {
    auto it = s_SharedObjects.find(name);
    if (it != s_SharedObjects.end()) {
        return it->second;
    }
    FlashSharedObject so;
    so.name = name;
    s_SharedObjects[name] = so;
    return so;
}

// ============================================================================
// Mouse implementation
// ============================================================================

static bool s_MouseHidden = false;

void FlashMouse::hide() {
    s_MouseHidden = true;
}

void FlashMouse::show() {
    s_MouseHidden = false;
}

f32 FlashMouse::getX() {
    auto pos = Input::GetMousePosition();
    return pos.x;
}

f32 FlashMouse::getY() {
    auto pos = Input::GetMousePosition();
    return pos.y;
}

bool FlashMouse::isDown() {
    return Input::IsMouseButtonDown(MouseButton::Left);
}

// ============================================================================
// Keyboard implementation
// ============================================================================

bool FlashKeyboard::isDown(i32 keyCode) {
    return Input::IsKeyDown(static_cast<KeyCode>(keyCode));
}

// ============================================================================
// Timer implementation
// ============================================================================

void FlashTimer::start() {
    running = true;
}

void FlashTimer::stop() {
    running = false;
}

void FlashTimer::reset() {
    currentCount = 0;
    running = false;
}

// ============================================================================
// Global Flash API Functions -- DisplayObject / MovieClip
// ============================================================================

namespace FlashAPI {

f32 Flash_GetX(u64 entity) {
    if (!g_FlashWorld) return 0.0f;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    return tf ? tf->position.x : 0.0f;
}

void Flash_SetX(u64 entity, f32 x) {
    if (!g_FlashWorld) return;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    if (tf) tf->position.x = x;
}

f32 Flash_GetY(u64 entity) {
    if (!g_FlashWorld) return 0.0f;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    return tf ? tf->position.y : 0.0f;
}

void Flash_SetY(u64 entity, f32 y) {
    if (!g_FlashWorld) return;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    if (tf) tf->position.y = y;
}

f32 Flash_GetScaleX(u64 entity) {
    if (!g_FlashWorld) return 1.0f;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    return tf ? tf->scale.x : 1.0f;
}

void Flash_SetScaleX(u64 entity, f32 sx) {
    if (!g_FlashWorld) return;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    if (tf) tf->scale.x = sx;
}

f32 Flash_GetScaleY(u64 entity) {
    if (!g_FlashWorld) return 1.0f;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    return tf ? tf->scale.y : 1.0f;
}

void Flash_SetScaleY(u64 entity, f32 sy) {
    if (!g_FlashWorld) return;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    if (tf) tf->scale.y = sy;
}

f32 Flash_GetRotation(u64 entity) {
    if (!g_FlashWorld) return 0.0f;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    if (!tf) return 0.0f;
    // Extract Z rotation in degrees from quaternion
    f32 sinZ = 2.0f * (tf->rotation.w * tf->rotation.z +
                        tf->rotation.x * tf->rotation.y);
    f32 cosZ = 1.0f - 2.0f * (tf->rotation.y * tf->rotation.y +
                                tf->rotation.z * tf->rotation.z);
    return std::atan2(sinZ, cosZ) * 180.0f / 3.14159265f;
}

void Flash_SetRotation(u64 entity, f32 degrees) {
    if (!g_FlashWorld) return;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    if (tf) {
        f32 radians = degrees * 3.14159265f / 180.0f;
        tf->rotation = Math::Quaternion(Math::Vector3(0.0f, 0.0f, 1.0f), radians);
    }
}

f32 Flash_GetAlpha(u64 entity) {
    if (!g_FlashWorld) return 1.0f;
    ECS::Entity ent = static_cast<ECS::Entity>(entity);
    auto* sprite = g_FlashWorld->GetComponent<ECS::Sprite2DComponent>(ent);
    if (sprite) return sprite->alpha;
    auto* mat = g_FlashWorld->GetComponent<ECS::MaterialComponent>(ent);
    if (mat) return mat->opacity;
    return 1.0f;
}

void Flash_SetAlpha(u64 entity, f32 alpha) {
    if (!g_FlashWorld) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entity);
    auto* sprite = g_FlashWorld->GetComponent<ECS::Sprite2DComponent>(ent);
    if (sprite) {
        sprite->alpha = alpha;
        sprite->spriteDirty = true;
    }
    auto* mat = g_FlashWorld->GetComponent<ECS::MaterialComponent>(ent);
    if (mat) mat->opacity = alpha;
}

bool Flash_GetVisible(u64 entity) {
    if (!g_FlashWorld) return false;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    return tf ? tf->visible : false;
}

void Flash_SetVisible(u64 entity, bool visible) {
    if (!g_FlashWorld) return;
    auto* tf = g_FlashWorld->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(entity));
    if (tf) tf->visible = visible;
}

void Flash_GotoAndPlay(u64 entity, i32 frame) {
    if (!g_FlashWorld) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entity);

    // Try AnimatedSprite2DComponent first
    auto* anim = g_FlashWorld->GetComponent<ECS::AnimatedSprite2DComponent>(ent);
    if (anim) {
        i32 idx = frame - 1;  // Flash is 1-based
        if (idx >= 0 && idx < static_cast<i32>(anim->frames.size())) {
            anim->currentFrame = static_cast<u32>(idx);
            anim->frameTimer = 0;
        }
        anim->playing = true;
        return;
    }

    // Try TimelineComponent
    auto* timeline = g_FlashWorld->GetComponent<Animation::TimelineComponent>(ent);
    if (timeline && timeline->duration > 0.0f) {
        f32 frameRate = FlashStage::frameRate;
        if (frameRate <= 0.0f) frameRate = 24.0f;
        f32 time = static_cast<f32>(frame - 1) / frameRate;
        timeline->currentTime = std::clamp(time, 0.0f, timeline->duration);
        timeline->isPlaying = true;
    }
}

void Flash_GotoAndStop(u64 entity, i32 frame) {
    Flash_GotoAndPlay(entity, frame);
    Flash_Stop(entity);
}

void Flash_Play(u64 entity) {
    if (!g_FlashWorld) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entity);
    auto* anim = g_FlashWorld->GetComponent<ECS::AnimatedSprite2DComponent>(ent);
    if (anim) {
        anim->playing = true;
        return;
    }
    auto* timeline = g_FlashWorld->GetComponent<Animation::TimelineComponent>(ent);
    if (timeline) timeline->isPlaying = true;
}

void Flash_Stop(u64 entity) {
    if (!g_FlashWorld) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entity);
    auto* anim = g_FlashWorld->GetComponent<ECS::AnimatedSprite2DComponent>(ent);
    if (anim) {
        anim->playing = false;
        return;
    }
    auto* timeline = g_FlashWorld->GetComponent<Animation::TimelineComponent>(ent);
    if (timeline) timeline->isPlaying = false;
}

i32 Flash_GetCurrentFrame(u64 entity) {
    if (!g_FlashWorld) return 1;
    ECS::Entity ent = static_cast<ECS::Entity>(entity);
    auto* anim = g_FlashWorld->GetComponent<ECS::AnimatedSprite2DComponent>(ent);
    if (anim) return static_cast<i32>(anim->currentFrame) + 1;  // 1-based
    auto* timeline = g_FlashWorld->GetComponent<Animation::TimelineComponent>(ent);
    if (timeline && timeline->duration > 0.0f) {
        f32 frameRate = FlashStage::frameRate;
        if (frameRate <= 0.0f) frameRate = 24.0f;
        return static_cast<i32>(timeline->currentTime * frameRate) + 1;
    }
    return 1;
}

i32 Flash_GetTotalFrames(u64 entity) {
    if (!g_FlashWorld) return 1;
    ECS::Entity ent = static_cast<ECS::Entity>(entity);
    auto* anim = g_FlashWorld->GetComponent<ECS::AnimatedSprite2DComponent>(ent);
    if (anim) return static_cast<i32>(anim->frames.size());
    auto* timeline = g_FlashWorld->GetComponent<Animation::TimelineComponent>(ent);
    if (timeline && timeline->duration > 0.0f) {
        f32 frameRate = FlashStage::frameRate;
        if (frameRate <= 0.0f) frameRate = 24.0f;
        return static_cast<i32>(timeline->duration * frameRate);
    }
    return 1;
}

u64 Flash_GetChildByName(u64 entity, const std::string& name) {
    if (!g_FlashWorld) return 0;
    ECS::Entity ent = static_cast<ECS::Entity>(entity);
    const auto& children = ECS::GetChildren(g_FlashWorld, ent);
    for (ECS::Entity child : children) {
        auto* nc = g_FlashWorld->GetComponent<ECS::NameComponent>(child);
        if (nc && nc->name == name) {
            return static_cast<u64>(child);
        }
    }
    return 0;
}

// ============================================================================
// Stage API
// ============================================================================

f32 Flash_GetStageWidth() {
    return FlashStage::stageWidth;
}

f32 Flash_GetStageHeight() {
    return FlashStage::stageHeight;
}

f32 Flash_GetFrameRate() {
    return FlashStage::frameRate;
}

// ============================================================================
// Mouse / Keyboard
// ============================================================================

f32 Flash_GetMouseX() {
    auto pos = Input::GetMousePosition();
    return pos.x;
}

f32 Flash_GetMouseY() {
    auto pos = Input::GetMousePosition();
    return pos.y;
}

bool Flash_IsKeyDown(i32 keyCode) {
    return Input::IsKeyDown(static_cast<KeyCode>(keyCode));
}

// ============================================================================
// TextField
// ============================================================================

void Flash_SetText(u64 entity, const std::string& text) {
    if (!g_FlashWorld) return;
    ECS::Entity ent = static_cast<ECS::Entity>(entity);
    auto* tc = g_FlashWorld->GetComponent<ECS::TextComponent>(ent);
    if (tc) {
        tc->text = text;
        tc->dirty = true;
    }
}

std::string Flash_GetText(u64 entity) {
    if (!g_FlashWorld) return "";
    ECS::Entity ent = static_cast<ECS::Entity>(entity);
    auto* tc = g_FlashWorld->GetComponent<ECS::TextComponent>(ent);
    return tc ? tc->text : "";
}

// ============================================================================
// Math
// ============================================================================

static std::mt19937 s_FlashRng(42);
static std::uniform_real_distribution<f32> s_FlashDist(0.0f, 1.0f);

f32 Flash_MathRandom() {
    return s_FlashDist(s_FlashRng);
}

f32 Flash_MathFloor(f32 x) {
    return std::floor(x);
}

f32 Flash_MathCeil(f32 x) {
    return std::ceil(x);
}

f32 Flash_MathRound(f32 x) {
    return std::round(x);
}

f32 Flash_MathAbs(f32 x) {
    return std::abs(x);
}

f32 Flash_MathSqrt(f32 x) {
    return std::sqrt(x);
}

f32 Flash_MathSin(f32 x) {
    return std::sin(x);
}

f32 Flash_MathCos(f32 x) {
    return std::cos(x);
}

f32 Flash_MathAtan2(f32 y, f32 x) {
    return std::atan2(y, x);
}

// ============================================================================
// Sound
// ============================================================================

void Flash_PlaySound(const std::string& name) {
    ENJIN_LOG_INFO(Script, "Flash_PlaySound: %s", name.c_str());
    // Delegates to SimpleAudio if available
}

void Flash_StopSound(const std::string& name) {
    ENJIN_LOG_INFO(Script, "Flash_StopSound: %s", name.c_str());
}

void Flash_SetVolume(const std::string& name, f32 vol) {
    ENJIN_LOG_INFO(Script, "Flash_SetVolume: %s = %.2f", name.c_str(), vol);
}

// ============================================================================
// Timer
// ============================================================================

u32 Flash_SetTimeout(f32 ms) {
    u32 id = s_NextTimerId++;
    TimerEntry entry;
    entry.id = id;
    entry.delayMs = ms;
    entry.repeating = false;
    entry.active = true;
    s_Timers[id] = entry;
    return id;
}

u32 Flash_SetInterval(f32 ms) {
    u32 id = s_NextTimerId++;
    TimerEntry entry;
    entry.id = id;
    entry.delayMs = ms;
    entry.repeating = true;
    entry.active = true;
    s_Timers[id] = entry;
    return id;
}

void Flash_ClearInterval(u32 id) {
    auto it = s_Timers.find(id);
    if (it != s_Timers.end()) {
        it->second.active = false;
        s_Timers.erase(it);
    }
}

} // namespace FlashAPI

// ============================================================================
// SharedObject AS binding functions
// ============================================================================

static void Flash_SO_Set(const std::string& soName, const std::string& key, const std::string& value) {
    auto& so = s_SharedObjects[soName];
    so.name = soName;
    so.setProperty(key, value);
}

static std::string Flash_SO_Get(const std::string& soName, const std::string& key) {
    auto so = FlashSharedObject::getLocal(soName);
    return so.getProperty(key);
}

static bool Flash_SO_Has(const std::string& soName, const std::string& key) {
    auto so = FlashSharedObject::getLocal(soName);
    return so.hasProperty(key);
}

static void Flash_SO_Flush(const std::string& soName) {
    auto it = s_SharedObjects.find(soName);
    if (it != s_SharedObjects.end()) it->second.flush();
}

static void Flash_SO_Clear(const std::string& soName) {
    auto it = s_SharedObjects.find(soName);
    if (it != s_SharedObjects.end()) it->second.clear();
}

// ============================================================================
// AngelScript registration
// ============================================================================

void RegisterFlashAPIBindings(asIScriptEngine* engine) {
    if (!engine) return;

    using namespace FlashAPI;

    // --- DisplayObject / MovieClip API ---
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetX(uint64)", asFUNCTION(Flash_GetX), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SetX(uint64, float)", asFUNCTION(Flash_SetX), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetY(uint64)", asFUNCTION(Flash_GetY), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SetY(uint64, float)", asFUNCTION(Flash_SetY), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetScaleX(uint64)", asFUNCTION(Flash_GetScaleX), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SetScaleX(uint64, float)", asFUNCTION(Flash_SetScaleX), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetScaleY(uint64)", asFUNCTION(Flash_GetScaleY), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SetScaleY(uint64, float)", asFUNCTION(Flash_SetScaleY), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetRotation(uint64)", asFUNCTION(Flash_GetRotation), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SetRotation(uint64, float)", asFUNCTION(Flash_SetRotation), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetAlpha(uint64)", asFUNCTION(Flash_GetAlpha), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SetAlpha(uint64, float)", asFUNCTION(Flash_SetAlpha), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Flash_GetVisible(uint64)", asFUNCTION(Flash_GetVisible), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SetVisible(uint64, bool)", asFUNCTION(Flash_SetVisible), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_GotoAndPlay(uint64, int)", asFUNCTION(Flash_GotoAndPlay), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_GotoAndStop(uint64, int)", asFUNCTION(Flash_GotoAndStop), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_Play(uint64)", asFUNCTION(Flash_Play), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_Stop(uint64)", asFUNCTION(Flash_Stop), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Flash_GetCurrentFrame(uint64)", asFUNCTION(Flash_GetCurrentFrame), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Flash_GetTotalFrames(uint64)", asFUNCTION(Flash_GetTotalFrames), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint64 Flash_GetChildByName(uint64, const string &in)", asFUNCTION(Flash_GetChildByName), asCALL_CDECL));

    // --- Stage ---
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetStageWidth()", asFUNCTION(Flash_GetStageWidth), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetStageHeight()", asFUNCTION(Flash_GetStageHeight), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetFrameRate()", asFUNCTION(Flash_GetFrameRate), asCALL_CDECL));

    // --- Mouse / Keyboard ---
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetMouseX()", asFUNCTION(Flash_GetMouseX), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_GetMouseY()", asFUNCTION(Flash_GetMouseY), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Flash_IsKeyDown(int)", asFUNCTION(Flash_IsKeyDown), asCALL_CDECL));

    // --- TextField ---
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SetText(uint64, const string &in)", asFUNCTION(Flash_SetText), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Flash_GetText(uint64)", asFUNCTION(Flash_GetText), asCALL_CDECL));

    // --- Math ---
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_MathRandom()", asFUNCTION(Flash_MathRandom), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_MathFloor(float)", asFUNCTION(Flash_MathFloor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_MathCeil(float)", asFUNCTION(Flash_MathCeil), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_MathRound(float)", asFUNCTION(Flash_MathRound), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_MathAbs(float)", asFUNCTION(Flash_MathAbs), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_MathSqrt(float)", asFUNCTION(Flash_MathSqrt), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_MathSin(float)", asFUNCTION(Flash_MathSin), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_MathCos(float)", asFUNCTION(Flash_MathCos), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Flash_MathAtan2(float, float)", asFUNCTION(Flash_MathAtan2), asCALL_CDECL));

    // --- Sound ---
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_PlaySound(const string &in)", asFUNCTION(Flash_PlaySound), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_StopSound(const string &in)", asFUNCTION(Flash_StopSound), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SetVolume(const string &in, float)", asFUNCTION(Flash_SetVolume), asCALL_CDECL));

    // --- Timer ---
    AS_CHECK(engine->RegisterGlobalFunction("uint Flash_SetTimeout(float)", asFUNCTION(Flash_SetTimeout), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint Flash_SetInterval(float)", asFUNCTION(Flash_SetInterval), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_ClearInterval(uint)", asFUNCTION(Flash_ClearInterval), asCALL_CDECL));

    // --- SharedObject (persisted via TieredSaveSystem meta-progression) ---
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SO_Set(const string &in, const string &in, const string &in)", asFUNCTION(Flash_SO_Set), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Flash_SO_Get(const string &in, const string &in)", asFUNCTION(Flash_SO_Get), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Flash_SO_Has(const string &in, const string &in)", asFUNCTION(Flash_SO_Has), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SO_Flush(const string &in)", asFUNCTION(Flash_SO_Flush), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Flash_SO_Clear(const string &in)", asFUNCTION(Flash_SO_Clear), asCALL_CDECL));

    // --- Compatibility aliases (match transpiler output function names) ---
    AS_CHECK(engine->RegisterGlobalFunction("float Math_Random()", asFUNCTION(Flash_MathRandom), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Math_Floor(float)", asFUNCTION(Flash_MathFloor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Math_Ceil(float)", asFUNCTION(Flash_MathCeil), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Math_Round(float)", asFUNCTION(Flash_MathRound), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Math_Abs(float)", asFUNCTION(Flash_MathAbs), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Math_Sqrt(float)", asFUNCTION(Flash_MathSqrt), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Math_Sin(float)", asFUNCTION(Flash_MathSin), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Math_Cos(float)", asFUNCTION(Flash_MathCos), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Math_Atan2(float, float)", asFUNCTION(Flash_MathAtan2), asCALL_CDECL));

    ENJIN_LOG_INFO(Script, "Flash API shim registered: 48 global functions "
                           "(DisplayObject, Stage, Mouse, Keyboard, TextField, Math, Sound, Timer)");
}

} // namespace Scripting
} // namespace Enjin
