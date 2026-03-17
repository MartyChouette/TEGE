#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>
#include <string>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// Helper: ensure entity has a TweenComponent, add a tween entry, start it
// Returns index of the newly added tween
// ============================================================================

static u32 AddAndStartTween(u64 entityId, TweenProperty prop, const Vector3& start, const Vector3& target, f32 duration, int easing, bool useCurrentAsStart) {
    if (!s_BindingsWorld) return 0;
    Entity entity = static_cast<Entity>(entityId);

    if (!s_BindingsWorld->HasComponent<TweenComponent>(entity)) {
        s_BindingsWorld->AddComponent<TweenComponent>(entity);
    }
    auto* tc = s_BindingsWorld->GetComponent<TweenComponent>(entity);
    if (!tc) return 0;

    TweenEntry te;
    te.property = prop;
    te.easing = static_cast<EasingType>(easing);
    te.mode = TweenMode::Once;
    te.startValue = start;
    te.endValue = target;
    te.duration = duration > 0.0f ? duration : 0.001f;
    te.useCurrentAsStart = useCurrentAsStart;
    te.isPlaying = true;
    te.isComplete = false;
    te.elapsed = 0.0f;
    te.direction = 1;

    // Capture current value as start if requested
    if (useCurrentAsStart) {
        switch (prop) {
        case TweenProperty::Position: {
            auto* tr = s_BindingsWorld->GetComponent<TransformComponent>(entity);
            if (tr) te.startValue = tr->position;
            break;
        }
        case TweenProperty::Rotation: {
            auto* tr = s_BindingsWorld->GetComponent<TransformComponent>(entity);
            if (tr) te.startValue = tr->rotation.ToEuler();
            break;
        }
        case TweenProperty::Scale: {
            auto* tr = s_BindingsWorld->GetComponent<TransformComponent>(entity);
            if (tr) te.startValue = tr->scale;
            break;
        }
        case TweenProperty::BaseColor: {
            auto* mat = s_BindingsWorld->GetComponent<MaterialComponent>(entity);
            if (mat) te.startValue = mat->baseColor;
            break;
        }
        case TweenProperty::EmissiveColor: {
            auto* mat = s_BindingsWorld->GetComponent<MaterialComponent>(entity);
            if (mat) te.startValue = mat->emissiveColor;
            break;
        }
        case TweenProperty::Opacity: {
            auto* mat = s_BindingsWorld->GetComponent<MaterialComponent>(entity);
            if (mat) te.startValue.x = mat->opacity;
            break;
        }
        default:
            break;
        }
    }

    u32 index = static_cast<u32>(tc->tweens.size());
    tc->tweens.push_back(te);
    return index;
}

// ============================================================================
// Script-callable functions (return uint tween index)
// ============================================================================

static u32 Tween_Position(u64 entity, const Vector3& target, f32 duration, int easing) {
    return AddAndStartTween(entity, TweenProperty::Position, Vector3(0.0f), target, duration, easing, true);
}

static u32 Tween_Rotation(u64 entity, const Vector3& target, f32 duration, int easing) {
    return AddAndStartTween(entity, TweenProperty::Rotation, Vector3(0.0f), target, duration, easing, true);
}

static u32 Tween_Scale(u64 entity, const Vector3& target, f32 duration, int easing) {
    return AddAndStartTween(entity, TweenProperty::Scale, Vector3(0.0f), target, duration, easing, true);
}

static u32 Tween_Color(u64 entity, const Vector3& target, f32 duration, int easing) {
    return AddAndStartTween(entity, TweenProperty::BaseColor, Vector3(0.0f), target, duration, easing, true);
}

static u32 Tween_Opacity(u64 entity, f32 target, f32 duration, int easing) {
    return AddAndStartTween(entity, TweenProperty::Opacity, Vector3(0.0f), Vector3(target, 0.0f, 0.0f), duration, easing, true);
}

static u32 Tween_Float(u64 entityId, f32 start, f32 end, f32 duration, int easing) {
    return AddAndStartTween(entityId, TweenProperty::Float, Vector3(start, 0.0f, 0.0f), Vector3(end, 0.0f, 0.0f), duration, easing, false);
}

static void Tween_SetOnComplete(u64 entityId, u32 tweenIndex, const std::string& funcName) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(entityId);
    auto* tc = s_BindingsWorld->GetComponent<TweenComponent>(entity);
    if (!tc || tweenIndex >= static_cast<u32>(tc->tweens.size())) return;
    tc->tweens[tweenIndex].onCompleteCallback = funcName;
}

static void Tween_SetDelay(u64 entityId, u32 tweenIndex, f32 delay) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(entityId);
    auto* tc = s_BindingsWorld->GetComponent<TweenComponent>(entity);
    if (!tc || tweenIndex >= static_cast<u32>(tc->tweens.size())) return;
    tc->tweens[tweenIndex].delay = delay > 0.0f ? delay : 0.0f;
}

static f32 Tween_GetValue(u64 entityId, u32 tweenIndex) {
    if (!s_BindingsWorld) return 0.0f;
    Entity entity = static_cast<Entity>(entityId);
    auto* tc = s_BindingsWorld->GetComponent<TweenComponent>(entity);
    if (!tc || tweenIndex >= static_cast<u32>(tc->tweens.size())) return 0.0f;
    return tc->tweens[tweenIndex].currentValue.x;
}

static void Tween_StopAll(u64 entityId) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(entityId);
    auto* tc = s_BindingsWorld->GetComponent<TweenComponent>(entity);
    if (!tc) return;

    for (auto& tw : tc->tweens) {
        tw.isPlaying = false;
        tw.isComplete = true;
    }
    tc->tweens.clear();
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterTweenBindings(asIScriptEngine* engine) {
    // Easing type constants
    AS_CHECK(engine->RegisterEnum("EasingType"));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_LINEAR", 0));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_QUAD", 1));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_OUT_QUAD", 2));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_OUT_QUAD", 3));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_CUBIC", 4));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_OUT_CUBIC", 5));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_OUT_CUBIC", 6));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_QUART", 7));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_OUT_QUART", 8));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_OUT_QUART", 9));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_SINE", 10));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_OUT_SINE", 11));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_OUT_SINE", 12));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_EXPO", 13));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_OUT_EXPO", 14));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_OUT_EXPO", 15));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_BACK", 16));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_OUT_BACK", 17));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_OUT_BACK", 18));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_ELASTIC", 19));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_OUT_ELASTIC", 20));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_OUT_ELASTIC", 21));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_BOUNCE", 22));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_OUT_BOUNCE", 23));
    AS_CHECK(engine->RegisterEnumValue("EasingType", "EASE_IN_OUT_BOUNCE", 24));

    // Tween functions — all return uint (tween index) except StopAll and setters
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint Tween_Position(uint64, const Vector3&in, float, int)",
        asFUNCTION(Tween_Position), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint Tween_Rotation(uint64, const Vector3&in, float, int)",
        asFUNCTION(Tween_Rotation), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint Tween_Scale(uint64, const Vector3&in, float, int)",
        asFUNCTION(Tween_Scale), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint Tween_Color(uint64, const Vector3&in, float, int)",
        asFUNCTION(Tween_Color), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint Tween_Opacity(uint64, float, float, int)",
        asFUNCTION(Tween_Opacity), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint Tween_Float(uint64, float, float, float, int)",
        asFUNCTION(Tween_Float), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Tween_SetOnComplete(uint64, uint, const string&in)",
        asFUNCTION(Tween_SetOnComplete), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Tween_SetDelay(uint64, uint, float)",
        asFUNCTION(Tween_SetDelay), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "float Tween_GetValue(uint64, uint)",
        asFUNCTION(Tween_GetValue), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Tween_StopAll(uint64)",
        asFUNCTION(Tween_StopAll), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
