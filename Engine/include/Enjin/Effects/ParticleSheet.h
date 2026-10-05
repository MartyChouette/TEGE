#pragma once
// The sprite sheet frame a CPU particle shows, shared by the Vulkan
// ParticleRenderer and the web particle path so the two cannot disagree.
//
// Per particle, from its own AGE, not global time: an explosion is many puffs
// each playing its animation from its own birth. That is what separates this
// from the material flipbook.
//
// `lifetime` is the time REMAINING (ParticleSystem counts it down to zero), so
// the age fraction is 1 - lifetime / maxLifetime. The desktop copy this came
// from read lifetime / maxLifetime as the age, and every sheet played
// backwards, starting on its last frame.
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <algorithm>

namespace Enjin {
namespace Effects {

struct ParticleSheetFrame {
    Math::Vector2 uvOffset = Math::Vector2(0.0f, 0.0f);
    Math::Vector2 uvScale = Math::Vector2(1.0f, 1.0f);   // the whole texture when there is no sheet
};

// Columns and rows are clamped to 1..16, the inspector's slider range.
inline ParticleSheetFrame ComputeParticleSheetFrame(f32 lifetime, f32 maxLifetime,
                                                    i32 sheetCols, i32 sheetRows) {
    ParticleSheetFrame f;
    const i32 cols = std::max(1, std::min(sheetCols, 16));
    const i32 rows = std::max(1, std::min(sheetRows, 16));
    if (cols == 1 && rows == 1) return f;
    const i32 frameCount = cols * rows;
    const f32 span = (maxLifetime > 0.0001f) ? maxLifetime : 1.0f;
    const f32 age = std::clamp(1.0f - lifetime / span, 0.0f, 0.9999f);
    const i32 frame = std::min(static_cast<i32>(age * static_cast<f32>(frameCount)), frameCount - 1);
    f.uvScale = Math::Vector2(1.0f / static_cast<f32>(cols), 1.0f / static_cast<f32>(rows));
    f.uvOffset = Math::Vector2(static_cast<f32>(frame % cols) * f.uvScale.x,
                               static_cast<f32>(frame / cols) * f.uvScale.y);
    return f;
}

} // namespace Effects
} // namespace Enjin
