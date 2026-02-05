#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Math.h"
#include <cmath>

namespace Enjin {
namespace Math {

// ============================================================================
// Internal helpers
// ============================================================================
namespace detail {

// Integer hash for seed-based randomization (xxhash-style mixing)
inline u32 HashInt(u32 x) {
    x ^= x >> 16;
    x *= 0x45d9f3bU;
    x ^= x >> 16;
    x *= 0x45d9f3bU;
    x ^= x >> 16;
    return x;
}

// Hash 2D coordinates with seed
inline u32 Hash2D(i32 x, i32 y, u32 seed) {
    u32 h = seed;
    h ^= static_cast<u32>(x) * 0x9e3779b1U;
    h ^= static_cast<u32>(y) * 0x517cc1b7U;
    return HashInt(h);
}

// Hash 3D coordinates with seed
inline u32 Hash3D(i32 x, i32 y, i32 z, u32 seed) {
    u32 h = seed;
    h ^= static_cast<u32>(x) * 0x9e3779b1U;
    h ^= static_cast<u32>(y) * 0x517cc1b7U;
    h ^= static_cast<u32>(z) * 0x6c62272eU;
    return HashInt(h);
}

// Convert hash to float in [0, 1)
inline f32 HashToFloat01(u32 h) {
    return static_cast<f32>(h & 0x00FFFFFF) / 16777216.0f; // 2^24
}

// Convert hash to float in [-1, 1]
inline f32 HashToFloatSigned(u32 h) {
    return HashToFloat01(h) * 2.0f - 1.0f;
}

// Quintic smoothstep: 6t^5 - 15t^4 + 10t^3 (Perlin improved)
inline f32 Fade(f32 t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// Linear interpolation (unclamped)
inline f32 Lerp(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

// Fast floor to integer
inline i32 FastFloor(f32 x) {
    i32 xi = static_cast<i32>(x);
    return x < static_cast<f32>(xi) ? xi - 1 : xi;
}

// 2D gradient vectors (8 directions on the unit circle)
inline f32 Grad2D(u32 hash, f32 x, f32 y) {
    u32 h = hash & 7;
    f32 u = h < 4 ? x : y;
    f32 v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

// 3D gradient vectors (12 edges of a cube)
inline f32 Grad3D(u32 hash, f32 x, f32 y, f32 z) {
    u32 h = hash & 15;
    f32 u = h < 8 ? x : y;
    f32 v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

// Simplex skew factors
// 2D: F2 = (sqrt(3) - 1) / 2,  G2 = (3 - sqrt(3)) / 6
constexpr f32 F2 = 0.3660254037844386f;
constexpr f32 G2 = 0.21132486540518713f;

// 3D: F3 = 1/3,  G3 = 1/6
constexpr f32 F3 = 1.0f / 3.0f;
constexpr f32 G3 = 1.0f / 6.0f;

// Simplex 2D gradient contribution
inline f32 SimplexGrad2D(u32 hash, f32 x, f32 y) {
    f32 t = 0.5f - x * x - y * y;
    if (t < 0.0f) return 0.0f;
    t *= t;
    return t * t * Grad2D(hash, x, y);
}

// Simplex 3D gradient contribution
inline f32 SimplexGrad3D(u32 hash, f32 x, f32 y, f32 z) {
    f32 t = 0.6f - x * x - y * y - z * z;
    if (t < 0.0f) return 0.0f;
    t *= t;
    return t * t * Grad3D(hash, x, y, z);
}

} // namespace detail

// ============================================================================
// Value Noise
// ============================================================================

// Value noise 2D: lattice-based random interpolation
// Returns value in [-1, 1]
inline f32 ValueNoise2D(f32 x, f32 y, u32 seed = 0) {
    i32 x0 = detail::FastFloor(x);
    i32 y0 = detail::FastFloor(y);
    i32 x1 = x0 + 1;
    i32 y1 = y0 + 1;

    f32 sx = detail::Fade(x - static_cast<f32>(x0));
    f32 sy = detail::Fade(y - static_cast<f32>(y0));

    f32 n00 = detail::HashToFloatSigned(detail::Hash2D(x0, y0, seed));
    f32 n10 = detail::HashToFloatSigned(detail::Hash2D(x1, y0, seed));
    f32 n01 = detail::HashToFloatSigned(detail::Hash2D(x0, y1, seed));
    f32 n11 = detail::HashToFloatSigned(detail::Hash2D(x1, y1, seed));

    f32 nx0 = detail::Lerp(n00, n10, sx);
    f32 nx1 = detail::Lerp(n01, n11, sx);

    return detail::Lerp(nx0, nx1, sy);
}

// Value noise 3D
// Returns value in [-1, 1]
inline f32 ValueNoise3D(f32 x, f32 y, f32 z, u32 seed = 0) {
    i32 x0 = detail::FastFloor(x);
    i32 y0 = detail::FastFloor(y);
    i32 z0 = detail::FastFloor(z);
    i32 x1 = x0 + 1;
    i32 y1 = y0 + 1;
    i32 z1 = z0 + 1;

    f32 sx = detail::Fade(x - static_cast<f32>(x0));
    f32 sy = detail::Fade(y - static_cast<f32>(y0));
    f32 sz = detail::Fade(z - static_cast<f32>(z0));

    f32 n000 = detail::HashToFloatSigned(detail::Hash3D(x0, y0, z0, seed));
    f32 n100 = detail::HashToFloatSigned(detail::Hash3D(x1, y0, z0, seed));
    f32 n010 = detail::HashToFloatSigned(detail::Hash3D(x0, y1, z0, seed));
    f32 n110 = detail::HashToFloatSigned(detail::Hash3D(x1, y1, z0, seed));
    f32 n001 = detail::HashToFloatSigned(detail::Hash3D(x0, y0, z1, seed));
    f32 n101 = detail::HashToFloatSigned(detail::Hash3D(x1, y0, z1, seed));
    f32 n011 = detail::HashToFloatSigned(detail::Hash3D(x0, y1, z1, seed));
    f32 n111 = detail::HashToFloatSigned(detail::Hash3D(x1, y1, z1, seed));

    f32 nx00 = detail::Lerp(n000, n100, sx);
    f32 nx10 = detail::Lerp(n010, n110, sx);
    f32 nx01 = detail::Lerp(n001, n101, sx);
    f32 nx11 = detail::Lerp(n011, n111, sx);

    f32 nxy0 = detail::Lerp(nx00, nx10, sy);
    f32 nxy1 = detail::Lerp(nx01, nx11, sy);

    return detail::Lerp(nxy0, nxy1, sz);
}

// ============================================================================
// Perlin Noise (Improved, Ken Perlin 2002)
// ============================================================================

// Perlin noise 2D: gradient noise with quintic fade
// Returns value in approximately [-1, 1]
inline f32 PerlinNoise2D(f32 x, f32 y, u32 seed = 0) {
    i32 x0 = detail::FastFloor(x);
    i32 y0 = detail::FastFloor(y);
    i32 x1 = x0 + 1;
    i32 y1 = y0 + 1;

    f32 dx0 = x - static_cast<f32>(x0);
    f32 dy0 = y - static_cast<f32>(y0);
    f32 dx1 = dx0 - 1.0f;
    f32 dy1 = dy0 - 1.0f;

    f32 sx = detail::Fade(dx0);
    f32 sy = detail::Fade(dy0);

    f32 n00 = detail::Grad2D(detail::Hash2D(x0, y0, seed), dx0, dy0);
    f32 n10 = detail::Grad2D(detail::Hash2D(x1, y0, seed), dx1, dy0);
    f32 n01 = detail::Grad2D(detail::Hash2D(x0, y1, seed), dx0, dy1);
    f32 n11 = detail::Grad2D(detail::Hash2D(x1, y1, seed), dx1, dy1);

    f32 nx0 = detail::Lerp(n00, n10, sx);
    f32 nx1 = detail::Lerp(n01, n11, sx);

    return detail::Lerp(nx0, nx1, sy);
}

// Perlin noise 3D
// Returns value in approximately [-1, 1]
inline f32 PerlinNoise3D(f32 x, f32 y, f32 z, u32 seed = 0) {
    i32 x0 = detail::FastFloor(x);
    i32 y0 = detail::FastFloor(y);
    i32 z0 = detail::FastFloor(z);
    i32 x1 = x0 + 1;
    i32 y1 = y0 + 1;
    i32 z1 = z0 + 1;

    f32 dx0 = x - static_cast<f32>(x0);
    f32 dy0 = y - static_cast<f32>(y0);
    f32 dz0 = z - static_cast<f32>(z0);
    f32 dx1 = dx0 - 1.0f;
    f32 dy1 = dy0 - 1.0f;
    f32 dz1 = dz0 - 1.0f;

    f32 sx = detail::Fade(dx0);
    f32 sy = detail::Fade(dy0);
    f32 sz = detail::Fade(dz0);

    f32 n000 = detail::Grad3D(detail::Hash3D(x0, y0, z0, seed), dx0, dy0, dz0);
    f32 n100 = detail::Grad3D(detail::Hash3D(x1, y0, z0, seed), dx1, dy0, dz0);
    f32 n010 = detail::Grad3D(detail::Hash3D(x0, y1, z0, seed), dx0, dy1, dz0);
    f32 n110 = detail::Grad3D(detail::Hash3D(x1, y1, z0, seed), dx1, dy1, dz0);
    f32 n001 = detail::Grad3D(detail::Hash3D(x0, y0, z1, seed), dx0, dy0, dz1);
    f32 n101 = detail::Grad3D(detail::Hash3D(x1, y0, z1, seed), dx1, dy0, dz1);
    f32 n011 = detail::Grad3D(detail::Hash3D(x0, y1, z1, seed), dx0, dy1, dz1);
    f32 n111 = detail::Grad3D(detail::Hash3D(x1, y1, z1, seed), dx1, dy1, dz1);

    f32 nx00 = detail::Lerp(n000, n100, sx);
    f32 nx10 = detail::Lerp(n010, n110, sx);
    f32 nx01 = detail::Lerp(n001, n101, sx);
    f32 nx11 = detail::Lerp(n011, n111, sx);

    f32 nxy0 = detail::Lerp(nx00, nx10, sy);
    f32 nxy1 = detail::Lerp(nx01, nx11, sy);

    return detail::Lerp(nxy0, nxy1, sz);
}

// ============================================================================
// Simplex Noise
// ============================================================================

// Simplex noise 2D: fewer artifacts and faster than Perlin
// Returns value in approximately [-1, 1]
inline f32 SimplexNoise2D(f32 x, f32 y, u32 seed = 0) {
    // Skew to simplex grid
    f32 s = (x + y) * detail::F2;
    i32 i = detail::FastFloor(x + s);
    i32 j = detail::FastFloor(y + s);

    // Unskew back
    f32 t = static_cast<f32>(i + j) * detail::G2;
    f32 x0 = x - (static_cast<f32>(i) - t);
    f32 y0 = y - (static_cast<f32>(j) - t);

    // Determine which simplex we're in
    i32 i1, j1;
    if (x0 > y0) { i1 = 1; j1 = 0; }
    else          { i1 = 0; j1 = 1; }

    f32 x1 = x0 - static_cast<f32>(i1) + detail::G2;
    f32 y1 = y0 - static_cast<f32>(j1) + detail::G2;
    f32 x2 = x0 - 1.0f + 2.0f * detail::G2;
    f32 y2 = y0 - 1.0f + 2.0f * detail::G2;

    // Three corner contributions
    f32 n0 = detail::SimplexGrad2D(detail::Hash2D(i, j, seed), x0, y0);
    f32 n1 = detail::SimplexGrad2D(detail::Hash2D(i + i1, j + j1, seed), x1, y1);
    f32 n2 = detail::SimplexGrad2D(detail::Hash2D(i + 1, j + 1, seed), x2, y2);

    // Scale to [-1, 1]
    return 70.0f * (n0 + n1 + n2);
}

// Simplex noise 3D
// Returns value in approximately [-1, 1]
inline f32 SimplexNoise3D(f32 x, f32 y, f32 z, u32 seed = 0) {
    // Skew to simplex grid
    f32 s = (x + y + z) * detail::F3;
    i32 i = detail::FastFloor(x + s);
    i32 j = detail::FastFloor(y + s);
    i32 k = detail::FastFloor(z + s);

    // Unskew back
    f32 t = static_cast<f32>(i + j + k) * detail::G3;
    f32 x0 = x - (static_cast<f32>(i) - t);
    f32 y0 = y - (static_cast<f32>(j) - t);
    f32 z0 = z - (static_cast<f32>(k) - t);

    // Determine which simplex we're in (6 possible tetrahedra)
    i32 i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0)      { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }
        else if (x0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }
        else               { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }
    } else {
        if (y0 < z0)       { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }
        else if (x0 < z0)  { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }
        else               { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }
    }

    f32 x1 = x0 - static_cast<f32>(i1) + detail::G3;
    f32 y1 = y0 - static_cast<f32>(j1) + detail::G3;
    f32 z1 = z0 - static_cast<f32>(k1) + detail::G3;
    f32 x2 = x0 - static_cast<f32>(i2) + 2.0f * detail::G3;
    f32 y2 = y0 - static_cast<f32>(j2) + 2.0f * detail::G3;
    f32 z2 = z0 - static_cast<f32>(k2) + 2.0f * detail::G3;
    f32 x3 = x0 - 1.0f + 3.0f * detail::G3;
    f32 y3 = y0 - 1.0f + 3.0f * detail::G3;
    f32 z3 = z0 - 1.0f + 3.0f * detail::G3;

    // Four corner contributions
    f32 n0 = detail::SimplexGrad3D(detail::Hash3D(i, j, k, seed), x0, y0, z0);
    f32 n1 = detail::SimplexGrad3D(detail::Hash3D(i+i1, j+j1, k+k1, seed), x1, y1, z1);
    f32 n2 = detail::SimplexGrad3D(detail::Hash3D(i+i2, j+j2, k+k2, seed), x2, y2, z2);
    f32 n3 = detail::SimplexGrad3D(detail::Hash3D(i+1, j+1, k+1, seed), x3, y3, z3);

    // Scale to [-1, 1]
    return 32.0f * (n0 + n1 + n2 + n3);
}

// ============================================================================
// Worley / Cellular Noise
// ============================================================================

// Worley noise 2D: distance to nearest feature point (F1)
// Returns value in approximately [0, 1]
inline f32 WorleyNoise2D(f32 x, f32 y, u32 seed = 0) {
    i32 xi = detail::FastFloor(x);
    i32 yi = detail::FastFloor(y);

    f32 minDist = 1e10f;

    // Check 3x3 neighborhood
    for (i32 dy = -1; dy <= 1; dy++) {
        for (i32 dx = -1; dx <= 1; dx++) {
            i32 cx = xi + dx;
            i32 cy = yi + dy;

            // Feature point position within cell
            u32 h = detail::Hash2D(cx, cy, seed);
            f32 fx = static_cast<f32>(cx) + detail::HashToFloat01(h);
            h = detail::HashInt(h);
            f32 fy = static_cast<f32>(cy) + detail::HashToFloat01(h);

            f32 ddx = x - fx;
            f32 ddy = y - fy;
            f32 dist = ddx * ddx + ddy * ddy;

            if (dist < minDist) {
                minDist = dist;
            }
        }
    }

    return Math::Clamp(std::sqrt(minDist), 0.0f, 1.0f);
}

// Worley noise 3D: distance to nearest feature point (F1)
// Returns value in approximately [0, 1]
inline f32 WorleyNoise3D(f32 x, f32 y, f32 z, u32 seed = 0) {
    i32 xi = detail::FastFloor(x);
    i32 yi = detail::FastFloor(y);
    i32 zi = detail::FastFloor(z);

    f32 minDist = 1e10f;

    // Check 3x3x3 neighborhood
    for (i32 dz = -1; dz <= 1; dz++) {
        for (i32 dy = -1; dy <= 1; dy++) {
            for (i32 dx = -1; dx <= 1; dx++) {
                i32 cx = xi + dx;
                i32 cy = yi + dy;
                i32 cz = zi + dz;

                u32 h = detail::Hash3D(cx, cy, cz, seed);
                f32 fx = static_cast<f32>(cx) + detail::HashToFloat01(h);
                h = detail::HashInt(h);
                f32 fy = static_cast<f32>(cy) + detail::HashToFloat01(h);
                h = detail::HashInt(h);
                f32 fz = static_cast<f32>(cz) + detail::HashToFloat01(h);

                f32 ddx = x - fx;
                f32 ddy = y - fy;
                f32 ddz = z - fz;
                f32 dist = ddx * ddx + ddy * ddy + ddz * ddz;

                if (dist < minDist) {
                    minDist = dist;
                }
            }
        }
    }

    return Math::Clamp(std::sqrt(minDist), 0.0f, 1.0f);
}

// ============================================================================
// Fractal Brownian Motion (fBm)
// ============================================================================

// FBM 2D: octaved summation of Perlin noise
inline f32 FBM2D(f32 x, f32 y, i32 octaves = 6, f32 lacunarity = 2.0f,
                 f32 persistence = 0.5f, f32 frequency = 1.0f, u32 seed = 0) {
    f32 sum = 0.0f;
    f32 amplitude = 1.0f;
    f32 maxAmplitude = 0.0f;
    f32 freq = frequency;

    for (i32 i = 0; i < octaves; i++) {
        sum += amplitude * PerlinNoise2D(x * freq, y * freq, seed + static_cast<u32>(i));
        maxAmplitude += amplitude;
        amplitude *= persistence;
        freq *= lacunarity;
    }

    return sum / maxAmplitude;
}

// FBM 3D: octaved summation of Perlin noise
inline f32 FBM3D(f32 x, f32 y, f32 z, i32 octaves = 6, f32 lacunarity = 2.0f,
                 f32 persistence = 0.5f, f32 frequency = 1.0f, u32 seed = 0) {
    f32 sum = 0.0f;
    f32 amplitude = 1.0f;
    f32 maxAmplitude = 0.0f;
    f32 freq = frequency;

    for (i32 i = 0; i < octaves; i++) {
        sum += amplitude * PerlinNoise3D(x * freq, y * freq, z * freq, seed + static_cast<u32>(i));
        maxAmplitude += amplitude;
        amplitude *= persistence;
        freq *= lacunarity;
    }

    return sum / maxAmplitude;
}

// ============================================================================
// Ridged Multifractal Noise
// ============================================================================

// Ridged multifractal 2D: abs + inversion for ridges/mountains
inline f32 RidgedMultifractal2D(f32 x, f32 y, i32 octaves = 6, f32 lacunarity = 2.0f,
                                f32 gain = 0.5f, f32 frequency = 1.0f, u32 seed = 0) {
    f32 sum = 0.0f;
    f32 amplitude = 1.0f;
    f32 freq = frequency;
    f32 weight = 1.0f;

    for (i32 i = 0; i < octaves; i++) {
        f32 signal = PerlinNoise2D(x * freq, y * freq, seed + static_cast<u32>(i));
        signal = 1.0f - Math::Abs(signal);  // Create ridges
        signal *= signal;                     // Sharpen ridges
        signal *= weight;
        weight = Math::Clamp(signal * gain, 0.0f, 1.0f);

        sum += signal * amplitude;
        freq *= lacunarity;
        amplitude *= gain;
    }

    return sum;
}

// Ridged multifractal 3D
inline f32 RidgedMultifractal3D(f32 x, f32 y, f32 z, i32 octaves = 6, f32 lacunarity = 2.0f,
                                f32 gain = 0.5f, f32 frequency = 1.0f, u32 seed = 0) {
    f32 sum = 0.0f;
    f32 amplitude = 1.0f;
    f32 freq = frequency;
    f32 weight = 1.0f;

    for (i32 i = 0; i < octaves; i++) {
        f32 signal = PerlinNoise3D(x * freq, y * freq, z * freq, seed + static_cast<u32>(i));
        signal = 1.0f - Math::Abs(signal);
        signal *= signal;
        signal *= weight;
        weight = Math::Clamp(signal * gain, 0.0f, 1.0f);

        sum += signal * amplitude;
        freq *= lacunarity;
        amplitude *= gain;
    }

    return sum;
}

// ============================================================================
// Billow Noise
// ============================================================================

// Billow noise 2D: abs of Perlin noise for puffy/cloud-like shapes
inline f32 BillowNoise2D(f32 x, f32 y, i32 octaves = 6, f32 lacunarity = 2.0f,
                         f32 persistence = 0.5f, f32 frequency = 1.0f, u32 seed = 0) {
    f32 sum = 0.0f;
    f32 amplitude = 1.0f;
    f32 maxAmplitude = 0.0f;
    f32 freq = frequency;

    for (i32 i = 0; i < octaves; i++) {
        f32 signal = PerlinNoise2D(x * freq, y * freq, seed + static_cast<u32>(i));
        signal = Math::Abs(signal) * 2.0f - 1.0f; // Remap [0,1] -> [-1,1]
        sum += amplitude * signal;
        maxAmplitude += amplitude;
        amplitude *= persistence;
        freq *= lacunarity;
    }

    return sum / maxAmplitude;
}

// Billow noise 3D
inline f32 BillowNoise3D(f32 x, f32 y, f32 z, i32 octaves = 6, f32 lacunarity = 2.0f,
                         f32 persistence = 0.5f, f32 frequency = 1.0f, u32 seed = 0) {
    f32 sum = 0.0f;
    f32 amplitude = 1.0f;
    f32 maxAmplitude = 0.0f;
    f32 freq = frequency;

    for (i32 i = 0; i < octaves; i++) {
        f32 signal = PerlinNoise3D(x * freq, y * freq, z * freq, seed + static_cast<u32>(i));
        signal = Math::Abs(signal) * 2.0f - 1.0f;
        sum += amplitude * signal;
        maxAmplitude += amplitude;
        amplitude *= persistence;
        freq *= lacunarity;
    }

    return sum / maxAmplitude;
}

// ============================================================================
// Domain Warping
// ============================================================================

// Domain warp 2D: warp input coordinates through noise, then sample Perlin
// Modifies x, y in-place and returns the warped noise value
inline f32 DomainWarp2D(f32& x, f32& y, f32 warpStrength = 1.0f,
                        f32 frequency = 1.0f, u32 seed = 0) {
    // Use offset seeds for each warp axis to avoid correlation
    f32 warpX = PerlinNoise2D(x * frequency + 0.0f, y * frequency + 0.0f, seed) * warpStrength;
    f32 warpY = PerlinNoise2D(x * frequency + 5.2f, y * frequency + 1.3f, seed + 1) * warpStrength;
    x += warpX;
    y += warpY;
    return PerlinNoise2D(x * frequency, y * frequency, seed);
}

// Domain warp 3D: warp input coordinates through noise, then sample Perlin
// Modifies x, y, z in-place and returns the warped noise value
inline f32 DomainWarp3D(f32& x, f32& y, f32& z, f32 warpStrength = 1.0f,
                        f32 frequency = 1.0f, u32 seed = 0) {
    f32 warpX = PerlinNoise3D(x * frequency + 0.0f, y * frequency + 0.0f, z * frequency + 0.0f, seed) * warpStrength;
    f32 warpY = PerlinNoise3D(x * frequency + 5.2f, y * frequency + 1.3f, z * frequency + 2.7f, seed + 1) * warpStrength;
    f32 warpZ = PerlinNoise3D(x * frequency + 3.7f, y * frequency + 7.1f, z * frequency + 4.3f, seed + 2) * warpStrength;
    x += warpX;
    y += warpY;
    z += warpZ;
    return PerlinNoise3D(x * frequency, y * frequency, z * frequency, seed);
}

} // namespace Math
} // namespace Enjin
