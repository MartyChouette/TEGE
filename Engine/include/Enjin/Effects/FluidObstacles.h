#pragma once
// Which fluid cells sit inside solid geometry.
//
// The Stam solver has only ever known about the six walls of its own box --
// SetBoundary2D/SetBoundary3D touch the shell of the grid and nothing else --
// so smoke passed straight through every crate, wall and chimney placed inside
// a volume. That is tolerable for a plume in open air and useless for what
// this solver is actually for: baking a high-fidelity simulation against real
// level geometry and playing the recording back at runtime.
//
// The mask is voxelised from the scene's colliders, NOT re-tested per step:
// it only changes when the colliders move, and a bake runs against static
// geometry by definition.
//
// Cell centres are mapped to world space exactly as FluidRenderer maps them
// (origin = centre - halfExtents, cell = 2 * halfExtents / N, sample at
// i - 0.5). If those two ever disagree, the smoke renders offset from the
// obstacles it is flowing around, which looks like a physics bug and is a
// coordinate bug.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Effects/ParticleColliders.h"

#include <vector>

namespace Enjin {
namespace ECS { class World; }
namespace Effects {

// True when `worldPos` is inside the shape. Box/sphere/capsule, world space,
// matching the engine's collider conventions (capsule height is the cylinder
// section only, so the total is height + 2 * radius).
ENJIN_API bool PointInsideColliderShape(const ParticleColliderShape& shape,
                                        const Math::Vector3& worldPos);

// Fill `outSolid` with one byte per grid cell -- 1 = solid, 0 = fluid -- sized
// (N+2)^3 for a 3D grid or (N+2)^2 for 2D, so it indexes identically to the
// grid's own arrays. The padding shell is left 0; the solver already treats it
// as a wall, and marking it solid as well would double the reflection.
ENJIN_API void BuildFluidObstacleMask(const std::vector<ParticleColliderShape>& shapes,
                                      const Math::Vector3& volumeCenter,
                                      const Math::Vector3& halfExtents,
                                      u32 N, bool is3D,
                                      std::vector<u8>& outSolid);

// The same, gathering the world's colliders first (uncapped).
//
// A 3D volume gathers the Jolt collider components. A 2D volume gathers BOTH
// those and the Box2D `Body2DComponent` shapes, and the union is deliberate:
// obstacles here are geometry rather than physics, and a 2D fluid volume in
// front of 3D meshes with ordinary BoxColliders is the normal case. Gathering
// only one kind leaves the other silently invisible to the solver, which looks
// exactly like obstacles not working at all.
//
// The 3D shapes are tested against the volume's Z plane, so a box intersecting
// the sheet counts and one passing behind it does not. A 2D body has no Z and
// counts at every depth, which is what a 2D scene means by a wall.
ENJIN_API void BuildFluidObstacleMask(ECS::World* world,
                                      const Math::Vector3& volumeCenter,
                                      const Math::Vector3& halfExtents,
                                      u32 N, bool is3D,
                                      std::vector<u8>& outSolid);

// A fingerprint of everything the mask is built FROM -- every gathered
// collider's shape and world transform, for the dimension in question.
//
// The live solver rebuilds a volume's mask when this changes and not
// otherwise. Voxelising every volume every frame would be honest and costs a
// full scan per volume per frame; re-using a stale mask silently would not be.
// A collider that MOVES changes its transform, so it changes this.
ENJIN_API u64 FluidObstacleFingerprint(ECS::World* world, bool is3D);

} // namespace Effects
} // namespace Enjin
