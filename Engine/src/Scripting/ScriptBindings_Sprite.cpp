#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <angelscript.h>
#include <string>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// Sprite2DComponent bindings
// ============================================================================

static void Sprite_SetTexture(u64 id, const std::string& path) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<Sprite2DComponent>(static_cast<Entity>(id));
    if (sc) {
        sc->texturePath = path;
        sc->spriteDirty = true;
    }
}

static std::string Sprite_GetTexture(u64 id) {
    if (!s_BindingsWorld) return "";
    auto* sc = s_BindingsWorld->GetComponent<Sprite2DComponent>(static_cast<Entity>(id));
    return sc ? sc->texturePath : "";
}

static void Sprite_SetColor(u64 id, f32 r, f32 g, f32 b, f32 a) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<Sprite2DComponent>(static_cast<Entity>(id));
    if (sc) {
        sc->tint = Vector3(r, g, b);
        sc->alpha = a;
        sc->spriteDirty = true;
    }
}

static void Sprite_SetAlpha(u64 id, f32 alpha) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<Sprite2DComponent>(static_cast<Entity>(id));
    if (sc) {
        sc->alpha = alpha;
        sc->spriteDirty = true;
    }
}

static void Sprite_SetFlipX(u64 id, bool flip) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<Sprite2DComponent>(static_cast<Entity>(id));
    if (sc) {
        sc->flipX = flip;
        sc->spriteDirty = true;
    }
}

static void Sprite_SetFlipY(u64 id, bool flip) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<Sprite2DComponent>(static_cast<Entity>(id));
    if (sc) {
        sc->flipY = flip;
        sc->spriteDirty = true;
    }
}

static void Sprite_SetSortOrder(u64 id, i32 order) {
    if (!s_BindingsWorld) return;
    auto* sc = s_BindingsWorld->GetComponent<Sprite2DComponent>(static_cast<Entity>(id));
    if (sc) sc->orderInLayer = order;
}

static void Sprite_SetVisible(u64 id, bool visible) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<TransformComponent>(static_cast<Entity>(id));
    if (tc) tc->visible = visible;
}

// ============================================================================
// AnimatedSprite2DComponent bindings
// ============================================================================

static void SpriteAnim_Play(u64 id, const std::string& /*animName*/) {
    if (!s_BindingsWorld) return;
    auto* ac = s_BindingsWorld->GetComponent<AnimatedSprite2DComponent>(static_cast<Entity>(id));
    if (ac) {
        ac->playing = true;
        ac->animationComplete = false;
        ac->currentFrame = 0;
        ac->frameTimer = 0.0f;
    }
}

static void SpriteAnim_Stop(u64 id) {
    if (!s_BindingsWorld) return;
    auto* ac = s_BindingsWorld->GetComponent<AnimatedSprite2DComponent>(static_cast<Entity>(id));
    if (ac) ac->playing = false;
}

static void SpriteAnim_SetSpeed(u64 id, f32 speed) {
    if (!s_BindingsWorld) return;
    auto* ac = s_BindingsWorld->GetComponent<AnimatedSprite2DComponent>(static_cast<Entity>(id));
    if (ac) ac->playbackSpeed = speed;
}

static bool SpriteAnim_IsPlaying(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* ac = s_BindingsWorld->GetComponent<AnimatedSprite2DComponent>(static_cast<Entity>(id));
    return ac ? ac->playing : false;
}

static u32 SpriteAnim_GetCurrentFrame(u64 id) {
    if (!s_BindingsWorld) return 0;
    auto* ac = s_BindingsWorld->GetComponent<AnimatedSprite2DComponent>(static_cast<Entity>(id));
    return ac ? ac->currentFrame : 0;
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterSpriteBindings(asIScriptEngine* engine) {
    // Sprite2D
    AS_CHECK(engine->RegisterGlobalFunction("void Sprite_SetTexture(uint64, const string &in)", asFUNCTION(Sprite_SetTexture), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Sprite_GetTexture(uint64)", asFUNCTION(Sprite_GetTexture), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Sprite_SetColor(uint64, float, float, float, float)", asFUNCTION(Sprite_SetColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Sprite_SetAlpha(uint64, float)", asFUNCTION(Sprite_SetAlpha), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Sprite_SetFlipX(uint64, bool)", asFUNCTION(Sprite_SetFlipX), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Sprite_SetFlipY(uint64, bool)", asFUNCTION(Sprite_SetFlipY), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Sprite_SetSortOrder(uint64, int)", asFUNCTION(Sprite_SetSortOrder), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Sprite_SetVisible(uint64, bool)", asFUNCTION(Sprite_SetVisible), asCALL_CDECL));

    // AnimatedSprite2D
    AS_CHECK(engine->RegisterGlobalFunction("void SpriteAnim_Play(uint64, const string &in)", asFUNCTION(SpriteAnim_Play), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SpriteAnim_Stop(uint64)", asFUNCTION(SpriteAnim_Stop), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SpriteAnim_SetSpeed(uint64, float)", asFUNCTION(SpriteAnim_SetSpeed), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool SpriteAnim_IsPlaying(uint64)", asFUNCTION(SpriteAnim_IsPlaying), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint SpriteAnim_GetCurrentFrame(uint64)", asFUNCTION(SpriteAnim_GetCurrentFrame), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
