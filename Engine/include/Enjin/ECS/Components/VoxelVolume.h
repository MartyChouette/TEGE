#pragma once

// A block of space with rock in some of it.
//
// The component stores a SIGNED DISTANCE per voxel: negative inside solid,
// positive in air, and the surface is wherever it crosses zero. That is the
// only representation in the engine that can hold a cave.
//
// It exists because the other two cannot, and not for want of trying. A
// TerrainComponent stores one height per column, so a roof over a floor has
// nowhere to live. A BrushSolidComponent stores convex shapes, so a cavern
// wall is a hundred brushes badly approximating something that was never
// convex. Both also produce FINISHED SURFACES, which is why joining a tunnel to
// a hillside used to mean hiding a seam: two meshes that meet, rather than one
// shape.
//
// Unlike a brush solid, this component stores the RESULT of editing rather than
// a list of operations. That is a real trade and it goes the other way from the
// rest of the engine: a brush solid can have a doorway moved afterwards because
// the wall was never actually cut, and here a carve is destructive. It is the
// right trade for this shape of thing -- a cave is thousands of overlapping
// strokes, and a list of them would grow without bound, take longer to replay
// every time it was opened, and still not let you meaningfully move stroke 400.
// A painter keeps the painting, not the brush strokes.
//
// The field is f32 in memory and quantized on save; see VoxelVolumeSerializer.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <algorithm>
#include <vector>

namespace Enjin {
namespace ECS {

struct ENJIN_API VoxelVolumeComponent {
    // Samples per axis. The volume's extent is (dim - 1) * voxelSize, because
    // these are sample points and the cells sit between them.
    u32 dimX = 48;
    u32 dimY = 32;
    u32 dimZ = 48;

    // Metres between samples. This is the resolution limit of everything: no
    // passage narrower than about two voxels will survive meshing, so it is
    // also the smallest detail a person can carve.
    f32 voxelSize = 0.5f;

    // Signed distance per sample, in metres. Negative is solid.
    //
    // Empty means "all air" and costs nothing -- a volume dropped in a scene
    // and not yet carved does not allocate, for the same reason the terrain
    // hole mask does not.
    std::vector<f32> field;

    // How far a value is allowed to get from the surface before it stops being
    // tracked accurately. Values are clamped to this band, which is what makes
    // the save quantization lossless enough to be invisible: precision only
    // matters near the surface, and a sample twenty metres inside a mountain
    // only needs its sign.
    //
    // Derived from voxelSize rather than authored, because the band that
    // matters is the one the mesher can see across.
    f32 Band() const { return voxelSize * 4.0f; }

    // Set by anything that writes the field; cleared by VoxelVolumeSystem after
    // it remeshes. Same contract as TerrainComponent::meshDirty.
    bool meshDirty = true;

    usize Count() const {
        return static_cast<usize>(dimX) * static_cast<usize>(dimY) * static_cast<usize>(dimZ);
    }
    bool InBounds(u32 x, u32 y, u32 z) const { return x < dimX && y < dimY && z < dimZ; }
    usize Index(u32 x, u32 y, u32 z) const {
        return (static_cast<usize>(z) * dimY + y) * dimX + x;
    }

    // Air outside the volume, and air everywhere in a volume nobody has carved.
    // Both read the same way on purpose: an empty field is not a special case
    // anywhere, it is simply a volume made of air.
    f32 At(u32 x, u32 y, u32 z) const {
        if (field.empty() || !InBounds(x, y, z)) return Band();
        return field[Index(x, y, z)];
    }

    // Allocates on first write, like the terrain hole mask.
    void Set(u32 x, u32 y, u32 z, f32 v) {
        if (!InBounds(x, y, z)) return;
        if (field.empty()) field.assign(Count(), Band());
        field[Index(x, y, z)] = std::max(-Band(), std::min(Band(), v));
    }

    void Clear() {
        field.clear();
        meshDirty = true;
    }

    // The volume is CENTRED on its entity transform, like the terrain mesh and
    // for the same reason: a person drops it where they are looking and expects
    // it around that point, not stretching away to +X/+Z. The terrain tools got
    // this wrong in the other direction once and every stroke landed half a
    // terrain from the cursor.
    Math::Vector3 GridOrigin(const Math::Vector3& transformPosition) const {
        return Math::Vector3(
            transformPosition.x - static_cast<f32>(dimX - 1) * voxelSize * 0.5f,
            transformPosition.y - static_cast<f32>(dimY - 1) * voxelSize * 0.5f,
            transformPosition.z - static_cast<f32>(dimZ - 1) * voxelSize * 0.5f);
    }

    Math::Vector3 Extent() const {
        return Math::Vector3(static_cast<f32>(dimX - 1) * voxelSize,
                             static_cast<f32>(dimY - 1) * voxelSize,
                             static_cast<f32>(dimZ - 1) * voxelSize);
    }
};

} // namespace ECS
} // namespace Enjin
