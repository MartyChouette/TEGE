#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <string>
#include <algorithm>

namespace Enjin {
namespace ECS {

struct ENJIN_API Terrain2DComponent {
    std::vector<Math::Vector2> controlPoints;  // XY plane, sorted by X
    f32 depth = 5.0f;        // fill depth below surface
    f32 uvScale = 1.0f;
    std::string texturePath;
    bool autoColliders = true;
    bool meshDirty = true;

    void AddPoint(const Math::Vector2& p) {
        controlPoints.push_back(p);
        SortPoints();
        meshDirty = true;
    }

    void SortPoints() {
        std::sort(controlPoints.begin(), controlPoints.end(),
            [](const Math::Vector2& a, const Math::Vector2& b) {
                return a.x < b.x;
            });
    }
};

} // namespace ECS
} // namespace Enjin
