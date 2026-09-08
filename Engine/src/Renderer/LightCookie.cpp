#include "Enjin/Renderer/LightCookie.h"

#include "Enjin/Math/Noise.h"
#include "Enjin/Logging/Log.h"

// The implementation lives in PixelEditor.cpp; this is the declaration only.
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Renderer {

namespace {

f32 Clamp01(f32 v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

f32 Clampf(f32 v, f32 lo, f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Smoothstep with a guard for a zero-width edge, which is the whole point of
// softness = 0 producing a hard line rather than a divide by zero.
f32 SoftStep(f32 edge, f32 width, f32 x) {
    if (width <= 0.0001f) return x < edge ? 0.0f : 1.0f;
    const f32 t = Clamp01((x - (edge - width)) / (2.0f * width));
    return t * t * (3.0f - 2.0f * t);
}

// Distance from the nearest cell border, in cell units (0 at the border, 0.5 at
// the centre). This is what turns any tiling into "bars with panes between".
f32 CellEdgeDistance(f32 v) {
    const f32 f = v - std::floor(v);
    return std::min(f, 1.0f - f);
}

// Fractal noise in [0,1]. Math::ValueNoise2D returns [-1,1], so each octave is
// remapped before it is summed: treating it as [0,1] centres the whole field on
// black instead of mid-grey, and every threshold built on top then reads as an
// almost unlit cookie.
f32 Fbm(f32 x, f32 y, u32 seed, i32 octaves) {
    f32 sum = 0.0f, amp = 0.5f, freq = 1.0f, norm = 0.0f;
    for (i32 i = 0; i < octaves; ++i) {
        const f32 n = Math::ValueNoise2D(x * freq, y * freq, seed + static_cast<u32>(i) * 101u);
        sum += amp * (n * 0.5f + 0.5f);
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }
    return norm > 0.0f ? Clamp01(sum / norm) : 0.0f;
}

} // namespace

const char* CookiePatternName(CookiePattern p) {
    switch (p) {
        case CookiePattern::WindowPanes:    return "Window Panes";
        case CookiePattern::Blinds:         return "Venetian Blinds";
        case CookiePattern::Bars:           return "Vertical Bars";
        case CookiePattern::LeafDapple:     return "Leaf Dapple";
        case CookiePattern::CathedralGlass: return "Cathedral Glass";
        case CookiePattern::SoftCircle:     return "Soft Circle";
        case CookiePattern::Caustics:       return "Water Caustics";
        case CookiePattern::Custom:         return "Custom Image";
    }
    return "Window Panes";
}

void ClampCookieParams(CookieParams& p) {
    if (static_cast<u32>(p.pattern) >= kCookiePatternCount) {
        p.pattern = CookiePattern::WindowPanes;
    }
    p.resolution = std::clamp(p.resolution, kCookieResolutionMin, kCookieResolutionMax);
    p.columns    = Clampf(p.columns, 0.25f, 32.0f);
    p.rows       = Clampf(p.rows, 0.25f, 32.0f);
    // Above 0.49 the bars meet and the cookie is solid black, which reads as a
    // broken light rather than a choice.
    p.barWidth   = Clampf(p.barWidth, 0.0f, 0.49f);
    p.softness   = Clampf(p.softness, 0.0f, 0.5f);
    p.contrast   = Clampf(p.contrast, 0.0f, 8.0f);
    p.brightness = Clampf(p.brightness, -1.0f, 1.0f);
    p.vignette   = Clampf(p.vignette, 0.0f, 1.0f);
    // Keep rotation in one turn so the editor's readout does not wander off.
    p.rotation = std::fmod(p.rotation, 360.0f);
    if (p.rotation < 0.0f) p.rotation += 360.0f;
}

void GenerateCookie(const CookieParams& params, std::vector<u8>& out) {
    CookieParams p = params;
    ClampCookieParams(p);

    const u32 res = p.resolution;
    out.assign(static_cast<usize>(res) * res, 0u);

    const f32 rad = p.rotation * 3.14159265358979f / 180.0f;
    const f32 cs = std::cos(rad), sn = std::sin(rad);
    const f32 inv = 1.0f / static_cast<f32>(res);

    for (u32 y = 0; y < res; ++y) {
        for (u32 x = 0; x < res; ++x) {
            // Centred, rotated coordinates. Rotating about the middle is what
            // makes "tilt the blinds" behave the way a person expects.
            const f32 u = (static_cast<f32>(x) + 0.5f) * inv - 0.5f;
            const f32 v = (static_cast<f32>(y) + 0.5f) * inv - 0.5f;
            const f32 rx = u * cs - v * sn;
            const f32 ry = u * sn + v * cs;

            f32 value = 1.0f;

            switch (p.pattern) {
                case CookiePattern::WindowPanes: {
                    const f32 dx = CellEdgeDistance(rx * p.columns + 0.5f);
                    const f32 dy = CellEdgeDistance(ry * p.rows + 0.5f);
                    value = std::min(SoftStep(p.barWidth, p.softness, dx),
                                     SoftStep(p.barWidth, p.softness, dy));
                    break;
                }
                case CookiePattern::Blinds: {
                    const f32 dy = CellEdgeDistance(ry * p.rows + 0.5f);
                    value = SoftStep(p.barWidth, p.softness, dy);
                    break;
                }
                case CookiePattern::Bars: {
                    const f32 dx = CellEdgeDistance(rx * p.columns + 0.5f);
                    value = SoftStep(p.barWidth, p.softness, dx);
                    break;
                }
                case CookiePattern::LeafDapple: {
                    // Two octave sets at different scales: the big one is the
                    // canopy, the small one the individual leaves. Thresholded
                    // rather than used directly, because dapple is holes in an
                    // occluder, not a smooth gradient.
                    const f32 n = Fbm(rx * p.columns * 2.0f + 13.0f,
                                      ry * p.rows * 2.0f - 7.0f, p.seed, 4);
                    const f32 detail = Fbm(rx * p.columns * 6.0f,
                                           ry * p.rows * 6.0f, p.seed + 977u, 2);
                    const f32 combined = n * 0.75f + detail * 0.25f;
                    // Fractal noise clusters around 0.5, so the threshold has to
                    // sit near it: barWidth then reads as "how much canopy",
                    // with the default landing a little under half lit. A
                    // threshold far from the midpoint saturates to all-black or
                    // all-white and the control does nothing across most of its
                    // range.
                    value = SoftStep(0.42f + p.barWidth, 0.02f + p.softness, combined);
                    break;
                }
                case CookiePattern::CathedralGlass: {
                    // Radial tracery: spokes plus concentric rings, so it reads
                    // as a rose window rather than a dartboard.
                    const f32 r = std::sqrt(rx * rx + ry * ry) * 2.0f;
                    const f32 a = std::atan2(ry, rx) / (2.0f * 3.14159265358979f) + 0.5f;
                    const f32 spokes = CellEdgeDistance(a * std::max(1.0f, p.columns * 2.0f));
                    const f32 rings = CellEdgeDistance(r * p.rows);
                    value = std::min(SoftStep(p.barWidth, p.softness, spokes),
                                     SoftStep(p.barWidth, p.softness, rings));
                    // Outside the window is masonry, not glass.
                    value *= 1.0f - SoftStep(1.0f, 0.02f + p.softness, r);
                    break;
                }
                case CookiePattern::SoftCircle: {
                    const f32 r = std::sqrt(rx * rx + ry * ry) * 2.0f;
                    value = 1.0f - SoftStep(1.0f - p.barWidth, 0.02f + p.softness, r);
                    break;
                }
                case CookiePattern::Caustics: {
                    // Ridged noise: the sharp bright filaments of caustics come
                    // from folding the noise about its midpoint, not from a
                    // threshold.
                    const f32 n = Fbm(rx * p.columns * 3.0f + 5.0f,
                                      ry * p.rows * 3.0f + 11.0f, p.seed, 4);
                    const f32 ridged = 1.0f - std::fabs(n * 2.0f - 1.0f);
                    value = std::pow(Clamp01(ridged), 3.0f + p.barWidth * 8.0f);
                    break;
                }
                case CookiePattern::Custom:
                    // Nothing procedural to draw. A flat cookie is a no-op
                    // projection, which is the honest result when the image the
                    // params point at has not been loaded.
                    value = 1.0f;
                    break;
            }

            if (p.vignette > 0.0f) {
                const f32 r = std::sqrt(rx * rx + ry * ry) * 2.0f;
                value *= 1.0f - p.vignette * Clamp01(r);
            }

            value = (value - 0.5f) * p.contrast + 0.5f + p.brightness;
            value = Clamp01(value);
            if (p.invert) value = 1.0f - value;

            out[static_cast<usize>(y) * res + x] =
                static_cast<u8>(value * 255.0f + 0.5f);
        }
    }
}

bool WriteCookiePNG(const std::string& path, const CookieParams& params,
                    const std::vector<u8>& pixels) {
    CookieParams p = params;
    ClampCookieParams(p);
    const usize expected = static_cast<usize>(p.resolution) * p.resolution;
    if (pixels.size() != expected) {
        ENJIN_LOG_ERROR(Renderer, "Cookie PNG not written: %zu pixels for a %ux%u cookie",
                        pixels.size(), p.resolution, p.resolution);
        return false;
    }
    const int ok = stbi_write_png(path.c_str(), static_cast<int>(p.resolution),
                                  static_cast<int>(p.resolution), 1,
                                  pixels.data(), static_cast<int>(p.resolution));
    if (!ok) {
        ENJIN_LOG_ERROR(Renderer, "Cookie PNG could not be written to '%s'", path.c_str());
        return false;
    }
    return true;
}

} // namespace Renderer
} // namespace Enjin
