// Creative mode's build tools: what a drag on the ground actually makes.
//
// BuildBrushes is deliberately pure -- a drag and some numbers in, a brush list
// out -- so the whole behaviour of the build tools is checkable here without a
// viewport, a camera, ImGui or a GPU. Everything below is about geometry a
// person would notice being wrong: a wall that is not as long as the line they
// drew, a floor they stand inside instead of on, a staircase you fall through.
#include "EnjinTest.h"
#include "Enjin/Editor/CreativeMode.h"

#include <cstring>

#include <cmath>
#include <vector>

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

ToolPlacement Plan(BuildTool tool, const BuildToolSettings& s,
                   const Vector3& a, const Vector3& b, bool* ok = nullptr) {
    ToolPlacement out;
    const bool planned = CreativeMode::PlanPlacement(tool, s, a, b, out);
    if (ok) *ok = planned;
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

// Sides had no control on the surface at all, so the engine could build a
// prism and a person could not ask it to. It is stored as a float to ride the
// same field row as every other number, which means the value that reaches the
// geometry has to be rounded and not truncated: a drag landing on 5.9 is
// someone asking for six sides, not five.
ENJIN_TEST(CreativeMode, SidesIsAnEditableFieldAndRoundsToWholeSides) {
    BuildToolSettings s;
    BuildField fields[kBuildMaxFields];
    const u32 count = BuildToolFields(BuildTool::Brush, s, fields, kBuildMaxFields);

    const BuildField* sidesField = nullptr;
    for (u32 i = 0; i < count; ++i) {
        if (fields[i].value == &s.sides) sidesField = &fields[i];
    }
    ENJIN_ASSERT_TRUE(sidesField != nullptr);
    // 4 is the box, so the range must start there rather than below it.
    ENJIN_EXPECT_FLOAT_NEAR(sidesField->minValue, 4.0f, 0.001f);
    ENJIN_EXPECT_TRUE(BuildToolSettings::FieldIsIntegral(sidesField->value, s));

    s.sides = 5.9f;
    ECS::BrushSolidComponent solid =
        Build(BuildTool::Brush, s, false, Vector3(0, 0, 0), Vector3(2, 0, 2));
    ENJIN_ASSERT_EQ(solid.brushes.size(), (usize)1);
    ENJIN_EXPECT_EQ(solid.brushes[0].sides, 6u);
}

// Only the fields where a fraction means nothing are integral. Marking a length
// integral would quantise walls to the metre.
ENJIN_TEST(CreativeMode, LengthsAreNotIntegralFields) {
    BuildToolSettings s;
    ENJIN_EXPECT_FALSE(BuildToolSettings::FieldIsIntegral(&s.height, s));
    ENJIN_EXPECT_FALSE(BuildToolSettings::FieldIsIntegral(&s.thickness, s));
    ENJIN_EXPECT_TRUE(BuildToolSettings::FieldIsIntegral(&s.sides, s));
}

// The grid sizes the surface offers must all be usable, and SetGridSize has a
// guard that falls back to the default for anything at or below zero.
ENJIN_TEST(CreativeMode, EveryOfferedGridSizeIsAcceptedAsGiven) {
    CreativeMode mode;
    for (u32 i = 0; i < kCreativeGridChoiceCount; ++i) {
        mode.SetGridSize(kCreativeGridChoices[i]);
        ENJIN_EXPECT_FLOAT_NEAR(mode.GetGridSize(), kCreativeGridChoices[i], 0.0001f);
    }
    // And the default is one of them, so the surface always shows a chip lit.
    bool defaultIsOffered = false;
    for (u32 i = 0; i < kCreativeGridChoiceCount; ++i) {
        if (std::fabs(kCreativeGridChoices[i] - kCreativeGridDefault) < 0.0001f) {
            defaultIsOffered = true;
        }
    }
    ENJIN_EXPECT_TRUE(defaultIsOffered);
}

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
        BuildTool::Plants, BuildTool::Prop,
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

// ---------------------------------------------------------------------------
// The tools that place components rather than brushes
// ---------------------------------------------------------------------------

// Water is a plane, and the plane is exactly the rectangle that was dragged.
ENJIN_TEST(CreativeMode, WaterCoversTheDraggedRectangle) {
    BuildToolSettings s;
    s.elevation = -1.5f;

    bool ok = false;
    const ToolPlacement p =
        Plan(BuildTool::Water, s, Vector3(-4, 0, -2), Vector3(6, 0, 8), &ok);

    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_FLOAT_NEAR(p.halfExtents.x * 2.0f, 10.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.halfExtents.z * 2.0f, 10.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.origin.x, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.origin.z, 3.0f, 0.001f);
}

// The Surface number is the height of the water, not an offset from the drag.
// Dragging on the ground and asking for water at -1.5 has to give water at
// -1.5, or a pond authored into a cut basin floats above it.
ENJIN_TEST(CreativeMode, WaterSitsAtTheSurfaceHeightYouSet) {
    BuildToolSettings s;
    s.elevation = -1.5f;

    const ToolPlacement p =
        Plan(BuildTool::Water, s, Vector3(0, 0, 0), Vector3(4, 0, 4));

    ENJIN_EXPECT_FLOAT_NEAR(p.origin.y, -1.5f, 0.001f);
}

// A line is not a rectangle. One side of zero has no surface to draw, the same
// way a floor with one dimension has no slab.
ENJIN_TEST(CreativeMode, WaterNeedsBothDimensions) {
    BuildToolSettings s;
    bool ok = true;
    Plan(BuildTool::Water, s, Vector3(0, 0, 0), Vector3(5, 0, 0), &ok);
    ENJIN_EXPECT_FALSE(ok);
}

// A ladder leans on a wall, so the drag gives it a width and the other
// horizontal axis stays thin. Taking both from the gesture makes a crate.
ENJIN_TEST(CreativeMode, ALadderIsWideAlongTheDragAndThinAcrossIt) {
    BuildToolSettings s;
    s.height = 4.0f;

    const ToolPlacement alongX =
        Plan(BuildTool::Ladder, s, Vector3(0, 0, 0), Vector3(1.6f, 0, 0));
    ENJIN_EXPECT_FLOAT_NEAR(alongX.halfExtents.x * 2.0f, 1.6f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(alongX.halfExtents.z, kCreativeLadderThin, 0.001f);

    const ToolPlacement alongZ =
        Plan(BuildTool::Ladder, s, Vector3(0, 0, 0), Vector3(0, 0, 1.6f));
    ENJIN_EXPECT_FLOAT_NEAR(alongZ.halfExtents.z * 2.0f, 1.6f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(alongZ.halfExtents.x, kCreativeLadderThin, 0.001f);
}

// The volume runs from the ground to the set height, so its centre is halfway
// up. A ladder centred on the drag would be half buried, like a wall would be.
ENJIN_TEST(CreativeMode, ALadderStandsOnTheGroundAndReachesItsHeight) {
    BuildToolSettings s;
    s.height = 4.0f;

    const ToolPlacement p =
        Plan(BuildTool::Ladder, s, Vector3(0, 0, 0), Vector3(1.0f, 0, 0));

    ENJIN_EXPECT_FLOAT_NEAR(p.halfExtents.y * 2.0f, 4.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.origin.y, 2.0f, 0.001f);
}

// A click is a legitimate way to place a ladder -- you point at the wall. It has
// to come out usable rather than as a zero-width sliver.
ENJIN_TEST(CreativeMode, ALadderFromAClickIsStillWideEnoughToClimb) {
    BuildToolSettings s;
    bool ok = false;
    const ToolPlacement p =
        Plan(BuildTool::Ladder, s, Vector3(2, 0, 2), Vector3(2, 0, 2), &ok);

    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_TRUE(p.halfExtents.x >= kCreativeLadderMinHalf - 0.001f);
}

ENJIN_TEST(CreativeMode, LadderRungsFollowTheGapAndAlwaysNumberAtLeastOne) {
    BuildToolSettings s;
    s.height = 3.0f;
    s.rungGap = 0.30f;
    ENJIN_EXPECT_EQ(Plan(BuildTool::Ladder, s, Vector3(0, 0, 0), Vector3(1, 0, 0)).rungs, 10u);

    // A gap wider than the whole ladder still leaves a rung. A ladder drawn as
    // two bare uprights reads as a broken model, not as a design choice.
    s.height = 1.0f;
    s.rungGap = 4.0f;
    ENJIN_EXPECT_EQ(Plan(BuildTool::Ladder, s, Vector3(0, 0, 0), Vector3(1, 0, 0)).rungs, 1u);
}

// The bug this guards cost a review to find: the rails and rungs were built at
// `origin + offset` while the entity transform was ALSO set to origin, and a
// BrushSolidComponent's centres are entity-LOCAL. Every offset was applied
// twice, so a ladder dragged at (10, 0, 6) drew itself around (20, 4, 12) --
// eleven metres from the volume you could actually climb. Even at the origin it
// was wrong in Y, drawing y 2..6 for a climb volume of y 0..4.
ENJIN_TEST(CreativeMode, LadderGeometryIsEntityLocalAndNotOffsetTwice) {
    BuildToolSettings s;
    s.height = 4.0f;

    // Placed far from the origin, which is where a double offset shows.
    const ToolPlacement p =
        Plan(BuildTool::Ladder, s, Vector3(10, 0, 6), Vector3(11.6f, 0, 6));

    ECS::BrushSolidComponent solid;
    CreativeMode::BuildLadderVisual(p, solid);
    ENJIN_ASSERT_TRUE(solid.brushes.size() >= 3);

    // Local coordinates: nothing carries the placement, and the whole ladder
    // straddles its own centre.
    for (const auto& b : solid.brushes) {
        ENJIN_EXPECT_TRUE(std::fabs(b.center.x) <= p.halfExtents.x + 0.001f);
        ENJIN_EXPECT_TRUE(std::fabs(b.center.z) <= p.halfExtents.z + 0.001f);
        ENJIN_EXPECT_TRUE(std::fabs(b.center.y) <= p.halfExtents.y + 0.001f);
    }
}

// The rungs are the whole reason the ladder is visible at all, so a ladder that
// comes out as two bare uprights reads as a broken model.
ENJIN_TEST(CreativeMode, ALadderHasTwoRailsAndOneBrushPerRung) {
    BuildToolSettings s;
    s.height = 3.0f;
    s.rungGap = 0.30f;

    const ToolPlacement p =
        Plan(BuildTool::Ladder, s, Vector3(0, 0, 0), Vector3(1.0f, 0, 0));

    ECS::BrushSolidComponent solid;
    CreativeMode::BuildLadderVisual(p, solid);

    ENJIN_EXPECT_EQ(solid.brushes.size(), (usize)(2 + p.rungs));
    // And it must not become a wall in front of the volume that does the climbing.
    ENJIN_EXPECT_FALSE(solid.generateCollider);
}

// A terrain mesh is CENTRED on its transform -- MeshFactory::CreateTerrain
// builds its vertices at `x * cellSize - halfW` -- so the placement origin is
// the press point itself, with no corner arithmetic.
//
// This test used to assert the opposite, because the placement, the brush and
// the raycast all believed the grid ran from the transform out to +X/+Z. They
// agreed with each other and disagreed with the renderer, which is why the
// brush ring drew in exactly the right place and the bump appeared 31.5 metres
// away. TerrainComponent::GridOrigin is now the single conversion and
// TestTerrainOrigin ties it to the renderer's own first vertex.
ENJIN_TEST(CreativeMode, AFreshTerrainIsCentredOnTheStrokeThatMadeIt) {
    BuildToolSettings s;
    bool ok = false;
    const ToolPlacement p =
        Plan(BuildTool::Terrain, s, Vector3(10, 0, -6), Vector3(10, 0, -6), &ok);

    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_FLOAT_NEAR(p.origin.x, 10.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.origin.z, -6.0f, 0.001f);

    // The half-extents still describe the grid's reach, so the stroke sits in
    // the middle of a terrain that spans an equal distance either side of it.
    const f32 half = static_cast<f32>(kCreativeTerrainGrid) * kCreativeTerrainCell * 0.5f;
    ENJIN_EXPECT_FLOAT_NEAR(p.halfExtents.x, half, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.halfExtents.z, half, 0.001f);
}

// The creation paths do not overlap: BuildBrushes owns the brush tools and
// PlanPlacement owns the rest. Reduce and Edit belong to neither, because
// neither of them MAKES anything -- they act on something that is already
// there, and have no footprint of their own to describe. Path and Cave belong
// to neither either, for their own reasons, spelled out below.
//
// The point of the test is that no tool is claimed by TWO builders, and that a
// tool claimed by none is one whose exception is written down here. A tool
// silently claimed by nothing is a rail entry that does nothing when dragged.
ENJIN_TEST(CreativeMode, EachToolBelongsToExactlyOneOfTheTwoPaths) {
    BuildToolSettings s;
    const Vector3 a(0, 0, 0), b(4, 0, 4);

    for (u8 i = 0; i < static_cast<u8>(BuildTool::Count); ++i) {
        const BuildTool tool = static_cast<BuildTool>(i);
        bool brushed = false, planned = false;
        Build(tool, s, false, a, b, &brushed);
        Plan(tool, s, a, b, &planned);
        ENJIN_EXPECT_FALSE(brushed && planned);

        // Path belongs to neither path because it builds from a CLICKED POINT
        // LIST rather than from two ground points, so it has its own builder.
        //
        // Cave belongs to neither because it does not BUILD anything at all:
        // it carves a signed distance field, and one drag also punches the
        // terrain above it open. PlanPlacement describes a footprint for one
        // entity, which is not what either half of that is. The carve lives in
        // Geometry::VoxelEdit and is covered by TestVoxelEdit.
        //
        // Door and Paint belong to neither because they act on what the cursor
        // RAY lands on, a wall or a model, and the ground points a drag hands
        // out are on the far side of it. PlanOpening is Door's builder.
        const bool notFromADrag =
            (tool == BuildTool::Reduce) || BuildToolIsEdit(tool) ||
            BuildToolIsPath(tool) || (tool == BuildTool::Cave) ||
            (tool == BuildTool::Door) || (tool == BuildTool::Paint);
        if (notFromADrag) {
            ENJIN_EXPECT_FALSE(brushed || planned);
        } else {
            ENJIN_EXPECT_TRUE(brushed || planned);
        }
    }
}

// The rail draws a separator wherever the group changes, and the surface's
// height budget counts those separators to decide whether everything fits. One
// grouping, two readers: if they ever disagree the footer moves by exactly the
// separators the budget missed.
ENJIN_TEST(CreativeMode, EveryToolHasAGroupAndTheBandsAreContiguous) {
    u8 previous = BuildToolGroup(static_cast<BuildTool>(0));
    for (u8 i = 1; i < static_cast<u8>(BuildTool::Count); ++i) {
        const u8 g = BuildToolGroup(static_cast<BuildTool>(i));
        // A band never resumes after another has started, or the rail would draw
        // two separators for one visual group.
        ENJIN_EXPECT_TRUE(g == previous || g == previous + 1);
        previous = g;
    }
    // Edit is on its own at the end: it is the only tool that makes nothing.
    ENJIN_EXPECT_TRUE(BuildToolGroup(BuildTool::Edit) >
                      BuildToolGroup(BuildTool::Reduce));
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

// Terrain shows Raise/Lower, and Lower has to be selectable. It was not: the
// mode state gated on BuildToolCanSubtract, which is about CSG and is false for
// Terrain, so the surface offered a button that could never take.
ENJIN_TEST(CreativeMode, TerrainCanBeSwitchedToLower) {
    CreativeMode mode;
    mode.SetTool(BuildTool::Terrain);
    ENJIN_ASSERT_TRUE(BuildToolModeLabels(BuildTool::Terrain) != nullptr);

    mode.SetSubtracting(true);
    ENJIN_EXPECT_TRUE(mode.IsSubtracting());

    // And it is still not a CSG cut, so nothing downstream treats it as one.
    ENJIN_EXPECT_FALSE(BuildToolCanSubtract(BuildTool::Terrain));
}

// Every tool the surface draws a mode toggle for must accept its second mode,
// and no tool without one may be left stuck in it.
ENJIN_TEST(CreativeMode, ASecondModeIsSelectableExactlyWhenItIsShown) {
    for (u8 i = 0; i < static_cast<u8>(BuildTool::Count); ++i) {
        const BuildTool tool = static_cast<BuildTool>(i);
        CreativeMode mode;
        mode.SetTool(tool);
        mode.SetSubtracting(true);
        ENJIN_EXPECT_EQ(mode.IsSubtracting(), BuildToolModeLabels(tool) != nullptr);
    }
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

// ---------------------------------------------------------------------------
// The build plane
// ---------------------------------------------------------------------------

ENJIN_TEST(CreativeMode, ARayPointingAtTheGroundLandsWhereItShould) {
    Vector3 hit;
    // From 10 up, heading down and forward at 45 degrees: 10 units along too.
    ENJIN_ASSERT_TRUE(CreativeMode::GroundHit(Vector3(2, 10, 3), Vector3(0, -1, -1), hit));
    ENJIN_EXPECT_FLOAT_NEAR(hit.x, 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(hit.y, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(hit.z, -7.0f, 0.001f);
}

// This is every frame of a 2D scene: the editor camera looks along -Z and the
// ground plane is edge-on, so no drag can ever land. It has to REFUSE rather
// than divide by a number near zero and hand back a placement at infinity --
// and creative mode now says so on screen instead of appearing to be broken.
ENJIN_TEST(CreativeMode, ACameraLevelWithTheGroundHasNoAnswer) {
    Vector3 hit(999.0f, 999.0f, 999.0f);
    ENJIN_EXPECT_FALSE(CreativeMode::GroundHit(Vector3(0, 5, 20), Vector3(0, 0, -1), hit));
    // And leaves the caller's value alone rather than half-writing it.
    ENJIN_EXPECT_FLOAT_NEAR(hit.x, 999.0f, 0.001f);
}

// Looking UP from above the plane never reaches it. Solving anyway puts the
// placement behind the camera, which is worse than nothing because it looks
// like a real number.
ENJIN_TEST(CreativeMode, ARayPointingAwayFromTheGroundHasNoAnswer) {
    Vector3 hit;
    ENJIN_EXPECT_FALSE(CreativeMode::GroundHit(Vector3(0, 5, 0), Vector3(0, 1, 0), hit));
    // Same from below, looking further down.
    ENJIN_EXPECT_FALSE(CreativeMode::GroundHit(Vector3(0, -5, 0), Vector3(0, -1, 0), hit));
}

// From under the floor looking up IS a legitimate hit: you can build a ceiling
// standing beneath it.
ENJIN_TEST(CreativeMode, LookingUpFromBelowTheGroundStillReachesIt) {
    Vector3 hit;
    ENJIN_ASSERT_TRUE(CreativeMode::GroundHit(Vector3(1, -4, 1), Vector3(0, 1, 0), hit));
    ENJIN_EXPECT_FLOAT_NEAR(hit.y, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(hit.x, 1.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Resizing what you already built
// ---------------------------------------------------------------------------

namespace {
ECS::BrushSolidComponent::Brush BoxAt(const Vector3& centre, const Vector3& half,
                                      f32 yawDegrees = 0.0f) {
    ECS::BrushSolidComponent::Brush b;
    b.shape = ECS::BrushSolidComponent::Shape::Box;
    b.center = centre;
    b.halfExtents = half;
    b.rotation = Quaternion::FromEuler(Vector3(0.0f, Radians(yawDegrees), 0.0f));
    return b;
}
} // namespace

// The point of edge handles, and the reason the engine's existing axis-arrow
// scaling is the wrong verb: pulling one edge must leave the other three where
// they are. ImGuizmo scales about the CENTRE, so its handles move the far side
// too, and "make this room a metre wider" comes out as "make it a metre wider
// in both directions and move the wall I was lining up against".
ENJIN_TEST(CreativeMode, DraggingAnEdgeLeavesTheOppositeEdgeWhereItWas) {
    auto b = BoxAt(Vector3(0, 1.5f, 0), Vector3(2, 1.5f, 1));
    const f32 fixedEdgeBefore = b.center.x - b.halfExtents.x;   // -2

    ENJIN_ASSERT_TRUE(ResizeBrushByGrip(b, BrushGrip::MaxX, Vector3(5, 0, 0)));

    ENJIN_EXPECT_FLOAT_NEAR(b.center.x - b.halfExtents.x, fixedEdgeBefore, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.center.x + b.halfExtents.x, 5.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.x * 2.0f, 7.0f, 0.001f);
    // The other axis and the height are none of this handle's business.
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.z, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.y, 1.5f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.center.y, 1.5f, 0.001f);
}

ENJIN_TEST(CreativeMode, DraggingTheMinEdgeHoldsTheMaxEdge) {
    auto b = BoxAt(Vector3(0, 1.5f, 0), Vector3(2, 1.5f, 1));
    ENJIN_ASSERT_TRUE(ResizeBrushByGrip(b, BrushGrip::MinX, Vector3(-6, 0, 0)));
    ENJIN_EXPECT_FLOAT_NEAR(b.center.x + b.halfExtents.x, 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.x * 2.0f, 8.0f, 0.001f);
}

// A wall dragged along a diagonal carries a yaw, so its own edges are not the
// world's. Working in world axes here would stretch a rotated wall sideways
// instead of lengthening it -- and every creative wall that is not axis-aligned
// is exactly this case.
ENJIN_TEST(CreativeMode, ARotatedBrushResizesAlongItsOwnAxes) {
    // 90 degrees of yaw: the brush's local +X now points along world -Z.
    auto b = BoxAt(Vector3(0, 1.5f, 0), Vector3(2, 1.5f, 0.125f), 90.0f);

    const Vector3 axisX = b.rotation.Rotate(Vector3(1, 0, 0));
    ENJIN_ASSERT_TRUE(std::fabs(axisX.x) < 0.01f);      // no longer world X
    ENJIN_ASSERT_TRUE(std::fabs(axisX.z) > 0.99f);

    // Where the untouched end sits before the drag, in world terms.
    const Vector3 farEndBefore = b.center - axisX * b.halfExtents.x;

    // Drag the local +X edge out to 5 m along that axis.
    ENJIN_ASSERT_TRUE(ResizeBrushByGrip(b, BrushGrip::MaxX, axisX * 5.0f));

    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.x * 2.0f, 7.0f, 0.001f);
    // Thickness untouched: the drag was along the length, not across it.
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.z, 0.125f, 0.001f);
    // And the far end has not moved anywhere in the world.
    const Vector3 farEndAfter = b.center - axisX * b.halfExtents.x;
    ENJIN_EXPECT_FLOAT_NEAR(farEndAfter.x, farEndBefore.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(farEndAfter.z, farEndBefore.z, 0.001f);
}

ENJIN_TEST(CreativeMode, ACornerGripMovesBothAxesAtOnce) {
    auto b = BoxAt(Vector3(0, 1.0f, 0), Vector3(1, 1.0f, 1));
    ENJIN_ASSERT_TRUE(ResizeBrushByGrip(b, BrushGrip::MaxXMaxZ, Vector3(4, 0, 3)));

    ENJIN_EXPECT_FLOAT_NEAR(b.center.x + b.halfExtents.x, 4.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.center.z + b.halfExtents.z, 3.0f, 0.001f);
    // Both opposite edges held.
    ENJIN_EXPECT_FLOAT_NEAR(b.center.x - b.halfExtents.x, -1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.center.z - b.halfExtents.z, -1.0f, 0.001f);
}

// Dragging an edge past its opposite number would invert the brush, and a brush
// with no width contributes no faces -- the solid disappears mid-gesture, which
// reads as the tool eating your work. It refuses instead, leaving the brush as
// it was so the drag simply stops.
ENJIN_TEST(CreativeMode, ADragThatWouldCollapseABrushIsRefusedOutright) {
    auto b = BoxAt(Vector3(0, 1.0f, 0), Vector3(2, 1.0f, 1));
    const auto before = b;

    ENJIN_EXPECT_FALSE(ResizeBrushByGrip(b, BrushGrip::MaxX, Vector3(-3, 0, 0)));
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.x, before.halfExtents.x, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.center.x, before.center.x, 0.0001f);
}

// A corner that collapses on ONE axis must not apply the other either, or a
// single gesture leaves the brush half-resized and no undo entry describes it.
ENJIN_TEST(CreativeMode, AHalfCollapsedCornerDragChangesNothing) {
    auto b = BoxAt(Vector3(0, 1.0f, 0), Vector3(2, 1.0f, 2));
    const auto before = b;

    // Valid on Z, inverted on X.
    ENJIN_EXPECT_FALSE(ResizeBrushByGrip(b, BrushGrip::MaxXMaxZ, Vector3(-5, 0, 4)));
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.z, before.halfExtents.z, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.center.z, before.center.z, 0.0001f);
}

// A grip has to be drawn where dragging it will actually take hold, or the
// handle and the edge it moves are in different places.
ENJIN_TEST(CreativeMode, AGripIsDrawnOnTheEdgeItMoves) {
    auto b = BoxAt(Vector3(1, 1.5f, -2), Vector3(3, 1.5f, 0.5f));

    const Vector3 maxX = BrushGripPosition(b, BrushGrip::MaxX);
    ENJIN_EXPECT_FLOAT_NEAR(maxX.x, 4.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(maxX.z, -2.0f, 0.001f);

    const Vector3 corner = BrushGripPosition(b, BrushGrip::MinXMaxZ);
    ENJIN_EXPECT_FLOAT_NEAR(corner.x, -2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(corner.z, -1.5f, 0.001f);

    // Handles sit at the brush's own height, because height is not resized here.
    ENJIN_EXPECT_FLOAT_NEAR(maxX.y, 1.5f, 0.001f);
}

ENJIN_TEST(CreativeMode, FourGripsAreCornersAndFourAreEdges) {
    u32 corners = 0;
    for (u8 i = 0; i < static_cast<u8>(BrushGrip::Count); ++i) {
        if (BrushGripIsCorner(static_cast<BrushGrip>(i))) ++corners;
    }
    ENJIN_EXPECT_EQ(corners, 4u);
    ENJIN_EXPECT_EQ(static_cast<u8>(BrushGrip::Count), (u8)8);
}

// A prism is round, so it has one radius and no far side to hold. Sliding an
// edge the way a box does would make the shape drift sideways as it grew, which
// is not what a handle on its rim promises.
ENJIN_TEST(CreativeMode, APrismGripSetsItsRadiusAndLeavesItCentred) {
    ECS::BrushSolidComponent::Brush b;
    b.shape = ECS::BrushSolidComponent::Shape::Prism;
    b.center = Vector3(2, 1, 2);
    b.radius = 1.0f;
    b.halfHeight = 1.0f;
    b.sides = 8;

    ENJIN_ASSERT_TRUE(ResizeBrushByGrip(b, BrushGrip::MaxX, Vector3(5, 0, 2)));
    ENJIN_EXPECT_FLOAT_NEAR(b.radius, 3.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.center.x, 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.center.z, 2.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Path: walls that are not straight
// ---------------------------------------------------------------------------

// A straight span is one brush no matter what the segment count says. There is
// nothing to approximate, and spending 8 brushes on a straight line would make
// the segment field a tax on every path with a straight bit in it.
ENJIN_TEST(CreativeMode, AStraightSpanIsOneBrushWhateverTheSegmentCount) {
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(6,0,0) };
    const std::vector<f32> bows = { 0.0f };

    ENJIN_EXPECT_EQ(CreativeMode::CountPathBrushes(pts, bows, 16), 1u);
    ENJIN_EXPECT_EQ(CreativeMode::CountPathBrushes(pts, bows, 2), 1u);
}

ENJIN_TEST(CreativeMode, ABowedSpanCostsOneBrushPerSegment) {
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(6,0,0) };
    ENJIN_EXPECT_EQ(CreativeMode::CountPathBrushes(pts, { 1.5f }, 8), 8u);
    ENJIN_EXPECT_EQ(CreativeMode::CountPathBrushes(pts, { 1.5f }, 3), 3u);
}

// The count shown while authoring has to be the count that gets built, or the
// number on screen is worth nothing.
ENJIN_TEST(CreativeMode, TheQuotedBrushCountIsWhatGetsBuilt) {
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(4,0,0), Vector3(8,0,3) };
    const std::vector<f32> bows = { 1.0f, 0.0f };
    BuildToolSettings s;

    const u32 quoted = CreativeMode::CountPathBrushes(pts, bows, 6);
    ECS::BrushSolidComponent out;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildPathBrushes(pts, bows, 6, s, false, out));
    ENJIN_EXPECT_EQ(static_cast<u32>(out.brushes.size()), quoted);
    ENJIN_EXPECT_EQ(quoted, 7u);      // 6 for the bowed span, 1 for the straight one
}

// A bow under the threshold is a straight line someone nudged by a millimetre.
// Sampling it would buy two dozen brushes that all lie on the same line.
ENJIN_TEST(CreativeMode, ABowTooSmallToSeeStaysStraight) {
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(6,0,0) };
    ENJIN_EXPECT_EQ(CreativeMode::CountPathBrushes(pts, { kCreativePathMinBow * 0.5f }, 12), 1u);
}

// The segment count is clamped rather than trusted: it rides a draggable field,
// and every segment is also a CSG operand for anything cut through that wall
// later, so an unbounded number is a way to make the solid stop rebuilding.
ENJIN_TEST(CreativeMode, TheSegmentCountIsClampedBothWays) {
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(6,0,0) };
    const std::vector<f32> bows = { 2.0f };

    ENJIN_EXPECT_EQ(CreativeMode::CountPathBrushes(pts, bows, 9999), kCreativePathSegmentsMax);
    ENJIN_EXPECT_EQ(CreativeMode::CountPathBrushes(pts, bows, 0), kCreativePathSegmentsMin);
}

ENJIN_TEST(CreativeMode, APathNeedsTwoPointsBeforeItIsAnything) {
    BuildToolSettings s;
    ECS::BrushSolidComponent out;
    ENJIN_EXPECT_FALSE(CreativeMode::BuildPathBrushes({ Vector3(1,0,1) }, {}, 8, s, false, out));
    ENJIN_EXPECT_FALSE(CreativeMode::BuildPathBrushes({}, {}, 8, s, false, out));
    ENJIN_EXPECT_EQ(out.brushes.size(), (usize)0);
}

// Every segment stands ON its line, the same as the Wall tool's single brush.
// A path whose walls were half underground would be a different tool.
ENJIN_TEST(CreativeMode, EveryPathSegmentStandsOnTheGround) {
    BuildToolSettings s;
    s.height = 3.0f;
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(5,0,0), Vector3(5,0,5) };

    ECS::BrushSolidComponent out;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildPathBrushes(pts, { 0.0f, 0.0f }, 8, s, false, out));
    for (const auto& b : out.brushes) {
        ENJIN_EXPECT_FLOAT_NEAR(b.center.y, 1.5f, 0.001f);
        ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.y * 2.0f, 3.0f, 0.001f);
    }
}

// The corner fill, borrowed from the Stalberg building tools: two boxes meeting
// at an angle leave a wedge-shaped notch on the OUTSIDE of the bend, because
// each one stops at the shared point. The segments run past the joint so the
// corner closes, instead of the tool handing back its own seam for someone to
// patch by hand.
ENJIN_TEST(CreativeMode, SegmentsRunPastAJointSoTheCornerFillsIn) {
    BuildToolSettings s;
    s.thickness = 0.4f;
    // A right-angle corner: the extension should be exactly half the thickness.
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(4,0,0), Vector3(4,0,4) };

    ECS::BrushSolidComponent out;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildPathBrushes(pts, { 0.0f, 0.0f }, 8, s, false, out));
    ENJIN_ASSERT_EQ(out.brushes.size(), (usize)2);

    ENJIN_EXPECT_FLOAT_NEAR(out.brushes[0].halfExtents.x * 2.0f, 4.0f + 0.2f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(out.brushes[1].halfExtents.x * 2.0f, 4.0f + 0.2f, 0.01f);
}

// ...and only at INTERIOR joints. A lone span has none, so it is exactly as long
// as it was drawn -- otherwise a wall would grow past the corner it was drawn to.
ENJIN_TEST(CreativeMode, ThePathEndsStayWhereTheyWereClicked) {
    BuildToolSettings s;
    s.thickness = 0.4f;
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(4,0,0) };

    ECS::BrushSolidComponent out;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildPathBrushes(pts, { 0.0f }, 8, s, false, out));
    ENJIN_ASSERT_EQ(out.brushes.size(), (usize)1);
    ENJIN_EXPECT_FLOAT_NEAR(out.brushes[0].halfExtents.x * 2.0f, 4.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(out.brushes[0].center.x, 2.0f, 0.001f);
}

// A straight joint adds nothing: tan(0) is 0, and a wall that grew at every
// sampled point of a straight line would creep.
ENJIN_TEST(CreativeMode, AStraightJointAddsNothing) {
    BuildToolSettings s;
    s.thickness = 0.4f;
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(4,0,0), Vector3(8,0,0) };

    ECS::BrushSolidComponent out;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildPathBrushes(pts, { 0.0f, 0.0f }, 8, s, false, out));
    ENJIN_ASSERT_EQ(out.brushes.size(), (usize)2);
    for (const auto& b : out.brushes) {
        ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.x * 2.0f, 4.0f, 0.01f);
    }
}

ENJIN_TEST(CreativeMode, ASubtractedPathCutsWithEverySegment) {
    BuildToolSettings s;
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(6,0,0) };

    ECS::BrushSolidComponent out;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildPathBrushes(pts, { 1.5f }, 5, s, true, out));
    ENJIN_ASSERT_EQ(out.brushes.size(), (usize)5);
    for (const auto& b : out.brushes) {
        ENJIN_EXPECT_EQ((int)b.op, (int)Geometry::BrushOp::Subtract);
    }
}

// The preview and the geometry come from ONE function, so what you are shown
// while clicking is the thing that gets built -- not a smooth curve that turns
// into something coarser the moment you finish.
ENJIN_TEST(CreativeMode, ThePreviewLineAndTheBuiltWallAgree) {
    BuildToolSettings s;
    const std::vector<Vector3> pts = { Vector3(0,0,0), Vector3(6,0,0), Vector3(10,0,4) };
    const std::vector<f32> bows = { 1.2f, 0.0f };

    const std::vector<Vector3> line = CreativeMode::SamplePath(pts, bows, 7);
    ECS::BrushSolidComponent out;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildPathBrushes(pts, bows, 7, s, false, out));

    ENJIN_EXPECT_EQ(out.brushes.size(), line.size() - 1);
    ENJIN_EXPECT_FLOAT_NEAR(line.front().x, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(line.back().x, 10.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(line.back().z, 4.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Plants
// ---------------------------------------------------------------------------
//
// Grass, shrubs and trees existed ONLY in the older Build palette -- View >
// Build Palette (Creative), which is off by default -- so the whole of the
// engine's vegetation was reachable only from a window you had to already know
// about. Two creative systems, and the one people find had no plants in it.
//
// Worse than missing: with the tool absent from this enum, the realisation in
// EditorLayerCreative fell through its Water branch to the Ladder branch, so a
// drag with any unhandled tool silently produced a ladder.

ENJIN_TEST(CreativeMode, PlantsCoverTheDraggedPatch) {
    BuildToolSettings s;

    bool ok = false;
    const ToolPlacement p =
        Plan(BuildTool::Plants, s, Vector3(-4, 0, -2), Vector3(6, 0, 8), &ok);

    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_FLOAT_NEAR(p.halfExtents.x * 2.0f, 10.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.halfExtents.z * 2.0f, 10.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.origin.x, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.origin.z, 3.0f, 0.001f);
}

ENJIN_TEST(CreativeMode, PlantsSitOnTheGroundTheyWereDraggedOn) {
    // Deliberately NOT settings.elevation, which is what Water uses. Water is a
    // surface you place at a height; plants grow where you dragged them, and a
    // patch that floated at some authored elevation would be a different tool.
    BuildToolSettings s;
    s.elevation = -5.0f;

    const ToolPlacement p =
        Plan(BuildTool::Plants, s, Vector3(0, 2.25f, 0), Vector3(4, 2.25f, 4));

    ENJIN_EXPECT_FLOAT_NEAR(p.origin.y, 2.25f, 0.001f);
}

ENJIN_TEST(CreativeMode, APatchWithNoAreaIsRefused) {
    // A line has nothing to scatter into. Accepting it would place a volume that
    // looks put down and grows nothing, which reads as broken vegetation rather
    // than as a gesture that did not describe an area.
    BuildToolSettings s;

    bool ok = true;
    Plan(BuildTool::Plants, s, Vector3(0, 0, 0), Vector3(8, 0, 0), &ok);
    ENJIN_EXPECT_FALSE(ok);

    ok = true;
    Plan(BuildTool::Plants, s, Vector3(0, 0, 0), Vector3(0, 0, 8), &ok);
    ENJIN_EXPECT_FALSE(ok);
}

ENJIN_TEST(CreativeMode, EveryToolHasAName) {
    // BuildToolName feeds the rail's tooltip and the entity's name. A tool added
    // to the enum and missed here gets whatever the default arm returns, which
    // is how a new tool ends up on the rail labelled as another one.
    for (int i = 0; i < static_cast<int>(BuildTool::Count); ++i) {
        const char* n = BuildToolName(static_cast<BuildTool>(i));
        ENJIN_ASSERT_TRUE(n != nullptr);
        ENJIN_EXPECT_TRUE(n[0] != '\0');
    }
}

ENJIN_TEST(CreativeMode, EveryToolSaysHowToUseIt) {
    // The verb line under the rail. An empty one is a tool the surface will not
    // explain, which is the discoverability half of the golden rule.
    for (int i = 0; i < static_cast<int>(BuildTool::Count); ++i) {
        const char* v = BuildToolVerb(static_cast<BuildTool>(i));
        ENJIN_ASSERT_TRUE(v != nullptr);
        ENJIN_EXPECT_TRUE(v[0] != '\0');
    }
}

// ---------------------------------------------------------------------------
// Props
// ---------------------------------------------------------------------------
//
// A ball, a point light, a physics box, a barrel and a spawn point: five
// ready-made objects that existed ONLY in the older Build Palette, behind a
// View-menu entry that is off by default. Same gap as the vegetation, five more
// tools.

ENJIN_TEST(CreativeMode, APropIsPlacedByAClickNotADrag) {
    // Every other tool on this rail refuses a gesture with no span, because a
    // wall or a patch with no length describes nothing. A prop is the opposite:
    // it has its own size, the click IS the whole interaction, and refusing a
    // zero-span gesture would make the tool look inert.
    BuildToolSettings s;

    bool ok = false;
    const ToolPlacement p =
        Plan(BuildTool::Prop, s, Vector3(3, 1, -2), Vector3(3, 1, -2), &ok);

    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_FLOAT_NEAR(p.origin.x, 3.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.origin.z, -2.0f, 0.001f);
}

ENJIN_TEST(CreativeMode, APropLandsWhereTheGestureEnded) {
    // Dragging does not size a prop -- it moves where the thing lands, so the
    // END of the gesture is the answer. Taking the start would drop it wherever
    // the mouse happened to go down, which is not where you are looking when you
    // let go.
    BuildToolSettings s;

    const ToolPlacement p =
        Plan(BuildTool::Prop, s, Vector3(0, 0, 0), Vector3(7, 0, 9));

    ENJIN_EXPECT_FLOAT_NEAR(p.origin.x, 7.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(p.origin.z, 9.0f, 0.001f);
}

ENJIN_TEST(CreativeMode, PropsBuildNoBrushes) {
    // Props are components and meshes, not brush solids, so the brush path must
    // decline them rather than emit an empty solid that reads as a failed build.
    BuildToolSettings s;
    bool ok = true;
    ECS::BrushSolidComponent solid =
        Build(BuildTool::Prop, s, false, Vector3(0, 0, 0), Vector3(0, 0, 0), &ok);
    ENJIN_EXPECT_FALSE(ok);
    ENJIN_EXPECT_EQ(solid.brushes.size(), (usize)0);
}

// ---------------------------------------------------------------------------
// Water is two features, not one
// ---------------------------------------------------------------------------
//
// Water3DComponent is a rendered plane: Gerstner waves, styles, tessellation.
// WaterVolumeComponent is a BODY: depth, shore foam, and the component
// ControllerSystem reads to put a character into a swim state.
//
// The Water tool only ever made the first, so "Water" in a level-building rail
// produced water you sink through, and the swimmable kind was reachable only
// from the older Build Palette. Playground's own Pool is a WaterVolume.

ENJIN_TEST(CreativeMode, WaterDefaultsToTheSwimmableKind) {
    // A build tool exists to make a level somebody plays. Water you fall through
    // is the surprising answer, so it is not the default one.
    BuildToolSettings s;
    ENJIN_EXPECT_TRUE(s.waterKind >= 0.5f);
    ENJIN_EXPECT_TRUE(s.waterDepth > 0.0f);
}

ENJIN_TEST(CreativeMode, BothWaterKindsCoverTheDraggedRectangle) {
    // The footprint is the gesture either way -- only what gets built from it
    // differs, and a tool whose shape changed with a settings toggle would be
    // two tools wearing one name.
    BuildToolSettings surface;
    surface.waterKind = 0.0f;
    BuildToolSettings swimmable;
    swimmable.waterKind = 1.0f;

    bool okA = false, okB = false;
    const ToolPlacement a =
        Plan(BuildTool::Water, surface, Vector3(-4, 0, -2), Vector3(6, 0, 8), &okA);
    const ToolPlacement b =
        Plan(BuildTool::Water, swimmable, Vector3(-4, 0, -2), Vector3(6, 0, 8), &okB);

    ENJIN_ASSERT_TRUE(okA && okB);
    ENJIN_EXPECT_FLOAT_NEAR(a.halfExtents.x, b.halfExtents.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(a.halfExtents.z, b.halfExtents.z, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(a.origin.x, b.origin.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(a.origin.z, b.origin.z, 0.001f);
}

// The bug this pins down shipped for three days and killed seven tools.
//
// HandleBuildDrag ends an abandoned gesture so a release eaten by a focus loss
// does not leave the mode stuck mid-drag forever. It tested only "the button is
// not down" -- and on the frame a drag ends NORMALLY the button is not down
// either, because that is what a release is. So every successful drag was
// cancelled one frame before the commit could run: the preview tracked the
// cursor, the length in metres updated, and the mouse-up built nothing.
ENJIN_TEST(CreativeGesture, ANormalReleaseIsNotAnAbandonedGesture) {
    // The release frame: button already up, edge present. This is the case the
    // original condition got wrong.
    ENJIN_EXPECT_FALSE(GestureWasAbandoned(false, true));
}

ENJIN_TEST(CreativeGesture, AnEatenReleaseIsAbandoned) {
    // Button up, and no edge ever arrived -- a focus loss mid-drag.
    ENJIN_EXPECT_TRUE(GestureWasAbandoned(false, false));
}

ENJIN_TEST(CreativeGesture, AGestureStillHeldIsNeitherFinishedNorAbandoned) {
    ENJIN_EXPECT_FALSE(GestureWasAbandoned(true, false));
}

// Every tool answers to the name the rail prints for it, whatever the case, and
// no two tools answer to the same name.
ENJIN_TEST(CreativeGesture, EveryToolRoundTripsThroughItsOwnName) {
    usize matched = 0;
    for (u8 i = 0; i < static_cast<u8>(BuildTool::Count); ++i) {
        const BuildTool want = static_cast<BuildTool>(i);
        BuildTool got = BuildTool::Count;
        ENJIN_ASSERT_TRUE(BuildToolFromName(BuildToolName(want), got));
        ENJIN_EXPECT_TRUE(got == want);
        ++matched;
    }
    ENJIN_EXPECT_TRUE(matched == static_cast<usize>(BuildTool::Count));
}

ENJIN_TEST(CreativeGesture, ToolNamesAreCaseInsensitiveAndUnknownNamesAreRefused) {
    BuildTool got = BuildTool::Count;
    ENJIN_ASSERT_TRUE(BuildToolFromName("wall", got));
    ENJIN_EXPECT_TRUE(got == BuildTool::Wall);
    ENJIN_ASSERT_TRUE(BuildToolFromName("TERRAIN", got));
    ENJIN_EXPECT_TRUE(got == BuildTool::Terrain);

    // A name no tool has must fail rather than fall back to the first tool --
    // a silent fallback would arm Wall and build a wall where a typo asked for
    // something else entirely.
    ENJIN_EXPECT_FALSE(BuildToolFromName("Wal", got));
    ENJIN_EXPECT_FALSE(BuildToolFromName("", got));
    ENJIN_EXPECT_FALSE(BuildToolFromName(nullptr, got));
}

// The footprint is measured against the SEGMENT, not the infinite line it sits
// on. Unclamped, the mouth would open in a stripe running to the horizon.
ENJIN_TEST(CreativeCave, TheFootprintStopsAtTheEndsOfTheDrag) {
    const Vector3 a(0, 0, 0), b(10, 0, 0);

    ENJIN_EXPECT_FLOAT_NEAR(CreativeMode::CaveDistanceToAxisXZ(a, b, 5.0f, 0.0f), 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(CreativeMode::CaveDistanceToAxisXZ(a, b, 5.0f, 3.0f), 3.0f, 0.001f);

    // Past the end, distance is measured to the END, not to the line.
    ENJIN_EXPECT_FLOAT_NEAR(CreativeMode::CaveDistanceToAxisXZ(a, b, 40.0f, 0.0f), 30.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(CreativeMode::CaveDistanceToAxisXZ(a, b, -10.0f, 0.0f), 10.0f, 0.001f);
}

// The roof height is what decides where the hill opens, so it has to agree
// with the carve: a roof reported lower than the passage actually reaches would
// leave the surface skinned over a cave that is poking through it.
ENJIN_TEST(CreativeCave, TheReportedRoofIsTheTopOfTheCarvedPassage) {
    BuildToolSettings s;
    s.radius = 2.0f;       // bore
    s.roughness = 0.0f;

    // The stroke puts the passage FLOOR on the drag, so the axis is one bore up
    // and the roof is one more.
    ENJIN_EXPECT_FLOAT_NEAR(CreativeMode::CaveTopY(s, Vector3(0, 0, 0)), 4.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(CreativeMode::CaveTopY(s, Vector3(0, 10, 0)), 14.0f, 0.001f);

    // Roughness pushes the wall outwards, so the roof goes up with it. Ignoring
    // it would skin the surface over exactly the bumps that broke through.
    s.roughness = 0.5f;
    ENJIN_EXPECT_FLOAT_NEAR(CreativeMode::CaveTopY(s, Vector3(0, 0, 0)), 4.5f, 0.001f);
}

ENJIN_TEST(CreativeCave, TheOuterRadiusCoversTheRoughnessAsWellAsTheBore) {
    BuildToolSettings s;
    s.radius = 3.0f;
    s.roughness = 0.75f;
    // The footprint has to cover where the wall actually wanders to, or the
    // terrain stays closed over the edges of the mouth.
    ENJIN_EXPECT_FLOAT_NEAR(CreativeMode::CaveOuterRadius(s), 3.75f, 0.001f);

    s.roughness = 0.0f;
    ENJIN_EXPECT_FLOAT_NEAR(CreativeMode::CaveOuterRadius(s), 3.0f, 0.001f);
}

// Cave's second mode is Fill, and Fill is NOT a CSG cut.
//
// Two predicates that are easy to confuse: BuildToolCanSubtract means the
// tool's second mode carves geometry, and BuildToolModeLabels means it has a
// second mode at all. Cave has one and not the other -- Fill closes the hole
// mask and touches no geometry. Reading the narrow one as "has two modes" is
// what made the MCP tool refuse Terrain's Lower.
ENJIN_TEST(CreativeCave, FillIsASecondModeAndNotACut) {
    const char* const* labels = BuildToolModeLabels(BuildTool::Cave);
    ENJIN_ASSERT_TRUE(labels != nullptr);
    ENJIN_EXPECT_TRUE(std::strcmp(labels[0], "Dig") == 0);
    ENJIN_EXPECT_TRUE(std::strcmp(labels[1], "Fill") == 0);

    ENJIN_EXPECT_FALSE(BuildToolCanSubtract(BuildTool::Cave));
}

// Every tool that carves geometry necessarily has a second mode; the reverse
// does not hold, and Terrain and Cave are why.
ENJIN_TEST(CreativeCave, EveryCuttingToolHasLabelsButNotEveryLabelledToolCuts) {
    usize labelledButNotCutting = 0;
    for (u8 i = 0; i < static_cast<u8>(BuildTool::Count); ++i) {
        const BuildTool tool = static_cast<BuildTool>(i);
        const bool cuts = BuildToolCanSubtract(tool);
        const bool labelled = (BuildToolModeLabels(tool) != nullptr);
        if (cuts) ENJIN_EXPECT_TRUE(labelled);
        if (labelled && !cuts) ++labelledButNotCutting;
    }
    // Terrain (Raise/Lower) and Cave (Dig/Fill).
    ENJIN_EXPECT_EQ(labelledButNotCutting, (usize)2);
}

// ---------------------------------------------------------------------------
// Shape handles: reshaping something after it is built
// ---------------------------------------------------------------------------

namespace {

// An L of two walls meeting at (4, 0, 0), as the Path tool would leave it.
ECS::WallPathComponent MakeCornerPath() {
    ECS::WallPathComponent p;
    p.points = {Vector3(0.0f, 0.0f, 0.0f), Vector3(4.0f, 0.0f, 0.0f), Vector3(4.0f, 0.0f, 3.0f)};
    p.bows = {0.0f, 0.0f};
    p.height = 3.0f;
    p.thickness = 0.25f;
    return p;
}

const ShapeHandle* FindHandle(const std::vector<ShapeHandle>& hs, ShapeHandleKind kind, i32 index) {
    for (const auto& h : hs) if (h.kind == kind && h.index == index) return &h;
    return nullptr;
}

// Where a wall brush's two ends are, in XZ.
void WallEnds(const ECS::BrushSolidComponent::Brush& b, Vector3& a, Vector3& c) {
    const Vector3 axis = b.rotation.Rotate(Vector3(1.0f, 0.0f, 0.0f));
    a = b.center - axis * b.halfExtents.x;
    c = b.center + axis * b.halfExtents.x;
}

bool NearXZ(const Vector3& a, const Vector3& b, f32 tol) {
    return std::fabs(a.x - b.x) < tol && std::fabs(a.z - b.z) < tol;
}

} // namespace

ENJIN_TEST(CreativeShape, WallHandlesAreOnePerCornerAndOnePerSpan) {
    // Arrange
    const ECS::WallPathComponent p = MakeCornerPath();
    std::vector<ShapeHandle> hs;

    // Act
    WallPathHandles(p, Vector3(10.0f, 0.0f, 0.0f), hs);

    // Assert: three corners, two bow handles, all offset by the entity origin.
    ENJIN_ASSERT_EQ(hs.size(), static_cast<usize>(5));
    const ShapeHandle* corner = FindHandle(hs, ShapeHandleKind::WallPoint, 1);
    ENJIN_ASSERT_NOT_NULL(corner);
    ENJIN_EXPECT_TRUE(NearXZ(corner->position, Vector3(14.0f, 0.0f, 0.0f), 1e-4f));
    ENJIN_EXPECT_TRUE(ShapeHandleIsRound(*corner));
    ENJIN_EXPECT_FALSE(ShapeHandleIsRound(*FindHandle(hs, ShapeHandleKind::WallBow, 0)));
}

ENJIN_TEST(CreativeShape, DraggingACornerMovesBothWallsThatMeetThere) {
    // Arrange: the corner is shared by both spans. Rebuilt from the line, the
    // two brushes must still meet at wherever the corner went.
    ECS::WallPathComponent p = MakeCornerPath();
    ECS::BrushSolidComponent solid;
    ENJIN_ASSERT_TRUE(RebuildWallPath(p, solid));
    const ECS::WallPathComponent start = p;
    std::vector<ShapeHandle> hs;
    WallPathHandles(p, Vector3(), hs);

    // Act
    const bool moved = DragWallPathHandle(p, start, *FindHandle(hs, ShapeHandleKind::WallPoint, 1),
                                          Vector3(), Vector3(6.0f, 0.0f, -1.0f));
    ENJIN_ASSERT_TRUE(moved);
    ENJIN_ASSERT_TRUE(RebuildWallPath(p, solid));

    // Assert: the first wall now ends at the new corner and the second starts
    // there. The mitre extends each past the joint by at most half the
    // thickness times tan(turn/2), so compare within that.
    ENJIN_ASSERT_EQ(solid.brushes.size(), static_cast<usize>(2));
    Vector3 a0, a1, b0, b1;
    WallEnds(solid.brushes[0], a0, a1);
    WallEnds(solid.brushes[1], b0, b1);
    ENJIN_EXPECT_TRUE(NearXZ(a0, Vector3(0.0f, 0.0f, 0.0f), 1e-3f));
    ENJIN_EXPECT_TRUE(NearXZ(a1, Vector3(6.0f, 0.0f, -1.0f), 0.5f));
    ENJIN_EXPECT_TRUE(NearXZ(b0, Vector3(6.0f, 0.0f, -1.0f), 0.5f));
    ENJIN_EXPECT_TRUE(NearXZ(b1, Vector3(4.0f, 0.0f, 3.0f), 1e-3f));
}

ENJIN_TEST(CreativeShape, ACornerDraggedOntoItsNeighbourIsRefused) {
    // Arrange
    ECS::WallPathComponent p = MakeCornerPath();
    const ECS::WallPathComponent start = p;
    std::vector<ShapeHandle> hs;
    WallPathHandles(p, Vector3(), hs);

    // Act: drop the corner onto the start of the path.
    const bool moved = DragWallPathHandle(p, start, *FindHandle(hs, ShapeHandleKind::WallPoint, 1),
                                          Vector3(), Vector3(0.01f, 0.0f, 0.0f));

    // Assert: a zero-length wall would vanish mid-drag; the path stays as it was.
    ENJIN_EXPECT_FALSE(moved);
    ENJIN_EXPECT_TRUE(NearXZ(p.points[1], Vector3(4.0f, 0.0f, 0.0f), 1e-6f));
}

ENJIN_TEST(CreativeShape, DraggingABowHandleCurvesThatSpanOnly) {
    // Arrange
    ECS::WallPathComponent p = MakeCornerPath();
    const ECS::WallPathComponent start = p;
    std::vector<ShapeHandle> hs;
    WallPathHandles(p, Vector3(), hs);

    // Act: pull the first span's middle 1.5 m off its line.
    DragWallPathHandle(p, start, *FindHandle(hs, ShapeHandleKind::WallBow, 0),
                       Vector3(), Vector3(2.0f, 0.0f, 1.5f));

    // Assert: the span normal of (0,0,0)->(4,0,0) is +Z, so the bow is +1.5.
    ENJIN_EXPECT_FLOAT_NEAR(p.bows[0], 1.5f, 1e-4f);
    ENJIN_EXPECT_FLOAT_NEAR(p.bows[1], 0.0f, 1e-6f);
    ECS::BrushSolidComponent solid;
    ENJIN_ASSERT_TRUE(RebuildWallPath(p, solid));
    ENJIN_EXPECT_TRUE(solid.brushes.size() > 2);   // the curve is sampled into segments
}

ENJIN_TEST(CreativeShape, ABowNudgedNearTheLineSnapsStraight) {
    ECS::WallPathComponent p = MakeCornerPath();
    const ECS::WallPathComponent start = p;
    std::vector<ShapeHandle> hs;
    WallPathHandles(p, Vector3(), hs);
    DragWallPathHandle(p, start, *FindHandle(hs, ShapeHandleKind::WallBow, 0),
                       Vector3(), Vector3(2.0f, 0.0f, kCreativePathMinBow * 0.5f));
    ENJIN_EXPECT_TRUE(p.bows[0] == 0.0f);
}

ENJIN_TEST(CreativeShape, RebuildingAWallKeepsTheDoorwayCutThroughIt) {
    // Arrange: a built wall with a Subtract brush appended after its own.
    ECS::WallPathComponent p = MakeCornerPath();
    ECS::BrushSolidComponent solid;
    ENJIN_ASSERT_TRUE(RebuildWallPath(p, solid));
    ENJIN_ASSERT_EQ(p.builtBrushes, 2u);
    ECS::BrushSolidComponent::Brush door;
    door.op = Geometry::BrushOp::Subtract;
    door.center = Vector3(2.0f, 1.0f, 0.0f);
    solid.brushes.push_back(door);

    // Act: bow the first span, which changes how many brushes the line makes.
    p.bows[0] = 1.0f;
    ENJIN_ASSERT_TRUE(RebuildWallPath(p, solid));

    // Assert: the cut is still there, still last, and the count moved with it.
    ENJIN_EXPECT_EQ(static_cast<usize>(p.builtBrushes) + 1, solid.brushes.size());
    ENJIN_EXPECT_TRUE(solid.brushes.back().op == Geometry::BrushOp::Subtract);
    ENJIN_EXPECT_TRUE(NearXZ(solid.brushes.back().center, Vector3(2.0f, 0.0f, 0.0f), 1e-6f));
}

ENJIN_TEST(CreativeShape, AWallsHeightAndThicknessCanBeChangedAfterItIsBuilt) {
    // Arrange: an L of two walls with a doorway cut through the first.
    ECS::WallPathComponent p = MakeCornerPath();
    ECS::BrushSolidComponent solid;
    ENJIN_ASSERT_TRUE(RebuildWallPath(p, solid));
    ECS::BrushSolidComponent::Brush door;
    door.op = Geometry::BrushOp::Subtract;
    door.center = Vector3(2.0f, 1.0f, 0.0f);
    solid.brushes.push_back(door);

    // Act
    ENJIN_ASSERT_TRUE(ResizeWallPath(p, solid, 5.0f, 0.5f));

    // Assert: every wall brush is the new size and still stands on the floor,
    // the corner points did not move, and the doorway is still last.
    ENJIN_EXPECT_FLOAT_NEAR(p.height, 5.0f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(p.thickness, 0.5f, 1e-6f);
    ENJIN_ASSERT_EQ(solid.brushes.size(), static_cast<usize>(3));
    for (usize i = 0; i < 2; ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(solid.brushes[i].halfExtents.y * 2.0f, 5.0f, 1e-4f);
        ENJIN_EXPECT_FLOAT_NEAR(solid.brushes[i].halfExtents.z * 2.0f, 0.5f, 1e-4f);
        ENJIN_EXPECT_FLOAT_NEAR(solid.brushes[i].center.y - solid.brushes[i].halfExtents.y, 0.0f, 1e-4f);
    }
    ENJIN_EXPECT_TRUE(NearXZ(p.points[1], Vector3(4.0f, 0.0f, 0.0f), 1e-6f));
    ENJIN_EXPECT_TRUE(solid.brushes[2].op == Geometry::BrushOp::Subtract);

    // Out of range is clamped to the Wall tool's own range, not refused.
    ENJIN_ASSERT_TRUE(ResizeWallPath(p, solid, 100.0f, 0.0f));
    ENJIN_EXPECT_FLOAT_NEAR(p.height, kCreativeWallHeightMax, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(p.thickness, kCreativeWallThicknessMin, 1e-6f);
}

ENJIN_TEST(CreativeShape, WallPointsCanBeAddedAndRemoved) {
    // Arrange
    ECS::WallPathComponent p = MakeCornerPath();
    p.bows[0] = 1.0f;

    // Act / Assert: a corner added in a bowed span lands on the curve's middle.
    ENJIN_ASSERT_TRUE(InsertWallPoint(p, 0));
    ENJIN_ASSERT_EQ(p.points.size(), static_cast<usize>(4));
    ENJIN_ASSERT_EQ(p.bows.size(), static_cast<usize>(3));
    ENJIN_EXPECT_TRUE(NearXZ(p.points[1], Vector3(2.0f, 0.0f, 1.0f), 1e-4f));

    ENJIN_EXPECT_TRUE(RemoveWallPoint(p, 1));
    ENJIN_EXPECT_EQ(p.points.size(), static_cast<usize>(3));
    ENJIN_EXPECT_EQ(p.bows.size(), static_cast<usize>(2));
    ENJIN_EXPECT_TRUE(RemoveWallPoint(p, 0));
    ENJIN_EXPECT_FALSE(RemoveWallPoint(p, 0));   // a wall keeps two points
}

ENJIN_TEST(CreativeShape, DraggingAShorelineEdgeMovesTheWholeEdge) {
    // Arrange: a 10 x 10 pond around an entity at (5, 1, 5).
    ECS::BoundaryPolygonComponent o;
    o.points = {Vector2(-5.0f, -5.0f), Vector2(5.0f, -5.0f), Vector2(5.0f, 5.0f), Vector2(-5.0f, 5.0f)};
    const ECS::BoundaryPolygonComponent start = o;
    const Vector3 origin(5.0f, 1.0f, 5.0f);
    std::vector<ShapeHandle> hs;
    OutlineHandles(o, origin, hs);
    ENJIN_ASSERT_EQ(hs.size(), static_cast<usize>(8));
    const ShapeHandle* east = FindHandle(hs, ShapeHandleKind::OutlineEdge, 1);   // (5,-5)->(5,5)
    ENJIN_ASSERT_NOT_NULL(east);
    ENJIN_EXPECT_TRUE(NearXZ(east->position, Vector3(10.0f, 1.0f, 5.0f), 1e-4f));

    // Act: pull it 3 m further east.
    ENJIN_ASSERT_TRUE(DragOutlineHandle(o, start, *east, origin, Vector3(13.0f, 1.0f, 5.0f)));

    // Assert: both of its points moved, the other two did not.
    ENJIN_EXPECT_TRUE(o.points[1].x == 8.0f && o.points[1].y == -5.0f);
    ENJIN_EXPECT_TRUE(o.points[2].x == 8.0f && o.points[2].y == 5.0f);
    ENJIN_EXPECT_TRUE(o.points[0].x == -5.0f && o.points[3].x == -5.0f);
    ENJIN_EXPECT_TRUE(o.dirty);
}

ENJIN_TEST(CreativeShape, AShorelineDraggedFlatIsRefused) {
    // Arrange: a triangle; drag its apex onto the base line.
    ECS::BoundaryPolygonComponent o;
    o.points = {Vector2(0.0f, 0.0f), Vector2(4.0f, 0.0f), Vector2(2.0f, 3.0f)};
    const ECS::BoundaryPolygonComponent start = o;
    std::vector<ShapeHandle> hs;
    OutlineHandles(o, Vector3(), hs);

    // Act
    const bool moved = DragOutlineHandle(o, start, *FindHandle(hs, ShapeHandleKind::OutlinePoint, 2),
                                         Vector3(), Vector3(2.0f, 0.0f, 0.0f));

    // Assert
    ENJIN_EXPECT_FALSE(moved);
    ENJIN_EXPECT_TRUE(o.points[2].y == 3.0f);
}

ENJIN_TEST(CreativeShape, ShorelinePointsCanBeAddedAndRemovedDownToATriangle) {
    ECS::BoundaryPolygonComponent o;
    o.points = {Vector2(0.0f, 0.0f), Vector2(4.0f, 0.0f), Vector2(4.0f, 4.0f), Vector2(0.0f, 4.0f)};
    ENJIN_ASSERT_TRUE(InsertOutlinePoint(o, 0));
    ENJIN_EXPECT_TRUE(o.points.size() == 5 && o.points[1].x == 2.0f && o.points[1].y == 0.0f);
    ENJIN_EXPECT_TRUE(RemoveOutlinePoint(o, 1));
    ENJIN_EXPECT_TRUE(RemoveOutlinePoint(o, 0));
    ENJIN_EXPECT_FALSE(RemoveOutlinePoint(o, 0));
    ENJIN_EXPECT_EQ(o.points.size(), static_cast<usize>(3));
}

ENJIN_TEST(CreativeShape, ASurfaceWaterEdgeMovesAndTheFarEdgeStays) {
    // Arrange: a 10 x 6 surface centred on (0, 2, 0).
    Vector3 centre(0.0f, 2.0f, 0.0f);
    f32 width = 10.0f, depth = 6.0f;

    // Act: drag the +X edge to x = 9.
    ENJIN_ASSERT_TRUE(DragRectGrip(centre, width, depth, BrushGrip::MaxX, Vector3(9.0f, 0.0f, 0.0f)));

    // Assert: the -X edge is still at -5, the height is untouched.
    ENJIN_EXPECT_FLOAT_NEAR(width, 14.0f, 1e-4f);
    ENJIN_EXPECT_FLOAT_NEAR(centre.x - width * 0.5f, -5.0f, 1e-4f);
    ENJIN_EXPECT_FLOAT_NEAR(depth, 6.0f, 1e-6f);
    ENJIN_EXPECT_FLOAT_NEAR(centre.y, 2.0f, 1e-6f);
}

ENJIN_TEST(CreativeShape, ASingleBoxStaysABox) {
    // Arrange: one wall-shaped brush, which is also what a plank or a door slab
    // made with the Box tool looks like.
    BuildToolSettings s;
    ECS::BrushSolidComponent solid;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildBrushes(BuildTool::Wall, s, false,
                                                 Vector3(1.0f, 0.0f, 2.0f), Vector3(5.0f, 0.0f, 5.0f), solid));

    // Act
    ECS::WallPathComponent path;
    const bool ok = RecoverWallPath(solid, path);

    // Assert: refused, so it keeps its height and thickness grips.
    ENJIN_EXPECT_FALSE(ok);
}

ENJIN_TEST(CreativeShape, AnOldTwoWallRunGetsItsLineBack) {
    // Arrange: two walls meeting at a corner, with no line kept.
    BuildToolSettings s;
    const std::vector<Vector3> pts = {Vector3(1.0f, 0.0f, 2.0f), Vector3(5.0f, 0.0f, 2.0f),
                                      Vector3(5.0f, 0.0f, 6.0f)};
    ECS::BrushSolidComponent solid;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildPathBrushes(pts, {0.0f, 0.0f}, 8, s, false, solid));

    // Act
    ECS::WallPathComponent path;
    ENJIN_ASSERT_TRUE(RecoverWallPath(solid, path));

    // Assert
    ENJIN_ASSERT_EQ(path.points.size(), static_cast<usize>(3));
    for (usize i = 0; i < 3; ++i) ENJIN_EXPECT_TRUE(NearXZ(path.points[i], pts[i], 1e-3f));
    ENJIN_EXPECT_FLOAT_NEAR(path.height, s.height, 1e-4f);
    ENJIN_EXPECT_FLOAT_NEAR(path.thickness, s.thickness, 1e-4f);
    ENJIN_EXPECT_EQ(path.builtBrushes, 2u);
}

ENJIN_TEST(CreativeShape, AnOldPathGetsItsCornersBack) {
    // Arrange: a three-corner path, mitred at its joints, line thrown away.
    BuildToolSettings s;
    const std::vector<Vector3> pts = {Vector3(0.0f, 0.0f, 0.0f), Vector3(4.0f, 0.0f, 0.0f),
                                      Vector3(6.0f, 0.0f, 3.0f), Vector3(6.0f, 0.0f, 7.0f)};
    ECS::BrushSolidComponent solid;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildPathBrushes(pts, {0.0f, 0.0f, 0.0f}, 8, s, false, solid));

    // Act
    ECS::WallPathComponent path;
    ENJIN_ASSERT_TRUE(RecoverWallPath(solid, path));

    // Assert: the corners, not the mitred brush ends.
    ENJIN_ASSERT_EQ(path.points.size(), static_cast<usize>(4));
    for (usize i = 0; i < 4; ++i) ENJIN_EXPECT_TRUE(NearXZ(path.points[i], pts[i], 1e-3f));
}

ENJIN_TEST(CreativeShape, AFloorIsNotMistakenForAWall) {
    BuildToolSettings s;
    ECS::BrushSolidComponent floor;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildBrushes(BuildTool::Floor, s, false,
                                                 Vector3(0.0f, 0.0f, 0.0f), Vector3(4.0f, 0.0f, 0.5f), floor));
    ECS::WallPathComponent path;
    ENJIN_EXPECT_FALSE(RecoverWallPath(floor, path));

}

ENJIN_TEST(CreativeShape, AnOldWallWithADoorwayKeepsTheDoorway) {
    // Arrange: two walls, then a doorway cut through the first afterwards.
    BuildToolSettings s;
    ECS::BrushSolidComponent wall;
    ENJIN_ASSERT_TRUE(CreativeMode::BuildPathBrushes(
        {Vector3(0.0f, 0.0f, 0.0f), Vector3(4.0f, 0.0f, 0.0f), Vector3(4.0f, 0.0f, 4.0f)},
        {0.0f, 0.0f}, 8, s, false, wall));
    ECS::BrushSolidComponent::Brush door;
    door.op = Geometry::BrushOp::Subtract;
    door.center = Vector3(2.0f, 1.0f, 0.0f);
    wall.brushes.push_back(door);

    // Act
    ECS::WallPathComponent path;
    ENJIN_ASSERT_TRUE(RecoverWallPath(wall, path));
    path.points[2] = Vector3(4.0f, 0.0f, 6.0f);
    ENJIN_ASSERT_TRUE(RebuildWallPath(path, wall));

    // Assert: the line counts only the walls, and the doorway survives a rebuild.
    ENJIN_EXPECT_EQ(path.builtBrushes, 2u);
    ENJIN_ASSERT_EQ(wall.brushes.size(), static_cast<usize>(3));
    ENJIN_EXPECT_TRUE(wall.brushes[2].op == Geometry::BrushOp::Subtract);

    // An Add after the cut is not a wall with a door in it.
    wall.brushes.push_back(wall.brushes[0]);
    ENJIN_EXPECT_FALSE(RecoverWallPath(wall, path));
}

// ---------------------------------------------------------------------------
// Roof
// ---------------------------------------------------------------------------

namespace {

// Is the point still roof, after the cuts? Inside an Add box and inside no cut.
bool InRoof(const ECS::BrushSolidComponent& roof, const Vector3& p) {
    bool solid = false;
    for (const auto& b : roof.brushes) {
        if (b.op == Geometry::BrushOp::Add && PointInsideBoxBrush(b, p)) solid = true;
    }
    for (const auto& b : roof.brushes) {
        if (b.op == Geometry::BrushOp::Subtract && PointInsideBoxBrush(b, p)) return false;
    }
    return solid;
}

} // namespace

ENJIN_TEST(CreativeMode, AFlatRoofSitsOnItsHeightAndOverhangsTheDrag) {
    BuildToolSettings s;
    s.roofKind = static_cast<f32>(RoofKind::Flat);
    s.roofBase = 3.0f;
    s.thickness = 0.2f;
    s.overhang = 0.5f;

    const auto roof = Build(BuildTool::Roof, s, false, Vector3(0, 0, 0), Vector3(6, 0, 4));

    ENJIN_ASSERT_EQ(roof.brushes.size(), static_cast<usize>(1));
    const auto& b = roof.brushes[0];
    // On top of the walls, not hanging into the room the way a floor hangs
    // below its elevation.
    ENJIN_EXPECT_FLOAT_NEAR(b.center.y - b.halfExtents.y, 3.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.x * 2.0f, 6.0f + 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(b.halfExtents.z * 2.0f, 4.0f + 1.0f, 0.001f);
}

// The pitch is real: the slope is where the angle says, and both ends are
// closed. This is the test that fails if a cutter is turned the wrong way,
// which leaves a roof-shaped HOLE in a block instead of a roof.
ENJIN_TEST(CreativeMode, AGableSlopesBothWaysToARidgeAlongTheLongSide) {
    BuildToolSettings s;
    s.roofKind = static_cast<f32>(RoofKind::Gable);
    s.roofBase = 3.0f;
    s.pitch = 45.0f;
    s.overhang = 0.0f;

    // 8 long in X, 4 across in Z: ridge along X, at z = 2, rising 2 at 45 deg.
    const auto roof = Build(BuildTool::Roof, s, false, Vector3(0, 0, 0), Vector3(8, 0, 4));

    ENJIN_ASSERT_EQ(roof.brushes.size(), static_cast<usize>(3));
    ENJIN_EXPECT_TRUE(roof.brushes[0].op == Geometry::BrushOp::Add);
    ENJIN_EXPECT_TRUE(roof.brushes[1].op == Geometry::BrushOp::Subtract);
    ENJIN_EXPECT_TRUE(roof.brushes[2].op == Geometry::BrushOp::Subtract);

    const f32 eave = 3.0f + kCreativeRoofFascia;
    // Under the ridge, almost at the top.
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(4.0f, eave + 1.9f, 2.0f)));
    // At the same height over an eave: that corner is cut away, on both sides.
    ENJIN_EXPECT_FALSE(InRoof(roof, Vector3(4.0f, eave + 1.9f, 0.3f)));
    ENJIN_EXPECT_FALSE(InRoof(roof, Vector3(4.0f, eave + 1.9f, 3.7f)));
    // Either side of the slope, a quarter of the way in: 1 m in is 1 m up.
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(4.0f, eave + 0.9f, 1.0f)));
    ENJIN_EXPECT_FALSE(InRoof(roof, Vector3(4.0f, eave + 1.1f, 1.0f)));
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(4.0f, eave + 0.9f, 3.0f)));
    ENJIN_EXPECT_FALSE(InRoof(roof, Vector3(4.0f, eave + 1.1f, 3.0f)));
    // The gable END is closed: solid right up to the end wall, under the ridge.
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(0.05f, eave + 1.5f, 2.0f)));
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(7.95f, eave + 1.5f, 2.0f)));
    // And the fascia under the eave is left standing.
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(4.0f, 3.0f + kCreativeRoofFascia * 0.5f, 0.05f)));
}

ENJIN_TEST(CreativeMode, TheRidgeTurnsWhenTheDragIsLongerTheOtherWay) {
    BuildToolSettings s;
    s.roofKind = static_cast<f32>(RoofKind::Gable);
    s.roofBase = 0.0f;
    s.pitch = 45.0f;
    s.overhang = 0.0f;

    // 4 across in X, 8 long in Z: ridge along Z, at x = 2.
    const auto roof = Build(BuildTool::Roof, s, false, Vector3(0, 0, 0), Vector3(4, 0, 8));

    ENJIN_ASSERT_EQ(roof.brushes.size(), static_cast<usize>(3));
    const f32 eave = kCreativeRoofFascia;
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(2.0f, eave + 1.9f, 4.0f)));
    ENJIN_EXPECT_FALSE(InRoof(roof, Vector3(0.3f, eave + 1.9f, 4.0f)));
    ENJIN_EXPECT_FALSE(InRoof(roof, Vector3(3.7f, eave + 1.9f, 4.0f)));
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(1.0f, eave + 0.9f, 4.0f)));
    ENJIN_EXPECT_FALSE(InRoof(roof, Vector3(1.0f, eave + 1.1f, 4.0f)));
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(3.0f, eave + 0.9f, 4.0f)));
    ENJIN_EXPECT_FALSE(InRoof(roof, Vector3(3.0f, eave + 1.1f, 4.0f)));
}

ENJIN_TEST(CreativeMode, AShedHasOneSlopeAndClimbsTheWholeSpan) {
    BuildToolSettings s;
    s.roofKind = static_cast<f32>(RoofKind::Shed);
    s.roofBase = 0.0f;
    s.pitch = 45.0f;
    s.overhang = 0.0f;

    const auto roof = Build(BuildTool::Roof, s, false, Vector3(0, 0, 0), Vector3(8, 0, 4));

    ENJIN_ASSERT_EQ(roof.brushes.size(), static_cast<usize>(2));
    const f32 eave = kCreativeRoofFascia;
    // Low at z = 4, high at z = 0, rising 4 over the 4 m span.
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(4.0f, eave + 3.8f, 0.1f)));
    ENJIN_EXPECT_FALSE(InRoof(roof, Vector3(4.0f, eave + 0.5f, 3.9f)));
    ENJIN_EXPECT_TRUE(InRoof(roof, Vector3(4.0f, eave + 1.9f, 2.0f)));
    ENJIN_EXPECT_FALSE(InRoof(roof, Vector3(4.0f, eave + 2.1f, 2.0f)));
}

ENJIN_TEST(CreativeMode, ARoofNeedsBothDimensionsAndCannotBeSubtracted) {
    BuildToolSettings s;
    bool ok = true;
    Build(BuildTool::Roof, s, false, Vector3(0, 0, 0), Vector3(6, 0, 0), &ok);
    ENJIN_EXPECT_FALSE(ok);
    // Its slopes are already cuts. Subtracting the whole roof from something
    // would turn them inside out, so the toggle is not offered.
    ENJIN_EXPECT_FALSE(BuildToolCanSubtract(BuildTool::Roof));
    ENJIN_EXPECT_TRUE(BuildToolModeLabels(BuildTool::Roof) == nullptr);
}

// ---------------------------------------------------------------------------
// Door: where the click lands
// ---------------------------------------------------------------------------

namespace {

// The Wall tool's own wall, 6 m along X, 3 m tall, 0.25 thick, foot on y = 0.
ECS::BrushSolidComponent::Brush StraightWall() {
    BuildToolSettings s;
    return Build(BuildTool::Wall, s, false, Vector3(0, 0, 0), Vector3(6, 0, 0)).brushes[0];
}

} // namespace

ENJIN_TEST(CreativeMode, ARayLandsOnTheNearFaceOfAWall) {
    const auto wall = StraightWall();

    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(RayHitBoxBrush(wall, Vector3(2.0f, 1.0f, 5.0f), Vector3(0, 0, -1), hit));

    ENJIN_EXPECT_FLOAT_NEAR(hit.point.x, 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(hit.point.y, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(hit.point.z, 0.125f, 0.001f);   // the face toward the ray
    ENJIN_EXPECT_EQ(static_cast<int>(hit.axis), 2);
    ENJIN_EXPECT_FLOAT_NEAR(hit.t, 4.875f, 0.001f);
}

ENJIN_TEST(CreativeMode, ARayThatMissesOrStartsInsideTheWallHitsNothing) {
    const auto wall = StraightWall();
    BrushRayHit hit;

    ENJIN_EXPECT_FALSE(RayHitBoxBrush(wall, Vector3(9.0f, 1.0f, 5.0f), Vector3(0, 0, -1), hit));
    // Pointing away from it.
    ENJIN_EXPECT_FALSE(RayHitBoxBrush(wall, Vector3(2.0f, 1.0f, 5.0f), Vector3(0, 0, 1), hit));
    // From inside: there is no face in front to put a door in.
    ENJIN_EXPECT_FALSE(RayHitBoxBrush(wall, Vector3(2.0f, 1.0f, 0.0f), Vector3(0, 0, -1), hit));
    // Parallel to the faces and outside them.
    ENJIN_EXPECT_FALSE(RayHitBoxBrush(wall, Vector3(-2.0f, 1.0f, 1.0f), Vector3(1, 0, 0), hit));

    ECS::BrushSolidComponent::Brush prism;
    prism.shape = ECS::BrushSolidComponent::Shape::Prism;
    ENJIN_EXPECT_FALSE(RayHitBoxBrush(prism, Vector3(0, 0, 5), Vector3(0, 0, -1), hit));
}

ENJIN_TEST(CreativeMode, ADoorIsCutThroughTheWallWhereYouClicked) {
    const auto wall = StraightWall();
    BuildToolSettings s;
    s.openingKind = static_cast<f32>(OpeningKind::Door);
    s.doorWidth = 1.0f;
    s.doorHeight = 2.1f;

    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(RayHitBoxBrush(wall, Vector3(2.0f, 1.0f, 5.0f), Vector3(0, 0, -1), hit));
    OpeningPlan plan;
    ENJIN_ASSERT_TRUE(PlanOpening(wall, hit, s, 0.0f, plan));

    ENJIN_EXPECT_TRUE(plan.cut.op == Geometry::BrushOp::Subtract);
    // Centred on the click along the wall, whatever height was clicked.
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.x, 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.halfExtents.x * 2.0f, 1.0f, 0.001f);
    // From just under the floor line to the door's height.
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.y - plan.cut.halfExtents.y, -kCreativeOpeningOvercut, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.y + plan.cut.halfExtents.y, 2.1f, 0.001f);
    // All the way through, and a little past both faces.
    ENJIN_EXPECT_TRUE(plan.cut.halfExtents.z > wall.halfExtents.z);
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.z, 0.0f, 0.001f);

    ENJIN_EXPECT_FLOAT_NEAR(plan.width, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(plan.height, 2.1f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(plan.wallThickness, 0.25f, 0.001f);
    // The hinge is the foot of the opening's edge.
    ENJIN_EXPECT_FLOAT_NEAR(plan.hinge.x, 1.5f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(plan.hinge.y, 0.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(plan.hinge.z, 0.0f, 0.001f);
    const Vector3 leaf = plan.rotation.Rotate(Vector3(1, 0, 0));
    ENJIN_EXPECT_FLOAT_NEAR(leaf.x, 1.0f, 0.001f);
}

ENJIN_TEST(CreativeMode, AnOpeningStaysInsideTheWallsEnds) {
    const auto wall = StraightWall();
    BuildToolSettings s;
    s.doorWidth = 1.0f;

    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(RayHitBoxBrush(wall, Vector3(0.1f, 1.0f, 5.0f), Vector3(0, 0, -1), hit));
    OpeningPlan plan;
    ENJIN_ASSERT_TRUE(PlanOpening(wall, hit, s, 0.0f, plan));

    // Clicked 10 cm from the end; pushed in until a jamb is left.
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.x - plan.cut.halfExtents.x, kCreativeOpeningJamb, 0.001f);
}

ENJIN_TEST(CreativeMode, AnOpeningSnapsFromTheWallsEndNotItsMiddle) {
    // 5 m long, so its middle is at 2.5: half a cell off a 1 m grid.
    BuildToolSettings ws;
    const auto wall = Build(BuildTool::Wall, ws, false, Vector3(0, 0, 0), Vector3(5, 0, 0)).brushes[0];
    BuildToolSettings s;
    s.doorWidth = 1.0f;

    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(RayHitBoxBrush(wall, Vector3(2.2f, 1.0f, 5.0f), Vector3(0, 0, -1), hit));
    OpeningPlan plan;
    ENJIN_ASSERT_TRUE(PlanOpening(wall, hit, s, 1.0f, plan));

    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.x, 2.0f, 0.001f);
}

ENJIN_TEST(CreativeMode, AWallTooShortForTheOpeningIsRefused) {
    BuildToolSettings ws;
    const auto wall = Build(BuildTool::Wall, ws, false, Vector3(0, 0, 0), Vector3(1, 0, 0)).brushes[0];
    BuildToolSettings s;
    s.doorWidth = 1.0f;   // a 1 m door in a 1 m wall leaves no jambs

    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(RayHitBoxBrush(wall, Vector3(0.5f, 1.0f, 5.0f), Vector3(0, 0, -1), hit));
    OpeningPlan plan;
    ENJIN_EXPECT_FALSE(PlanOpening(wall, hit, s, 0.0f, plan));
}

ENJIN_TEST(CreativeMode, ADoorTallerThanTheWallOpensItToTheTop) {
    BuildToolSettings ws;
    ws.height = 2.0f;
    const auto wall = Build(BuildTool::Wall, ws, false, Vector3(0, 0, 0), Vector3(6, 0, 0)).brushes[0];
    BuildToolSettings s;
    s.doorHeight = 2.1f;

    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(RayHitBoxBrush(wall, Vector3(3.0f, 1.0f, 5.0f), Vector3(0, 0, -1), hit));
    OpeningPlan plan;
    ENJIN_ASSERT_TRUE(PlanOpening(wall, hit, s, 0.0f, plan));

    // Past the top, so no paper-thin lintel is left across the gap.
    ENJIN_EXPECT_TRUE(plan.cut.center.y + plan.cut.halfExtents.y > 2.0f);
    ENJIN_EXPECT_FLOAT_NEAR(plan.height, 2.0f, 0.001f);
}

ENJIN_TEST(CreativeMode, AWindowSitsOnItsSillAndLeavesALintel) {
    const auto wall = StraightWall();   // 3 m tall
    BuildToolSettings s;
    s.openingKind = static_cast<f32>(OpeningKind::Window);
    s.windowWidth = 1.2f;
    s.windowHeight = 1.2f;
    s.sill = 0.9f;

    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(RayHitBoxBrush(wall, Vector3(3.0f, 2.5f, 5.0f), Vector3(0, 0, -1), hit));
    OpeningPlan plan;
    ENJIN_ASSERT_TRUE(PlanOpening(wall, hit, s, 0.0f, plan));

    // The sill sets the height, not where on the wall the click landed.
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.y - plan.cut.halfExtents.y, 0.9f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.y + plan.cut.halfExtents.y, 2.1f, 0.001f);

    // Asked for more than the wall has: stops short of the top.
    s.windowHeight = 4.0f;
    ENJIN_ASSERT_TRUE(PlanOpening(wall, hit, s, 0.0f, plan));
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.y + plan.cut.halfExtents.y,
                            3.0f - kCreativeOpeningLintel, 0.001f);

    // A sill above the wall leaves nothing to cut.
    s.sill = 3.5f;
    ENJIN_EXPECT_FALSE(PlanOpening(wall, hit, s, 0.0f, plan));
}

ENJIN_TEST(CreativeMode, TheTopOfAWallIsNotADoor) {
    const auto wall = StraightWall();
    BuildToolSettings s;

    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(RayHitBoxBrush(wall, Vector3(3.0f, 9.0f, 0.0f), Vector3(0, -1, 0), hit));
    ENJIN_EXPECT_EQ(static_cast<int>(hit.axis), 1);
    OpeningPlan plan;
    ENJIN_EXPECT_FALSE(PlanOpening(wall, hit, s, 0.0f, plan));
}

ENJIN_TEST(CreativeMode, ADoorInADiagonalWallTurnsWithIt) {
    BuildToolSettings ws;
    const auto wall = Build(BuildTool::Wall, ws, false, Vector3(0, 0, 0), Vector3(4, 0, 4)).brushes[0];
    BuildToolSettings s;
    s.doorWidth = 1.0f;

    // Square on to the wall, from its (+x, -z) side, aimed at its middle.
    const Vector3 mid(2.0f, 1.0f, 2.0f);
    const Vector3 dir(-0.70710678f, 0.0f, 0.70710678f);
    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(RayHitBoxBrush(wall, mid - dir * 5.0f, dir, hit));
    OpeningPlan plan;
    ENJIN_ASSERT_TRUE(PlanOpening(wall, hit, s, 0.0f, plan));

    // On the wall's line (x == z), and turned the way the wall is.
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.x, 2.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.z, 2.0f, 0.01f);
    const Vector3 a = plan.cut.rotation.Rotate(Vector3(1, 0, 0));
    const Vector3 b = wall.rotation.Rotate(Vector3(1, 0, 0));
    ENJIN_EXPECT_FLOAT_NEAR(a.x, b.x, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(a.z, b.z, 0.001f);
    // The hinge is half a door back along the wall from the middle.
    ENJIN_EXPECT_FLOAT_NEAR(plan.hinge.x, plan.hinge.z, 0.01f);
    const f32 back = std::sqrt((plan.hinge.x - 2.0f) * (plan.hinge.x - 2.0f) * 2.0f);
    ENJIN_EXPECT_FLOAT_NEAR(back, 0.5f, 0.01f);
}

// Any box, not only one the Wall tool made: the opening goes through the face
// that was clicked. Here that face is across X, so the door runs along Z and
// its leaf has to turn a quarter to follow.
ENJIN_TEST(CreativeMode, ADoorGoesThroughWhicheverFaceWasClicked) {
    ECS::BrushSolidComponent::Brush block;
    block.center = Vector3(0.0f, 1.5f, 0.0f);
    block.halfExtents = Vector3(0.2f, 1.5f, 3.0f);
    BuildToolSettings s;
    s.doorWidth = 1.0f;

    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(RayHitBoxBrush(block, Vector3(5.0f, 1.0f, 1.0f), Vector3(-1, 0, 0), hit));
    ENJIN_EXPECT_EQ(static_cast<int>(hit.axis), 0);
    OpeningPlan plan;
    ENJIN_ASSERT_TRUE(PlanOpening(block, hit, s, 0.0f, plan));

    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.halfExtents.z * 2.0f, 1.0f, 0.001f);   // width along Z
    ENJIN_EXPECT_TRUE(plan.cut.halfExtents.x > block.halfExtents.x);        // through X
    ENJIN_EXPECT_FLOAT_NEAR(plan.cut.center.z, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(plan.wallThickness, 0.4f, 0.001f);

    ENJIN_EXPECT_FLOAT_NEAR(plan.hinge.z, 0.5f, 0.001f);
    const Vector3 leaf = plan.rotation.Rotate(Vector3(1, 0, 0));
    ENJIN_EXPECT_FLOAT_NEAR(leaf.z, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(leaf.x, 0.0f, 0.001f);
}

// A box test knows nothing about the hole already in it. Without the cut check
// a click through a doorway lands on the air in the doorway and cuts the same
// wall again, instead of reaching the wall behind.
ENJIN_TEST(CreativeMode, AClickThroughADoorwayDoesNotHitTheWallItIsIn) {
    ECS::BrushSolidComponent solid;
    solid.brushes.push_back(StraightWall());
    BuildToolSettings s;
    s.doorWidth = 1.0f;

    usize index = 99;
    BrushRayHit hit;
    const Vector3 from(2.0f, 1.0f, 5.0f), dir(0, 0, -1);
    ENJIN_ASSERT_TRUE(FindOpeningTarget(solid, from, dir, index, hit));
    ENJIN_EXPECT_EQ(index, static_cast<usize>(0));

    OpeningPlan plan;
    ENJIN_ASSERT_TRUE(PlanOpening(solid.brushes[0], hit, s, 0.0f, plan));
    solid.brushes.push_back(plan.cut);

    // Through the doorway now: nothing.
    ENJIN_EXPECT_FALSE(FindOpeningTarget(solid, from, dir, index, hit));
    // Beside it: still the wall.
    ENJIN_EXPECT_TRUE(FindOpeningTarget(solid, Vector3(4.5f, 1.0f, 5.0f), dir, index, hit));
    // Above it, on the lintel: still the wall.
    ENJIN_EXPECT_TRUE(FindOpeningTarget(solid, Vector3(2.0f, 2.6f, 5.0f), dir, index, hit));

    // A wall behind is reached through the doorway.
    BuildToolSettings ws;
    solid.brushes.push_back(
        Build(BuildTool::Wall, ws, false, Vector3(0, 0, -3), Vector3(6, 0, -3)).brushes[0]);
    ENJIN_ASSERT_TRUE(FindOpeningTarget(solid, from, dir, index, hit));
    ENJIN_EXPECT_EQ(index, static_cast<usize>(2));
}

ENJIN_TEST(CreativeMode, TheNearestWallWinsAndDisabledOnesAreSkipped) {
    BuildToolSettings ws;
    ECS::BrushSolidComponent solid;
    solid.brushes.push_back(
        Build(BuildTool::Wall, ws, false, Vector3(0, 0, -3), Vector3(6, 0, -3)).brushes[0]);
    solid.brushes.push_back(StraightWall());

    usize index = 99;
    BrushRayHit hit;
    ENJIN_ASSERT_TRUE(FindOpeningTarget(solid, Vector3(2, 1, 5), Vector3(0, 0, -1), index, hit));
    ENJIN_EXPECT_EQ(index, static_cast<usize>(1));

    solid.brushes[1].enabled = false;
    ENJIN_ASSERT_TRUE(FindOpeningTarget(solid, Vector3(2, 1, 5), Vector3(0, 0, -1), index, hit));
    ENJIN_EXPECT_EQ(index, static_cast<usize>(0));
}

// ---------------------------------------------------------------------------
// Named choices, props and paint
// ---------------------------------------------------------------------------

// A Kind used to be a number box: Water 0..1, Plants 0..2, Prop 0..4, with
// nothing saying which number was the barrel. Every choice now carries a name
// for every value it can take.
ENJIN_TEST(CreativeMode, EveryChoiceHasANameForEveryValue) {
    for (u8 i = 0; i < static_cast<u8>(BuildTool::Count); ++i) {
        BuildToolSettings s;
        BuildField fields[kBuildMaxFields];
        const u32 n = BuildToolFields(static_cast<BuildTool>(i), s, fields, kBuildMaxFields);
        for (u32 f = 0; f < n; ++f) {
            if (fields[f].kind != BuildFieldKind::Choice) continue;
            ENJIN_ASSERT_TRUE(fields[f].choices != nullptr);
            ENJIN_EXPECT_TRUE(fields[f].choiceCount >= 2);
            ENJIN_EXPECT_FLOAT_NEAR(fields[f].maxValue,
                                    static_cast<f32>(fields[f].choiceCount - 1), 0.001f);
            for (u32 c = 0; c < fields[f].choiceCount; ++c) {
                ENJIN_ASSERT_TRUE(fields[f].choices[c] != nullptr);
                ENJIN_EXPECT_TRUE(fields[f].choices[c][0] != '\0');
            }
        }
    }
}

ENJIN_TEST(CreativeMode, NoKindIsABareNumberAnyMore) {
    const BuildTool withKinds[] = { BuildTool::Water, BuildTool::Plants, BuildTool::Prop,
                                    BuildTool::Cave, BuildTool::Roof, BuildTool::Door };
    for (BuildTool tool : withKinds) {
        BuildToolSettings s;
        BuildField fields[kBuildMaxFields];
        const u32 n = BuildToolFields(tool, s, fields, kBuildMaxFields);
        ENJIN_ASSERT_TRUE(n >= 1);
        ENJIN_EXPECT_TRUE(fields[0].kind == BuildFieldKind::Choice);
    }
}

ENJIN_TEST(CreativeMode, AChoiceLeftOutOfRangeIsBroughtBackIn) {
    BuildToolSettings s;
    s.propKind = 400.0f;
    s.plantKind = -3.0f;
    BuildField fields[kBuildMaxFields];

    BuildToolFields(BuildTool::Prop, s, fields, kBuildMaxFields);
    u32 count = 0;
    RailProps(count);
    ENJIN_EXPECT_FLOAT_NEAR(s.propKind, static_cast<f32>(count - 1), 0.001f);

    BuildToolFields(BuildTool::Plants, s, fields, kBuildMaxFields);
    ENJIN_EXPECT_FLOAT_NEAR(s.plantKind, 0.0f, 0.001f);
}

ENJIN_TEST(CreativeMode, EveryPropSaysWhereItComesFrom) {
    u32 count = 0;
    const RailProp* props = RailProps(count);
    ENJIN_ASSERT_TRUE(count >= 5);

    for (u32 i = 0; i < count; ++i) {
        ENJIN_EXPECT_TRUE(props[i].name && props[i].name[0] != '\0');
        if (props[i].kind == PropKind::Count) {
            // From the Entity menu: it has to name the entry.
            ENJIN_ASSERT_TRUE(props[i].menuGroup != nullptr);
            ENJIN_ASSERT_TRUE(props[i].menuLabel != nullptr);
            ENJIN_EXPECT_TRUE(props[i].menuLabel[0] != '\0');
        } else {
            // Block is the Brush tool's job, not a second button for a box.
            ENJIN_EXPECT_TRUE(props[i].kind != PropKind::Block);
        }
        for (u32 j = i + 1; j < count; ++j) {
            ENJIN_EXPECT_TRUE(std::strcmp(props[i].name, props[j].name) != 0);
        }
    }
}

ENJIN_TEST(CreativeMode, ThePropPickerOffersTheWholeTable) {
    BuildToolSettings s;
    BuildField fields[kBuildMaxFields];
    const u32 n = BuildToolFields(BuildTool::Prop, s, fields, kBuildMaxFields);
    u32 count = 0;
    const RailProp* props = RailProps(count);

    ENJIN_ASSERT_EQ(n, 1u);
    ENJIN_ASSERT_EQ(fields[0].choiceCount, count);
    for (u32 i = 0; i < count; ++i) ENJIN_EXPECT_STR_EQ(fields[0].choices[i], props[i].name);
}

ENJIN_TEST(CreativeMode, PaintOffersAColourAndAFinish) {
    BuildToolSettings s;
    BuildField fields[kBuildMaxFields];
    const u32 n = BuildToolFields(BuildTool::Paint, s, fields, kBuildMaxFields);

    ENJIN_ASSERT_EQ(n, 2u);
    ENJIN_EXPECT_TRUE(fields[0].kind == BuildFieldKind::Swatch);
    ENJIN_EXPECT_EQ(fields[0].choiceCount, kCreativePaintColourCount);
    ENJIN_EXPECT_TRUE(fields[1].kind == BuildFieldKind::Choice);
    ENJIN_EXPECT_EQ(fields[1].choiceCount, kCreativePaintFinishCount);

    for (u32 i = 0; i < kCreativePaintColourCount; ++i) {
        const PaintColour& c = kCreativePaintColours[i];
        ENJIN_EXPECT_TRUE(c.name[0] != '\0');
        ENJIN_EXPECT_TRUE(c.r >= 0.0f && c.r <= 1.0f && c.g >= 0.0f && c.g <= 1.0f &&
                          c.b >= 0.0f && c.b <= 1.0f);
    }
}

// A flat roof has no slope and a doorway has no sill. A box for either would be
// a control that does nothing.
ENJIN_TEST(CreativeMode, FieldsFollowTheKindTheyBelongTo) {
    auto has = [](BuildTool tool, BuildToolSettings& s, const char* label) {
        BuildField fields[kBuildMaxFields];
        const u32 n = BuildToolFields(tool, s, fields, kBuildMaxFields);
        for (u32 i = 0; i < n; ++i) if (std::strcmp(fields[i].label, label) == 0) return true;
        return false;
    };
    BuildToolSettings s;

    s.roofKind = static_cast<f32>(RoofKind::Flat);
    ENJIN_EXPECT_FALSE(has(BuildTool::Roof, s, "Pitch"));
    ENJIN_EXPECT_TRUE(has(BuildTool::Roof, s, "Thickness"));
    s.roofKind = static_cast<f32>(RoofKind::Gable);
    ENJIN_EXPECT_TRUE(has(BuildTool::Roof, s, "Pitch"));

    s.openingKind = static_cast<f32>(OpeningKind::Door);
    ENJIN_EXPECT_FALSE(has(BuildTool::Door, s, "Sill"));
    s.openingKind = static_cast<f32>(OpeningKind::Window);
    ENJIN_EXPECT_TRUE(has(BuildTool::Door, s, "Sill"));
}

ENJIN_TEST_MAIN()
