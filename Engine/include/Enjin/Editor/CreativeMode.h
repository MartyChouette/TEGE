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
    Brush,
    Water,
    Terrain,
    Ladder,
    Reduce,
    Count
};

ENJIN_API const char* BuildToolName(BuildTool tool);

// One line saying what the tool does, shown under its name. Present tense,
// second person, describing the gesture rather than the feature.
ENJIN_API const char* BuildToolVerb(BuildTool tool);

// Does this tool produce brush geometry from a drag? Water, Terrain, Ladder and
// Reduce do not: they act on components or on an existing mesh.
ENJIN_API bool BuildToolMakesBrushes(BuildTool tool);

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

    f32 depth     = 1.60f;   // Water
    f32 waveScale = 0.35f;   // Water

    f32 radius   = 4.00f;    // Terrain sculpt
    f32 strength = 0.60f;    // Terrain sculpt

    f32 rungGap = 0.30f;     // Ladder
    f32 keepPercent = 50.0f; // Reduce

    u32 sides = 4;           // Brush: 4 = box, more = prism
};

// How far apart the grid lines are, in world units. Everything a tool places is
// snapped to this, because a blockout that is half a centimetre off reads as a
// bug in the engine rather than a slip of the mouse.
inline constexpr f32 kCreativeGridDefault = 0.25f;

// Guard rails on a drag. A gesture smaller than this is a click, not a drag, and
// building a zero-width wall from it produces a solid with no faces that renders
// nothing and looks like a broken tool.
inline constexpr f32 kCreativeMinDragLength = 0.05f;

// A stair run has to terminate. Without a ceiling, a long drag with a small run
// asks for tens of thousands of tread brushes and the CSG build stops being
// interactive.
inline constexpr u32 kCreativeMaxStairSteps = 256;

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
