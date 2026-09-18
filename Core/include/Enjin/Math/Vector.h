#pragma once

// There were TWO files at the include path "Enjin/Math/Vector.h": this one and
// Engine/include/Enjin/Math/Vector.h. Whichever include directory a translation
// unit searched first decided which Vector3 it got, and the Engine copy -- the
// one the engine and every test actually compiled against -- was the pre-SIMD
// version. So bf5c2d6e's SIMD math sprint never ran anywhere, and Core and
// Engine object files disagreed about the inline bodies of a type they pass to
// each other.
//
// It was invisible from the outside: both copies compute the same answers, so
// nothing failed. It surfaced only when a mutation test broke Normalized() HERE
// and all 65 math tests stayed green, because the header under test was the
// other one.
//
// The Engine copy is deleted. Do not add a second Enjin/Math/*.h anywhere.


#include "Enjin/Math/Math.h"
#include "Enjin/Math/Simd.h"
#include <cstring>
#include <type_traits> // std::is_constant_evaluated

namespace Enjin {
namespace Math {

// Forward declarations
struct Vector2;
struct Vector3;
struct Vector4;

// Vector2 - 2D vector
struct ENJIN_API Vector2 {
    f32 x, y;

    constexpr Vector2() : x(0.0f), y(0.0f) {}
    constexpr Vector2(f32 x, f32 y) : x(x), y(y) {}
    explicit constexpr Vector2(f32 scalar) : x(scalar), y(scalar) {}

    // Operators
    constexpr Vector2 operator+(const Vector2& other) const { return Vector2(x + other.x, y + other.y); }
    constexpr Vector2 operator-(const Vector2& other) const { return Vector2(x - other.x, y - other.y); }
    constexpr Vector2 operator*(f32 scalar) const { return Vector2(x * scalar, y * scalar); }
    constexpr Vector2 operator/(f32 scalar) const { return Vector2(x / scalar, y / scalar); }
    constexpr Vector2 operator-() const { return Vector2(-x, -y); }

    constexpr Vector2& operator+=(const Vector2& other) { x += other.x; y += other.y; return *this; }
    constexpr Vector2& operator-=(const Vector2& other) { x -= other.x; y -= other.y; return *this; }
    constexpr Vector2& operator*=(f32 scalar) { x *= scalar; y *= scalar; return *this; }
    constexpr Vector2& operator/=(f32 scalar) { x /= scalar; y /= scalar; return *this; }

    constexpr bool operator==(const Vector2& other) const {
        return IsEqual(x, other.x) && IsEqual(y, other.y);
    }
    constexpr bool operator!=(const Vector2& other) const { return !(*this == other); }

    f32& operator[](usize index) { return (&x)[index]; }
    const f32& operator[](usize index) const { return (&x)[index]; }

    // Functions
    constexpr f32 LengthSquared() const { return x * x + y * y; }
    f32 Length() const { return Sqrt(LengthSquared()); }
    Vector2 Normalized() const {
        f32 len = Length();
        return len > EPSILON ? (*this / len) : Vector2(0.0f);
    }
    void Normalize() { *this = Normalized(); }
    constexpr f32 Dot(const Vector2& other) const { return x * other.x + y * other.y; }
};

// Vector3 - 3D vector
struct ENJIN_API Vector3 {
    f32 x, y, z;

    constexpr Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr Vector3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}
    explicit constexpr Vector3(f32 scalar) : x(scalar), y(scalar), z(scalar) {}
    constexpr Vector3(const Vector2& v, f32 z) : x(v.x), y(v.y), z(z) {}

    // Operators
    constexpr Vector3 operator+(const Vector3& other) const { return Vector3(x + other.x, y + other.y, z + other.z); }
    constexpr Vector3 operator-(const Vector3& other) const { return Vector3(x - other.x, y - other.y, z - other.z); }
    constexpr Vector3 operator*(f32 scalar) const { return Vector3(x * scalar, y * scalar, z * scalar); }
    constexpr Vector3 operator/(f32 scalar) const { return Vector3(x / scalar, y / scalar, z / scalar); }
    constexpr Vector3 operator-() const { return Vector3(-x, -y, -z); }

    constexpr Vector3& operator+=(const Vector3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    constexpr Vector3& operator-=(const Vector3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    constexpr Vector3& operator*=(f32 scalar) { x *= scalar; y *= scalar; z *= scalar; return *this; }
    constexpr Vector3& operator/=(f32 scalar) { x /= scalar; y /= scalar; z /= scalar; return *this; }

    constexpr bool operator==(const Vector3& other) const {
        return IsEqual(x, other.x) && IsEqual(y, other.y) && IsEqual(z, other.z);
    }
    constexpr bool operator!=(const Vector3& other) const { return !(*this == other); }

    f32& operator[](usize index) { return (&x)[index]; }
    const f32& operator[](usize index) const { return (&x)[index]; }

    // Functions
    constexpr f32 LengthSquared() const { return x * x + y * y + z * z; }
    f32 Length() const { return Sqrt(LengthSquared()); }
    Vector3 Normalized() const {
        f32 len = Length();
        return len > EPSILON ? (*this / len) : Vector3(0.0f);
    }
    void Normalize() { *this = Normalized(); }
    constexpr f32 Dot(const Vector3& other) const {
#if ENJIN_SIMD_SSE2
        if (!std::is_constant_evaluated()) {
            return Simd::Vec3DotSSE(&x, &other.x);
        }
#endif
        return x * other.x + y * other.y + z * other.z;
    }
    constexpr Vector3 Cross(const Vector3& other) const {
#if ENJIN_SIMD_SSE2
        if (!std::is_constant_evaluated()) {
            Vector3 result;
            Simd::Vec3CrossSSE(&x, &other.x, &result.x);
            return result;
        }
#endif
        return Vector3(
            y * other.z - z * other.y,
            z * other.x - x * other.z,
            x * other.y - y * other.x
        );
    }
};

// Vector4 - 4D vector / Homogeneous coordinates
struct ENJIN_API Vector4 {
    f32 x, y, z, w;

    constexpr Vector4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    constexpr Vector4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}
    explicit constexpr Vector4(f32 scalar) : x(scalar), y(scalar), z(scalar), w(scalar) {}
    constexpr Vector4(const Vector3& v, f32 w) : x(v.x), y(v.y), z(v.z), w(w) {}
    constexpr Vector4(const Vector2& v, f32 z, f32 w) : x(v.x), y(v.y), z(z), w(w) {}

    // Operators
    constexpr Vector4 operator+(const Vector4& other) const {
#if ENJIN_SIMD_SSE2
        if (!std::is_constant_evaluated()) {
            Vector4 result;
            Simd::Vec4AddSSE(&x, &other.x, &result.x);
            return result;
        }
#endif
        return Vector4(x + other.x, y + other.y, z + other.z, w + other.w);
    }
    constexpr Vector4 operator-(const Vector4& other) const {
#if ENJIN_SIMD_SSE2
        if (!std::is_constant_evaluated()) {
            Vector4 result;
            Simd::Vec4SubSSE(&x, &other.x, &result.x);
            return result;
        }
#endif
        return Vector4(x - other.x, y - other.y, z - other.z, w - other.w);
    }
    constexpr Vector4 operator*(f32 scalar) const {
#if ENJIN_SIMD_SSE2
        if (!std::is_constant_evaluated()) {
            Vector4 result;
            Simd::Vec4MulScalarSSE(&x, scalar, &result.x);
            return result;
        }
#endif
        return Vector4(x * scalar, y * scalar, z * scalar, w * scalar);
    }
    constexpr Vector4 operator/(f32 scalar) const { return Vector4(x / scalar, y / scalar, z / scalar, w / scalar); }
    constexpr Vector4 operator-() const { return Vector4(-x, -y, -z, -w); }

    constexpr Vector4& operator+=(const Vector4& other) { x += other.x; y += other.y; z += other.z; w += other.w; return *this; }
    constexpr Vector4& operator-=(const Vector4& other) { x -= other.x; y -= other.y; z -= other.z; w -= other.w; return *this; }
    constexpr Vector4& operator*=(f32 scalar) { x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; }
    constexpr Vector4& operator/=(f32 scalar) { x /= scalar; y /= scalar; z /= scalar; w /= scalar; return *this; }

    constexpr bool operator==(const Vector4& other) const {
        return IsEqual(x, other.x) && IsEqual(y, other.y) && IsEqual(z, other.z) && IsEqual(w, other.w);
    }
    constexpr bool operator!=(const Vector4& other) const { return !(*this == other); }

    f32& operator[](usize index) { return (&x)[index]; }
    const f32& operator[](usize index) const { return (&x)[index]; }

    // Functions
    constexpr f32 LengthSquared() const { return x * x + y * y + z * z + w * w; }
    f32 Length() const { return Sqrt(LengthSquared()); }
    Vector4 Normalized() const {
        f32 len = Length();
        return len > EPSILON ? (*this / len) : Vector4(0.0f);
    }
    void Normalize() { *this = Normalized(); }
    constexpr f32 Dot(const Vector4& other) const {
#if ENJIN_SIMD_SSE2
        if (!std::is_constant_evaluated()) {
            return Simd::Vec4DotSSE(&x, &other.x);
        }
#endif
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }
};

// Type aliases
using Vec2 = Vector2;
using Vec3 = Vector3;
using Vec4 = Vector4;

// Interpolation functions.
// These lived only in the Engine copy of this header. See the note at the top
// of the file for why there was a second copy.
ENJIN_FORCE_INLINE Vector2 Lerp(const Vector2& a, const Vector2& b, f32 t) {
    return a + (b - a) * Clamp(t, 0.0f, 1.0f);
}

ENJIN_FORCE_INLINE Vector3 Lerp(const Vector3& a, const Vector3& b, f32 t) {
    return a + (b - a) * Clamp(t, 0.0f, 1.0f);
}

ENJIN_FORCE_INLINE Vector4 Lerp(const Vector4& a, const Vector4& b, f32 t) {
    return a + (b - a) * Clamp(t, 0.0f, 1.0f);
}

} // namespace Math
} // namespace Enjin
