#pragma once

// Creative mode: a small, hand-drawn build surface for blocking out a level.
//
// The verb it is built around, and the thing every decision here answers to:
//
//     someone should be able to block out a playable level and press play
//     without ever opening a settings window.
//
// This is option B from the creative-mode investigation: a separate surface with
// its own vocabulary and its own look, drawn by us with ImDrawList, hosted by
// ImGui. The thirty-one dockable panels keep working and stay one click away.
// Nothing here replaces ImGui -- that was option C, and it costs months of work
// a player never sees.
//
// The split in this file is deliberate. Everything that decides WHAT GEOMETRY A
// GESTURE MAKES is pure and lives in BuildBrushes: a drag plus some numbers in,
// a brush list out. That is the part worth testing, and it is testable with no
// viewport, no ImGui and no GPU. The drawing is a separate concern layered on
// top of it.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"

#include <vector>
#include "Enjin/ECS/Components/BrushSolid.h"

namespace Enjin {
namespace Editor {

// The build tools, in rail order. Grouped on the surface as
// structure (Wall/Floor/Stairs), volume (Brush/Water/Terrain) and
// object (Ladder/Reduce).
enum class BuildTool : u8 {
    Wall = 0,
    Floor,
    Stairs,
    // A run of connected walls, straight or bowed. Sits with the other
    // structure tools because that is what it makes; it is only the GESTURE
    // that differs, being the first here that is not press-drag-release.
    Path,
    Brush,
    Water,
    Terrain,
    Ladder,
    Reduce,
    // Not a build tool: the one that changes what is already there. Last on the
    // rail and in its own group, because everything above it makes something and
    // this one does not.
    Edit,
    Count
};

ENJIN_API const char* BuildToolName(BuildTool tool);

// Which band of the rail a tool sits in: structure, volume, object, edit. A
// separator is drawn wherever this changes, and the surface's height budget
// counts them, so it lives here rather than in the drawing code.
ENJIN_API u8 BuildToolGroup(BuildTool tool);

// One line saying what the tool does, shown under its name. Present tense,
// second person, describing the gesture rather than the feature.
ENJIN_API const char* BuildToolVerb(BuildTool tool);

// Does this tool produce brush geometry from a DRAG -- two ground points and
// some numbers? Water, Terrain, Ladder, Reduce and Edit do not: they act on
// components or on something already in the scene. Path does not either, and it
// is the interesting one: it builds brushes, but from a list of clicked points
// rather than from a drag, so it has its own builder.
ENJIN_API bool BuildToolMakesBrushes(BuildTool tool);

// Does this tool build from a clicked point list instead of a drag?
ENJIN_API bool BuildToolIsPath(BuildTool tool);

// Does this tool change what is already there rather than making something new?
// It is the one tool a left click in the viewport must SELECT with, instead of
// starting a build drag.
ENJIN_API bool BuildToolIsEdit(BuildTool tool);

// Can this tool cut as well as add? A tool with no subtract mode must not show
// the toggle, or the surface offers a switch that does nothing.
ENJIN_API bool BuildToolCanSubtract(BuildTool tool);

// The numbers a tool is currently set to.
//
// Kept PER TOOL rather than globally, so dialling a wall to 4 m and switching to
// stairs and back does not silently reset it. Defaults are the ones a person
// blocking out a room would otherwise type first: a 3 m wall, a 0.25 m
// thickness, an 18 cm rise.
struct BuildToolSettings {
    f32 height    = 3.00f;   // Wall, Ladder, Brush
    f32 thickness = 0.25f;   // Wall, Floor
    f32 elevation = 0.00f;   // Floor: height of the slab's underside

    f32 rise  = 0.18f;       // Stairs: vertical per step
    f32 run   = 0.28f;       // Stairs: horizontal per step
    f32 width = 1.20f;       // Stairs

    // Water rides `elevation` for its surface height, the same field Floor uses
    // for its slab -- settings are per tool, so the two never collide. There is
    // no water DEPTH to author: Water3D is a surface, and the basin under it is
    // whatever you cut with the brush tools. A field labelled Depth that moved
    // the plane's footprint instead would be a lie on the surface.
    f32 waveScale = 0.35f;   // Water: wave height in metres

    f32 radius   = 4.00f;    // Terrain sculpt
    f32 strength = 0.60f;    // Terrain sculpt

    // Path: how many wall brushes a BOWED span is cut into. A straight span is
    // always one, however this is set -- there is nothing to approximate.
    f32 segments = 8.0f;

    f32 rungGap = 0.30f;     // Ladder: spacing of the rungs you can see
    f32 keepPercent = 50.0f; // Reduce

    // Brush: 4 is a box, more is a prism. An f32 so it is one more row in the
    // field table like everything else, rounded where it is used. It had no
    // control at all, which meant the engine could build a cylinder and a
    // person could not ask it to.
    f32 sides = 4.0f;

    // A field is INTEGRAL when a fraction of it means nothing. The options
    // column prints those without decimals, because "6.00 sides" invites you to
    // try 6.5.
    static bool FieldIsIntegral(const f32* value, const BuildToolSettings& s) {
        return value == &s.sides || value == &s.segments;
    }
};

// The grid sizes the surface offers. A blockout grid is a handful of round
// numbers, not a continuum, so these are one click each rather than a slider
// nobody can land on 0.5 with.
inline constexpr f32 kCreativeGridChoices[] = { 0.25f, 0.5f, 1.0f, 2.0f };
inline constexpr u32 kCreativeGridChoiceCount =
    static_cast<u32>(sizeof(kCreativeGridChoices) / sizeof(kCreativeGridChoices[0]));

// How far apart the grid lines are, in world units. Everything a tool places is
// snapped to this, because a blockout that is half a centimetre off reads as a
// bug in the engine rather than a slip of the mouse.
inline constexpr f32 kCreativeGridDefault = 0.25f;

// Width of the whole build surface (rail + options). The dockspace is inset by
// this while creative mode is on, so the two never overlap.
inline constexpr f32 kCreativeRailWidth    = 64.0f;
inline constexpr f32 kCreativeOptionsWidth = 232.0f;
inline constexpr f32 kCreativeSurfaceWidth = kCreativeRailWidth + kCreativeOptionsWidth;

// Guard rails on a drag. A gesture smaller than this is a click, not a drag, and
// building a zero-width wall from it produces a solid with no faces that renders
// nothing and looks like a broken tool.
inline constexpr f32 kCreativeMinDragLength = 0.05f;

// What a bowed span costs.
//
// Brush CSG has no curved face, so a curve is always N straight pieces and the
// only question is who picks N. Every one of them is also a CSG operand for
// anything cut through that wall later, which is why there is a ceiling at all
// rather than just a slider that goes up.
inline constexpr u32 kCreativePathSegmentsMax = 24;
inline constexpr u32 kCreativePathSegmentsMin = 2;

// A path has to terminate too, for the same reason a stair run does.
inline constexpr u32 kCreativePathMaxPoints = 64;

// Below this a span is straight, and a "bow" of a millimetre would otherwise
// buy 24 brushes that all lie on the same line.
inline constexpr f32 kCreativePathMinBow = 0.02f;

// A stair run has to terminate. Without a ceiling, a long drag with a small run
// asks for tens of thousands of tread brushes and the CSG build stops being
// interactive.
inline constexpr u32 kCreativeMaxStairSteps = 256;

// A fresh terrain, made by sculpting where there is no terrain yet. 64 cells of
// 1 m is a 64 m square: big enough to be a hillside, small enough that the
// heightmap rebuild stays interactive while you drag across it.
inline constexpr u32 kCreativeTerrainGrid = 64;
inline constexpr f32 kCreativeTerrainCell = 1.0f;

// A ladder is dragged along the wall it leans on, so one horizontal axis is its
// width and the other is its thickness. The thin axis is fixed rather than taken
// from the drag: a ladder as deep as it is wide is a crate.
inline constexpr f32 kCreativeLadderThin    = 0.12f;
inline constexpr f32 kCreativeLadderMinHalf = 0.25f;

// The grips on a brush's ground footprint: four that move one edge and four that
// move a corner.
//
// Named for the LOCAL edge each one moves, not a world direction, because a wall
// dragged along a diagonal carries a rotation and its own left edge is not the
// world's. Everything below works in the brush's frame and converts at the
// boundary.
enum class BrushGrip : u8 {
    MinX = 0, MaxX, MinZ, MaxZ,                 // one edge each
    MinXMinZ, MaxXMinZ, MinXMaxZ, MaxXMaxZ,     // a corner moves two
    Count
};

// A corner grip drives both axes; an edge grip drives one.
ENJIN_API bool BrushGripIsCorner(BrushGrip grip);

// Where a grip sits in world space. Y is the brush's centre height, which is
// where the handle is drawn -- height is not resized here (it stays a number in
// the options column, because a vertical handle over a floor reads as being
// about the floor's elevation as often as its thickness).
ENJIN_API Math::Vector3 BrushGripPosition(const ECS::BrushSolidComponent::Brush& brush,
                                          BrushGrip grip);

// The smallest a brush may be dragged down to. Below this it bounds nothing, the
// CSG build produces no faces, and the solid vanishes mid-gesture -- which reads
// as the tool deleting your work rather than as the drag being too small.
inline constexpr f32 kCreativeMinBrushExtent = 0.05f;

// Move one grip to a world point, keeping the OPPOSITE edge exactly where it is.
//
// That is the whole difference between this and the axis-arrow scaling the
// engine already has on a brush: ImGuizmo scales about the CENTRE, so pulling
// one arrow moves the far side too. "Make this room a metre wider" is not a
// scale, it is one edge moving.
//
// The point is expected to be grid-snapped already, the same way the build drag
// snaps before it builds. Returns false and leaves the brush untouched when the
// drag would collapse it.
ENJIN_API bool ResizeBrushByGrip(ECS::BrushSolidComponent::Brush& brush,
                                 BrushGrip grip,
                                 const Math::Vector3& worldPoint);

// One editable number on the surface, bound straight to the tool's settings.
//
// The options column is drawn from these rather than from a switch per tool, so
// adding a parameter is one row in one table and the surface picks it up. It is
// the same reason the options menu became a row list.
struct BuildField {
    const char* label = "";
    f32* value = nullptr;      // points into the tool's own settings
    f32 minValue = 0.0f;
    f32 maxValue = 1.0f;
    const char* unit = "";
};

inline constexpr u32 kBuildMaxFields = 4;

// Fills `out` with the tool's fields and returns how many. The pointers alias
// `settings`, so it must outlive them -- which it does, because it lives on the
// CreativeMode that owns the tool.
ENJIN_API u32 BuildToolFields(BuildTool tool, BuildToolSettings& settings,
                                 BuildField* out, u32 maxFields);

// The labels of a tool's two modes ("Add"/"Subtract", "Raise"/"Lower"), or
// nullptr when it has none.
ENJIN_API const char* const* BuildToolModeLabels(BuildTool tool);

// What a drag gives the tools that do NOT build brushes.
//
// Water, Terrain and Ladder each place or edit a component, and a component is
// not a brush list -- but the part a person can see going wrong is the same
// part: where it landed and how big it is. So that decision is pulled out here
// and made pure, for the same reason BuildBrushes is. The caller turns this into
// components; it does not do any deciding of its own.
struct ToolPlacement {
    // Where the new entity's transform goes. For Water this is the centre of the
    // surface at its authored height; for Ladder the centre of the climb volume;
    // for Terrain the CORNER the heightmap grows from, because a TerrainComponent
    // spans [0, grid * cellSize] out from its origin rather than straddling it.
    Math::Vector3 origin;

    // Half the footprint. y is half the tool's height, and is zero for the tools
    // that are flat.
    Math::Vector3 halfExtents;

    // Ladder: how many rungs fit in the height at the authored gap. Zero for
    // every other tool.
    u32 rungs = 0;
};

class ENJIN_API CreativeMode {
public:
    bool IsActive() const { return m_Active; }
    void SetActive(bool active) { m_Active = active; }

    BuildTool GetTool() const { return m_Tool; }
    void SetTool(BuildTool tool);

    // Cutting rather than adding. Reset when switching to a tool that cannot
    // subtract, so the mode can never be stuck on for a tool that ignores it.
    bool IsSubtracting() const { return m_Subtracting; }
    void SetSubtracting(bool subtracting);

    f32 GetGridSize() const { return m_GridSize; }
    void SetGridSize(f32 size) { m_GridSize = (size > 0.001f) ? size : kCreativeGridDefault; }
    bool IsSnapEnabled() const { return m_Snap; }
    void SetSnapEnabled(bool snap) { m_Snap = snap; }

    BuildToolSettings& Settings(BuildTool tool);
    const BuildToolSettings& Settings(BuildTool tool) const;
    BuildToolSettings& CurrentSettings() { return Settings(m_Tool); }
    const BuildToolSettings& CurrentSettings() const { return Settings(m_Tool); }

    // Snap a world point to the grid. Y is left alone: the grid is a floor plan,
    // and snapping height here would fight the per-tool elevation setting.
    Math::Vector3 SnapToGrid(const Math::Vector3& point) const;

    // Turn a ground-plane drag into the brushes the tool makes.
    //
    // Pure: no world, no camera, no ImGui. This is the whole behaviour of the
    // build tools, which is why it is a free function of its inputs rather than
    // something buried in a mouse handler.
    //
    // Returns false and leaves `out` untouched when the tool does not build
    // brushes, or the drag is too small to describe a solid.
    static bool BuildBrushes(BuildTool tool,
                             const BuildToolSettings& settings,
                             bool subtract,
                             const Math::Vector3& dragStart,
                             const Math::Vector3& dragEnd,
                             ECS::BrushSolidComponent& out);

    // The same idea for the tools that place components instead of brushes.
    //
    // Fills `out` for Water, Terrain and Ladder. Returns false for the brush
    // tools -- BuildBrushes owns those -- for Reduce, which has no footprint at
    // all because it acts on whatever mesh is under the cursor, and for a drag
    // too small to describe a footprint.
    static bool PlanPlacement(BuildTool tool,
                              const BuildToolSettings& settings,
                              const Math::Vector3& dragStart,
                              const Math::Vector3& dragEnd,
                              ToolPlacement& out);

    // Where a ray meets the y = 0 build plane, which is the plane every tool
    // but Reduce works on.
    //
    // Returns FALSE rather than a point when the ray cannot reach it: running
    // parallel to the plane, or pointing away from it. Both have no answer, and
    // the arithmetic that would produce one gives a placement at infinity or
    // behind the camera. The parallel case is not hypothetical -- it is every
    // frame of a 2D scene, where the editor camera looks along -Z and the ground
    // plane is edge-on, so no drag can ever land.
    static bool GroundHit(const Math::Vector3& rayOrigin,
                          const Math::Vector3& rayDirection,
                          Math::Vector3& out);

    // The polyline a path actually describes: every bowed span sampled into
    // segments, every straight span left as one.
    //
    // The PREVIEW and the GEOMETRY both come from here, which is the whole point
    // -- what you are shown while clicking is the thing that gets built, not a
    // smooth curve that turns into something coarser on release.
    static std::vector<Math::Vector3> SamplePath(const std::vector<Math::Vector3>& points,
                                                 const std::vector<f32>& bows,
                                                 u32 segmentsPerBow);

    // How many wall brushes a path will cost, without building it. Shown while
    // you author, because finding the number in the triangle counter afterwards
    // is finding it too late.
    static u32 CountPathBrushes(const std::vector<Math::Vector3>& points,
                                const std::vector<f32>& bows,
                                u32 segmentsPerBow);

    // One wall brush per segment, each standing ON its segment exactly the way
    // the Wall tool's single brush stands on its drag.
    static bool BuildPathBrushes(const std::vector<Math::Vector3>& points,
                                 const std::vector<f32>& bows,
                                 u32 segmentsPerBow,
                                 const BuildToolSettings& settings,
                                 bool subtract,
                                 ECS::BrushSolidComponent& out);

    // The rails and rungs that make a ladder something you can SEE.
    //
    // A LadderComponent on its own is an invisible box: it climbs correctly and
    // there is nothing on screen to climb, which is why the component was
    // add-a-component-only and unusable -- placing one meant already knowing it
    // was there. This is geometry decided from a gesture, so it is pure and
    // tested like the rest of it.
    //
    // Centres are ENTITY-LOCAL, because that is what a BrushSolidComponent
    // holds: the viewport composes `entityWorld * brushLocal` and the rebuilt
    // mesh is drawn through the entity's world matrix. The brush TOOLS only get
    // away with world coordinates because they leave the transform at the
    // origin; a ladder cannot, since its transform has to sit on the climb
    // volume for the controller to find it.
    static void BuildLadderVisual(const ToolPlacement& placement,
                                  ECS::BrushSolidComponent& out);

private:
    bool m_Active = false;
    BuildTool m_Tool = BuildTool::Wall;
    bool m_Subtracting = false;
    f32 m_GridSize = kCreativeGridDefault;
    bool m_Snap = true;
    BuildToolSettings m_Settings[static_cast<usize>(BuildTool::Count)];
};

} // namespace Editor
} // namespace Enjin
