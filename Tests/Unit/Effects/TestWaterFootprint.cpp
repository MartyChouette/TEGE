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
#include <algorithm>
#include <cmath>
#include <vector>
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

// ---------------------------------------------------------------- surface

// A U whose centroid sits in the notch, outside the water. The surface used to
// be a fan from that centroid, so it covered the notch and double-covered the
// arms: water drawn across dry ground.
BoundaryPolygonComponent MakeUShape() {
    BoundaryPolygonComponent b;
    b.points = {
        {-10.0f, -10.0f}, {10.0f, -10.0f}, {10.0f, 10.0f}, {5.0f, 10.0f},
        {5.0f, -5.0f}, {-5.0f, -5.0f}, {-5.0f, 10.0f}, {-10.0f, 10.0f},
    };
    return b;
}

ENJIN_TEST(WaterFootprint, SurfaceCoversTheShapeAndNothingElse) {
    // Arrange: area 400 - 10 x 15 = 250.
    const BoundaryPolygonComponent b = MakeUShape();
    std::vector<Math::Vector2> pos; std::vector<f32> shore; std::vector<u32> idx;

    // Act
    const bool ok = BoundaryPolygonComponent::BuildSurface(b.points, 2.0f, pos, shore, idx);

    // Assert: the triangles add up to the shape, and every one lies inside it.
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_ASSERT_TRUE(idx.size() % 3 == 0 && !idx.empty());
    f32 area = 0.0f;
    bool allInside = true;
    for (usize t = 0; t < idx.size(); t += 3) {
        const auto& p0 = pos[idx[t]]; const auto& p1 = pos[idx[t + 1]]; const auto& p2 = pos[idx[t + 2]];
        area += std::fabs((p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x)) * 0.5f;
        const f32 cx = (p0.x + p1.x + p2.x) / 3.0f, cz = (p0.y + p1.y + p2.y) / 3.0f;
        if (!b.ContainsXZ(kOrigin, cx, cz)) allInside = false;
    }
    ENJIN_EXPECT_TRUE(std::fabs(area - 250.0f) < 0.01f);
    ENJIN_EXPECT_TRUE(allInside);
}

ENJIN_TEST(WaterFootprint, SurfaceShoreIsZeroOnTheRimAndOneInside) {
    // Arrange
    const BoundaryPolygonComponent b = MakeUShape();
    std::vector<Math::Vector2> pos; std::vector<f32> shore; std::vector<u32> idx;

    // Act
    BoundaryPolygonComponent::BuildSurface(b.points, 2.0f, pos, shore, idx);

    // Assert: the ring keeps its own points first, on the shore; the foam
    // gradient needs vertices away from it, so the split must have made some.
    ENJIN_ASSERT_TRUE(pos.size() > b.points.size());
    for (usize i = 0; i < b.points.size(); ++i) ENJIN_EXPECT_TRUE(shore[i] == 0.0f);
    f32 top = 0.0f;
    for (f32 s : shore) top = std::max(top, s);
    ENJIN_EXPECT_TRUE(top == 1.0f);
}

// True when some vertex sits strictly inside some triangle edge: a T-junction,
// where one side of the edge has a vertex the other side does not.
static bool HasTJunction(const std::vector<Math::Vector2>& pos, const std::vector<u32>& idx) {
    for (usize t = 0; t < idx.size(); t += 3) {
        for (int e = 0; e < 3; ++e) {
            const Math::Vector2 a = pos[idx[t + e]], b = pos[idx[t + (e + 1) % 3]];
            const f32 dx = b.x - a.x, dy = b.y - a.y, len2 = dx * dx + dy * dy;
            for (usize v = 0; v < pos.size(); ++v) {
                if (v == idx[t + e] || v == idx[t + (e + 1) % 3]) continue;
                const f32 px = pos[v].x - a.x, py = pos[v].y - a.y;
                if (std::fabs(dx * py - dy * px) > 1e-5f * std::sqrt(len2)) continue;
                const f32 along = px * dx + py * dy;
                if (along > 1e-5f * len2 && along < len2 * (1.0f - 1e-5f)) return true;
            }
        }
    }
    return false;
}

ENJIN_TEST(WaterFootprint, SurfaceSplitsLeaveNoTJunctions) {
    // A long thin ear beside a small one, sharing a short edge. Splitting by
    // triangle cut that short edge on the long side only, which put seven
    // vertices in the middle of an edge the small side kept whole.
    const std::vector<Math::Vector2> sliver = {{0, 0}, {10, 0}, {0, 1}, {-0.5f, 0.5f}};
    std::vector<Math::Vector2> pos; std::vector<f32> shore; std::vector<u32> idx;
    ENJIN_ASSERT_TRUE(BoundaryPolygonComponent::BuildSurface(sliver, 2.0f, pos, shore, idx));
    ENJIN_EXPECT_FALSE(HasTJunction(pos, idx));

    ENJIN_ASSERT_TRUE(BoundaryPolygonComponent::BuildSurface(MakeUShape().points, 2.0f, pos, shore, idx));
    ENJIN_EXPECT_FALSE(HasTJunction(pos, idx));
}

ENJIN_TEST(WaterFootprint, SurfaceRefusesARingWithNoArea) {
    std::vector<Math::Vector2> line = {{0, 0}, {1, 0}, {2, 0}};
    std::vector<Math::Vector2> pos; std::vector<f32> shore; std::vector<u32> idx;
    ENJIN_EXPECT_FALSE(BoundaryPolygonComponent::BuildSurface(line, 2.0f, pos, shore, idx));
    ENJIN_EXPECT_TRUE(idx.empty());
}

ENJIN_TEST_MAIN()
