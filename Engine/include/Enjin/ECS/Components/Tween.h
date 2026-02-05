#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS {

// 25 standard easing functions
enum class EasingType : u8 {
    Linear = 0,
    EaseInQuad, EaseOutQuad, EaseInOutQuad,
    EaseInCubic, EaseOutCubic, EaseInOutCubic,
    EaseInQuart, EaseOutQuart, EaseInOutQuart,
    EaseInSine, EaseOutSine, EaseInOutSine,
    EaseInExpo, EaseOutExpo, EaseInOutExpo,
    EaseInBack, EaseOutBack, EaseInOutBack,
    EaseInElastic, EaseOutElastic, EaseInOutElastic,
    EaseInBounce, EaseOutBounce, EaseInOutBounce,
    COUNT
};

// What property to tween
enum class TweenProperty : u8 {
    Position = 0,
    Rotation,       // Euler degrees
    Scale,
    BaseColor,      // MaterialComponent.baseColor
    EmissiveColor,  // MaterialComponent.emissiveColor
    Opacity,        // MaterialComponent.opacity
    Float,          // Generic float interpolation (no component write, value in currentValue.x)
    COUNT
};

// Playback mode
enum class TweenMode : u8 {
    Once = 0,       // Play once then stop
    Loop,           // Restart from beginning
    PingPong,       // Reverse direction at each end
    COUNT
};

// Single tween entry — one property animation
struct TweenEntry {
    TweenProperty property = TweenProperty::Position;
    EasingType easing = EasingType::EaseInOutCubic;
    TweenMode mode = TweenMode::Once;

    Math::Vector3 startValue = Math::Vector3(0.0f);
    Math::Vector3 endValue = Math::Vector3(0.0f);

    f32 duration = 1.0f;
    f32 delay = 0.0f;
    f32 elapsed = 0.0f;

    bool isPlaying = false;
    bool isComplete = false;
    bool useCurrentAsStart = true; // grab current value at play start
    i32 direction = 1;             // 1=forward, -1=reverse (ping-pong)

    std::string onCompleteCallback; // AngelScript function name called on completion (Once mode)
    Math::Vector3 currentValue = Math::Vector3(0.0f); // Interpolated value each frame (runtime only, not serialized)
};

// Component holding one or more active tweens on an entity
struct TweenComponent {
    std::vector<TweenEntry> tweens;
    bool autoPlay = false; // start all tweens on play mode enter
};

// Evaluate an easing function for t in [0,1]
ENJIN_API f32 ApplyEasing(f32 t, EasingType type);

} // namespace ECS
} // namespace Enjin
