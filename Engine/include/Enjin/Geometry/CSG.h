#pragma once

// Constructive solid geometry on CONVEX brushes.
//
// This is the "doorways and windows" system: build from convex solids, subtract
// one from another, get a mesh. It is deliberately not a general mesh-versus-mesh
// boolean. Arbitrary triangle soup booleans live or die on coplanar faces,
// near-degenerate triangles and floating point robustness, and the production
// implementations that survive it lean on exact predicates. Every operand here
// is a convex volume described by half-space planes, so the whole problem
// reduces to clipping polygons against planes, which is small and predictable.
//
// The trade is stated plainly: you cannot subtract an imported rock from another
// imported rock. You can cut a window out of a wall, a doorway out of a room and
// a stairwell out of a floor, which is what level building actually needs.
//
// Non-destructive by construction. A CSG result is a list of brushes and the
// operation each performs; the mesh is derived. Edit a brush, rebuild the mesh.
// The brush list is what gets serialized, because a room is a few dozen planes
// and its triangulation is megabytes.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/Components/Mesh.h"

#include <vector>

namespace Enjin {
namespace ECS { struct MeshColliderComponent; }
namespace Geometry {

// A half-space. Points with Distance(p) < 0 are INSIDE the space the plane
// bounds; a convex brush is the intersection of the insides of all its planes.
struct ENJIN_API Plane {
    Math::Vector3 normal = Math::Vector3(0.0f, 1.0f, 0.0f);   // unit, points OUT of the solid
    f32 offset = 0.0f;                                        // normal . p + offset = 0 on the plane

    f32 Distance(const Math::Vector3& p) const {
        return normal.x * p.x + normal.y * p.y + normal.z * p.z + offset;
    }

    static Plane FromPointNormal(const Math::Vector3& point, const Math::Vector3& n);
};

// How close to a plane still counts as ON it. Every clip decision goes through
// this one constant: with two of them the same vertex can be judged inside by
// one test and outside by another, which is how these systems grow cracks.
constexpr f32 kPlaneEpsilon = 1e-4f;

// A convex solid: the intersection of its planes' inside half-spaces. Planes
// must enclose a bounded volume; Box and Prism below guarantee that, and
// BuildFaces returns nothing for a set that does not.
struct ENJIN_API Brush {
    std::vector<Plane> planes;

    // Axis-aligned box, then rotated about its own centre. The six planes are
    // generated from the rotated axes, so a rotated brush is still exactly a
    // brush rather than an approximation of one.
    static Brush Box(const Math::Vector3& center, const Math::Vector3& halfExtents,
                     const Math::Quaternion& rotation = Math::Quaternion::Identity());

    // Regular n-sided prism about the Y axis: cylinders, ramps read as prisms,
    // and the round-headed doorway everyone wants to cut.
    static Brush Prism(const Math::Vector3& center, f32 radius, f32 halfHeight, u32 sides,
                       const Math::Quaternion& rotation = Math::Quaternion::Identity());

    bool Contains(const Math::Vector3& p, f32 eps = kPlaneEpsilon) const;
};

// One convex face of a brush, wound counter-clockwise seen from outside.
//
// Named BrushFace and not Polygon because <windows.h> defines a Polygon()
// function, so any translation unit that pulls in both and says
// `using namespace Enjin::Geometry` gets an ambiguous symbol. Cheaper to have a
// clearer name than to make every caller qualify it.
struct ENJIN_API BrushFace {
    std::vector<Math::Vector3> vertices;
    Plane plane;

    bool Valid() const { return vertices.size() >= 3; }
};

// What a brush does to the solid being built.
enum class BrushOp : u8 {
    Add = 0,      // union: the brush contributes solid
    Subtract,     // difference: the brush carves the solid away
    Intersect,    // the result keeps only what is also inside this brush
};

struct ENJIN_API BrushEntry {
    Brush brush;
    BrushOp op = BrushOp::Add;
};

// --- The pieces, exposed because they are individually testable ------------

// The brush's faces, one polygon per plane, each clipped by every other plane.
// Empty when the planes do not bound a volume.
ENJIN_API std::vector<BrushFace> BuildFaces(const Brush& brush);

// Sutherland-Hodgman against one plane. `keepInside` selects which side
// survives. A polygon entirely on the discarded side returns empty.
ENJIN_API BrushFace ClipFace(const BrushFace& poly, const Plane& plane, bool keepInside);

// The parts of `poly` that lie OUTSIDE the brush, as zero or more pieces. A
// polygon fully outside comes back unchanged; one fully inside comes back empty.
ENJIN_API std::vector<BrushFace> ClipFaceOutsideBrush(const BrushFace& poly, const Brush& brush);

// --- The whole operation ---------------------------------------------------

// Evaluate a brush list into faces, in order. Add contributes its faces minus
// anything later brushes remove; Subtract carves, and contributes its own faces
// flipped so the cut has a visible inner surface; Intersect keeps only the
// overlap.
ENJIN_API std::vector<BrushFace> BuildSolid(const std::vector<BrushEntry>& brushes);

// Triangulate to a mesh. Normals come from the source planes rather than from
// cross products of the triangles, so a sliver triangle cannot produce a wrong
// or NaN normal. UVs are planar-projected on each polygon's dominant axis,
// scaled by `uvScale` world units per tile, which is what a wall wants.
ENJIN_API ECS::MeshComponent ToMesh(const std::vector<BrushFace>& polygons, f32 uvScale = 1.0f);

// Convenience: brushes straight to a mesh.
ENJIN_API ECS::MeshComponent BuildMesh(const std::vector<BrushEntry>& brushes, f32 uvScale = 1.0f);

// --- Collision -------------------------------------------------------------
//
// A carved solid needs collision that matches what you can see through, and
// convex shapes cannot express a hole: a union of convex hulls over the Add
// brushes puts the doorway back. So the collider is the same triangles as the
// render mesh. Jolt cooks an exact MeshShape for static bodies whatever the
// convex flag says, which is precisely this case -- level geometry does not
// move.
//
// Not the render mesh verbatim, though. That duplicates every vertex per face
// so flat normals work, which a collider has no use for: a box arrives with 24
// vertices and needs 8. Welding also closes the seams between faces that shared
// a corner, and physics is happier with a closed mesh than with coincident
// duplicates.
struct ENJIN_API CollisionMesh {
    std::vector<Math::Vector3> vertices;
    std::vector<u32> indices;
};

// Weld coincident vertices and drop degenerate triangles. Degenerate triangles
// are not cosmetic here: a zero-area triangle has no normal, and a physics
// engine asked to cook one either rejects the whole shape or keeps a face that
// can never be hit.
ENJIN_API CollisionMesh BuildCollision(const std::vector<BrushFace>& faces,
                                       f32 weldEpsilon = 1e-3f);

// Fill a MeshColliderComponent from a brush list. Sets convex=false, because a
// carved solid is concave by definition and a hull of it would be a lie.
ENJIN_API void FillMeshCollider(ECS::MeshColliderComponent& out,
                                const std::vector<BrushEntry>& brushes);

} // namespace Geometry
} // namespace Enjin
