#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <string>

namespace Enjin {
namespace ECS {

struct ENJIN_API TerrainComponent {
    u32 gridWidth = 64, gridHeight = 64;
    f32 cellSize = 1.0f;
    f32 maxHeight = 20.0f;
    std::vector<f32> heightmap;  // gridWidth * gridHeight

    struct TextureLayer {
        std::string texturePath;
        f32 tileScale = 1.0f;
    };
    TextureLayer layers[4];
    std::vector<f32> splatmap;  // gridWidth * gridHeight * 4 (RGBA weights)

    bool meshDirty = true;

    f32 GetHeight(u32 x, u32 z) const {
        if (x >= gridWidth || z >= gridHeight) return 0.0f;
        if (heightmap.empty()) return 0.0f;
        return heightmap[z * gridWidth + x];
    }

    void SetHeight(u32 x, u32 z, f32 h) {
        if (x >= gridWidth || z >= gridHeight) return;
        if (heightmap.size() != static_cast<usize>(gridWidth) * gridHeight) return;
        heightmap[z * gridWidth + x] = h;
        meshDirty = true;
    }

    void InitializeFlat(f32 height = 0.0f) {
        usize count = static_cast<usize>(gridWidth) * gridHeight;
        heightmap.resize(count, height);
        splatmap.resize(count * 4, 0.0f);
        // Default: first layer covers everything
        for (usize i = 0; i < count; ++i) {
            splatmap[i * 4 + 0] = 1.0f;
        }
        meshDirty = true;
    }
};

} // namespace ECS
} // namespace Enjin
