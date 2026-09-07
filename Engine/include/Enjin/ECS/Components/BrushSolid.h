#pragma once

// A solid built from convex brushes: walls, floors, ramps, and the doorways and
// windows cut out of them.
//
// The component stores the BRUSH LIST, not the mesh. A room is a few dozen
// brushes and its triangulation is megabytes, so the brushes are what gets
// saved, and the geometry is derived on load and after every edit. That is also
// what makes the thing non-destructive: move a doorway and the wall closes
// behind it, because the wall was never actually cut, only described as cut.
//
// Brushes are stored as SHAPES rather than as raw planes. Planes would be a
// smaller and more general representation, and would be miserable to author:
// the inspector needs a centre and a size to put on a row, a gizmo needs
// something to drag, and a rotated box built from its own rotated axes stays
// exactly a box instead of drifting as planes get nudged one at a time.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Geometry/CSG.h"

#include <vector>

namespace Enjin {
namespace ECS {

struct ENJIN_API BrushSolidComponent {
    // What kind of convex solid this brush is. Box covers walls, floors,
    // pillars and most doorways; Prism covers cylinders, round-topped openings
    // and anything read as a wedge.
    enum class Shape : u8 { Box = 0, Prism };

    struct Brush {
        Shape shape = Shape::Box;
        Geometry::BrushOp op = Geometry::BrushOp::Add;

        Math::Vector3 center = Math::Vector3(0.0f, 0.0f, 0.0f);
        Math::Quaternion rotation = Math::Quaternion::Identity();

        // Box
        Math::Vector3 halfExtents = Math::Vector3(1.0f, 1.0f, 1.0f);

        // Prism
        f32 radius = 1.0f;
        f32 halfHeight = 1.0f;
        u32 sides = 8;

        // Off means the brush stays in the list and stops contributing, which is
        // how you check what a cut is doing without losing it.
        bool enabled = true;

        Geometry::Brush ToGeometry() const {
            if (shape == Shape::Prism) {
                return Geometry::Brush::Prism(center, radius, halfHeight, sides, rotation);
            }
            return Geometry::Brush::Box(center, halfExtents, rotation);
        }
    };

    std::vector<Brush> brushes;

    // World units per texture tile. Brush geometry is planar-projected, so this
    // is the only UV control there is, and it is the one a wall wants.
    f32 uvScale = 1.0f;

    // Collision is the same triangles as the render mesh, because convex shapes
    // cannot express a hole. Off for decorative solids nothing touches.
    bool generateCollider = true;

    // Set by anything that edits the list. The rebuild runs on the next system
    // pass and clears it: rebuilding on every edit would re-clip the whole solid
    // for each dragged handle.
    //
    // It is a fast path, NOT the correctness mechanism. BrushSolidSystem also
    // hashes the list and rebuilds when the hash moved, because a flag that
    // every writer must remember to set is a flag somebody will forget: undo
    // writes an old value straight back through a raw pointer, a script could
    // poke a brush, and a future tool will do something nobody predicted. The
    // hash means the geometry follows the data no matter who wrote it.
    bool dirty = true;

    // Hash of the brush list the current geometry was built from. Zero means
    // nothing has been built yet.
    u64 builtHash = 0;

    // Which brush the viewport gizmo drives, or -1 for the entity itself.
    // Runtime only and deliberately not serialized: it is a selection, not data,
    // and a scene that reopened with a brush still "being edited" would be
    // surprising. Set by the inspector's Edit toggle.
    i32 gizmoBrush = -1;

    // Last rebuild's output size, for the inspector to show. A brush solid that
    // has quietly grown to thousands of faces is worth being able to see.
    u32 lastFaceCount = 0;
    u32 lastTriangleCount = 0;

    // Cheap content hash of everything that changes the geometry. Deliberately
    // excludes lastFaceCount and friends, which are outputs.
    u64 ContentHash() const {
        u64 h = 1469598103934665603ull;
        auto mix = [&h](const void* data, usize bytes) {
            const u8* p = static_cast<const u8*>(data);
            for (usize i = 0; i < bytes; ++i) { h ^= p[i]; h *= 1099511628211ull; }
        };
        mix(&uvScale, sizeof(uvScale));
        mix(&generateCollider, sizeof(generateCollider));
        for (const Brush& b : brushes) {
            mix(&b.shape, sizeof(b.shape));
            mix(&b.op, sizeof(b.op));
            mix(&b.center, sizeof(b.center));
            mix(&b.rotation, sizeof(b.rotation));
            mix(&b.halfExtents, sizeof(b.halfExtents));
            mix(&b.radius, sizeof(b.radius));
            mix(&b.halfHeight, sizeof(b.halfHeight));
            mix(&b.sides, sizeof(b.sides));
            mix(&b.enabled, sizeof(b.enabled));
        }
        return h;
    }

    std::vector<Geometry::BrushEntry> ToBrushEntries() const {
        std::vector<Geometry::BrushEntry> out;
        out.reserve(brushes.size());
        for (const Brush& b : brushes) {
            if (!b.enabled) continue;
            out.push_back({ b.ToGeometry(), b.op });
        }
        return out;
    }
};

} // namespace ECS
} // namespace Enjin
