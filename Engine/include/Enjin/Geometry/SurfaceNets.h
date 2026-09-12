// Surface nets: a scalar field in, a triangle surface out.
//
// The mesher for signed distance fields. It walks a regular grid, finds every
// cell the surface passes through, places one vertex inside each, and joins
// them -- so a shape that no set of planes and no heightmap could describe ends
// up as an ordinary mesh with an ordinary collider.
//
// Why a field rather than the two things the engine already had: both of those
// can only express what their storage can hold. A heightmap has one height per
// column and therefore no roof over a floor. A brush solid is an intersection
// of half-spaces and therefore convex until you start subtracting. A field has
// neither limit, so an overhang, a chamber, a branching passage and an arch are
// all just places where the sign changes.
//
// Why surface nets and not marching cubes. Marching cubes is the name everyone
// knows, and it needs a 256-entry triangle table. That table is transcribed,
// not derived, and one wrong row is a hole in a cave wall that shows up in one
// configuration out of two hundred and fifty six -- a bug no test I could
// reasonably write would catch, in geometry a person would have to go and stand
// inside to find. Surface nets needs no tables at all: one vertex per crossed
// cell, one quad per crossed edge. It is manifold by construction, it is what
// smooth voxel terrain generally ships with, and every claim it makes is
// checkable. The cost is that it rounds hard corners, which for rock is not a
// cost.
//
// Pure: field in, mesh out. No World, no GPU, no hidden state. That is what
// makes the shape of a cave something a test can assert on.

#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <functional>
#include <vector>

namespace Enjin {
namespace Geometry {

// What comes out: a plain indexed triangle mesh in the field's own space.
struct ENJIN_API SurfaceMesh {
    std::vector<Math::Vector3> positions;
    std::vector<Math::Vector3> normals;
    std::vector<u32> indices;

    bool Empty() const { return indices.empty(); }
    usize TriangleCount() const { return indices.size() / 3; }
};

// A dense sampled field: `dim` samples per axis, `voxelSize` apart, with sample
// (0,0,0) at `origin`.
//
// The grid is SAMPLES, not cells -- a 2x2x2 grid is one cell. Confusing the two
// is the classic off-by-one here, and it shows up as a surface one voxel small
// in every direction.
struct ENJIN_API ScalarGrid {
    std::vector<f32> values;
    u32 dimX = 0, dimY = 0, dimZ = 0;
    f32 voxelSize = 1.0f;
    Math::Vector3 origin = Math::Vector3(0.0f, 0.0f, 0.0f);

    usize Count() const {
        return static_cast<usize>(dimX) * static_cast<usize>(dimY) * static_cast<usize>(dimZ);
    }
    bool InBounds(u32 x, u32 y, u32 z) const { return x < dimX && y < dimY && z < dimZ; }
    usize Index(u32 x, u32 y, u32 z) const {
        return (static_cast<usize>(z) * dimY + y) * dimX + x;
    }

    // Outside the grid reads as AIR, positive and far.
    //
    // A volume has to close itself at its own boundary or the mesh is an open
    // shell, and an open shell is not a solid: its collider has an inside you
    // can fall out through. Reading out-of-bounds as air means the surface
    // wraps round and seals against the edge of the volume.
    f32 At(u32 x, u32 y, u32 z) const {
        if (!InBounds(x, y, z)) return 1.0f;
        return values[Index(x, y, z)];
    }
    void Set(u32 x, u32 y, u32 z, f32 v) {
        if (InBounds(x, y, z)) values[Index(x, y, z)] = v;
    }
    Math::Vector3 PositionOf(u32 x, u32 y, u32 z) const {
        return Math::Vector3(origin.x + static_cast<f32>(x) * voxelSize,
                             origin.y + static_cast<f32>(y) * voxelSize,
                             origin.z + static_cast<f32>(z) * voxelSize);
    }
    void Resize(u32 nx, u32 ny, u32 nz, f32 fill = 1.0f) {
        dimX = nx; dimY = ny; dimZ = nz;
        values.assign(Count(), fill);
    }
};

// Mesh the whole grid. `isoLevel` is the value the surface sits at: 0 for a
// signed distance field, where negative is solid.
ENJIN_API SurfaceMesh BuildSurfaceNet(const ScalarGrid& grid, f32 isoLevel = 0.0f);

// Sample an arbitrary field into a grid, once per sample point, in world space.
//
// Separate from the mesher on purpose. Baking a field down to a grid is also
// how a terrain, a set of brush solids and a tunnel become ONE volume that can
// then be edited as itself -- which is the other half of why fields are here.
ENJIN_API void SampleField(ScalarGrid& grid,
                           const std::function<f32(const Math::Vector3&)>& field);

} // namespace Geometry
} // namespace Enjin
