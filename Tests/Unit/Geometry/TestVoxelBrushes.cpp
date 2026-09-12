// The authoring brushes, and aiming at rock.
//
// Marty, having used it: "the way caves currently work is a bit hard to dig and
// other we neeed a feew wdiffeerent tunneel authoring brushees".
//
// Two problems in one sentence. Strokes landed on the y = 0 build plane, so a
// dig could only run horizontally at ground height -- no shafts, no deepening a
// floor, no cutting into the wall in front of you. And one swept sphere carves
// one kind of hole, so a cave made entirely of them reads as plumbing however
// rough its walls are.
//
// What each brush MEANS is the thing worth testing: that a Shaft goes down,
// that a Chamber has no direction, that a Ramp descends. A brush that quietly
// did the same as Passage would still carve, still mesh, and still be useless.

#include "EnjinTest.h"
#include "Enjin/Geometry/VoxelEdit.h"
#include "Enjin/Geometry/SurfaceNets.h"
#include "Enjin/Geometry/Sdf.h"
#include "Enjin/ECS/Components/VoxelVolume.h"

#include <cmath>
#include <cstring>

using namespace Enjin;
using namespace Enjin::Geometry;
using Enjin::Math::Vector3;

namespace {

ECS::VoxelVolumeComponent SolidRock(u32 n = 32, f32 voxel = 1.0f) {
    ECS::VoxelVolumeComponent v;
    v.dimX = v.dimY = v.dimZ = n;
    v.voxelSize = voxel;
    v.field.assign(v.Count(), -v.Band());
    return v;
}

bool IsSolid(const ECS::VoxelVolumeComponent& v, u32 x, u32 y, u32 z) {
    return v.At(x, y, z) < 0.0f;
}

BrushGesture Drag(const Vector3& from, const Vector3& to, f32 bore = 2.0f, f32 depth = 6.0f) {
    BrushGesture g;
    g.from = from;
    g.to = to;
    g.bore = bore;
    g.depth = depth;
    g.roughness = 0.0f;   // shape claims, not noise claims
    g.blend = 0.0f;
    return g;
}

} // namespace

// --------------------------------------------------------------------------
// What each brush means
// --------------------------------------------------------------------------

ENJIN_TEST(VoxelBrushes, EveryBrushHasAName) {
    // Arrange / Act / Assert: a rail entry with no name is a blank button.
    for (u8 i = 0; i < static_cast<u8>(VoxelBrush::Count); ++i) {
        const char* name = VoxelBrushName(static_cast<VoxelBrush>(i));
        ENJIN_EXPECT_TRUE(name != nullptr);
        ENJIN_EXPECT_TRUE(std::strlen(name) > 0);
        ENJIN_EXPECT_FALSE(std::strcmp(name, "Unknown") == 0);
    }
}

ENJIN_TEST(VoxelBrushes, APassageRunsAlongTheDrag) {
    // Arrange
    const BrushGesture g = Drag(Vector3(0, 10, 0), Vector3(10, 10, 0));

    // Act
    const VoxelStroke s = MakeBrushStroke(VoxelBrush::Passage, g);

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(s.a.x, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.b.x, 10.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.a.y, s.b.y, 0.001f);   // level
    ENJIN_EXPECT_FLOAT_NEAR(s.heightScale, 1.0f, 0.001f);
}

ENJIN_TEST(VoxelBrushes, AChamberHasNoDirectionAndTheDragSetsItsSize) {
    // Arrange: the same start, dragged two different ways.
    const BrushGesture small = Drag(Vector3(5, 10, 5), Vector3(8, 10, 5));
    const BrushGesture big = Drag(Vector3(5, 10, 5), Vector3(5, 10, 17));

    // Act
    const VoxelStroke a = MakeBrushStroke(VoxelBrush::Chamber, small);
    const VoxelStroke b = MakeBrushStroke(VoxelBrush::Chamber, big);

    // Assert: both are spheres at the point aimed at -- a room has no
    // direction -- and the longer drag makes the bigger room.
    ENJIN_EXPECT_FLOAT_NEAR(a.a.x, a.b.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(a.a.y, a.b.y, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(a.a.z, a.b.z, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(a.a.x, 5.0f, 0.001f);
    ENJIN_EXPECT_TRUE(b.radiusA > a.radiusA);
    ENJIN_EXPECT_FLOAT_NEAR(b.radiusA, 12.0f, 0.001f);
}

ENJIN_TEST(VoxelBrushes, AChamberNeverShrinksBelowTheBore) {
    // Arrange: a click, not a drag.
    const BrushGesture click = Drag(Vector3(5, 10, 5), Vector3(5, 10, 5), 3.0f);

    // Act
    const VoxelStroke s = MakeBrushStroke(VoxelBrush::Chamber, click);

    // Assert: a click still makes a room, rather than a point that carves
    // nothing and looks like the tool ignoring you.
    ENJIN_EXPECT_FLOAT_NEAR(s.radiusA, 3.0f, 0.001f);
}

ENJIN_TEST(VoxelBrushes, AShaftGoesStraightDownFromWhereItWasAimed) {
    // Arrange: a drag sideways, which a Shaft must ignore.
    const BrushGesture g = Drag(Vector3(5, 20, 5), Vector3(15, 20, 9), 2.0f, 8.0f);

    // Act
    const VoxelStroke s = MakeBrushStroke(VoxelBrush::Shaft, g);

    // Assert: same x and z at both ends, and eight metres lower at the bottom.
    // This is the gesture a ground-plane drag cannot express at all.
    ENJIN_EXPECT_FLOAT_NEAR(s.a.x, s.b.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.a.z, s.b.z, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.a.x, 5.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.a.y - s.b.y, 8.0f, 0.001f);
}

ENJIN_TEST(VoxelBrushes, ARampDescendsAsItRuns) {
    // Arrange
    const BrushGesture g = Drag(Vector3(0, 20, 0), Vector3(12, 20, 0), 2.0f, 5.0f);

    // Act
    const VoxelStroke s = MakeBrushStroke(VoxelBrush::Ramp, g);

    // Assert: it goes along AND down, which is what lets a cave reach another
    // level instead of staying on one.
    ENJIN_EXPECT_FLOAT_NEAR(s.b.x, 12.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(s.a.y - s.b.y, 5.0f, 0.001f);
    // Narrower at the bottom, so it reads as going somewhere.
    ENJIN_EXPECT_TRUE(s.radiusB < s.radiusA);
}

ENJIN_TEST(VoxelBrushes, ACrackIsSquashedAndTheOthersAreNot) {
    // Arrange
    const BrushGesture g = Drag(Vector3(0, 10, 0), Vector3(10, 10, 0));

    // Act / Assert
    ENJIN_EXPECT_TRUE(MakeBrushStroke(VoxelBrush::Crack, g).heightScale < 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(MakeBrushStroke(VoxelBrush::Passage, g).heightScale, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(MakeBrushStroke(VoxelBrush::Shaft, g).heightScale, 1.0f, 0.001f);
}

ENJIN_TEST(VoxelBrushes, NoTwoBrushesCarveTheSameHoleFromTheSameDrag) {
    // Arrange: the failure this catches is a brush that quietly behaves like
    // Passage. It would still carve, still mesh, and still be a rail entry that
    // does nothing new.
    const Vector3 origin(0, 0, 0);
    const BrushGesture g = Drag(Vector3(10, 20, 10), Vector3(20, 20, 14), 2.0f, 7.0f);

    std::vector<std::vector<f32>> results;
    for (u8 i = 0; i < static_cast<u8>(VoxelBrush::Count); ++i) {
        ECS::VoxelVolumeComponent v = SolidRock();
        ApplyStroke(v, origin, MakeBrushStroke(static_cast<VoxelBrush>(i), g));
        results.push_back(v.field);
    }

    // Act / Assert
    for (usize i = 0; i < results.size(); ++i) {
        for (usize j = i + 1; j < results.size(); ++j) {
            ENJIN_EXPECT_FALSE(results[i] == results[j]);
        }
    }
}

ENJIN_TEST(VoxelBrushes, AShaftActuallyHollowsRockBelowTheAimPoint) {
    // Arrange
    ECS::VoxelVolumeComponent v = SolidRock();
    const Vector3 origin(0, 0, 0);

    // Act
    ApplyStroke(v, origin,
                MakeBrushStroke(VoxelBrush::Shaft,
                                Drag(Vector3(16, 28, 16), Vector3(16, 28, 16), 2.0f, 10.0f)));

    // Assert: open down the shaft, solid beside it. A brush whose stroke was
    // right but whose write went elsewhere would pass the stroke tests above
    // and fail here.
    ENJIN_EXPECT_FALSE(IsSolid(v, 16, 26, 16));
    ENJIN_EXPECT_FALSE(IsSolid(v, 16, 22, 16));
    ENJIN_EXPECT_FALSE(IsSolid(v, 16, 19, 16));
    ENJIN_EXPECT_TRUE(IsSolid(v, 16, 12, 16));    // below the bottom
    ENJIN_EXPECT_TRUE(IsSolid(v, 24, 22, 16));    // beside it
}

ENJIN_TEST(VoxelBrushes, ACrackReachesFurtherUpAndDownThanItsBore) {
    // Arrange
    ECS::VoxelVolumeComponent crack = SolidRock();
    ECS::VoxelVolumeComponent round = SolidRock();
    const Vector3 origin(0, 0, 0);
    const BrushGesture g = Drag(Vector3(8, 16, 16), Vector3(24, 16, 16), 2.0f);

    // Act
    ApplyStroke(crack, origin, MakeBrushStroke(VoxelBrush::Crack, g));
    ApplyStroke(round, origin, MakeBrushStroke(VoxelBrush::Passage, g));

    // Assert: four voxels above the axis is open in the fissure and still solid
    // in the round passage. The bounds have to allow for the squash or the
    // crack is clipped flat by a box the shape was never inside.
    ENJIN_EXPECT_FALSE(IsSolid(crack, 16, 20, 16));
    ENJIN_EXPECT_TRUE(IsSolid(round, 16, 20, 16));
    // And it is NARROWER sideways than the round one is.
    ENJIN_EXPECT_TRUE(IsSolid(crack, 16, 16, 19) || !IsSolid(round, 16, 16, 19));
}

// --------------------------------------------------------------------------
// Aiming
// --------------------------------------------------------------------------

ENJIN_TEST(VoxelAim, ARayFindsTheSurfaceOfTheRock) {
    // Arrange: solid below y = 16, air above.
    ECS::VoxelVolumeComponent v;
    v.dimX = v.dimY = v.dimZ = 32;
    v.voxelSize = 1.0f;
    const Vector3 origin(0, 0, 0);
    BakeField(v, origin, [](const Vector3& p) { return SdfHeightfield(p, 16.0f); });

    // Act: straight down from above.
    Vector3 hit;
    const bool found = RaycastVolume(v, origin, Vector3(16, 30, 16), Vector3(0, -1, 0), 100.0f, hit);

    // Assert: on the surface, within a step.
    ENJIN_ASSERT_TRUE(found);
    ENJIN_EXPECT_FLOAT_NEAR(hit.y, 16.0f, v.voxelSize);
    ENJIN_EXPECT_FLOAT_NEAR(hit.x, 16.0f, 0.001f);
}

ENJIN_TEST(VoxelAim, ARayPointingAtNothingMisses) {
    // Arrange
    ECS::VoxelVolumeComponent v;
    v.dimX = v.dimY = v.dimZ = 32;
    v.voxelSize = 1.0f;
    const Vector3 origin(0, 0, 0);
    BakeField(v, origin, [](const Vector3& p) { return SdfHeightfield(p, 16.0f); });

    // Act: upwards, into open sky.
    Vector3 hit;
    const bool found = RaycastVolume(v, origin, Vector3(16, 20, 16), Vector3(0, 1, 0), 100.0f, hit);

    // Assert: a miss, rather than a plausible point somewhere.
    ENJIN_EXPECT_FALSE(found);
}

ENJIN_TEST(VoxelAim, ARayFindsTheFarWallOfACaveFromInsideIt) {
    // Arrange: rock with a chamber hollowed in it, aiming from inside the
    // chamber at its wall. This is the gesture the ground plane could not
    // express -- standing in a cave and digging further in.
    ECS::VoxelVolumeComponent v = SolidRock(32, 1.0f);
    const Vector3 origin(0, 0, 0);
    ApplyStroke(v, origin,
                MakeBrushStroke(VoxelBrush::Chamber,
                                Drag(Vector3(16, 16, 16), Vector3(16, 16, 22), 2.0f)));

    // Act
    Vector3 hit;
    const bool found = RaycastVolume(v, origin, Vector3(16, 16, 16), Vector3(1, 0, 0), 60.0f, hit);

    // Assert: the wall of the chamber, not the outside of the block.
    ENJIN_ASSERT_TRUE(found);
    ENJIN_EXPECT_TRUE(hit.x > 16.0f);
    ENJIN_EXPECT_TRUE(hit.x < 26.0f);
}

ENJIN_TEST(VoxelAim, AnUncarvedVolumeCannotBeAimedAt) {
    // Arrange / Act / Assert: a volume with no field is air, and air has no
    // surface. Returning a hit here would let the first stroke of a scene land
    // on nothing.
    const ECS::VoxelVolumeComponent v;
    Vector3 hit;
    ENJIN_EXPECT_FALSE(RaycastVolume(v, Vector3(0, 0, 0), Vector3(0, 10, 0),
                                     Vector3(0, -1, 0), 100.0f, hit));
}

ENJIN_TEST_MAIN()
