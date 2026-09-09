#pragma once

// Palette cycling, and the indexed-colour palettes it runs on.
//
// The technique: a texture stores a palette INDEX per pixel instead of a
// colour, and the colours live in a small table. Rotating a slice of that table
// animates every pixel using it, at the cost of rewriting a few entries. Water
// flows, fire flickers, rain falls, and the per-frame work is independent of how
// much of the screen is moving.
//
// This is the Mark Ferrari technique from the 8-bit era. It went away when
// hardware stopped being palette-based, not because it stopped looking good, and
// almost nothing ships it now. It also passes the test the rest of this list is
// sorted by: the motion is AUTHORED, drawn by a person deciding which entries
// belong to a run, rather than a screen-space effect that admits where the frame
// ends.
//
// The cycling itself is pure and lives here so a game can drive a palette
// without the editor, and so it can be tested with no GPU.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

inline constexpr u32 kPaletteMaxColors = 256;

struct PaletteColor {
    u8 r = 0, g = 0, b = 0, a = 255;
    bool operator==(const PaletteColor& o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    bool operator!=(const PaletteColor& o) const { return !(*this == o); }
};

struct Palette {
    std::string name;
    // Always kPaletteMaxColors long once built; `count` is how many an author
    // actually uses. Keeping the array full-size means an index from a texture
    // can never read past the end, whatever the art does.
    std::vector<PaletteColor> colors;
    u32 count = 0;

    Palette() : colors(kPaletteMaxColors) {}
};

// One run of entries that rotates. This is the whole authored unit: a person
// picks a contiguous slice of the palette and how fast it should travel.
struct PaletteCycleRange {
    u32 first = 0;        // first palette index in the run
    u32 count = 0;        // how many entries rotate; < 2 does nothing
    f32 speed = 1.0f;     // entries per second, negative runs the other way
    bool enabled = true;
};

// Rotate every enabled range by `timeSeconds` and write the result to `out`.
//
// Pure and deterministic: the same time always produces the same palette, which
// is what lets a recorded replay and a live session agree, and what makes the
// tests meaningful. Entries not covered by a range are copied unchanged.
//
// `out` may alias nothing: it is fully written, so callers can reuse a buffer.
void ApplyPaletteCycles(const Palette& base,
                        const std::vector<PaletteCycleRange>& ranges,
                        f32 timeSeconds,
                        Palette& out);

// Clamp a range to something buildable. A run reaching past the end of the
// palette, or wider than the palette itself, is authored data and must not read
// out of bounds. Idempotent, so a caller can compare a clamped request against
// the current one without it moving twice.
void ClampCycleRange(PaletteCycleRange& r, u32 paletteCount);

// A few palettes worth having out of the box, each with the cycling ranges the
// technique is famous for. Named so the editor can offer them by eye.
enum class PalettePreset : u32 {
    Water = 0,      // a blue run that flows
    Fire = 1,       // hot to dark, travelling downward
    Rain = 2,       // a narrow grey run
    Aurora = 3,     // slow wide greens
    Count
};

const char* PalettePresetName(PalettePreset p);

// Build a preset palette and the ranges that animate it.
void MakePalettePreset(PalettePreset preset, Palette& outPalette,
                       std::vector<PaletteCycleRange>& outRanges);

} // namespace Renderer
} // namespace Enjin
