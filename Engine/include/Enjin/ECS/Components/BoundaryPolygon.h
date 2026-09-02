#pragma once
#include "Enjin/Math/Vector.h"
#include "Enjin/Platform/Platform.h"
#include <vector>

namespace Enjin {
namespace ECS {

// A drag-editable ground outline for Creative-mode objects (lakes first). Points are
// LOCAL XZ offsets from the entity's transform (Y comes from the transform). The
// object's mesh/fill is generated from this polygon instead of a plain box, so the
// user can drag out a rough size and then pull/bend the boundary into any shape.
struct ENJIN_API BoundaryPolygonComponent {
    std::vector<Math::Vector2> points;   // CCW-ish ring, local XZ
    bool dirty = true;                    // regenerate the dependent mesh when set
};

} // namespace ECS
} // namespace Enjin
