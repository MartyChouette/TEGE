// Creative mode's build tools: what a drag on the ground actually makes.
//
// BuildBrushes is deliberately pure -- a drag and some numbers in, a brush list
// out -- so the whole behaviour of the build tools is checkable here without a
// viewport, a camera, ImGui or a GPU. Everything below is about geometry a
// person would notice being wrong: a wall that is not as long as the line they
// drew, a floor they stand inside instead of on, a staircase you fall through.
#include "EnjinTest.h"
#include "Enjin/Editor/CreativeMode.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Math;
using namespace Enjin::Editor;

namespace {

ECS::BrushSolidComponent Build(BuildTool tool, const BuildToolSettings& s,
                               bool subtract, const Vector3& a, const Vector3& b,
                               bool* ok = nullptr) {
    ECS::BrushSolidComponent out;
    const bool built = CreativeMode::BuildBrushes(tool, s, subtract, a, b, out);
    if (ok) *ok = built;
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Wall
// ---------------------------------------------------------------------------

ENJIN_TEST(CreativeMode, AWallIsAsLongAsTheLineYouDrew) {
    BuildToolSettings s;
    s.height = 3.0f;
    s.thickness = 0.25f;

    // A 4 m drag along +X.
    ECS::BrushSolidComponent solid =
        Build(BuildTool::Wall, s, false, Vector3(0, 0, 0), Vector3(4, 0, 0));

    ENJIN_ASSERT_EQ(solid.brushes.size(), (usize)1);
    const auto& b = solid.brushes[0];
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.x * 2.0f, 4.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.y * 2.0f, 3.0f, 0.001f);   // height
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.z * 2.0f, 0.25f, 0.001f);  // thickness
}

// The line you drag is the wall's FOOT. If the wall centred on the drag instead,
// half of every wall would be underground.
ENJIN_TEST(CreativeMode, AWallStandsOnTheLineRatherThanStraddlingIt) {
    BuildToolSettings s;
    s.height = 3.0f;

    ECS::BrushSolidComponent solid =
        Build(BuildTool::Wall, s, false, Vector3(0, 0, 0), Vector3(4, 0, 0));

    // Centre sits at half the height above the drag, so the base is at y = 0.
    ENJIN_EXPECT_FLOAT_NEAR(solid.brushes[0].center.y, 1.5f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(solid.brushes[0].center.x, 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(solid.brushes[0].center.z, 0.0f, 0.001f);
}

// A wall dragged diagonally has to follow the drag, not snap to an axis.
ENJIN_TEST(CreativeMode, ADiagonalWallKeepsItsLengthAndTurnsToFollowTheDrag) {
    BuildToolSettings s;
    ECS::BrushSolidComponent solid =
        Build(BuildTool::Wall, s, false, Vector3(0, 0, 0), Vector3(3, 0, 4));

    ENJIN_ASSERT_EQ(solid.brushes.size(), (usize)1);
    // 3-4-5 triangle: the wall is 5 m long however it is oriented.
    ENJIN_EXPECT_FLOAT_NEAR(solid.brushes[0].halfExtents.x * 2.0f, 5.0f, 0.001f);

    // And it is actually rotated, rather than left axis-aligned.
    const auto& q = solid.brushes[0].rotation;
    const bool identity = std::fabs(q.x) < 0.0001f && std::fabs(q.y) < 0.0001f &&
                          std::fabs(q.z) < 0.0001f;
    ENJIN_EXPECT_FALSE(identity);
}

// A click is not a drag. Building from one produces a solid with no width: it
// contributes no faces, renders nothing, and reads as a broken tool.
ENJIN_TEST(CreativeMode, AClickTooSmallToBeADragBuildsNothing) {
    BuildToolSettings s;
    bool ok = true;
    ECS::BrushSolidComponent solid =
        Build(BuildTool::Wall, s, false, Vector3(1, 0, 1), Vector3(1.001f, 0, 1.0f), &ok);

    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_EQ(solid.brushes.size(), (usize)0);
}

// ---------------------------------------------------------------------------
// Floor
// ---------------------------------------------------------------------------

ENJIN_TEST(CreativeMode, AFloorSpansTheDraggedRegion) {
    BuildToolSettings s;
    s.thickness = 0.20f;
    s.elevation = 0.0f;

    ECS::BrushSolidComponent solid =
        Build(BuildTool::Floor, s, false, Vector3(0, 0, 0), Vector3(6, 0, 4));

    ENJIN_ASSERT_EQ(solid.brushes.size(), (usize)1);
    const auto& b = solid.brushes[0];
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.x * 2.0f, 6.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.z * 2.0f, 4.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.center.x, 3.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.center.z, 2.0f, 0.001f);
}

// Elevation 0 has to mean a floor you stand ON at y=0, not one whose middle is
// at y=0 and that you stand inside up to the ankles.
ENJIN_TEST(CreativeMode, AFloorHangsBelowItsElevation) {
    BuildToolSettings s;
    s.thickness = 0.20f;
    s.elevation = 0.0f;

    ECS::BrushSolidComponent solid =
        Build(BuildTool::Floor, s, false, Vector3(0, 0, 0), Vector3(6, 0, 4));

    const auto& b = solid.brushes[0];
    const f32 top = b.center.y + b.halfExtents.y;
    ENJIN_EXPECT_FLOAT_NEAR(top, 0.0f, 0.001f);
}

ENJIN_TEST(CreativeMode, AFloorNeedsBothDimensions) {
    BuildToolSettings s;
    bool ok = true;
    // Dragged along one axis only: a line, not a region.
    Build(BuildTool::Floor, s, false, Vector3(0, 0, 0), Vector3(6, 0, 0), &ok);
    ENJIN_EXPECT_FALSE(ok);
}

// ---------------------------------------------------------------------------
// Stairs
// ---------------------------------------------------------------------------

ENJIN_TEST(CreativeMode, StairsGetOneTreadPerRunLength) {
    BuildToolSettings s;
    s.rise = 0.18f;
    s.run  = 0.25f;
    s.width = 1.2f;

    // 2 m of run at 0.25 m per step.
    ECS::BrushSolidComponent solid =
        Build(BuildTool::Stairs, s, false, Vector3(0, 0, 0), Vector3(2, 0, 0));

    ENJIN_EXPECT_EQ(solid.brushes.size(), (usize)8);
}

// Each tread is solid from the ground up, so the run has a side and a character
// cannot fall between floating slabs.
ENJIN_TEST(CreativeMode, EachTreadIsTallerThanTheOneBeforeAndReachesTheGround) {
    BuildToolSettings s;
    s.rise = 0.20f;
    s.run  = 0.50f;

    ECS::BrushSolidComponent solid =
        Build(BuildTool::Stairs, s, false, Vector3(0, 0, 0), Vector3(2, 0, 0));
    ENJIN_ASSERT_EQ(solid.brushes.size(), (usize)4);

    f32 previousTop = 0.0f;
    for (const auto& tread : solid.brushes) {
        const f32 bottom = tread.center.y - tread.halfExtents.y;
        const f32 top    = tread.center.y + tread.halfExtents.y;
        ENJIN_EXPECT_FLOAT_NEAR(bottom, 0.0f, 0.001f);   // reaches the ground
        ENJIN_EXPECT_TRUE(top > previousTop);            // and climbs
        previousTop = top;
    }
    // Four steps at 0.20 m arrive at 0.80 m.
    ENJIN_EXPECT_FLOAT_NEAR(previousTop, 0.80f, 0.001f);
}

// Without a ceiling, a long drag with a small run asks for tens of thousands of
// brushes and the CSG build stops being interactive.
ENJIN_TEST(CreativeMode, AVeryLongStairRunIsCapped) {
    BuildToolSettings s;
    s.run = 0.01f;

    ECS::BrushSolidComponent solid =
        Build(BuildTool::Stairs, s, false, Vector3(0, 0, 0), Vector3(10000, 0, 0));

    ENJIN_EXPECT_EQ(solid.brushes.size(), (usize)kCreativeMaxStairSteps);
}

ENJIN_TEST(CreativeMode, AStairDragShorterThanOneStepStillMakesAStep) {
    BuildToolSettings s;
    s.run = 1.0f;
    ECS::BrushSolidComponent solid =
        Build(BuildTool::Stairs, s, false, Vector3(0, 0, 0), Vector3(0.3f, 0, 0));
    ENJIN_EXPECT_EQ(solid.brushes.size(), (usize)1);
}

// ---------------------------------------------------------------------------
// Brush
// ---------------------------------------------------------------------------

ENJIN_TEST(CreativeMode, FourSidesMakesABoxAndMoreMakesAPrism) {
    BuildToolSettings s;
    s.sides = 4;
    ECS::BrushSolidComponent box =
        Build(BuildTool::Brush, s, false, Vector3(0, 0, 0), Vector3(2, 0, 2));
    ENJIN_ASSERT_EQ(box.brushes.size(), (usize)1);
    ENJIN_EXPECT_EQ((int)box.brushes[0].shape, (int)ECS::BrushSolidComponent::Shape::Box);

    s.sides = 8;
    ECS::BrushSolidComponent prism =
        Build(BuildTool::Brush, s, false, Vector3(0, 0, 0), Vector3(2, 0, 2));
    ENJIN_ASSERT_EQ(prism.brushes.size(), (usize)1);
    ENJIN_EXPECT_EQ((int)prism.brushes[0].shape, (int)ECS::BrushSolidComponent::Shape::Prism);
    ENJIN_EXPECT_EQ(prism.brushes[0].sides, (u32)8);
}

// A prism is round, so it takes one radius. Taking the larger half-span would
// make the shape spill out of the rectangle the person just dragged.
ENJIN_TEST(CreativeMode, APrismFitsInsideTheDragRatherThanOverflowingIt) {
    BuildToolSettings s;
    s.sides = 12;
    ECS::BrushSolidComponent solid =
        Build(BuildTool::Brush, s, false, Vector3(0, 0, 0), Vector3(6, 0, 2));

    // Smaller span is 2, so the radius is 1.
    ENJIN_EXPECT_FLOAT_NEAR(solid.brushes[0].radius, 1.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Cutting
// ---------------------------------------------------------------------------

ENJIN_TEST(CreativeMode, SubtractingProducesCutBrushesNotAddedOnes) {
    BuildToolSettings s;
    ECS::BrushSolidComponent added =
        Build(BuildTool::Wall, s, false, Vector3(0, 0, 0), Vector3(4, 0, 0));
    ECS::BrushSolidComponent cut =
        Build(BuildTool::Wall, s, true, Vector3(0, 0, 0), Vector3(4, 0, 0));

    ENJIN_EXPECT_EQ((int)added.brushes[0].op, (int)Geometry::BrushOp::Add);
    ENJIN_EXPECT_EQ((int)cut.brushes[0].op, (int)Geometry::BrushOp::Subtract);
}

ENJIN_TEST(CreativeMode, EveryTreadOfASubtractedStaircaseCuts) {
    BuildToolSettings s;
    s.run = 0.5f;
    ECS::BrushSolidComponent solid =
        Build(BuildTool::Stairs, s, true, Vector3(0, 0, 0), Vector3(2, 0, 0));

    ENJIN_ASSERT_TRUE(solid.brushes.size() > 1);
    for (const auto& tread : solid.brushes) {
        ENJIN_EXPECT_EQ((int)tread.op, (int)Geometry::BrushOp::Subtract);
    }
}

// ---------------------------------------------------------------------------
// Which tools build at all
// ---------------------------------------------------------------------------

ENJIN_TEST(CreativeMode, ToolsThatActOnComponentsBuildNoBrushes) {
    BuildToolSettings s;
    const BuildTool notBrushes[] = {
        BuildTool::Water, BuildTool::Terrain,
        BuildTool::Ladder, BuildTool::Reduce
    };
    for (BuildTool tool : notBrushes) {
        bool ok = true;
        ECS::BrushSolidComponent solid =
            Build(tool, s, false, Vector3(0, 0, 0), Vector3(4, 0, 4), &ok);
        ENJIN_EXPECT_FALSE(ok);
        ENJIN_EXPECT_EQ(solid.brushes.size(), (usize)0);
        ENJIN_EXPECT_FALSE(BuildToolMakesBrushes(tool));
    }
}

// A tool with no second mode must not show a toggle that does nothing.
ENJIN_TEST(CreativeMode, OnlyBrushToolsOfferSubtract) {
    ENJIN_EXPECT_TRUE(BuildToolCanSubtract(BuildTool::Wall));
    ENJIN_EXPECT_TRUE(BuildToolCanSubtract(BuildTool::Brush));
    ENJIN_EXPECT_FALSE(BuildToolCanSubtract(BuildTool::Ladder));
    ENJIN_EXPECT_FALSE(BuildToolCanSubtract(BuildTool::Reduce));
}

ENJIN_TEST(CreativeMode, EveryToolHasANameAndAVerb) {
    for (u8 i = 0; i < static_cast<u8>(BuildTool::Count); ++i) {
        const BuildTool tool = static_cast<BuildTool>(i);
        ENJIN_EXPECT_TRUE(BuildToolName(tool)[0] != '\0');
        // The verb is what tells someone with no AI in the loop what the tool
        // does. A blank one is a tool that explains itself to nobody.
        ENJIN_EXPECT_TRUE(BuildToolVerb(tool)[0] != '\0');
    }
}

// ---------------------------------------------------------------------------
// Mode state
// ---------------------------------------------------------------------------

ENJIN_TEST(CreativeMode, SwitchingToAToolThatCannotCutClearsCutting) {
    CreativeMode mode;
    mode.SetTool(BuildTool::Wall);
    mode.SetSubtracting(true);
    ENJIN_ASSERT_TRUE(mode.IsSubtracting());

    mode.SetTool(BuildTool::Ladder);
    // Otherwise the surface shows the cut colour for a tool that only ever adds.
    ENJIN_EXPECT_FALSE(mode.IsSubtracting());
}

ENJIN_TEST(CreativeMode, CuttingCannotBeTurnedOnForAToolThatCannotCut) {
    CreativeMode mode;
    mode.SetTool(BuildTool::Reduce);
    mode.SetSubtracting(true);
    ENJIN_EXPECT_FALSE(mode.IsSubtracting());
}

// Numbers are kept per tool: dialling a wall to 4 m and going to stairs and back
// must not quietly reset it.
ENJIN_TEST(CreativeMode, EachToolRemembersItsOwnNumbers) {
    CreativeMode mode;
    mode.SetTool(BuildTool::Wall);
    mode.CurrentSettings().height = 4.0f;

    mode.SetTool(BuildTool::Stairs);
    mode.CurrentSettings().rise = 0.22f;

    mode.SetTool(BuildTool::Wall);
    ENJIN_EXPECT_FLOAT_NEAR(mode.CurrentSettings().height, 4.0f, 0.001f);
    mode.SetTool(BuildTool::Stairs);
    ENJIN_EXPECT_FLOAT_NEAR(mode.CurrentSettings().rise, 0.22f, 0.001f);
}

ENJIN_TEST(CreativeMode, SnapRoundsToTheGridAndLeavesHeightAlone) {
    CreativeMode mode;
    mode.SetGridSize(0.25f);
    mode.SetSnapEnabled(true);

    const Vector3 snapped = mode.SnapToGrid(Vector3(1.06f, 2.37f, -0.61f));
    ENJIN_EXPECT_FLOAT_NEAR(snapped.x, 1.00f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(snapped.z, -0.50f, 0.001f);
    // Y is the tool's business (elevation, height), not the floor grid's.
    ENJIN_EXPECT_FLOAT_NEAR(snapped.y, 2.37f, 0.001f);
}

ENJIN_TEST(CreativeMode, SnapCanBeTurnedOff) {
    CreativeMode mode;
    mode.SetSnapEnabled(false);
    const Vector3 free = mode.SnapToGrid(Vector3(1.06f, 0.0f, -0.61f));
    ENJIN_EXPECT_FLOAT_NEAR(free.x, 1.06f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(free.z, -0.61f, 0.001f);
}

// A zero or negative grid would divide by zero or snap everything onto a single
// point, and it is reachable from a settings field.
ENJIN_TEST(CreativeMode, AnUnusableGridSizeFallsBackToTheDefault) {
    CreativeMode mode;
    mode.SetGridSize(0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(mode.GetGridSize(), kCreativeGridDefault, 0.0001f);
    mode.SetGridSize(-4.0f);
    ENJIN_EXPECT_FLOAT_NEAR(mode.GetGridSize(), kCreativeGridDefault, 0.0001f);
}

ENJIN_TEST_MAIN()
