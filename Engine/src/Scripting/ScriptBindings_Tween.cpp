#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Tween.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// Helper: ensure entity has a TweenComponent, add a tween entry, start it
// ============================================================================

static void AddAndStartTween(u64 entityId, TweenProperty prop, const Vector3& target, f32 duration, int easing) {
    if (!s_BindingsWorld) return;
    Entity entity = static_cast<Entity>(entityId);

    if (!s_BindingsWorld->HasComponent<TweenComponent>(entity)) {
        s_BindingsWorld->AddComponent<TweenComponent>(entity);
    }
    auto* tc = s_BindingsWorld->GetComponent<TweenComponent>(entity);
    if (!tc) return;

    TweenEntry te;
    te.property = prop;
    te.easing = static_cast<EasingType>(easing);
    te.mode = TweenMode::Once;
    te.endValue = target;
    te.duration = duration > 0.0f ? duration : 0.001f;
    te.useCurrentAsStart = true;
    te.isPlaying = true;
    te.isComplete = false;
    te.elapsed = 0.0f;
    te.direction = 1;

    // Capture current value as start
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

    tc->tweens.push_back(te);
}

// ============================================================================
// Script-callable functions
// ============================================================================

static void Tween_Position(u64 entity, const Vector3& target, f32 duration, int easing) {
    AddAndStartTween(entity, TweenProperty::Position, target, duration, easing);
}

static void Tween_Rotation(u64 entity, const Vector3& target, f32 duration, int easing) {
    AddAndStartTween(entity, TweenProperty::Rotation, target, duration, easing);
}

static void Tween_Scale(u64 entity, const Vector3& target, f32 duration, int easing) {
    AddAndStartTween(entity, TweenProperty::Scale, target, duration, easing);
}

static void Tween_Color(u64 entity, const Vector3& target, f32 duration, int easing) {
    AddAndStartTween(entity, TweenProperty::BaseColor, target, duration, easing);
}

static void Tween_Opacity(u64 entity, f32 target, f32 duration, int easing) {
    AddAndStartTween(entity, TweenProperty::Opacity, Vector3(target, 0.0f, 0.0f), duration, easing);
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

    // Tween functions
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Tween_Position(uint64, const Vector3&in, float, int)",
        asFUNCTION(Tween_Position), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Tween_Rotation(uint64, const Vector3&in, float, int)",
        asFUNCTION(Tween_Rotation), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Tween_Scale(uint64, const Vector3&in, float, int)",
        asFUNCTION(Tween_Scale), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Tween_Color(uint64, const Vector3&in, float, int)",
        asFUNCTION(Tween_Color), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Tween_Opacity(uint64, float, float, int)",
        asFUNCTION(Tween_Opacity), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Tween_StopAll(uint64)",
        asFUNCTION(Tween_StopAll), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
