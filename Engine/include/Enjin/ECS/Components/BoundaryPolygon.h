#pragma once
#include "Enjin/Math/Vector.h"
#include "Enjin/Platform/Platform.h"
#include <cstddef>
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

    // Is a world-space XZ point inside the outline? `origin` is the entity's
    // world position, because the points are offsets from it.
    //
    // This exists because the outline used to be a RENDERING detail and nothing
    // else. RenderSystem triangulated the ring into the water surface, so a lake
    // dragged into a kidney LOOKED like a kidney while swimming, floating and
    // buoyancy all kept testing the halfExtents box the ring was seeded from.
    // You swam in the corners with no water under you, and things bobbed on dry
    // deck. The shape has to be one thing every system asks, not a mesh one
    // system happens to build.
    //
    // Fewer than three points is not a usable ring, so it answers "inside" and
    // leaves the caller's box test to decide alone. That is the same condition
    // RenderSystem uses to choose the polygon path over the plain plane, and it
    // is what keeps every scene authored before this unchanged.
    //
    // Like the box tests it sits beside, this ignores the entity's ROTATION and
    // SCALE: the whole water path assumes identity rotation and unit scale, and
    // the viewport's drag handles place their points the same way.
    //
    // Crossing number, cast along -X. A point exactly on an edge can fall either
    // side, which no swimmer can feel.
    bool ContainsXZ(const Math::Vector3& origin, f32 x, f32 z) const {
        const std::size_t n = points.size();
        if (n < 3) return true;

        const f32 lx = x - origin.x;
        const f32 lz = z - origin.z;

        bool inside = false;
        for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
            const Math::Vector2& a = points[i];
            const Math::Vector2& b = points[j];
            // The straddle test comes first and is what makes the division safe:
            // a horizontal edge fails it, so b.y - a.y is never zero below.
            if ((a.y > lz) != (b.y > lz) &&
                lx < (b.x - a.x) * (lz - a.y) / (b.y - a.y) + a.x) {
                inside = !inside;
            }
        }
        return inside;
    }
};

} // namespace ECS
} // namespace Enjin
