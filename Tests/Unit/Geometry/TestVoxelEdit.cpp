// Carving a voxel volume.
//
// The claims worth pinning here are the ones that separate a cave from a pipe:
// a stroke hollows rock and leaves the rock around it standing, two strokes
// that cross make one connected space, a stroke can narrow, roughness actually
// changes the surface, and none of it touches voxels it has no business
// touching.

#include "EnjinTest.h"
#include "Enjin/Geometry/VoxelEdit.h"
#include "Enjin/Geometry/SurfaceNets.h"
#include "Enjin/Geometry/Sdf.h"
#include "Enjin/ECS/Components/VoxelVolume.h"

#include <cmath>
#include <map>

using namespace Enjin;
using namespace Enjin::Geometry;
using Enjin::Math::Vector3;

namespace {

// Arrange: a block of solid rock, origin at (0,0,0), one metre per voxel.
ECS::VoxelVolumeComponent SolidRock(u32 n = 24, f32 voxel = 1.0f) {
    ECS::VoxelVolumeComponent v;
    v.dimX = v.dimY = v.dimZ = n;
    v.voxelSize = voxel;
    v.field.assign(v.Count(), -v.Band());   // negative everywhere: all solid
    v.meshDirty = true;
    return v;
}

bool IsSolid(const ECS::VoxelVolumeComponent& v, u32 x, u32 y, u32 z) {
    return v.At(x, y, z) < 0.0f;
}

ScalarGrid ToGrid(const ECS::VoxelVolumeComponent& v, const Vector3& origin) {
    ScalarGrid g;
    g.dimX = v.dimX; g.dimY = v.dimY; g.dimZ = v.dimZ;
    g.voxelSize = v.voxelSize;
    g.origin = origin;
    g.values.assign(v.Count(), v.Band());
    for (u32 z = 0; z < v.dimZ; ++z)
        for (u32 y = 0; y < v.dimY; ++y)
            for (u32 x = 0; x < v.dimX; ++x)
                g.values[g.Index(x, y, z)] = v.At(x, y, z);
    return g;
}

usize NonManifoldEdges(const SurfaceMesh& m) {
    std::map<std::pair<u32, u32>, u32> counts;
    for (usize i = 0; i + 2 < m.indices.size(); i += 3) {
        const u32 t[3] = { m.indices[i], m.indices[i + 1], m.indices[i + 2] };
        for (u32 e = 0; e < 3; ++e) {
            u32 a = t[e], b = t[(e + 1) % 3];
            if (a > b) std::swap(a, b);
            ++counts[{a, b}];
        }
    }
    usize bad = 0;
    for (const auto& kv : counts) if (kv.second != 2) ++bad;
    return bad;
}

} // namespace

ENJIN_TEST(VoxelEdit, AFreshVolumeIsAirAndCostsNoMemory) {
    // Arrange / Act
    const ECS::VoxelVolumeComponent v;

    // Assert: an untouched volume allocates nothing, and reads as air.
    ENJIN_EXPECT_TRUE(v.field.empty());
    ENJIN_EXPECT_TRUE(v.At(0, 0, 0) > 0.0f);
    ENJIN_EXPECT_TRUE(v.At(5, 5, 5) > 0.0f);
}

ENJIN_TEST(VoxelEdit, CarvingHollowsTheRockItPassesThroughAndLeavesTheRestStanding) {
    // Arrange
    ECS::VoxelVolumeComponent v = SolidRock();
    const Vector3 origin(0, 0, 0);
    VoxelStroke s;
    s.a = Vector3(12, 12, 4);
    s.b = Vector3(12, 12, 20);
    s.radiusA = s.radiusB = 2.5f;
    s.mode = VoxelEditMode::Carve;

    // Act
    const EditRegion r = ApplyStroke(v, origin, s);

    // Assert: the passage is open along its whole run...
    ENJIN_ASSERT_FALSE(r.Empty());
    ENJIN_EXPECT_FALSE(IsSolid(v, 12, 12, 6));
    ENJIN_EXPECT_FALSE(IsSolid(v, 12, 12, 12));
    ENJIN_EXPECT_FALSE(IsSolid(v, 12, 12, 18));

    // ...and the rock beside and above it is untouched.
    ENJIN_EXPECT_TRUE(IsSolid(v, 12, 18, 12));
    ENJIN_EXPECT_TRUE(IsSolid(v, 4, 12, 12));
    ENJIN_EXPECT_TRUE(IsSolid(v, 12, 12, 1));
}

ENJIN_TEST(VoxelEdit, AStrokeOnlyTouchesVoxelsNearItself) {
    // Arrange
    ECS::VoxelVolumeComponent v = SolidRock();
    const Vector3 origin(0, 0, 0);
    VoxelStroke s;
    s.a = s.b = Vector3(12, 12, 12);
    s.radiusA = s.radiusB = 2.0f;

    // Act
    const EditRegion r = ApplyStroke(v, origin, s);

    // Assert: the reported region is a small box around the stroke, not the
    // whole volume. Remeshing the whole volume per frame of a drag is the
    // difference between carving and waiting.
    ENJIN_ASSERT_FALSE(r.Empty());
    ENJIN_EXPECT_TRUE(r.x1 - r.x0 < v.dimX);
    ENJIN_EXPECT_TRUE(r.y1 - r.y0 < v.dimY);
    ENJIN_EXPECT_TRUE(r.z1 - r.z0 < v.dimZ);
    // And it actually contains the stroke.
    ENJIN_EXPECT_TRUE(r.x0 <= 12 && 12 < r.x1);
    ENJIN_EXPECT_TRUE(r.y0 <= 12 && 12 < r.y1);
    ENJIN_EXPECT_TRUE(r.z0 <= 12 && 12 < r.z1);
}

ENJIN_TEST(VoxelEdit, AStrokeThatMissesTheVolumeChangesNothing) {
    // Arrange
    ECS::VoxelVolumeComponent v = SolidRock();
    const auto before = v.field;
    VoxelStroke s;
    s.a = s.b = Vector3(500, 500, 500);
    s.radiusA = s.radiusB = 2.0f;

    // Act
    const EditRegion r = ApplyStroke(v, Vector3(0, 0, 0), s);

    // Assert
    ENJIN_EXPECT_TRUE(r.Empty());
    ENJIN_EXPECT_TRUE(v.field == before);
}

ENJIN_TEST(VoxelEdit, TwoCrossingStrokesMakeOneConnectedSpace) {
    // Arrange: this is what a cave is and a pipe is not -- passages that join.
    ECS::VoxelVolumeComponent v = SolidRock();
    const Vector3 origin(0, 0, 0);
    VoxelStroke along;
    along.a = Vector3(4, 12, 12); along.b = Vector3(20, 12, 12);
    along.radiusA = along.radiusB = 2.0f;
    VoxelStroke across;
    across.a = Vector3(12, 12, 4); across.b = Vector3(12, 12, 20);
    across.radiusA = across.radiusB = 2.0f;

    // Act
    ApplyStroke(v, origin, along);
    ApplyStroke(v, origin, across);

    // Assert: all four arms and the junction are open.
    ENJIN_EXPECT_FALSE(IsSolid(v, 6, 12, 12));
    ENJIN_EXPECT_FALSE(IsSolid(v, 18, 12, 12));
    ENJIN_EXPECT_FALSE(IsSolid(v, 12, 12, 6));
    ENJIN_EXPECT_FALSE(IsSolid(v, 12, 12, 18));
    ENJIN_EXPECT_FALSE(IsSolid(v, 12, 12, 12));
    // And the corner between two arms is still rock.
    ENJIN_EXPECT_TRUE(IsSolid(v, 6, 12, 6));
}

ENJIN_TEST(VoxelEdit, ATaperedStrokeCarvesWiderAtItsWideEnd) {
    // Arrange
    ECS::VoxelVolumeComponent v = SolidRock();
    const Vector3 origin(0, 0, 0);
    VoxelStroke s;
    s.a = Vector3(12, 12, 4);
    s.b = Vector3(12, 12, 20);
    s.radiusA = 4.0f;   // chamber
    s.radiusB = 1.0f;   // crawl
    s.blend = 0.0f;

    // Act
    ApplyStroke(v, origin, s);

    // Assert: three voxels off-axis is open at the wide end and solid at the
    // narrow one. A cave that is one width everywhere reads as plumbing.
    ENJIN_EXPECT_FALSE(IsSolid(v, 15, 12, 4));
    ENJIN_EXPECT_TRUE(IsSolid(v, 15, 12, 20));
}

ENJIN_TEST(VoxelEdit, FillPutsRockBackWhereCarveTookItAway) {
    // Arrange
    ECS::VoxelVolumeComponent v = SolidRock();
    const Vector3 origin(0, 0, 0);
    VoxelStroke s;
    s.a = s.b = Vector3(12, 12, 12);
    s.radiusA = s.radiusB = 3.0f;
    s.blend = 0.0f;

    // Act
    s.mode = VoxelEditMode::Carve;
    ApplyStroke(v, origin, s);
    ENJIN_ASSERT_FALSE(IsSolid(v, 12, 12, 12));

    s.mode = VoxelEditMode::Fill;
    ApplyStroke(v, origin, s);

    // Assert
    ENJIN_EXPECT_TRUE(IsSolid(v, 12, 12, 12));
}

ENJIN_TEST(VoxelEdit, RoughnessActuallyChangesTheSurface) {
    // Arrange: the same stroke, once smooth and once rough. If roughness did
    // nothing, the two fields would be identical -- and a cave that is a
    // mathematically perfect tube is the exact thing that was wrong before.
    const Vector3 origin(0, 0, 0);
    ECS::VoxelVolumeComponent smooth = SolidRock(24, 0.5f);
    ECS::VoxelVolumeComponent rough = SolidRock(24, 0.5f);

    VoxelStroke s;
    s.a = Vector3(3, 6, 2);
    s.b = Vector3(3, 6, 10);
    s.radiusA = s.radiusB = 1.5f;

    // Act
    ApplyStroke(smooth, origin, s);
    s.roughness = 0.5f;
    ApplyStroke(rough, origin, s);

    // Assert
    ENJIN_EXPECT_FALSE(smooth.field == rough.field);

    // And the roughness is bounded: it must not eat so far that the passage
    // bursts out of the rock it was bored through.
    ENJIN_EXPECT_TRUE(IsSolid(rough, 3, 20, 6));
}

ENJIN_TEST(VoxelEdit, SmoothingReadsTheFieldAsItWasAndNotAsItIsBeingChanged) {
    // Arrange: a jagged dig.
    ECS::VoxelVolumeComponent v = SolidRock(20, 1.0f);
    const Vector3 origin(0, 0, 0);
    for (u32 i = 0; i < 5; ++i) {
        VoxelStroke bite;
        bite.a = bite.b = Vector3(10.0f + static_cast<f32>(i % 2), 10.0f,
                                  6.0f + static_cast<f32>(i) * 1.5f);
        bite.radiusA = bite.radiusB = 1.6f;
        bite.blend = 0.0f;
        ApplyStroke(v, origin, bite);
    }
    const auto jagged = v.field;

    // Act
    VoxelStroke polish;
    polish.a = Vector3(10, 10, 6);
    polish.b = Vector3(10, 10, 13);
    polish.radiusA = polish.radiusB = 3.0f;
    polish.mode = VoxelEditMode::Smooth;
    const EditRegion r = ApplyStroke(v, origin, polish);

    // Assert: it changed something, and it stayed a finite field. An in-place
    // average would read values this same pass had already written, which
    // smears the shape along the iteration order instead of softening it.
    ENJIN_ASSERT_FALSE(r.Empty());
    ENJIN_EXPECT_FALSE(v.field == jagged);
    for (f32 value : v.field) {
        ENJIN_EXPECT_TRUE(std::isfinite(value));
        ENJIN_EXPECT_TRUE(std::fabs(value) <= v.Band() + 0.001f);
    }
}

ENJIN_TEST(VoxelEdit, ACarvedVolumeMeshesToAClosedSolid) {
    // Arrange
    ECS::VoxelVolumeComponent v = SolidRock(24, 0.5f);
    const Vector3 origin(0, 0, 0);
    VoxelStroke passage;
    passage.a = Vector3(2, 6, 6);
    passage.b = Vector3(9, 6, 6);
    passage.radiusA = 1.0f;
    passage.radiusB = 2.0f;
    passage.roughness = 0.2f;

    VoxelStroke chamber;
    chamber.a = chamber.b = Vector3(9, 6, 6);
    chamber.radiusA = chamber.radiusB = 2.5f;
    chamber.roughness = 0.2f;

    // Act
    ApplyStroke(v, origin, passage);
    ApplyStroke(v, origin, chamber);
    const SurfaceMesh m = BuildSurfaceNet(ToGrid(v, origin), 0.0f);

    // Assert: a real cave that is still one watertight solid. An open shell
    // here is a collider you fall out through.
    ENJIN_ASSERT_TRUE(!m.Empty());
    ENJIN_EXPECT_EQ(NonManifoldEdges(m), (usize)0);
}

ENJIN_TEST(VoxelEdit, BakingAFieldReplacesTheWholeVolume) {
    // Arrange: this is how a terrain, some flats and a tunnel become ONE
    // volume that can then be carved as itself.
    ECS::VoxelVolumeComponent v;
    v.dimX = v.dimY = v.dimZ = 16;
    v.voxelSize = 1.0f;
    const Vector3 origin(0, 0, 0);

    // Act: solid below y = 8, air above, with a chamber hollowed in the middle.
    BakeField(v, origin, [](const Vector3& p) {
        const f32 ground = SdfHeightfield(p, 8.0f);
        const f32 chamber = SdfSphere(p, Vector3(8, 5, 8), 2.5f);
        return SdfSmoothSubtract(ground, chamber, 0.5f);
    });

    // Assert
    ENJIN_EXPECT_EQ(v.field.size(), v.Count());
    ENJIN_EXPECT_TRUE(IsSolid(v, 8, 2, 8));        // rock down low
    ENJIN_EXPECT_FALSE(IsSolid(v, 8, 12, 8));      // air up high
    ENJIN_EXPECT_FALSE(IsSolid(v, 8, 5, 8));       // the chamber is hollow
    ENJIN_EXPECT_TRUE(IsSolid(v, 2, 5, 2));        // and rock beside it
    ENJIN_EXPECT_TRUE(v.meshDirty);
}

ENJIN_TEST(VoxelVolume, TheVolumeIsCentredOnItsTransform) {
    // Arrange
    ECS::VoxelVolumeComponent v;
    v.dimX = v.dimY = v.dimZ = 11;
    v.voxelSize = 2.0f;

    // Act
    const Vector3 o = v.GridOrigin(Vector3(100, 50, -20));

    // Assert: extent is (11-1)*2 = 20, so the origin sits 10 below the centre
    // on each axis. The terrain tools once assumed the corner instead and every
    // stroke landed half a terrain from the cursor.
    ENJIN_EXPECT_FLOAT_NEAR(o.x, 90.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(o.y, 40.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(o.z, -30.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(v.Extent().x, 20.0f, 0.001f);
}

ENJIN_TEST_MAIN()
