#pragma once

// Carving.
//
// Every edit here writes a signed distance field by combining it with the field
// of a shape: a sphere for a chamber, a capsule for a passage, a tapered
// capsule for one that narrows. Subtracting hollows rock out, adding puts it
// back, and both go through the smooth operators so the join is a fillet rather
// than a crease.
//
// Free functions over a component rather than methods on it, for the same
// reason BuildBrushes is not a method on the editor: the shape a stroke makes
// is then checkable without a World, and the tool that drives it has nowhere to
// hide a second opinion about what a stroke does.
//
// Each edit reports the sample box it touched, so the caller can remesh that
// region instead of the whole volume. A stroke is a handful of voxels and a
// volume is a hundred thousand; remeshing everything per frame of a drag is the
// difference between carving and waiting.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Components/VoxelVolume.h"

#include <functional>

namespace Enjin {
namespace Geometry {

// The half-open sample range an edit touched. Empty when nothing changed, which
// is the normal result of a stroke that missed the volume entirely.
struct ENJIN_API EditRegion {
    u32 x0 = 0, y0 = 0, z0 = 0;
    u32 x1 = 0, y1 = 0, z1 = 0;   // exclusive

    bool Empty() const { return x1 <= x0 || y1 <= y0 || z1 <= z0; }
    void Merge(const EditRegion& o) {
        if (o.Empty()) return;
        if (Empty()) { *this = o; return; }
        x0 = (o.x0 < x0) ? o.x0 : x0;  y0 = (o.y0 < y0) ? o.y0 : y0;
        z0 = (o.z0 < z0) ? o.z0 : z0;
        x1 = (o.x1 > x1) ? o.x1 : x1;  y1 = (o.y1 > y1) ? o.y1 : y1;
        z1 = (o.z1 > z1) ? o.z1 : z1;
    }
};

// What a stroke does to the rock it touches.
enum class VoxelEditMode : u8 {
    Carve = 0,   // take rock away -- the cave itself
    Fill,        // put rock back
    Smooth,      // average the field locally: knocks the edges off a rough dig
};

// One stroke, described the way the tool thinks about it.
struct ENJIN_API VoxelStroke {
    // A stroke is a swept sphere from `a` to `b`. A single click has a == b,
    // which is a sphere, which is a chamber. The same shape covers both because
    // a passage IS a dragged chamber.
    Math::Vector3 a = Math::Vector3(0.0f, 0.0f, 0.0f);
    Math::Vector3 b = Math::Vector3(0.0f, 0.0f, 0.0f);

    // Radius at each end, so a passage can narrow into a crawl or open out.
    f32 radiusA = 2.0f;
    f32 radiusB = 2.0f;

    VoxelEditMode mode = VoxelEditMode::Carve;

    // The fillet where this stroke meets what is already there. Zero gives a
    // hard crease, which is what makes a cave read as pipework; a blend of
    // about a voxel is what makes two strokes look like one passage.
    f32 blend = 0.5f;

    // Amplitude of the roughness added to this stroke's surface, in metres.
    //
    // Zero is a mathematically perfect tube, which is exactly the thing that
    // was wrong with the old Cave tool. Rock is not smooth, and the difference
    // between a tunnel and a cave is mostly this number being non-zero.
    f32 roughness = 0.0f;
    f32 roughnessScale = 0.35f;   // features per metre
    u32 seed = 1337u;
};

// Apply one stroke. Returns the sample range that changed.
ENJIN_API EditRegion ApplyStroke(ECS::VoxelVolumeComponent& volume,
                                 const Math::Vector3& volumeOrigin,
                                 const VoxelStroke& stroke);

// Fill the volume from an arbitrary field, replacing whatever was there.
//
// This is how a terrain, some brush solids and a tunnel become ONE volume: bake
// their combined field down once, and from then on it is a single thing that
// can be carved as itself. The seam does not get hidden, it stops existing.
ENJIN_API void BakeField(ECS::VoxelVolumeComponent& volume,
                         const Math::Vector3& volumeOrigin,
                         const std::function<f32(const Math::Vector3&)>& field);

} // namespace Geometry
} // namespace Enjin
