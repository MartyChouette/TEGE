#pragma once

#include "Enjin/Math/Vector.h"
#include "Enjin/Memory/Memory.h"
#include <cstring>

namespace Enjin {
namespace Math {

// Forward declarations
struct Matrix3;
struct Matrix4;

// Matrix4 - 4x4 matrix (column-major order for OpenGL/Vulkan compatibility)
struct ENJIN_API Matrix4 {
    f32 m[16]; // Column-major: m[0-3] = col0, m[4-7] = col1, etc.

    Matrix4() {
        MemoryZero(m, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = 1.0f; // Identity
    }

    explicit Matrix4(f32 diagonal) {
        MemoryZero(m, sizeof(m));
        m[0] = m[5] = m[10] = m[15] = diagonal;
    }

    Matrix4(
        f32 m00, f32 m01, f32 m02, f32 m03,
        f32 m10, f32 m11, f32 m12, f32 m13,
        f32 m20, f32 m21, f32 m22, f32 m23,
        f32 m30, f32 m31, f32 m32, f32 m33
    ) {
        // Column 0
        m[0] = m00; m[4] = m01; m[8]  = m02; m[12] = m03;
        // Column 1
        m[1] = m10; m[5] = m11; m[9]  = m12; m[13] = m13;
        // Column 2
        m[2] = m20; m[6] = m21; m[10] = m22; m[14] = m23;
        // Column 3
        m[3] = m30; m[7] = m31; m[11] = m32; m[15] = m33;
    }

    // Accessors (column-major)
    f32& operator()(usize row, usize col) { return m[col * 4 + row]; }
    const f32& operator()(usize row, usize col) const { return m[col * 4 + row]; }

    f32* Data() { return m; }
    const f32* Data() const { return m; }

    // Operators
    Matrix4 operator+(const Matrix4& other) const {
        Matrix4 result;
        for (usize i = 0; i < 16; ++i) {
            result.m[i] = m[i] + other.m[i];
        }
        return result;
    }

    Matrix4 operator-(const Matrix4& other) const {
        Matrix4 result;
        for (usize i = 0; i < 16; ++i) {
            result.m[i] = m[i] - other.m[i];
        }
        return result;
    }

    Matrix4 operator*(const Matrix4& other) const {
        Matrix4 result;
        for (usize col = 0; col < 4; ++col) {
            for (usize row = 0; row < 4; ++row) {
                result(row, col) = 0.0f;
                for (usize k = 0; k < 4; ++k) {
                    result(row, col) += (*this)(row, k) * other(k, col);
                }
            }
        }
        return result;
    }

    Vector4 operator*(const Vector4& v) const {
        return Vector4(
            m[0] * v.x + m[4] * v.y + m[8]  * v.z + m[12] * v.w,
            m[1] * v.x + m[5] * v.y + m[9]  * v.z + m[13] * v.w,
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
            m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w
        );
    }

    Matrix4 operator*(f32 scalar) const {
        Matrix4 result;
        for (usize i = 0; i < 16; ++i) {
            result.m[i] = m[i] * scalar;
        }
        return result;
    }

    Matrix4& operator*=(const Matrix4& other) {
        *this = *this * other;
        return *this;
    }

    // Static factory functions
    static Matrix4 Identity() {
        return Matrix4(1.0f);
    }

    static Matrix4 Translation(const Vector3& translation) {
        Matrix4 result = Identity();
        result.m[12] = translation.x;
        result.m[13] = translation.y;
        result.m[14] = translation.z;
        return result;
    }

    static Matrix4 Rotation(const Vector3& axis, f32 angle) {
        Vector3 normalizedAxis = axis.Normalized();
        f32 c = Cos(angle);
        f32 s = Sin(angle);
        f32 t = 1.0f - c;

        f32 x = normalizedAxis.x;
        f32 y = normalizedAxis.y;
        f32 z = normalizedAxis.z;

        Matrix4 result;
        result.m[0] = t * x * x + c;
        result.m[1] = t * x * y + s * z;
        result.m[2] = t * x * z - s * y;
        result.m[3] = 0.0f;

        result.m[4] = t * x * y - s * z;
        result.m[5] = t * y * y + c;
        result.m[6] = t * y * z + s * x;
        result.m[7] = 0.0f;

        result.m[8]  = t * x * z + s * y;
        result.m[9]  = t * y * z - s * x;
        result.m[10] = t * z * z + c;
        result.m[11] = 0.0f;

        result.m[12] = 0.0f;
        result.m[13] = 0.0f;
        result.m[14] = 0.0f;
        result.m[15] = 1.0f;

        return result;
    }

    static Matrix4 Scale(const Vector3& scale) {
        Matrix4 result = Identity();
        result.m[0] = scale.x;
        result.m[5] = scale.y;
        result.m[10] = scale.z;
        return result;
    }

    static Matrix4 Perspective(f32 fov, f32 aspect, f32 nearPlane, f32 farPlane) {
        f32 tanHalfFov = Tan(fov * 0.5f);
        f32 range = farPlane - nearPlane;

        Matrix4 result;
        result.m[0] = 1.0f / (aspect * tanHalfFov);
        result.m[5] = 1.0f / tanHalfFov;
        result.m[10] = -(farPlane + nearPlane) / range;
        result.m[11] = -1.0f;
        result.m[14] = -(2.0f * farPlane * nearPlane) / range;
        result.m[15] = 0.0f;

        return result;
    }

    static Matrix4 Orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearPlane, f32 farPlane) {
        Matrix4 result = Identity();
        result.m[0] = 2.0f / (right - left);
        result.m[5] = 2.0f / (top - bottom);
        result.m[10] = -2.0f / (farPlane - nearPlane);
        result.m[12] = -(right + left) / (right - left);
        result.m[13] = -(top + bottom) / (top - bottom);
        result.m[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
        return result;
    }

    static Matrix4 LookAt(const Vector3& eye, const Vector3& center, const Vector3& up) {
        Vector3 f = (center - eye).Normalized();
        Vector3 s = f.Cross(up).Normalized();
        Vector3 u = s.Cross(f);

        Matrix4 result = Identity();
        result.m[0] = s.x;
        result.m[4] = s.y;
        result.m[8] = s.z;
        result.m[1] = u.x;
        result.m[5] = u.y;
        result.m[9] = u.z;
        result.m[2] = -f.x;
        result.m[6] = -f.y;
        result.m[10] = -f.z;
        result.m[12] = -s.Dot(eye);
        result.m[13] = -u.Dot(eye);
        result.m[14] = f.Dot(eye);
        return result;
    }

    Matrix4 Transposed() const {
        Matrix4 result;
        for (usize row = 0; row < 4; ++row) {
            for (usize col = 0; col < 4; ++col) {
                result(row, col) = (*this)(col, row);
            }
        }
        return result;
    }

    Matrix4 Inverse() const {
        // Full 4x4 matrix inversion using cofactors
        f32 a00 = m[0], a01 = m[4], a02 = m[8],  a03 = m[12];
        f32 a10 = m[1], a11 = m[5], a12 = m[9],  a13 = m[13];
        f32 a20 = m[2], a21 = m[6], a22 = m[10], a23 = m[14];
        f32 a30 = m[3], a31 = m[7], a32 = m[11], a33 = m[15];

        f32 b00 = a00 * a11 - a01 * a10;
        f32 b01 = a00 * a12 - a02 * a10;
        f32 b02 = a00 * a13 - a03 * a10;
        f32 b03 = a01 * a12 - a02 * a11;
        f32 b04 = a01 * a13 - a03 * a11;
        f32 b05 = a02 * a13 - a03 * a12;
        f32 b06 = a20 * a31 - a21 * a30;
        f32 b07 = a20 * a32 - a22 * a30;
        f32 b08 = a20 * a33 - a23 * a30;
        f32 b09 = a21 * a32 - a22 * a31;
        f32 b10 = a21 * a33 - a23 * a31;
        f32 b11 = a22 * a33 - a23 * a32;

        f32 det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

        if (Abs(det) < 1e-10f) {
            // Matrix is singular, return identity
            return Identity();
        }

        f32 invDet = 1.0f / det;

        Matrix4 result;
        result.m[0]  = ( a11 * b11 - a12 * b10 + a13 * b09) * invDet;
        result.m[1]  = (-a10 * b11 + a12 * b08 - a13 * b07) * invDet;
        result.m[2]  = ( a10 * b10 - a11 * b08 + a13 * b06) * invDet;
        result.m[3]  = (-a10 * b09 + a11 * b07 - a12 * b06) * invDet;
        result.m[4]  = (-a01 * b11 + a02 * b10 - a03 * b09) * invDet;
        result.m[5]  = ( a00 * b11 - a02 * b08 + a03 * b07) * invDet;
        result.m[6]  = (-a00 * b10 + a01 * b08 - a03 * b06) * invDet;
        result.m[7]  = ( a00 * b09 - a01 * b07 + a02 * b06) * invDet;
        result.m[8]  = ( a31 * b05 - a32 * b04 + a33 * b03) * invDet;
        result.m[9]  = (-a30 * b05 + a32 * b02 - a33 * b01) * invDet;
        result.m[10] = ( a30 * b04 - a31 * b02 + a33 * b00) * invDet;
        result.m[11] = (-a30 * b03 + a31 * b01 - a32 * b00) * invDet;
        result.m[12] = (-a21 * b05 + a22 * b04 - a23 * b03) * invDet;
        result.m[13] = ( a20 * b05 - a22 * b02 + a23 * b01) * invDet;
        result.m[14] = (-a20 * b04 + a21 * b02 - a23 * b00) * invDet;
        result.m[15] = ( a20 * b03 - a21 * b01 + a22 * b00) * invDet;

        return result;
    }

    Matrix4 Inverted() const {
        return Inverse();
    }
};

// Type alias
using Mat4 = Matrix4;

} // namespace Math
} // namespace Enjin
