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

    // Convert quaternion to euler angles in degrees (XYZ order)
    Vector3 ToEulerDegrees() const {
        Vector3 rad = ToEuler();
        return Vector3(Degrees(rad.x), Degrees(rad.y), Degrees(rad.z));
    }

    // Create quaternion from euler angles in degrees (XYZ order)
    static Quaternion FromEulerDegrees(const Vector3& degrees) {
        return FromEuler(Vector3(Radians(degrees.x), Radians(degrees.y), Radians(degrees.z)));
    }

    // Extract rotation quaternion from a 4x4 matrix (ignores translation and scale)
    static Quaternion FromMatrix(const Matrix4& mat) {
        // Normalize the 3x3 columns to strip scale
        Vector3 col0(mat.m[0], mat.m[1], mat.m[2]);
        Vector3 col1(mat.m[4], mat.m[5], mat.m[6]);
        Vector3 col2(mat.m[8], mat.m[9], mat.m[10]);
        f32 lenCol0 = col0.Length();
        f32 lenCol1 = col1.Length();
        f32 lenCol2 = col2.Length();
        if (lenCol0 < EPSILON) lenCol0 = 1.0f;
        if (lenCol1 < EPSILON) lenCol1 = 1.0f;
        if (lenCol2 < EPSILON) lenCol2 = 1.0f;
        f32 m00 = col0.x / lenCol0, m01 = col1.x / lenCol1, m02 = col2.x / lenCol2;
        f32 m10 = col0.y / lenCol0, m11 = col1.y / lenCol1, m12 = col2.y / lenCol2;
        f32 m20 = col0.z / lenCol0, m21 = col1.z / lenCol1, m22 = col2.z / lenCol2;

        f32 trace = m00 + m11 + m22;
        Quaternion q;
        if (trace > 0.0f) {
            f32 s = Sqrt(trace + 1.0f) * 2.0f;
            q.w = 0.25f * s;
            q.x = (m21 - m12) / s;
            q.y = (m02 - m20) / s;
            q.z = (m10 - m01) / s;
        } else if (m00 > m11 && m00 > m22) {
            f32 s = Sqrt(1.0f + m00 - m11 - m22) * 2.0f;
            q.w = (m21 - m12) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        } else if (m11 > m22) {
            f32 s = Sqrt(1.0f + m11 - m00 - m22) * 2.0f;
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        } else {
            f32 s = Sqrt(1.0f + m22 - m00 - m11) * 2.0f;
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }
        return q.Normalized();
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

    // Orientation whose local +Z maps to `forward` and local +Y maps as close as
    // possible to `up`. Unlike FromToRotation this fully determines the yaw, so
    // it is stable across the poles of a sphere (no arbitrary spin when up flips).
    // `forward` and `up` need not be exactly orthogonal; up is re-orthogonalized.
    static Quaternion LookRotation(const Vector3& forward, const Vector3& up) {
        Vector3 f = forward.Normalized();
        Vector3 r = up.Cross(f);            // right = up x forward
        if (r.LengthSquared() < 1e-8f) {    // forward ~parallel to up: pick any right
            Vector3 alt = (std::abs(f.y) < 0.99f) ? Vector3(0, 1, 0) : Vector3(1, 0, 0);
            r = alt.Cross(f);
        }
        r = r.Normalized();
        Vector3 u = f.Cross(r);             // re-orthogonalized up

        // Rotation matrix columns are (r, u, f); convert to quaternion (trace method).
        f32 m00 = r.x, m01 = u.x, m02 = f.x;
        f32 m10 = r.y, m11 = u.y, m12 = f.y;
        f32 m20 = r.z, m21 = u.z, m22 = f.z;
        f32 trace = m00 + m11 + m22;
        Quaternion q;
        if (trace > 0.0f) {
            f32 s = Sqrt(trace + 1.0f) * 2.0f;
            q.w = 0.25f * s;
            q.x = (m21 - m12) / s;
            q.y = (m02 - m20) / s;
            q.z = (m10 - m01) / s;
        } else if (m00 > m11 && m00 > m22) {
            f32 s = Sqrt(1.0f + m00 - m11 - m22) * 2.0f;
            q.w = (m21 - m12) / s;
            q.x = 0.25f * s;
            q.y = (m01 + m10) / s;
            q.z = (m02 + m20) / s;
        } else if (m11 > m22) {
            f32 s = Sqrt(1.0f + m11 - m00 - m22) * 2.0f;
            q.w = (m02 - m20) / s;
            q.x = (m01 + m10) / s;
            q.y = 0.25f * s;
            q.z = (m12 + m21) / s;
        } else {
            f32 s = Sqrt(1.0f + m22 - m00 - m11) * 2.0f;
            q.w = (m10 - m01) / s;
            q.x = (m02 + m20) / s;
            q.y = (m12 + m21) / s;
            q.z = 0.25f * s;
        }
        return q.Normalized();
    }
};

// Type alias
using Quat = Quaternion;

} // namespace Math
} // namespace Enjin
