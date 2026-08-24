#pragma once

#include "Enjin/Platform/Types.h"
#include <cmath>
#include <limits>
#include <cstdlib>

namespace Enjin {
namespace Math {

// Constants
constexpr f32 PI = 3.14159265358979323846f;
constexpr f32 PI_2 = PI * 2.0f;
constexpr f32 PI_HALF = PI * 0.5f;
constexpr f32 EPSILON = std::numeric_limits<f32>::epsilon();
constexpr f32 FLOAT_MAX = std::numeric_limits<f32>::max();
constexpr f32 FLOAT_MIN = std::numeric_limits<f32>::lowest();

// Utility functions
constexpr ENJIN_FORCE_INLINE f32 Radians(f32 degrees) { return degrees * PI / 180.0f; }
constexpr ENJIN_FORCE_INLINE f32 Degrees(f32 radians) { return radians * 180.0f / PI; }
constexpr ENJIN_FORCE_INLINE f32 Abs(f32 value) { return value < 0.0f ? -value : value; }
ENJIN_FORCE_INLINE f32 Sqrt(f32 value) { return std::sqrt(value); }
ENJIN_FORCE_INLINE f32 Sin(f32 value) { return std::sin(value); }
ENJIN_FORCE_INLINE f32 Cos(f32 value) { return std::cos(value); }
ENJIN_FORCE_INLINE f32 Tan(f32 value) { return std::tan(value); }
ENJIN_FORCE_INLINE f32 Asin(f32 value) { return std::asin(value); }
ENJIN_FORCE_INLINE f32 Acos(f32 value) { return std::acos(value); }
ENJIN_FORCE_INLINE f32 Atan(f32 value) { return std::atan(value); }
ENJIN_FORCE_INLINE f32 Atan2(f32 y, f32 x) { return std::atan2(y, x); }
ENJIN_FORCE_INLINE f32 Pow(f32 base, f32 exponent) { return std::pow(base, exponent); }
ENJIN_FORCE_INLINE f32 Exp(f32 value) { return std::exp(value); }
ENJIN_FORCE_INLINE f32 Log(f32 value) { return std::log(value); }
ENJIN_FORCE_INLINE f32 Log2(f32 value) { return std::log2(value); }
ENJIN_FORCE_INLINE f32 Floor(f32 value) { return std::floor(value); }
ENJIN_FORCE_INLINE f32 Ceil(f32 value) { return std::ceil(value); }
ENJIN_FORCE_INLINE f32 Round(f32 value) { return std::round(value); }
ENJIN_FORCE_INLINE f32 Fmod(f32 x, f32 y) { return std::fmod(x, y); }

constexpr ENJIN_FORCE_INLINE f32 Min(f32 a, f32 b) { return a < b ? a : b; }
constexpr ENJIN_FORCE_INLINE f32 Max(f32 a, f32 b) { return a > b ? a : b; }
constexpr ENJIN_FORCE_INLINE f32 Clamp(f32 value, f32 min, f32 max) {
    return value < min ? min : (value > max ? max : value);
}
constexpr ENJIN_FORCE_INLINE f32 Lerp(f32 a, f32 b, f32 t) {
    return a + (b - a) * Clamp(t, 0.0f, 1.0f);
}
constexpr ENJIN_FORCE_INLINE f32 SmoothStep(f32 edge0, f32 edge1, f32 x) {
    f32 t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Comparison functions
constexpr ENJIN_FORCE_INLINE bool IsNearZero(f32 value, f32 epsilon = EPSILON) {
    return Abs(value) < epsilon;
}
constexpr ENJIN_FORCE_INLINE bool IsEqual(f32 a, f32 b, f32 epsilon = EPSILON) {
    return Abs(a - b) < epsilon;
}

constexpr ENJIN_FORCE_INLINE f32 Sign(f32 value) {
    return (value > 0.0f) ? 1.0f : ((value < 0.0f) ? -1.0f : 0.0f);
}

// Move value towards target by maxDelta (for smooth movement)
constexpr ENJIN_FORCE_INLINE f32 MoveTowards(f32 current, f32 target, f32 maxDelta) {
    if (Abs(target - current) <= maxDelta) {
        return target;
    }
    return current + Sign(target - current) * maxDelta;
}

// Normalize angle to -180 to 180 range
constexpr ENJIN_FORCE_INLINE f32 NormalizeAngle(f32 angle) {
    while (angle > 180.0f) angle -= 360.0f;
    while (angle < -180.0f) angle += 360.0f;
    return angle;
}

// Move angle towards target by maxDelta, handling wrap-around
constexpr ENJIN_FORCE_INLINE f32 MoveTowardsAngle(f32 current, f32 target, f32 maxDelta) {
    f32 delta = NormalizeAngle(target - current);
    if (Abs(delta) <= maxDelta) {
        return target;
    }
    return current + Sign(delta) * maxDelta;
}

// Calculate shortest angular delta between two angles
constexpr ENJIN_FORCE_INLINE f32 DeltaAngle(f32 current, f32 target) {
    return NormalizeAngle(target - current);
}

// Inverse lerp - finds t given value between a and b
constexpr ENJIN_FORCE_INLINE f32 InverseLerp(f32 a, f32 b, f32 value) {
    if (Abs(b - a) < EPSILON) return 0.0f;
    return Clamp((value - a) / (b - a), 0.0f, 1.0f);
}

// Remap value from one range to another
constexpr ENJIN_FORCE_INLINE f32 Remap(f32 value, f32 fromMin, f32 fromMax, f32 toMin, f32 toMax) {
    f32 t = InverseLerp(fromMin, fromMax, value);
    return Lerp(toMin, toMax, t);
}

// S14: Fast xorshift32 PRNG replaces weak rand()
inline u32& XorShiftState() {
    static u32 state = 2463534242u;
    return state;
}

// Reseed the shared stream. Replay determinism: PlayMode seeds each play
// session and records the seed, so a replay reproduces every script
// Random()/RandomRange() call. 0 restores the historical default state.
inline void SetRandomSeed(u32 seed) {
    XorShiftState() = seed ? seed : 2463534242u;
}

ENJIN_FORCE_INLINE f32 Random01() {
    u32& s = XorShiftState();
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return static_cast<f32>(s & 0x7FFFFFFF) * (1.0f / 2147483647.0f);
}

ENJIN_FORCE_INLINE f32 Random(f32 min, f32 max) {
    return min + Random01() * (max - min);
}

ENJIN_FORCE_INLINE i32 RandomInt(i32 min, i32 max) {
    if (max <= min) return min;
    u32& s = XorShiftState();
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return min + static_cast<i32>(s % static_cast<u32>(max - min + 1));
}

} // namespace Math
} // namespace Enjin
