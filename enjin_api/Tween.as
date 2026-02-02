// Tween.as — Easing and interpolation utilities for TegeBehavior scripts
// Usage:
//   float t = Tween::EaseInOut(elapsed / duration);
//   SetPosition(LerpVector3(startPos, endPos, t));

// Easing functions — all take t in [0,1] and return [0,1]
namespace Tween {

// Linear (no easing)
float Linear(float t) {
    return t;
}

// Quadratic
float EaseInQuad(float t) {
    return t * t;
}
float EaseOutQuad(float t) {
    return t * (2.0f - t);
}
float EaseInOutQuad(float t) {
    if (t < 0.5f) return 2.0f * t * t;
    return -1.0f + (4.0f - 2.0f * t) * t;
}

// Cubic
float EaseInCubic(float t) {
    return t * t * t;
}
float EaseOutCubic(float t) {
    float u = t - 1.0f;
    return u * u * u + 1.0f;
}
float EaseInOutCubic(float t) {
    if (t < 0.5f) return 4.0f * t * t * t;
    float u = 2.0f * t - 2.0f;
    return 0.5f * u * u * u + 1.0f;
}

// Quartic
float EaseInQuart(float t) {
    return t * t * t * t;
}
float EaseOutQuart(float t) {
    float u = t - 1.0f;
    return 1.0f - u * u * u * u;
}
float EaseInOutQuart(float t) {
    if (t < 0.5f) return 8.0f * t * t * t * t;
    float u = t - 1.0f;
    return 1.0f - 8.0f * u * u * u * u;
}

// Sine
float EaseInSine(float t) {
    return 1.0f - Cos(t * 3.14159265f * 0.5f);
}
float EaseOutSine(float t) {
    return Sin(t * 3.14159265f * 0.5f);
}
float EaseInOutSine(float t) {
    return 0.5f * (1.0f - Cos(3.14159265f * t));
}

// Exponential
float EaseInExpo(float t) {
    if (t <= 0.0f) return 0.0f;
    return Pow(2.0f, 10.0f * (t - 1.0f));
}
float EaseOutExpo(float t) {
    if (t >= 1.0f) return 1.0f;
    return 1.0f - Pow(2.0f, -10.0f * t);
}
float EaseInOutExpo(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    if (t < 0.5f) return 0.5f * Pow(2.0f, 20.0f * t - 10.0f);
    return 1.0f - 0.5f * Pow(2.0f, -20.0f * t + 10.0f);
}

// Back (overshoot)
float EaseInBack(float t) {
    float s = 1.70158f;
    return t * t * ((s + 1.0f) * t - s);
}
float EaseOutBack(float t) {
    float s = 1.70158f;
    float u = t - 1.0f;
    return u * u * ((s + 1.0f) * u + s) + 1.0f;
}
float EaseInOutBack(float t) {
    float s = 1.70158f * 1.525f;
    if (t < 0.5f) {
        float u = 2.0f * t;
        return 0.5f * (u * u * ((s + 1.0f) * u - s));
    }
    float u = 2.0f * t - 2.0f;
    return 0.5f * (u * u * ((s + 1.0f) * u + s) + 2.0f);
}

// Elastic
float EaseInElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return -Pow(2.0f, 10.0f * t - 10.0f) * Sin((t * 10.0f - 10.75f) * (2.0f * 3.14159265f / 3.0f));
}
float EaseOutElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    return Pow(2.0f, -10.0f * t) * Sin((t * 10.0f - 0.75f) * (2.0f * 3.14159265f / 3.0f)) + 1.0f;
}
float EaseInOutElastic(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    float c = (2.0f * 3.14159265f) / 4.5f;
    if (t < 0.5f)
        return -0.5f * Pow(2.0f, 20.0f * t - 10.0f) * Sin((20.0f * t - 11.125f) * c);
    return 0.5f * Pow(2.0f, -20.0f * t + 10.0f) * Sin((20.0f * t - 11.125f) * c) + 1.0f;
}

// Bounce
float EaseOutBounce(float t) {
    if (t < 1.0f / 2.75f) {
        return 7.5625f * t * t;
    } else if (t < 2.0f / 2.75f) {
        float u = t - 1.5f / 2.75f;
        return 7.5625f * u * u + 0.75f;
    } else if (t < 2.5f / 2.75f) {
        float u = t - 2.25f / 2.75f;
        return 7.5625f * u * u + 0.9375f;
    } else {
        float u = t - 2.625f / 2.75f;
        return 7.5625f * u * u + 0.984375f;
    }
}
float EaseInBounce(float t) {
    return 1.0f - EaseOutBounce(1.0f - t);
}
float EaseInOutBounce(float t) {
    if (t < 0.5f) return 0.5f * EaseInBounce(2.0f * t);
    return 0.5f * EaseOutBounce(2.0f * t - 1.0f) + 0.5f;
}

// Convenience aliases
float EaseIn(float t)    { return EaseInCubic(t); }
float EaseOut(float t)   { return EaseOutCubic(t); }
float EaseInOut(float t) { return EaseInOutCubic(t); }

} // namespace Tween

// Interpolation helpers using easing
float TweenFloat(float from, float to, float t) {
    return from + (to - from) * t;
}

Vector3 TweenVector3(const Vector3 &in from, const Vector3 &in to, float t) {
    return LerpVector3(from, to, t);
}
