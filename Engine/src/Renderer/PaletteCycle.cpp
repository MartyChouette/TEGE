#include "Enjin/Renderer/PaletteCycle.h"

#include <cmath>

namespace Enjin {
namespace Renderer {

void ClampCycleRange(PaletteCycleRange& r, u32 paletteCount) {
    const u32 limit = paletteCount > kPaletteMaxColors ? kPaletteMaxColors : paletteCount;
    if (limit == 0) { r.first = 0; r.count = 0; return; }

    if (r.first >= limit) r.first = limit - 1;
    // A run may not reach past the end of the palette.
    const u32 room = limit - r.first;
    if (r.count > room) r.count = room;
    // Speed is authored, and a scene file is user-editable text. Absurd values
    // would still be deterministic, but they make the readout useless.
    if (r.speed > 240.0f) r.speed = 240.0f;
    if (r.speed < -240.0f) r.speed = -240.0f;
}

void ApplyPaletteCycles(const Palette& base,
                        const std::vector<PaletteCycleRange>& ranges,
                        f32 timeSeconds,
                        Palette& out) {
    // Start from the authored colours. Everything not inside a run keeps its
    // value, so a palette with no ranges is copied through untouched.
    out.name = base.name;
    out.count = base.count;
    out.colors = base.colors;
    if (out.colors.size() < kPaletteMaxColors) out.colors.resize(kPaletteMaxColors);

    for (const PaletteCycleRange& raw : ranges) {
        if (!raw.enabled) continue;
        PaletteCycleRange r = raw;
        ClampCycleRange(r, base.count);
        // A run of one entry has nothing to rotate into.
        if (r.count < 2) continue;

        // Whole-entry steps: the look depends on colours SWAPPING places, not on
        // blending between them. Interpolating here would turn a crisp band into
        // a gradient and lose the effect entirely.
        const f32 stepsF = std::floor(timeSeconds * r.speed);
        i64 steps = static_cast<i64>(stepsF);
        const i64 n = static_cast<i64>(r.count);
        steps %= n;
        if (steps < 0) steps += n;   // C++ modulo keeps the sign; palettes must not

        for (u32 i = 0; i < r.count; ++i) {
            const u32 dst = r.first + i;
            const u32 src = r.first + static_cast<u32>((static_cast<i64>(i) + steps) % n);
            out.colors[dst] = base.colors[src];
        }
    }
}

const char* PalettePresetName(PalettePreset p) {
    switch (p) {
        case PalettePreset::Water:  return "Water";
        case PalettePreset::Fire:   return "Fire";
        case PalettePreset::Rain:   return "Rain";
        case PalettePreset::Aurora: return "Aurora";
        default: break;
    }
    return "Water";
}

namespace {

PaletteColor RGB(u8 r, u8 g, u8 b) { PaletteColor c; c.r = r; c.g = g; c.b = b; c.a = 255; return c; }

// Fill [first, first+count) with a gradient between two colours. Presets are
// built from runs, because a run is exactly what cycling animates.
void Ramp(Palette& p, u32 first, u32 count, PaletteColor a, PaletteColor b) {
    if (count == 0) return;
    for (u32 i = 0; i < count; ++i) {
        const f32 t = (count == 1) ? 0.0f : static_cast<f32>(i) / static_cast<f32>(count - 1);
        PaletteColor c;
        c.r = static_cast<u8>(a.r + (b.r - a.r) * t);
        c.g = static_cast<u8>(a.g + (b.g - a.g) * t);
        c.b = static_cast<u8>(a.b + (b.b - a.b) * t);
        c.a = 255;
        if (first + i < kPaletteMaxColors) p.colors[first + i] = c;
    }
}

} // namespace

void MakePalettePreset(PalettePreset preset, Palette& outPalette,
                       std::vector<PaletteCycleRange>& outRanges) {
    outPalette = Palette{};
    outPalette.name = PalettePresetName(preset);
    outRanges.clear();

    // Index 0 is left black and OUTSIDE every run, so art can rely on one index
    // that never moves.
    switch (preset) {
        case PalettePreset::Water: {
            Ramp(outPalette, 1, 15, RGB(10, 40, 90), RGB(120, 200, 235));
            outPalette.count = 16;
            outRanges.push_back({ 1, 15, 6.0f, true });
            break;
        }
        case PalettePreset::Fire: {
            // Runs from hot to dark; a NEGATIVE speed makes the brightness climb
            // rather than fall, which is what reads as flame.
            Ramp(outPalette, 1, 15, RGB(255, 240, 160), RGB(90, 10, 0));
            outPalette.count = 16;
            outRanges.push_back({ 1, 15, -10.0f, true });
            break;
        }
        case PalettePreset::Rain: {
            Ramp(outPalette, 1, 7, RGB(60, 70, 90), RGB(190, 205, 225));
            outPalette.count = 8;
            outRanges.push_back({ 1, 7, 14.0f, true });
            break;
        }
        case PalettePreset::Aurora: {
            Ramp(outPalette, 1, 23, RGB(10, 60, 40), RGB(150, 255, 190));
            outPalette.count = 24;
            outRanges.push_back({ 1, 23, 1.5f, true });
            break;
        }
        default: break;
    }
}

} // namespace Renderer
} // namespace Enjin
