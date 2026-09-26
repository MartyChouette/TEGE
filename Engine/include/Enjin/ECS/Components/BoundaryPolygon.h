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

    // The flat surface this outline encloses, in local XZ (y carries Z).
    //
    // Ear-clipped, so a kidney or an L-shaped pond fills correctly. It used to be
    // a fan from the centroid, which is only right while the whole rim can be
    // seen from the middle: pull one side in past the centre and the fan folded
    // over itself and spilled water onto the bank.
    //
    // Triangles are then split until no edge is longer than `maxEdge`, because
    // the shoreline foam reads a per-vertex distance and a big triangle has none
    // inside it. `shore` is that distance, 0 on the rim and 1 at the point
    // furthest from any edge, which is what the centroid used to carry.
    //
    // Returns false for fewer than three points or a ring with no area.
    //
    // The water surface asks for kSurfaceMaxEdge: fine enough that the foam
    // fades over a couple of metres, coarse enough that a pond stays a few
    // hundred triangles.
    static constexpr f32 kSurfaceMaxEdge = 2.0f;
    static bool BuildSurface(const std::vector<Math::Vector2>& ring, f32 maxEdge,
                             std::vector<Math::Vector2>& positions,
                             std::vector<f32>& shore,
                             std::vector<u32>& indices);
};

} // namespace ECS
} // namespace Enjin
