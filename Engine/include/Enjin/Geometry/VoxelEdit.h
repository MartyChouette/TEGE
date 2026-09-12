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

    // Squash the stroke vertically. 1 is round; above 1 is a tall narrow crack,
    // below 1 is a low wide slot.
    //
    // Applied by scaling the sample point before the distance is taken, which
    // is not an exact distance field: distances along the squashed axis come
    // out stretched by the same factor. The SURFACE is exact, which is what the
    // mesher reads; only the blend width is slightly off, and it is clamped
    // anyway. The alternative is an ellipsoid-capsule distance, which has no
    // closed form and would be solved numerically per voxel.
    f32 heightScale = 1.0f;
};

// Where a ray first meets the rock.
//
// This is what makes a cave diggable. Every stroke used to land on the y = 0
// build plane, so a dig could only ever run horizontally at ground height: you
// could not sink a shaft, deepen a floor, raise a ceiling, or cut into the wall
// you were looking at. A cave is a three dimensional thing and the gesture was
// two dimensional.
//
// Marched in fixed steps rather than sphere-traced. The field is clamped to a
// band and has noise added to it, so it is not a true distance beyond a voxel
// or two, and a sphere-trace that trusted it would step straight through a thin
// wall. Half-voxel steps are slower and cannot miss anything the mesher can see.
//
// Returns false when the ray leaves the volume without meeting anything, which
// is the normal result of pointing at open sky.
ENJIN_API bool RaycastVolume(const ECS::VoxelVolumeComponent& volume,
                             const Math::Vector3& volumeOrigin,
                             const Math::Vector3& rayOrigin,
                             const Math::Vector3& rayDirection,
                             f32 maxDistance, Math::Vector3& outPoint);

// The authoring brushes.
//
// One swept sphere carves one kind of hole, and a cave made entirely of one
// kind of hole reads as plumbing however rough its walls are. These are the
// shapes a cave is actually made of, and each is a different answer to "what
// does this drag mean".
//
// They are mappings from a DRAG to a stroke rather than separate carving code:
// the carve is the same swept-sphere write in every case, so a new brush cannot
// introduce a new way for carving to be wrong.
enum class VoxelBrush : u8 {
    // A corridor along the drag. The workhorse.
    Passage = 0,
    // A room. The drag sets the radius rather than a direction, because a
    // chamber has no direction -- dragging further makes it bigger.
    Chamber,
    // Straight down from where you aimed, as deep as the depth setting. The
    // thing you cannot do with a drag on the ground plane, and the reason
    // digging felt like fighting the tool.
    Shaft,
    // A passage that descends as it runs, so a cave can go somewhere rather
    // than staying on one level.
    Ramp,
    // A tall narrow fissure, or a low wide crawl, depending on the squash. A
    // cave that is round everywhere looks bored rather than formed.
    Crack,
    Count
};

ENJIN_API const char* VoxelBrushName(VoxelBrush brush);

// How a brush turns a drag into a stroke.
//
// `from` and `to` are where the gesture started and ended on the rock. `depth`
// is how far a Shaft sinks or a Ramp descends; `bore` is the radius the rail
// is set to.
struct ENJIN_API BrushGesture {
    Math::Vector3 from = Math::Vector3(0.0f, 0.0f, 0.0f);
    Math::Vector3 to = Math::Vector3(0.0f, 0.0f, 0.0f);
    f32 bore = 2.0f;
    f32 depth = 6.0f;
    f32 roughness = 0.35f;
    f32 blend = 0.5f;
    u32 seed = 1337u;
    bool filling = false;
};

// Pure: a gesture in, a stroke out. The whole difference between the brushes
// lives here and is testable without an editor.
ENJIN_API VoxelStroke MakeBrushStroke(VoxelBrush brush, const BrushGesture& gesture);

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

// How many samples a stroke needs beyond each face of the volume.
//
// Zero on every axis means the stroke fits. Anything else is how far a dig has
// run past the edge of the rock it was cutting into -- which is a thing that
// happens constantly, because a cave system is bigger than the block the first
// stroke created and nobody knows how big it will be when they start.
struct ENJIN_API GrowthRequest {
    u32 negX = 0, negY = 0, negZ = 0;
    u32 posX = 0, posY = 0, posZ = 0;
    bool Any() const { return negX || negY || negZ || posX || posY || posZ; }
};

ENJIN_API GrowthRequest StrokeOverflow(const ECS::VoxelVolumeComponent& volume,
                                       const Math::Vector3& volumeOrigin,
                                       const VoxelStroke& stroke);

// Grow the volume, keeping every sample it already holds where it was in WORLD
// space. New samples are seeded by `seed`, called with the world position of
// each one, so a volume baked from a hillside grows into more hillside rather
// than into a wall of rock at the old boundary.
//
// The volume's origin moves, so the caller must move the entity transform by
// the returned offset or every existing sample silently shifts. That is why the
// offset is returned rather than applied: this layer knows nothing about
// entities, and a growth that moved the field without moving the transform
// would drag a finished cave sideways.
//
// Two budgets, because one is not enough. `maxDimension` stops any single axis
// running away; `maxSamples` stops the TOTAL running away, which a per-axis cap
// cannot do -- 174 x 53 x 148 is inside a 192 cap on every axis and is still
// 1.4 million samples to remesh after every stroke. Growth that would break
// either budget is refused whole rather than partially applied, so the volume
// and its field never disagree about how big it is.
ENJIN_API Math::Vector3 GrowVolume(ECS::VoxelVolumeComponent& volume,
                                   const Math::Vector3& volumeOrigin,
                                   const GrowthRequest& request, u32 maxDimension,
                                   usize maxSamples,
                                   const std::function<f32(const Math::Vector3&)>& seed);

} // namespace Geometry
} // namespace Enjin
