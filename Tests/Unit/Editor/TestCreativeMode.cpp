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

// The two creation paths do not overlap: BuildBrushes owns the brush tools and
// PlanPlacement owns the rest. Reduce and Edit belong to neither, because
// neither of them MAKES anything -- they act on something that is already
// there, and have no footprint of their own to describe.
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
        const bool notFromADrag =
            (tool == BuildTool::Reduce) || BuildToolIsEdit(tool) || BuildToolIsPath(tool);
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

ENJIN_TEST_MAIN()
