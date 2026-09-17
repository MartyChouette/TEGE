#pragma once
// A liquid surface from a fluid density field.
//
// Drawing a fluid volume as one billboard per cell is what makes it read as
// voxels: every quad sits on a lattice point at a fixed size, so the grid is
// literally what you are looking at. For smoke that is merely chunky. For
// liquid it is wrong -- a liquid's defining visual feature is that it HAS a
// surface, and a cloud of billboards has none.
//
// A density field is a scalar field, and the engine already owns a mesher for
// those: Geometry::BuildSurfaceNet, written for SDF voxel caves. It is pure --
// field in, mesh out, no World and no GPU -- so a fluid can hand it a grid and
// get back an ordinary triangle mesh. Surface nets rounds hard corners, which
// for rock was acceptable and for a liquid surface is the point.
//
// What this does NOT give you is splashes, droplets or thin sheets. Those are
// features of a free-surface solver (FLIP/PIC or a level set) and no amount of
// meshing conjures them out of a density field. What it does give is water
// that pours in, is contained by the geometry, pools, and has a smooth,
// readable, non-voxel surface -- which is what set dressing and a canned
// cinematic actually need.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/Geometry/SurfaceNets.h"

namespace Enjin {
namespace Effects {

// Mesh the isosurface where density crosses `surfaceThreshold`.
//
// `volumeCentre` and `halfExtents` place it in world space exactly as
// FluidRenderer places the per-cell billboards, so the surface and the
// obstacle mask agree about where the volume is.
//
// Returns an empty mesh when the volume holds nothing above the threshold,
// which is the normal state of an empty or barely-started volume and not an
// error -- callers should skip drawing rather than treat it as a failure.
ENJIN_API Geometry::SurfaceMesh BuildFluidSurface(const FluidGridData& grid,
                                                  const Math::Vector3& volumeCentre,
                                                  const Math::Vector3& halfExtents,
                                                  f32 surfaceThreshold = 0.5f);

} // namespace Effects
} // namespace Enjin
