#pragma once

#include "Enjin/Platform/Platform.h"

namespace Enjin {
namespace Editor {

enum class TerrainBrushMode : u8 { Raise, Lower, Flatten, Smooth, Paint };

struct TerrainBrush {
    TerrainBrushMode mode = TerrainBrushMode::Raise;
    f32 radius = 3.0f;
    f32 strength = 1.0f;
    f32 flattenHeight = 0.0f;
    f32 falloff = 0.5f;
    u32 paintLayer = 0;
};

} // namespace Editor
} // namespace Enjin
