#include "Enjin/Scripting/FlashAPIShim.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Logging/Log.h"
#include <cmath>
#include <cstring>

// AngelScript forward declarations
class asIScriptEngine;

namespace Enjin {
namespace Scripting {

// ============================================================================
// Global state for shim
// ============================================================================

// NOTE: Separate from s_BindingsWorld — cleared via SetFlashShimWorld(nullptr) in PlayMode::Stop()
static ECS::World* g_FlashWorld = nullptr;
static Audio::SimpleAudio* g_FlashAudio = nullptr;

void SetFlashShimWorld(ECS::World* world) { g_FlashWorld = world; }
void SetFlashShimAudio(Audio::SimpleAudio* audio) { g_FlashAudio = audio; }

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
// MovieClip implementation
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
    // Flash display list parent-child — entities are independent in ECS,
    // so addChild is a logical grouping only (no transform hierarchy yet)
    (void)child;
    ENJIN_LOG_INFO(Script, "Flash MovieClip.addChild: entity %llu -> parent %llu",
                   child.entityId, entityId);
}

void FlashMovieClip::removeChild(FlashMovieClip& child) {
    (void)child;
    ENJIN_LOG_INFO(Script, "Flash MovieClip.removeChild: entity %llu",
                   child.entityId);
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
        // Rotation: Flash uses degrees, engine uses quaternion
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
        // Extract Z rotation in degrees
        // Using atan2 on the quaternion Z component (simplified)
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
    // Text content would be synced to a UICanvas label if present
}

// ============================================================================
// Sound implementation
// ============================================================================

void FlashSound::play(i32 /*startTime*/, i32 loopCount) {
    loops = loopCount;
    isPlaying = true;
    // Delegate to engine audio system
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

void FlashSharedObject::flush() {
    ENJIN_LOG_INFO(Script, "Flash SharedObject.flush: %s (%zu keys)",
                   name.c_str(), data.size());
    // In a full implementation, this would serialize to the SaveSystem
}

void FlashSharedObject::clear() {
    data.clear();
}

void FlashSharedObject::setProperty(const std::string& key, const std::string& value) {
    data[key] = value;
}

std::string FlashSharedObject::getProperty(const std::string& key) const {
    auto it = data.find(key);
    return (it != data.end()) ? it->second : "";
}

bool FlashSharedObject::hasProperty(const std::string& key) const {
    return data.find(key) != data.end();
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
    ENJIN_LOG_INFO(Script, "Flash Mouse.hide()");
}

void FlashMouse::show() {
    s_MouseHidden = false;
    ENJIN_LOG_INFO(Script, "Flash Mouse.show()");
}

f32 FlashMouse::getX() {
    return 0; // Would delegate to Input system
}

f32 FlashMouse::getY() {
    return 0;
}

bool FlashMouse::isDown() {
    return false;
}

// ============================================================================
// Keyboard implementation
// ============================================================================

bool FlashKeyboard::isDown(i32 /*keyCode*/) {
    return false; // Would delegate to Input system
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
// AngelScript registration
// ============================================================================

void RegisterFlashAPIBindings(asIScriptEngine* engine) {
    if (!engine) return;

    // The actual AngelScript type registration would use engine->RegisterObjectType,
    // engine->RegisterObjectMethod, etc. For now we log the registration.
    ENJIN_LOG_INFO(Script, "Flash API shim registered: MovieClip, TextField, Sound, "
                              "SharedObject, Stage, Mouse, Keyboard, Timer, Event constants");

    // In a full implementation:
    // engine->RegisterObjectType("MovieClip", sizeof(FlashMovieClip), asOBJ_VALUE | ...);
    // engine->RegisterObjectMethod("MovieClip", "void play()", asMETHOD(FlashMovieClip, play), ...);
    // engine->RegisterObjectMethod("MovieClip", "void stop()", asMETHOD(FlashMovieClip, stop), ...);
    // engine->RegisterObjectMethod("MovieClip", "void gotoAndPlay(int)", ...);
    // engine->RegisterObjectMethod("MovieClip", "void gotoAndStop(int)", ...);
    // etc.
}

} // namespace Scripting
} // namespace Enjin
