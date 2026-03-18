#pragma once

#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include <type_traits> // std::is_constant_evaluated

namespace Enjin {
namespace Math {

// Quaternion - for rotations
struct ENJIN_API Quaternion {
    f32 x, y, z, w;

    constexpr Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
    constexpr Quaternion(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}
    Quaternion(const Vector3& axis, f32 angle) {
        f32 halfAngle = angle * 0.5f;
        f32 s = Sin(halfAngle);
        Vector3 normalizedAxis = axis.Normalized();
        x = normalizedAxis.x * s;
        y = normalizedAxis.y * s;
        z = normalizedAxis.z * s;
        w = Cos(halfAngle);
    }

    // Operators
    constexpr Quaternion operator+(const Quaternion& other) const {
        return Quaternion(x + other.x, y + other.y, z + other.z, w + other.w);
    }

    constexpr Quaternion operator*(const Quaternion& other) const {
        return Quaternion(
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w,
            w * other.w - x * other.x - y * other.y - z * other.z
        );
    }

    constexpr Quaternion operator*(f32 scalar) const {
        return Quaternion(x * scalar, y * scalar, z * scalar, w * scalar);
    }

    constexpr Quaternion& operator*=(const Quaternion& other) {
        *this = *this * other;
        return *this;
    }

    // Functions
    constexpr f32 LengthSquared() const { return x * x + y * y + z * z + w * w; }
    f32 Length() const { return Sqrt(LengthSquared()); }
    Quaternion Normalized() const {
        f32 len = Length();
        return len > EPSILON ? (*this * (1.0f / len)) : Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }
    void Normalize() { *this = Normalized(); }

    constexpr Quaternion Conjugate() const {
        return Quaternion(-x, -y, -z, w);
    }

    constexpr Quaternion Inverse() const {
        f32 lenSq = LengthSquared();
        if (lenSq > EPSILON) {
            Quaternion conj = Conjugate();
            return conj * (1.0f / lenSq);
        }
        return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }

    constexpr Vector3 Rotate(const Vector3& v) const {
        Quaternion qv(v.x, v.y, v.z, 0.0f);
        Quaternion result = (*this) * qv * Inverse();
        return Vector3(result.x, result.y, result.z);
    }

    constexpr Matrix4 ToMatrix() const {
#if ENJIN_SIMD_SSE2
        if (!std::is_constant_evaluated()) {
            Matrix4 result;
            Simd::QuatToMatSSE(&x, result.m);
            return result;
        }
#endif
        // Scalar fallback (constexpr-compatible)
        f32 xx = x * x;
        f32 yy = y * y;
        f32 zz = z * z;
        f32 xy = x * y;
        f32 xz = x * z;
        f32 yz = y * z;
        f32 wx = w * x;
        f32 wy = w * y;
        f32 wz = w * z;

        Matrix4 result;
        result.m[0] = 1.0f - 2.0f * (yy + zz);
        result.m[1] = 2.0f * (xy + wz);
        result.m[2] = 2.0f * (xz - wy);
        result.m[3] = 0.0f;

        result.m[4] = 2.0f * (xy - wz);
        result.m[5] = 1.0f - 2.0f * (xx + zz);
        result.m[6] = 2.0f * (yz + wx);
        result.m[7] = 0.0f;

        result.m[8]  = 2.0f * (xz + wy);
        result.m[9]  = 2.0f * (yz - wx);
        result.m[10] = 1.0f - 2.0f * (xx + yy);
        result.m[11] = 0.0f;

        result.m[12] = 0.0f;
        result.m[13] = 0.0f;
        result.m[14] = 0.0f;
        result.m[15] = 1.0f;

        return result;
    }

    static constexpr Quaternion Identity() {
        return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // Extract Z-axis rotation angle directly (avoids full ToEuler decomposition)
    f32 GetRotationZ() const {
        return Atan2(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z));
    }

    // Get forward direction (-Z axis rotated by this quaternion)
    constexpr Vector3 GetForward() const {
        return Rotate(Vector3(0, 0, -1));
    }

    // Get right direction (+X axis rotated by this quaternion)
    constexpr Vector3 GetRight() const {
        return Rotate(Vector3(1, 0, 0));
    }

    // Get up direction (+Y axis rotated by this quaternion)
    constexpr Vector3 GetUp() const {
        return Rotate(Vector3(0, 1, 0));
    }

    // Convert quaternion to euler angles (radians, XYZ order)
    Vector3 ToEuler() const {
        Vector3 euler;
        // Roll (X)
        f32 sinr_cosp = 2.0f * (w * x + y * z);
        f32 cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
        euler.x = Atan2(sinr_cosp, cosr_cosp);
        // Pitch (Y)
        f32 sinp = 2.0f * (w * y - z * x);
        if (sinp >= 1.0f) euler.y = PI_HALF;        // +pi/2
        else if (sinp <= -1.0f) euler.y = -PI_HALF; // -pi/2
        else euler.y = Asin(sinp);
        // Yaw (Z)
        f32 siny_cosp = 2.0f * (w * z + x * y);
        f32 cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
        euler.z = Atan2(siny_cosp, cosy_cosp);
        return euler;
    }

    static Quaternion FromEuler(const Vector3& euler) {
        f32 halfX = euler.x * 0.5f;
        f32 halfY = euler.y * 0.5f;
        f32 halfZ = euler.z * 0.5f;

        f32 cx = Cos(halfX);
        f32 sx = Sin(halfX);
        f32 cy = Cos(halfY);
        f32 sy = Sin(halfY);
        f32 cz = Cos(halfZ);
        f32 sz = Sin(halfZ);

        // ZYX intrinsic rotation order (matches ToEuler extraction)
        return Quaternion(
            cz * cy * sx - sz * sy * cx,
            cz * sy * cx + sz * cy * sx,
            sz * cy * cx - cz * sy * sx,
            cz * cy * cx + sz * sy * sx
        );
    }

    // Alias for ToMatrix to match expected API
    constexpr Matrix4 ToMatrix4() const { return ToMatrix(); }

    // Spherical linear interpolation
    static Quaternion Slerp(const Quaternion& a, const Quaternion& b, f32 t) {
        // Compute dot product
        f32 dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

        Quaternion end = b;
        // If negative dot, negate one to take shorter path
        if (dot < 0.0f) {
            dot = -dot;
            end = Quaternion(-b.x, -b.y, -b.z, -b.w);
        }

        // If quaternions are very close, use linear interpolation
        if (dot > 0.9995f) {
            Quaternion result(
                a.x + t * (end.x - a.x),
                a.y + t * (end.y - a.y),
                a.z + t * (end.z - a.z),
                a.w + t * (end.w - a.w)
            );
            return result.Normalized();
        }

        // Compute slerp
        f32 theta = Acos(dot);
        f32 sinTheta = Sin(theta);
        f32 wa = Sin((1.0f - t) * theta) / sinTheta;
        f32 wb = Sin(t * theta) / sinTheta;

        return Quaternion(
            wa * a.x + wb * end.x,
            wa * a.y + wb * end.y,
            wa * a.z + wb * end.z,
            wa * a.w + wb * end.w
        );
    }

    // Dot product
    constexpr f32 Dot(const Quaternion& other) const {
        return x * other.x + y * other.y + z * other.z + w * other.w;
    }

    // Compute quaternion that rotates vector 'from' to vector 'to'
    static Quaternion FromToRotation(const Vector3& from, const Vector3& to) {
        Vector3 f = from.Normalized();
        Vector3 t = to.Normalized();
        f32 d = f.Dot(t);
        if (d > 0.9999f) return Identity();
        if (d < -0.9999f) {
            // 180-degree rotation around a perpendicular axis
            Vector3 perp = Vector3(1, 0, 0);
            if (std::abs(f.x) > 0.9f) perp = Vector3(0, 1, 0);
            Vector3 axis = f.Cross(perp).Normalized();
            return Quaternion(axis.x, axis.y, axis.z, 0.0f).Normalized();
        }
        Vector3 axis = f.Cross(t);
        return Quaternion(axis.x, axis.y, axis.z, 1.0f + d).Normalized();
    }
};

// Type alias
using Quat = Quaternion;

} // namespace Math
} // namespace Enjin
