#pragma once
#include "Enjin/Math/Vector.h"
#include "Enjin/Platform/Types.h"
#include <vector>

namespace Enjin {
namespace ECS {

// The line a wall was drawn along, kept after it is built.
//
// The Wall and Path tools used to hand their brushes to a BrushSolidComponent
// and forget the gesture. A brush is a box with a centre, a size and a turn, so
// "move this corner" had nothing to move: two walls meeting at a corner are two
// boxes overlapping by a mitre, and neither knows the other is there. Keeping
// the line makes the corner a POINT again, and every wall that meets at it is
// rebuilt from it, which is how a corner stays joined when it is dragged.
//
// The brushes stay the geometry, so every system that draws, collides with or
// saves a BrushSolidComponent keeps working and knows nothing about this.
struct WallPathComponent {
    std::vector<Math::Vector3> points;   // entity-local; the foot of the wall
    std::vector<f32> bows;               // one per span, 0 = straight (see Path tool)
    u32 segmentsPerBow = 8;
    f32 height = 3.0f;
    f32 thickness = 0.25f;

    // How many of the solid's brushes, counted from the front, this line built.
    // Anything after them was added by hand -- a doorway cut through the wall is
    // a Subtract brush appended to the same list -- and a rebuild must leave it
    // alone, or reshaping a wall would fill in its doors.
    u32 builtBrushes = 0;
};

} // namespace ECS
} // namespace Enjin
