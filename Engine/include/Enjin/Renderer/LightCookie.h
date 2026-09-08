#pragma once

// Light cookies (gobos): a texture in a light's projection.
//
// Window mullions, venetian blinds, leaf dapple, cathedral glass. This is the
// actual vocabulary of film lighting and it is world-space and hand-authored,
// which is why it passes the "does it deepen the fiction" test that rules out
// screen-space tricks: a cookie tells you something is between the lamp and the
// wall, not that there is a screen.
//
// The generator is PURE and lives here rather than in the editor, because the
// golden rule is that everything must be reachable without AI and without the
// editor being the only path: a game can build a cookie at runtime with the same
// call the Cookie Creator panel makes. It also means the patterns are testable
// with no GPU, which is most of why they are trustworthy.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

enum class CookiePattern : u32 {
    WindowPanes = 0,     // mullioned window: bright panes divided by dark bars
    Blinds = 1,          // venetian blinds: horizontal slats
    Bars = 2,            // vertical slats, railings, prison bars
    LeafDapple = 3,      // broken light through foliage
    CathedralGlass = 4,  // radial tracery, rose window
    SoftCircle = 5,      // plain soft-edged pool of light
    Caustics = 6,        // rippled light off water
    Custom = 7,          // authored elsewhere and loaded from a file
};

inline constexpr u32 kCookiePatternCount = 8;

const char* CookiePatternName(CookiePattern p);

// Resolution is clamped to this range. 32 is already enough for soft shapes and
// 1024 is past the point where a gobo carries more detail than the projection
// can show; both ends exist to stop an authored or hand-edited value allocating
// something absurd.
inline constexpr u32 kCookieResolutionMin = 32;
inline constexpr u32 kCookieResolutionMax = 1024;
inline constexpr u32 kCookieResolutionDefault = 256;

struct CookieParams {
    CookiePattern pattern = CookiePattern::WindowPanes;
    u32 resolution = kCookieResolutionDefault;

    f32 columns = 3.0f;      // repetitions across
    f32 rows = 3.0f;         // repetitions down
    f32 barWidth = 0.12f;    // thickness of the dark structure, 0..0.5
    f32 softness = 0.04f;    // edge blur; 0 is a hard edge
    f32 rotation = 0.0f;     // degrees
    f32 contrast = 1.0f;     // 1 = unchanged
    f32 brightness = 0.0f;   // added after contrast
    f32 vignette = 0.0f;     // circular falloff toward the edge, 0 = none
    bool invert = false;
    u32 seed = 1337;         // organic patterns only

    // Only meaningful for CookiePattern::Custom. Project-relative.
    std::string sourcePath;
};

// Clamp params to buildable ranges. Separate from Generate so the editor can
// show the corrected values, and so a hand-edited scene file cannot ask for a
// zero-sized or half-gigabyte cookie. Idempotent: clamping twice is clamping
// once, which matters because callers compare a clamped request against the
// current one to decide whether to regenerate.
void ClampCookieParams(CookieParams& p);

// Render the pattern into a single-channel 8-bit buffer, row major, sized
// resolution * resolution. Deterministic: the same params always produce the
// same bytes, which is what makes the tests meaningful and what lets a cookie be
// regenerated from its params instead of shipping the image.
void GenerateCookie(const CookieParams& p, std::vector<u8>& out);

// Write a generated cookie to disk as an 8-bit greyscale PNG. Returns false and
// writes nothing if the buffer does not match the resolution.
bool WriteCookiePNG(const std::string& path, const CookieParams& p,
                    const std::vector<u8>& pixels);

} // namespace Renderer
} // namespace Enjin
