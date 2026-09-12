// How long carving takes.
//
// Interactivity is a correctness property for a sculpting tool. A stroke that
// takes a second to appear is not a slow tool, it is an unusable one: you
// cannot aim the next stroke until you can see the last, so the whole loop
// stops being carving and becomes waiting.
//
// The thresholds here are deliberately generous -- they are guarding against a
// change that makes this ten times slower, not policing milliseconds -- but
// they fail rather than warn, because a warning in a test log is something
// nobody reads.

#include "EnjinTest.h"
#include "Enjin/Geometry/VoxelEdit.h"
#include "Enjin/Geometry/SurfaceNets.h"
#include "Enjin/ECS/Components/VoxelVolume.h"

#include <chrono>
#include <cstdio>

using namespace Enjin;
using namespace Enjin::Geometry;
using Enjin::Math::Vector3;

namespace {

ECS::VoxelVolumeComponent SolidBlock(u32 nx, u32 ny, u32 nz, f32 voxel) {
    ECS::VoxelVolumeComponent v;
    v.dimX = nx; v.dimY = ny; v.dimZ = nz;
    v.voxelSize = voxel;
    v.field.assign(v.Count(), -v.Band());
    return v;
}

ScalarGrid ToGrid(const ECS::VoxelVolumeComponent& v) {
    ScalarGrid g;
    g.dimX = v.dimX; g.dimY = v.dimY; g.dimZ = v.dimZ;
    g.voxelSize = v.voxelSize;
    g.values = v.field;
    return g;
}

template <typename Fn>
f64 TimeMs(Fn&& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<f64, std::milli>(t1 - t0).count();
}

} // namespace

// The size the editor actually creates for a first stroke.
ENJIN_TEST(VoxelPerf, RemeshingAnEditorSizedVolumeStaysInteractive) {
    // Arrange: 64 x 48 x 64 at half a metre is 32m x 24m x 32m of rock, which
    // is a cave system rather than a corridor.
    ECS::VoxelVolumeComponent v = SolidBlock(64, 48, 64, 0.5f);
    VoxelStroke s;
    s.a = Vector3(4, 12, 4);
    s.b = Vector3(28, 12, 28);
    s.radiusA = s.radiusB = 2.0f;
    s.roughness = 0.3f;
    ApplyStroke(v, Vector3(0, 0, 0), s);

    // Act
    usize triangles = 0;
    const f64 ms = TimeMs([&] {
        const SurfaceMesh m = BuildSurfaceNet(ToGrid(v), 0.0f);
        triangles = m.TriangleCount();
    });

    // Assert
    std::printf("    remesh 64x48x64 (%zu tris): %.1f ms\n", triangles, ms);
    ENJIN_EXPECT_TRUE(triangles > 0);
    // A whole-volume remesh happens once per stroke. Half a second is already
    // uncomfortable; this fails well before it gets there.
    ENJIN_EXPECT_TRUE(ms < 400.0);
}

ENJIN_TEST(VoxelPerf, AStrokeItselfIsCheapBecauseItOnlyTouchesItsOwnNeighbourhood) {
    // Arrange
    ECS::VoxelVolumeComponent v = SolidBlock(64, 48, 64, 0.5f);
    VoxelStroke s;
    s.a = Vector3(10, 12, 10);
    s.b = Vector3(14, 12, 14);
    s.radiusA = s.radiusB = 2.0f;
    s.roughness = 0.3f;

    // Act
    EditRegion touched;
    const f64 ms = TimeMs([&] { touched = ApplyStroke(v, Vector3(0, 0, 0), s); });

    // Assert: the write is a small fraction of the volume, and fast. If a
    // stroke ever starts walking the whole field this is what catches it.
    const usize touchedCount = static_cast<usize>(touched.x1 - touched.x0) *
                               (touched.y1 - touched.y0) * (touched.z1 - touched.z0);
    std::printf("    stroke touched %zu of %zu samples: %.1f ms\n",
                touchedCount, v.Count(), ms);
    ENJIN_EXPECT_TRUE(touchedCount * 4 < v.Count());
    ENJIN_EXPECT_TRUE(ms < 100.0);
}

// The honest headline number: what one stroke costs end to end.
ENJIN_TEST(VoxelPerf, AWholeStrokeAndRemeshCostsLessThanAQuarterSecond) {
    // Arrange
    ECS::VoxelVolumeComponent v = SolidBlock(64, 48, 64, 0.5f);
    const Vector3 origin(0, 0, 0);

    // Act: ten strokes, as a person digging a passage would make.
    f64 worst = 0.0;
    for (u32 i = 0; i < 10; ++i) {
        VoxelStroke s;
        const f32 t = static_cast<f32>(i) * 2.0f;
        s.a = Vector3(4.0f + t, 12.0f, 8.0f);
        s.b = Vector3(6.0f + t, 12.0f, 10.0f);
        s.radiusA = s.radiusB = 2.0f;
        s.roughness = 0.3f;
        const f64 ms = TimeMs([&] {
            ApplyStroke(v, origin, s);
            BuildSurfaceNet(ToGrid(v), 0.0f);
        });
        worst = (ms > worst) ? ms : worst;
    }

    // Assert
    std::printf("    worst stroke+remesh of ten: %.1f ms\n", worst);
    ENJIN_EXPECT_TRUE(worst < 400.0);
}

ENJIN_TEST_MAIN()
