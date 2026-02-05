#include "Enjin/ECS/Systems/TweenSystem.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Math/Quaternion.h"
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace ECS {

// ============================================================================
// Easing functions
// ============================================================================

static const f32 PI = 3.14159265358979323846f;

static f32 EaseOutBounce(f32 t) {
    if (t < 1.0f / 2.75f) {
        return 7.5625f * t * t;
    } else if (t < 2.0f / 2.75f) {
        t -= 1.5f / 2.75f;
        return 7.5625f * t * t + 0.75f;
    } else if (t < 2.5f / 2.75f) {
        t -= 2.25f / 2.75f;
        return 7.5625f * t * t + 0.9375f;
    } else {
        t -= 2.625f / 2.75f;
        return 7.5625f * t * t + 0.984375f;
    }
}

f32 ApplyEasing(f32 t, EasingType type) {
    t = std::clamp(t, 0.0f, 1.0f);

    switch (type) {
    case EasingType::Linear:
        return t;

    // Quad
    case EasingType::EaseInQuad:
        return t * t;
    case EasingType::EaseOutQuad:
        return t * (2.0f - t);
    case EasingType::EaseInOutQuad:
        return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;

    // Cubic
    case EasingType::EaseInCubic:
        return t * t * t;
    case EasingType::EaseOutCubic: {
        f32 u = t - 1.0f;
        return u * u * u + 1.0f;
    }
    case EasingType::EaseInOutCubic:
        return t < 0.5f ? 4.0f * t * t * t : 1.0f + (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f);

    // Quart
    case EasingType::EaseInQuart:
        return t * t * t * t;
    case EasingType::EaseOutQuart: {
        f32 u = t - 1.0f;
        return 1.0f - u * u * u * u;
    }
    case EasingType::EaseInOutQuart: {
        if (t < 0.5f) return 8.0f * t * t * t * t;
        f32 u = t - 1.0f;
        return 1.0f - 8.0f * u * u * u * u;
    }

    // Sine
    case EasingType::EaseInSine:
        return 1.0f - std::cos(t * PI * 0.5f);
    case EasingType::EaseOutSine:
        return std::sin(t * PI * 0.5f);
    case EasingType::EaseInOutSine:
        return 0.5f * (1.0f - std::cos(PI * t));

    // Expo
    case EasingType::EaseInExpo:
        return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
    case EasingType::EaseOutExpo:
        return t == 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
    case EasingType::EaseInOutExpo:
        if (t == 0.0f) return 0.0f;
        if (t == 1.0f) return 1.0f;
        return t < 0.5f
            ? 0.5f * std::pow(2.0f, 20.0f * t - 10.0f)
            : 1.0f - 0.5f * std::pow(2.0f, -20.0f * t + 10.0f);

    // Back
    case EasingType::EaseInBack: {
        const f32 s = 1.70158f;
        return t * t * ((s + 1.0f) * t - s);
    }
    case EasingType::EaseOutBack: {
        const f32 s = 1.70158f;
        f32 u = t - 1.0f;
        return u * u * ((s + 1.0f) * u + s) + 1.0f;
    }
    case EasingType::EaseInOutBack: {
        const f32 s = 1.70158f * 1.525f;
        if (t < 0.5f) {
            return 0.5f * (4.0f * t * t * ((s + 1.0f) * 2.0f * t - s));
        }
        f32 u = 2.0f * t - 2.0f;
        return 0.5f * (u * u * ((s + 1.0f) * u + s) + 2.0f);
    }

    // Elastic
    case EasingType::EaseInElastic:
        if (t == 0.0f) return 0.0f;
        if (t == 1.0f) return 1.0f;
        return -std::pow(2.0f, 10.0f * t - 10.0f) * std::sin((t * 10.0f - 10.75f) * (2.0f * PI / 3.0f));
    case EasingType::EaseOutElastic:
        if (t == 0.0f) return 0.0f;
        if (t == 1.0f) return 1.0f;
        return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * (2.0f * PI / 3.0f)) + 1.0f;
    case EasingType::EaseInOutElastic:
        if (t == 0.0f) return 0.0f;
        if (t == 1.0f) return 1.0f;
        if (t < 0.5f)
            return -0.5f * std::pow(2.0f, 20.0f * t - 10.0f) * std::sin((20.0f * t - 11.125f) * (2.0f * PI / 4.5f));
        return 0.5f * std::pow(2.0f, -20.0f * t + 10.0f) * std::sin((20.0f * t - 11.125f) * (2.0f * PI / 4.5f)) + 1.0f;

    // Bounce
    case EasingType::EaseInBounce:
        return 1.0f - EaseOutBounce(1.0f - t);
    case EasingType::EaseOutBounce:
        return EaseOutBounce(t);
    case EasingType::EaseInOutBounce:
        return t < 0.5f
            ? 0.5f * (1.0f - EaseOutBounce(1.0f - 2.0f * t))
            : 0.5f * (1.0f + EaseOutBounce(2.0f * t - 1.0f));

    default:
        return t;
    }
}

// ============================================================================
// TweenSystem
// ============================================================================

void TweenSystem::CaptureStartValue(World* world, Entity entity, TweenEntry& tween) {
    switch (tween.property) {
    case TweenProperty::Position: {
        auto* tr = world->GetComponent<TransformComponent>(entity);
        if (tr) tween.startValue = tr->position;
        break;
    }
    case TweenProperty::Rotation: {
        auto* tr = world->GetComponent<TransformComponent>(entity);
        if (tr) tween.startValue = tr->rotation.ToEuler();
        break;
    }
    case TweenProperty::Scale: {
        auto* tr = world->GetComponent<TransformComponent>(entity);
        if (tr) tween.startValue = tr->scale;
        break;
    }
    case TweenProperty::BaseColor: {
        auto* mat = world->GetComponent<MaterialComponent>(entity);
        if (mat) tween.startValue = mat->baseColor;
        break;
    }
    case TweenProperty::EmissiveColor: {
        auto* mat = world->GetComponent<MaterialComponent>(entity);
        if (mat) tween.startValue = mat->emissiveColor;
        break;
    }
    case TweenProperty::Opacity: {
        auto* mat = world->GetComponent<MaterialComponent>(entity);
        if (mat) tween.startValue.x = mat->opacity;
        break;
    }
    case TweenProperty::Float:
        // No component to capture from — start value is set by the binding or user
        break;
    default:
        break;
    }
}

void TweenSystem::ApplyTween(World* world, Entity entity, TweenEntry& tween, f32 t) {
    f32 easedT = ApplyEasing(tween.direction > 0 ? t : 1.0f - t, tween.easing);

    Math::Vector3 value;
    value.x = tween.startValue.x + (tween.endValue.x - tween.startValue.x) * easedT;
    value.y = tween.startValue.y + (tween.endValue.y - tween.startValue.y) * easedT;
    value.z = tween.startValue.z + (tween.endValue.z - tween.startValue.z) * easedT;

    // Store interpolated value every frame (for Tween_GetValue and inspector readout)
    tween.currentValue = value;

    switch (tween.property) {
    case TweenProperty::Position: {
        auto* tr = world->GetComponent<TransformComponent>(entity);
        if (tr) tr->position = value;
        break;
    }
    case TweenProperty::Rotation: {
        auto* tr = world->GetComponent<TransformComponent>(entity);
        if (tr) tr->rotation = Math::Quaternion::FromEuler(value);
        break;
    }
    case TweenProperty::Scale: {
        auto* tr = world->GetComponent<TransformComponent>(entity);
        if (tr) tr->scale = value;
        break;
    }
    case TweenProperty::BaseColor: {
        auto* mat = world->GetComponent<MaterialComponent>(entity);
        if (mat) mat->baseColor = value;
        break;
    }
    case TweenProperty::EmissiveColor: {
        auto* mat = world->GetComponent<MaterialComponent>(entity);
        if (mat) mat->emissiveColor = value;
        break;
    }
    case TweenProperty::Opacity: {
        auto* mat = world->GetComponent<MaterialComponent>(entity);
        if (mat) mat->opacity = std::clamp(value.x, 0.0f, 1.0f);
        break;
    }
    case TweenProperty::Float:
        // No component write — value is stored in currentValue above
        break;
    default:
        break;
    }
}

void TweenSystem::PlayAll(World* world) {
    auto entities = world->GetEntitiesWithComponent<TweenComponent>();
    for (auto entity : entities) {
        auto* tc = world->GetComponent<TweenComponent>(entity);
        if (!tc) continue;

        for (auto& tween : tc->tweens) {
            if (tc->autoPlay || tween.isPlaying) {
                if (tween.useCurrentAsStart) {
                    CaptureStartValue(world, entity, tween);
                }
                tween.isPlaying = true;
                tween.isComplete = false;
                tween.elapsed = 0.0f;
                tween.direction = 1;
            }
        }
    }
}

void TweenSystem::InvokeCallback(World* world, Entity entity, const std::string& methodName) {
    if (!m_ScriptEngine || methodName.empty()) return;

    auto* scriptComp = world->GetComponent<ScriptComponent>(entity);
    if (!scriptComp) return;

    for (auto& attachment : scriptComp->scripts) {
        if (!attachment.enabled || !attachment.instance) continue;

        auto* obj = static_cast<asIScriptObject*>(attachment.instance);
        auto* func = m_ScriptEngine->FindMethod(obj, "void " + methodName + "()");
        if (!func) continue;

        auto* ctx = m_ScriptEngine->AcquireContext();
        if (ctx) {
            m_ScriptEngine->ExecuteMethod(ctx, obj, func);
            m_ScriptEngine->ReturnContext(ctx);
        }
    }
}

void TweenSystem::Update(World* world, f32 deltaTime) {
    // Collect callbacks to dispatch after iteration (prevents iterator invalidation
    // if a callback creates or removes tweens)
    struct PendingCallback {
        Entity entity;
        std::string methodName;
    };
    std::vector<PendingCallback> pendingCallbacks;

    auto entities = world->GetEntitiesWithComponent<TweenComponent>();
    for (auto entity : entities) {
        auto* tc = world->GetComponent<TweenComponent>(entity);
        if (!tc) continue;

        for (auto& tween : tc->tweens) {
            if (!tween.isPlaying || tween.isComplete) continue;

            tween.elapsed += deltaTime;

            // Still in delay period
            if (tween.elapsed < tween.delay) continue;

            f32 activeTime = tween.elapsed - tween.delay;
            f32 dur = std::max(tween.duration, 0.001f);
            f32 t = std::clamp(activeTime / dur, 0.0f, 1.0f);

            ApplyTween(world, entity, tween, t);

            // Check completion
            if (activeTime >= dur) {
                switch (tween.mode) {
                case TweenMode::Once:
                    tween.isComplete = true;
                    tween.isPlaying = false;
                    // Snap to final value
                    ApplyTween(world, entity, tween, 1.0f);
                    // Queue callback for deferred dispatch
                    if (!tween.onCompleteCallback.empty()) {
                        pendingCallbacks.push_back({entity, tween.onCompleteCallback});
                    }
                    break;

                case TweenMode::Loop:
                    tween.elapsed = tween.delay; // reset to start (keep delay offset)
                    break;

                case TweenMode::PingPong:
                    tween.direction *= -1;
                    tween.elapsed = tween.delay; // reset to start
                    break;

                default:
                    tween.isComplete = true;
                    tween.isPlaying = false;
                    break;
                }
            }
        }
    }

    // Dispatch deferred callbacks
    for (const auto& cb : pendingCallbacks) {
        InvokeCallback(world, cb.entity, cb.methodName);
    }
}

} // namespace ECS
} // namespace Enjin
