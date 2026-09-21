// A lake dragged into a shape behaves like that shape.
//
// BoundaryPolygonComponent has been in the engine for a while and only the
// RENDERER ever read it. Drag a lake into a kidney with the viewport handles and
// the surface triangulated into a kidney, while ControllerSystem's swim test,
// ControllerSystem's float test and JoltBackend's buoyancy all went on asking
// the halfExtents box the ring was seeded from. Three copies of the footprint
// rule, written out inline, none of which knew the shape existed.
//
// So you swam in the dry corners of a concave lake, and dropped props bobbed on
// the deck beside it. These tests are on the rule itself, which is pure: no
// World, no viewport, no GPU.
#include "EnjinTest.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/BoundaryPolygon.h"

using namespace Enjin;
using namespace Enjin::ECS;

namespace {

const Math::Vector3 kOrigin(0.0f, 0.0f, 0.0f);

WaterVolumeComponent MakeVolume(f32 hx = 10.0f, f32 hy = 2.0f, f32 hz = 10.0f) {
    WaterVolumeComponent v;
    v.halfExtents = Math::Vector3(hx, hy, hz);
    return v;
}

// An L, which is the simplest shape whose bounding box contains a point the
// shape does not. The missing quadrant is +X/+Z.
BoundaryPolygonComponent MakeLShape() {
    BoundaryPolygonComponent b;
    b.points = {
        {-10.0f, -10.0f},
        { 10.0f, -10.0f},
        { 10.0f,   0.0f},
        {  0.0f,   0.0f},
        {  0.0f,  10.0f},
        {-10.0f,  10.0f},
    };
    return b;
}

} // namespace

// ---------------------------------------------------------------- no outline

ENJIN_TEST(WaterFootprint, NoOutlineIsTheBox) {
    // Arrange: every scene authored before the outline existed passes null here.
    const WaterVolumeComponent v = MakeVolume();

    // Act / Assert
    ENJIN_EXPECT_TRUE(v.FootprintContainsXZ(kOrigin, nullptr, 0.0f, 0.0f));
    ENJIN_EXPECT_TRUE(v.FootprintContainsXZ(kOrigin, nullptr, 9.9f, 9.9f));
    ENJIN_EXPECT_FALSE(v.FootprintContainsXZ(kOrigin, nullptr, 10.1f, 0.0f));
    ENJIN_EXPECT_FALSE(v.FootprintContainsXZ(kOrigin, nullptr, 0.0f, -10.1f));
}

ENJIN_TEST(WaterFootprint, OutlineTooSmallToBeARingIsIgnored) {
    // Arrange: two points is a line, not a shape. RenderSystem takes the plain
    // plane path below three, so the footprint has to agree or the water would
    // render as a rectangle nothing could swim in.
    const WaterVolumeComponent v = MakeVolume();
    BoundaryPolygonComponent b;
    b.points = { {-1.0f, -1.0f}, {1.0f, 1.0f} };

    // Act / Assert
    ENJIN_EXPECT_TRUE(v.FootprintContainsXZ(kOrigin, &b, 5.0f, 5.0f));
    ENJIN_EXPECT_FALSE(v.FootprintContainsXZ(kOrigin, &b, 50.0f, 0.0f));
}

// ---------------------------------------------------------------- shapes

ENJIN_TEST(WaterFootprint, RectangularOutlineMatchesTheBox) {
    // Arrange: the ring the Lake tool seeds from the drag is the box itself, so
    // adding an outline must not move the shoreline before anyone bends it.
    const WaterVolumeComponent v = MakeVolume();
    BoundaryPolygonComponent b;
    b.points = { {-10.0f, -10.0f}, {10.0f, -10.0f}, {10.0f, 10.0f}, {-10.0f, 10.0f} };

    // Act / Assert
    ENJIN_EXPECT_TRUE(v.FootprintContainsXZ(kOrigin, &b, 0.0f, 0.0f));
    ENJIN_EXPECT_TRUE(v.FootprintContainsXZ(kOrigin, &b, 9.5f, 9.5f));
    ENJIN_EXPECT_FALSE(v.FootprintContainsXZ(kOrigin, &b, 10.5f, 0.0f));
}

ENJIN_TEST(WaterFootprint, ConcaveOutlineRejectsTheMissingCorner) {
    // Arrange: THE bug. (5, 5) is inside the bounding box and outside the L.
    const WaterVolumeComponent v = MakeVolume();
    const BoundaryPolygonComponent b = MakeLShape();

    // Act / Assert
    ENJIN_EXPECT_TRUE(v.FootprintContainsXZ(kOrigin, &b, -5.0f, -5.0f));   // in the L
    ENJIN_EXPECT_TRUE(v.FootprintContainsXZ(kOrigin, &b,  5.0f, -5.0f));   // in the L
    ENJIN_EXPECT_TRUE(v.FootprintContainsXZ(kOrigin, &b, -5.0f,  5.0f));   // in the L
    ENJIN_EXPECT_FALSE(v.FootprintContainsXZ(kOrigin, &b,  5.0f,  5.0f));  // the bite
}

ENJIN_TEST(WaterFootprint, BoxStillClipsAnOutlineThatReachesPastIt) {
    // Arrange: a ring dragged wider than the halfExtents it was seeded from.
    // Both have to hold, or the AABB stops being a valid broadphase for the
    // volume and buoyancy would skip water it should have found.
    const WaterVolumeComponent v = MakeVolume(4.0f, 2.0f, 4.0f);
    BoundaryPolygonComponent b;
    b.points = { {-20.0f, -20.0f}, {20.0f, -20.0f}, {20.0f, 20.0f}, {-20.0f, 20.0f} };

    // Act / Assert
    ENJIN_EXPECT_TRUE(v.FootprintContainsXZ(kOrigin, &b, 3.0f, 3.0f));
    ENJIN_EXPECT_FALSE(v.FootprintContainsXZ(kOrigin, &b, 10.0f, 0.0f));
}

ENJIN_TEST(WaterFootprint, OutlineFollowsTheEntity) {
    // Arrange: points are LOCAL offsets, so moving the lake moves its shape with
    // it. Testing them as world coordinates would leave the shoreline behind at
    // the origin the moment anyone dragged the entity.
    const WaterVolumeComponent v = MakeVolume();
    const BoundaryPolygonComponent b = MakeLShape();
    const Math::Vector3 moved(100.0f, 0.0f, 100.0f);

    // Act / Assert
    ENJIN_EXPECT_TRUE(v.FootprintContainsXZ(moved, &b, 95.0f, 95.0f));
    ENJIN_EXPECT_FALSE(v.FootprintContainsXZ(moved, &b, 105.0f, 105.0f));  // the bite
    ENJIN_EXPECT_FALSE(v.FootprintContainsXZ(moved, &b, 5.0f, 5.0f));      // where it was
}

// ---------------------------------------------------------------- depth

ENJIN_TEST(WaterFootprint, ContainsPointStillTestsDepth) {
    // Arrange: the shape is horizontal; how deep the water goes is still
    // halfExtents.y, and ContainsPoint owes both answers.
    const WaterVolumeComponent v = MakeVolume(10.0f, 2.0f, 10.0f);
    const BoundaryPolygonComponent b = MakeLShape();

    // Act / Assert
    ENJIN_EXPECT_TRUE(v.ContainsPoint(kOrigin, Math::Vector3(-5.0f, -1.0f, -5.0f), &b));
    ENJIN_EXPECT_FALSE(v.ContainsPoint(kOrigin, Math::Vector3(-5.0f, -5.0f, -5.0f), &b));  // below
    ENJIN_EXPECT_FALSE(v.ContainsPoint(kOrigin, Math::Vector3(5.0f, -1.0f, 5.0f), &b));    // the bite
}

ENJIN_TEST_MAIN()
