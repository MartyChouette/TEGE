#include "Enjin/Effects/FluidSurface.h"

namespace Enjin {
namespace Effects {

Geometry::SurfaceMesh BuildFluidSurface(const FluidGridData& grid,
                                        const Math::Vector3& volumeCentre,
                                        const Math::Vector3& halfExtents,
                                        f32 surfaceThreshold) {
    Geometry::SurfaceMesh empty;
    if (grid.N == 0 || !grid.is3D) return empty;   // a 2D grid has no surface to mesh

    const u32 N = grid.N;

    // Sample spacing. Surface nets takes ONE voxel size, so a volume with
    // unequal half-extents would need a non-uniform grid it cannot express;
    // the smallest axis is used, which keeps the surface correct along that
    // axis and squashes the others rather than tearing the mesh.
    const f32 cellX = (halfExtents.x * 2.0f) / static_cast<f32>(N);
    const f32 cellY = (halfExtents.y * 2.0f) / static_cast<f32>(N);
    const f32 cellZ = (halfExtents.z * 2.0f) / static_cast<f32>(N);
    const f32 voxel = std::min(cellX, std::min(cellY, cellZ));

    Geometry::ScalarGrid field;
    field.Resize(N, N, N, 1.0f);
    field.voxelSize = voxel;

    // Sample (0,0,0) is the centre of interior cell (1,1,1), matching how
    // FluidRenderer places that cell's billboard. Keeping these two in step is
    // what stops the surface rendering offset from the obstacles the fluid is
    // flowing around.
    field.origin = Math::Vector3(
        volumeCentre.x - halfExtents.x + 0.5f * cellX,
        volumeCentre.y - halfExtents.y + 0.5f * cellY,
        volumeCentre.z - halfExtents.z + 0.5f * cellZ);

    // Surface nets reads NEGATIVE as inside, and an out-of-bounds sample as
    // +1 (air), which is what seals the mesh at the volume's edge. So the
    // field is threshold-minus-density: negative wherever there is enough
    // liquid, positive in the air, and crossing zero exactly at the surface.
    bool any = false;
    for (u32 k = 1; k <= N; ++k) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                const f32 d = grid.density[grid.IX3(i, j, k)];
                if (d > surfaceThreshold) any = true;
                field.Set(i - 1, j - 1, k - 1, surfaceThreshold - d);
            }
        }
    }
    // Nothing is above the threshold, so there is no crossing and the mesher
    // would walk the whole grid to produce nothing. Normal for an empty or
    // barely-started volume, so it is a skip rather than a failure.
    if (!any) return empty;

    return Geometry::BuildSurfaceNet(field, 0.0f);
}

} // namespace Effects
} // namespace Enjin
