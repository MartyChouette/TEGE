#pragma once

// SIMD support detection and intrinsics for the Enjin math library.
//
// Provides SSE/SSE2/SSE4.1 accelerated math operations on x86/x64 platforms.
// All functions have scalar fallbacks for non-x86 targets.
//
// Usage: The math types (Matrix4, Vector4, etc.) call into these helpers
// from their runtime code paths, keeping constexpr scalar paths intact.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

// --- Platform SIMD detection ---

// On MSVC x64, SSE2 is always available (guaranteed by the ABI).
// SSE4.1 is available on virtually all x64 CPUs shipped since ~2008 (Penryn+).
// We gate SSE4.1 usage behind a separate define for safety.

#if defined(ENJIN_COMPILER_MSVC) && (defined(_M_X64) || defined(_M_AMD64))
    #define ENJIN_SIMD_SSE2   1
    #define ENJIN_SIMD_SSE4_1 1  // SSE4.1 for _mm_dp_ps, _mm_blendv_ps
    #include <xmmintrin.h>       // SSE
    #include <emmintrin.h>       // SSE2
    #include <smmintrin.h>       // SSE4.1
#elif defined(ENJIN_COMPILER_GCC) && (defined(__SSE2__))
    #define ENJIN_SIMD_SSE2   1
    #ifdef __SSE4_1__
        #define ENJIN_SIMD_SSE4_1 1
        #include <smmintrin.h>
    #else
        #include <emmintrin.h>
    #endif
    #include <xmmintrin.h>
#else
    #define ENJIN_SIMD_SSE2   0
    #define ENJIN_SIMD_SSE4_1 0
#endif

namespace Enjin {
namespace Math {
namespace Simd {

#if ENJIN_SIMD_SSE2

// ============================================================================
// Matrix4 * Matrix4 (column-major, SSE)
// ============================================================================
// Multiplies two 4x4 column-major matrices using SSE.
// Each output column = a linear combination of the LHS columns, weighted by
// the corresponding element of the RHS column.
//
// For column j of the result:
//   result_col_j = lhs_col0 * rhs(0,j) + lhs_col1 * rhs(1,j)
//                + lhs_col2 * rhs(2,j) + lhs_col3 * rhs(3,j)

ENJIN_FORCE_INLINE void Mat4MulSSE(const f32* ENJIN_RESTRICT lhs,
                                    const f32* ENJIN_RESTRICT rhs,
                                    f32* ENJIN_RESTRICT out) {
    // Load lhs columns
    __m128 lCol0 = _mm_loadu_ps(&lhs[0]);
    __m128 lCol1 = _mm_loadu_ps(&lhs[4]);
    __m128 lCol2 = _mm_loadu_ps(&lhs[8]);
    __m128 lCol3 = _mm_loadu_ps(&lhs[12]);

    for (int col = 0; col < 4; ++col) {
        // Broadcast each element of rhs column
        __m128 r0 = _mm_set1_ps(rhs[col * 4 + 0]);
        __m128 r1 = _mm_set1_ps(rhs[col * 4 + 1]);
        __m128 r2 = _mm_set1_ps(rhs[col * 4 + 2]);
        __m128 r3 = _mm_set1_ps(rhs[col * 4 + 3]);

        __m128 result = _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(lCol0, r0), _mm_mul_ps(lCol1, r1)),
            _mm_add_ps(_mm_mul_ps(lCol2, r2), _mm_mul_ps(lCol3, r3))
        );

        _mm_storeu_ps(&out[col * 4], result);
    }
}

// ============================================================================
// Matrix4 * Vector4 (column-major, SSE)
// ============================================================================
// result = col0 * v.x + col1 * v.y + col2 * v.z + col3 * v.w

ENJIN_FORCE_INLINE void Mat4MulVec4SSE(const f32* ENJIN_RESTRICT mat,
                                        const f32* ENJIN_RESTRICT vec,
                                        f32* ENJIN_RESTRICT out) {
    __m128 col0 = _mm_loadu_ps(&mat[0]);
    __m128 col1 = _mm_loadu_ps(&mat[4]);
    __m128 col2 = _mm_loadu_ps(&mat[8]);
    __m128 col3 = _mm_loadu_ps(&mat[12]);

    __m128 vx = _mm_set1_ps(vec[0]);
    __m128 vy = _mm_set1_ps(vec[1]);
    __m128 vz = _mm_set1_ps(vec[2]);
    __m128 vw = _mm_set1_ps(vec[3]);

    __m128 result = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(col0, vx), _mm_mul_ps(col1, vy)),
        _mm_add_ps(_mm_mul_ps(col2, vz), _mm_mul_ps(col3, vw))
    );

    _mm_storeu_ps(out, result);
}

// ============================================================================
// Vector4 dot product (SSE4.1 or SSE2 fallback)
// ============================================================================

ENJIN_FORCE_INLINE f32 Vec4DotSSE(const f32* a, const f32* b) {
    __m128 va = _mm_loadu_ps(a);
    __m128 vb = _mm_loadu_ps(b);
#if ENJIN_SIMD_SSE4_1
    // _mm_dp_ps with mask 0xFF: multiply all 4 lanes, store result in all 4
    __m128 dot = _mm_dp_ps(va, vb, 0xFF);
    return _mm_cvtss_f32(dot);
#else
    // SSE2 fallback: mul, hadd emulation
    __m128 mul = _mm_mul_ps(va, vb);
    // Shuffle: [z, w, z, w]
    __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
    // [x+y, z+w, ...]
    __m128 sums = _mm_add_ps(mul, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
#endif
}

// ============================================================================
// Vector3 dot product (loads 3 floats, pads w=0)
// ============================================================================

ENJIN_FORCE_INLINE f32 Vec3DotSSE(const f32* a, const f32* b) {
    __m128 va = _mm_set_ps(0.0f, a[2], a[1], a[0]);
    __m128 vb = _mm_set_ps(0.0f, b[2], b[1], b[0]);
#if ENJIN_SIMD_SSE4_1
    __m128 dot = _mm_dp_ps(va, vb, 0x7F); // mask: xyz multiply, store in all
    return _mm_cvtss_f32(dot);
#else
    __m128 mul = _mm_mul_ps(va, vb);
    __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(mul, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    return _mm_cvtss_f32(sums);
#endif
}

// ============================================================================
// Vector3 cross product (SSE)
// ============================================================================

ENJIN_FORCE_INLINE void Vec3CrossSSE(const f32* a, const f32* b, f32* out) {
    __m128 va = _mm_set_ps(0.0f, a[2], a[1], a[0]);
    __m128 vb = _mm_set_ps(0.0f, b[2], b[1], b[0]);

    // cross = (a.yzx * b.zxy) - (a.zxy * b.yzx)
    __m128 a_yzx = _mm_shuffle_ps(va, va, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 b_zxy = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 a_zxy = _mm_shuffle_ps(va, va, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 b_yzx = _mm_shuffle_ps(vb, vb, _MM_SHUFFLE(3, 0, 2, 1));

    __m128 result = _mm_sub_ps(_mm_mul_ps(a_yzx, b_zxy), _mm_mul_ps(a_zxy, b_yzx));

    // Store xyz only (avoid writing garbage to w)
    // _mm_store_ss stores only the lowest float
    alignas(16) f32 tmp[4];
    _mm_store_ps(tmp, result);
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}

// ============================================================================
// Vector4 arithmetic (add, sub, mul by scalar)
// ============================================================================

ENJIN_FORCE_INLINE void Vec4AddSSE(const f32* a, const f32* b, f32* out) {
    _mm_storeu_ps(out, _mm_add_ps(_mm_loadu_ps(a), _mm_loadu_ps(b)));
}

ENJIN_FORCE_INLINE void Vec4SubSSE(const f32* a, const f32* b, f32* out) {
    _mm_storeu_ps(out, _mm_sub_ps(_mm_loadu_ps(a), _mm_loadu_ps(b)));
}

ENJIN_FORCE_INLINE void Vec4MulScalarSSE(const f32* v, f32 s, f32* out) {
    _mm_storeu_ps(out, _mm_mul_ps(_mm_loadu_ps(v), _mm_set1_ps(s)));
}

// ============================================================================
// Quaternion to Matrix4 (SSE)
// ============================================================================
// Layout: q = {x, y, z, w}, output = column-major f32[16]

ENJIN_FORCE_INLINE void QuatToMatSSE(const f32* q, f32* out) {
    f32 x = q[0], y = q[1], z = q[2], w = q[3];

    // Compute products (these are cheap scalar ops, SSE overhead not worth it
    // for the irregular access pattern of quaternion-to-matrix)
    f32 xx = x * x, yy = y * y, zz = z * z;
    f32 xy = x * y, xz = x * z, yz = y * z;
    f32 wx = w * x, wy = w * y, wz = w * z;

    // Column 0
    __m128 col0 = _mm_set_ps(0.0f,
                              2.0f * (xz - wy),
                              2.0f * (xy + wz),
                              1.0f - 2.0f * (yy + zz));
    _mm_storeu_ps(&out[0], col0);

    // Column 1
    __m128 col1 = _mm_set_ps(0.0f,
                              2.0f * (yz + wx),
                              1.0f - 2.0f * (xx + zz),
                              2.0f * (xy - wz));
    _mm_storeu_ps(&out[4], col1);

    // Column 2
    __m128 col2 = _mm_set_ps(0.0f,
                              1.0f - 2.0f * (xx + yy),
                              2.0f * (yz - wx),
                              2.0f * (xz + wy));
    _mm_storeu_ps(&out[8], col2);

    // Column 3 = (0, 0, 0, 1)
    _mm_storeu_ps(&out[12], _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f));
}

#endif // ENJIN_SIMD_SSE2

} // namespace Simd
} // namespace Math
} // namespace Enjin
