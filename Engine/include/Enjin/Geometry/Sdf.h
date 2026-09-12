// Signed distance fields.
//
// A distance field answers one question for any point in space: how far is the
// nearest surface, and am I inside it or outside it. Negative is solid.
//
// It is here because two things the editor could not do turn out to be the same
// missing thing.
//
// A heightmap stores one height per column, so it can hold a landscape and can
// never hold a cave -- there is nowhere to put a roof and a floor at the same
// x,z. A brush solid is a set of planes, so it can hold a room and can never
// hold a hillside. Every way of joining them ends at the same wall: two
// finished surfaces meeting along a seam, which has to be hidden rather than
// solved.
//
// A field has no seam to hide, because the pieces are combined BEFORE anything
// is meshed. Union two fields and you get one field; mesh it and you get one
// surface. What was a join is now just a place where the distance happened to
// come from a different term.
//
// And the melding is a numeric operator, not a modelling chore: SmoothUnion is
// an ordinary union with the sharp corner rounded off over a radius you choose.
// That is the whole of "make terrain and tunnels mix".
//
// Everything here is pure and cheap enough to call per voxel. No allocation, no
// state, nothing that needs a GPU or a World -- which is what makes a cave
// shape checkable in a test.

#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Geometry {

// ---------------------------------------------------------------------------
// Combinators
// ---------------------------------------------------------------------------

// Both, hard. The classic min, and it leaves a crease wherever the two
// surfaces cross.
inline f32 SdfUnion(f32 a, f32 b) { return std::min(a, b); }

// A with B taken out of it.
inline f32 SdfSubtract(f32 a, f32 b) { return std::max(a, -b); }

// Only where both are solid.
inline f32 SdfIntersect(f32 a, f32 b) { return std::max(a, b); }

// Both, with a fillet of radius k where they meet.
//
// This is the melding operator. A hard union of a hillside and a tunnel mouth
// gives a crease you can see the seam in; blending them over half a metre gives
// the shape a cave mouth actually has, where the rock curves into the opening.
//
// The polynomial smooth-min (Quilez): exact at k = 0, and it never returns more
// than min(a,b), so a blended union can only ever ADD material near the join.
// That matters for a cave: a blend that ate into the surface would open holes
// nobody asked for.
inline f32 SdfSmoothUnion(f32 a, f32 b, f32 k) {
    if (k <= 1e-6f) return SdfUnion(a, b);
    const f32 h = std::max(0.0f, std::min(1.0f, 0.5f + 0.5f * (b - a) / k));
    return b * (1.0f - h) + a * h - k * h * (1.0f - h);
}

// A with B taken out, and the cut edge rounded rather than knife-sharp.
//
// Carving a passage with a hard subtract leaves an edge you could cut yourself
// on where it breaks the surface. Rock does not do that.
inline f32 SdfSmoothSubtract(f32 a, f32 b, f32 k) {
    if (k <= 1e-6f) return SdfSubtract(a, b);
    const f32 h = std::max(0.0f, std::min(1.0f, 0.5f - 0.5f * (a + b) / k));
    return (a * (1.0f - h) + (-b) * h) + k * h * (1.0f - h);
}

// ---------------------------------------------------------------------------
// Primitives
// ---------------------------------------------------------------------------

// Exact everywhere.
inline f32 SdfSphere(const Math::Vector3& p, const Math::Vector3& centre, f32 radius) {
    const f32 dx = p.x - centre.x, dy = p.y - centre.y, dz = p.z - centre.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz) - radius;
}

// Exact outside, and a lower bound inside (the usual box field). Inside-ness is
// all a mesher needs, and the bound is what keeps it cheap.
inline f32 SdfBox(const Math::Vector3& p, const Math::Vector3& centre,
                  const Math::Vector3& halfExtents) {
    const f32 qx = std::fabs(p.x - centre.x) - halfExtents.x;
    const f32 qy = std::fabs(p.y - centre.y) - halfExtents.y;
    const f32 qz = std::fabs(p.z - centre.z) - halfExtents.z;
    const f32 ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f), oz = std::max(qz, 0.0f);
    const f32 outside = std::sqrt(ox * ox + oy * oy + oz * oz);
    const f32 inside = std::min(std::max(qx, std::max(qy, qz)), 0.0f);
    return outside + inside;
}

// A rounded box: the same shape with its edges filleted by r. A room carved
// with this reads as a chamber rather than as a crate.
inline f32 SdfRoundBox(const Math::Vector3& p, const Math::Vector3& centre,
                       const Math::Vector3& halfExtents, f32 r) {
    const Math::Vector3 shrunk(std::max(halfExtents.x - r, 0.0f),
                               std::max(halfExtents.y - r, 0.0f),
                               std::max(halfExtents.z - r, 0.0f));
    return SdfBox(p, centre, shrunk) - r;
}

// A capsule: the swept sphere from a to b. This is the shape a passage is --
// not a prism with flat end caps, which is what a pipe is.
inline f32 SdfCapsule(const Math::Vector3& p, const Math::Vector3& a,
                      const Math::Vector3& b, f32 radius) {
    const Math::Vector3 pa(p.x - a.x, p.y - a.y, p.z - a.z);
    const Math::Vector3 ba(b.x - a.x, b.y - a.y, b.z - a.z);
    const f32 bb = ba.x * ba.x + ba.y * ba.y + ba.z * ba.z;
    f32 t = 0.0f;
    if (bb > 1e-8f) {
        t = (pa.x * ba.x + pa.y * ba.y + pa.z * ba.z) / bb;
        t = std::max(0.0f, std::min(1.0f, t));
    }
    const f32 dx = pa.x - ba.x * t, dy = pa.y - ba.y * t, dz = pa.z - ba.z * t;
    return std::sqrt(dx * dx + dy * dy + dz * dz) - radius;
}

// A capsule whose radius changes end to end, so a passage can narrow into a
// crawl or open into a chamber. A cave that is one width everywhere reads as
// plumbing.
//
// Not an exact distance -- interpolating the radius along a cone is only exact
// for a true round cone -- but it is conservative enough for a mesher sampling
// on a grid, and it is monotonic, which is what the surface actually needs.
inline f32 SdfTaperedCapsule(const Math::Vector3& p, const Math::Vector3& a,
                             const Math::Vector3& b, f32 radiusA, f32 radiusB) {
    const Math::Vector3 pa(p.x - a.x, p.y - a.y, p.z - a.z);
    const Math::Vector3 ba(b.x - a.x, b.y - a.y, b.z - a.z);
    const f32 bb = ba.x * ba.x + ba.y * ba.y + ba.z * ba.z;
    f32 t = 0.0f;
    if (bb > 1e-8f) {
        t = (pa.x * ba.x + pa.y * ba.y + pa.z * ba.z) / bb;
        t = std::max(0.0f, std::min(1.0f, t));
    }
    const f32 dx = pa.x - ba.x * t, dy = pa.y - ba.y * t, dz = pa.z - ba.z * t;
    const f32 r = radiusA + (radiusB - radiusA) * t;
    return std::sqrt(dx * dx + dy * dy + dz * dz) - r;
}

// A heightfield, as a field.
//
// This is how a sculpted landscape joins the same world as a cave. `surfaceY`
// is the terrain's height at p, sampled by the caller; the field is simply how
// far p is above it. Solid below, air above.
//
// It is a VERTICAL distance, not a Euclidean one, which overstates the distance
// on a steep slope. Meshers care about the sign and the zero crossing, and both
// are exact here; it is the step size of a sphere-tracer that would suffer, and
// nothing here sphere-traces.
inline f32 SdfHeightfield(const Math::Vector3& p, f32 surfaceY) {
    return p.y - surfaceY;
}

// ---------------------------------------------------------------------------
// Detail
// ---------------------------------------------------------------------------

// Push a surface around by an arbitrary amount.
//
// Adding noise to a distance is what turns a smooth tube into something that
// reads as rock. It also breaks the field's distance property -- the result is
// no longer a true distance to anything -- so the amplitude has to stay small
// against the sampling grid or the mesher starts missing thin features. The
// caller owns that trade, which is why this takes the displacement rather than
// generating it.
inline f32 SdfDisplace(f32 d, f32 displacement) { return d + displacement; }

} // namespace Geometry
} // namespace Enjin
