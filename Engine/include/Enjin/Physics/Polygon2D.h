#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <vector>

namespace Enjin {
namespace Physics {

// The most vertices a Box2D polygon may have.
inline constexpr usize kMaxPolygon2DVertices = 8;

// Turns any set of points into a polygon Box2D will accept: convex,
// counter-clockwise, at most maxVertices corners.
//
// A traced sprite outline is usually concave and has dozens of points. Box2D
// takes the convex hull of at most 8, and anything over 8 used to be replaced
// by a unit box. This takes the hull, then removes the corner whose removal
// loses the least area, one at a time, until it fits, so the result stays as
// close to the outline as eight corners allow. Returns fewer than 3 points
// when the input has no area.
ENJIN_API std::vector<Math::Vector2> FitBox2DPolygon(const std::vector<Math::Vector2>& points,
                                                     usize maxVertices = kMaxPolygon2DVertices);

} // namespace Physics
} // namespace Enjin
