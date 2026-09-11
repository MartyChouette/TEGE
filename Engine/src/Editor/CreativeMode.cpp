#include "Enjin/Editor/CreativeMode.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Editor {

namespace {

// Rounds to the nearest multiple, away from zero at the halfway point. Plain
// truncation would bias every placement toward the origin, which shows up as a
// blockout that drifts as you build outward from it.
f32 SnapAxis(f32 value, f32 grid) {
    if (!(grid > 0.001f)) return value;
    return std::round(value / grid) * grid;
}

// A yaw that points local +X along `dir` (which is expected to lie in the XZ
// plane). Built with FromEuler rather than a hand-rolled axis product: the
// engine's euler convention is ZYX intrinsic and hand-rolling it is exactly
// what corrupted every compound rotation until the gizmo write-back was fixed.
Math::Quaternion YawAlong(const Math::Vector3& dir) {
    const f32 yaw = std::atan2(-dir.z, dir.x);
    return Math::Quaternion::FromEuler(Math::Vector3(0.0f, yaw, 0.0f));
}

f32 HorizontalLength(const Math::Vector3& v) {
    return std::sqrt(v.x * v.x + v.z * v.z);
}

} // namespace

const char* BuildToolName(BuildTool tool) {
    switch (tool) {
        case BuildTool::Wall:    return "Wall";
        case BuildTool::Floor:   return "Floor";
        case BuildTool::Stairs:  return "Stairs";
        case BuildTool::Path:    return "Path";
        case BuildTool::Brush:   return "Brush";
        case BuildTool::Water:   return "Water";
        case BuildTool::Plants:  return "Plants";
        case BuildTool::Prop:    return "Prop";
        case BuildTool::Terrain: return "Terrain";
        case BuildTool::Ladder:  return "Ladder";
        case BuildTool::Reduce:  return "Reduce";
        case BuildTool::Edit:    return "Edit";
        default:                    return "Unknown";
    }
}

u8 BuildToolGroup(BuildTool tool) {
    switch (tool) {
        case BuildTool::Wall:
        case BuildTool::Floor:
        case BuildTool::Stairs:
        case BuildTool::Path:    return 0;   // structure
        case BuildTool::Brush:
        case BuildTool::Water:
        case BuildTool::Plants:
        case BuildTool::Terrain: return 1;   // volume
        case BuildTool::Ladder:
        case BuildTool::Prop:
        case BuildTool::Reduce:  return 2;   // object
        default:                 return 3;   // edit
    }
}

bool BuildToolIsEdit(BuildTool tool) { return tool == BuildTool::Edit; }

bool BuildToolIsPath(BuildTool tool) { return tool == BuildTool::Path; }

const char* BuildToolVerb(BuildTool tool) {
    switch (tool) {
        case BuildTool::Wall:
            return "Click, drag, click. Height and thickness stay put between walls.";
        case BuildTool::Floor:
            return "Drag a region. Fills to the grid, snaps to the walls already there.";
        case BuildTool::Stairs:
            return "Drag the run. Treads and a collider come with it.";
        case BuildTool::Path:
            return "Click corners along the wall. Pull a span sideways to bow it. Enter finishes.";
        case BuildTool::Brush:
            return "A convex solid. Subtract one from a wall and you have a doorway.";
        case BuildTool::Water:
            return "Drag a rectangle. Swimmable by default; Kind 0 is a surface only.";
        case BuildTool::Terrain:
            return "Drag over the ground to raise or lower it. Makes a terrain if there is none.";
        case BuildTool::Plants:
            return "Drag a patch. Grass, shrubs or trees, scattered inside it.";
        case BuildTool::Prop:
            return "Click the ground. Picks up where it lands, ready to play.";
        case BuildTool::Ladder:
            return "Drag along a wall. Climbing is already wired into the controller.";
        case BuildTool::Reduce:
            return "Point at a model and cut its triangles. Undo puts it straight back.";
        case BuildTool::Edit:
            return "Click something you built, then drag an edge or a corner to resize it.";
        default:
            return "";
    }
}

bool BuildToolMakesBrushes(BuildTool tool) {
    switch (tool) {
        case BuildTool::Wall:
        case BuildTool::Floor:
        case BuildTool::Stairs:
        case BuildTool::Brush:
            return true;
        default:
            return false;
    }
}

bool BuildToolCanSubtract(BuildTool tool) {
    // Strictly CSG subtraction. Terrain also has a second mode, but Raise/Lower
    // is its own pair and not a cut; Water, Ladder and Reduce have no second
    // mode at all, and offering the toggle on those would be a switch that does
    // nothing.
    // Path is here too. It does not build from a drag, but it does build
    // brushes, and a bowed cut is how an archway or a curved doorway happens.
    return BuildToolMakesBrushes(tool) || BuildToolIsPath(tool);
}

// Whether the second mode means anything for this tool at all -- CSG subtraction
// for the brush tools, Lower for Terrain. This is the one the mode state gates
// on, because gating on BuildToolCanSubtract drew Raise/Lower for Terrain and
// then refused to let Lower be selected.
static bool ToolHasSecondMode(BuildTool tool) {
    return BuildToolModeLabels(tool) != nullptr;
}

u32 BuildToolFields(BuildTool tool, BuildToolSettings& s,
                       BuildField* out, u32 maxFields) {
    if (!out || maxFields == 0) return 0;

    // Ranges are what a person blocking out a room would plausibly want, not
    // what the maths permits: a 40 m wall is a mistake, not a feature, and a
    // slider that reaches it makes the useful part of the range unusable.
    BuildField fields[kBuildMaxFields];
    u32 count = 0;
    auto add = [&](const char* label, f32* value, f32 lo, f32 hi, const char* unit) {
        if (count >= kBuildMaxFields) return;
        fields[count++] = BuildField{label, value, lo, hi, unit};
    };

    switch (tool) {
        case BuildTool::Wall:
            add("Height",    &s.height,    0.25f, 12.0f, "m");
            add("Thickness", &s.thickness, 0.05f,  2.0f, "m");
            break;
        case BuildTool::Floor:
            add("Thickness", &s.thickness, 0.05f,  2.0f, "m");
            add("Elevation", &s.elevation, -20.0f, 20.0f, "m");
            break;
        case BuildTool::Path:
            add("Height",    &s.height,    0.25f, 12.0f, "m");
            add("Thickness", &s.thickness, 0.05f,  2.0f, "m");
            add("Segments",  &s.segments,
                static_cast<f32>(kCreativePathSegmentsMin),
                static_cast<f32>(kCreativePathSegmentsMax), "");
            break;
        case BuildTool::Stairs:
            add("Rise",  &s.rise,  0.05f, 0.45f, "m");
            add("Run",   &s.run,   0.10f, 0.60f, "m");
            add("Width", &s.width, 0.40f, 6.00f, "m");
            break;
        case BuildTool::Brush: {
            add("Height", &s.height, 0.10f, 20.0f, "m");
            // 4 is a box and anything above it is a prism, so the range starts
            // at the box and runs to a circle you cannot count the sides of.
            add("Sides", &s.sides, 4.0f, 32.0f, "");
            break;
        }
        case BuildTool::Water:
            // Kind first, because it decides what the rest of these mean.
            // 0 = Surface (a Water3D plane), 1 = Swimmable (a WaterVolume body).
            add("Kind",    &s.waterKind,   0.0f,  1.0f, "");
            add("Surface", &s.elevation, -20.0f, 20.0f, "m");
            // Depth belongs to the swimmable body only. A Water3D plane has no
            // depth -- how deep THAT water looks is the basin you cut under it,
            // and a field labelled Depth that moved the plane's footprint
            // instead would be a lie on the surface.
            if (s.waterKind >= 0.5f) {
                add("Depth", &s.waterDepth, 0.25f, 20.0f, "m");
            } else {
                add("Waves", &s.waveScale,  0.0f,  2.0f, "m");
            }
            break;
        case BuildTool::Terrain:
            add("Radius",   &s.radius,   0.50f, 40.0f, "m");
            add("Strength", &s.strength, 0.05f,  2.0f, "");
            break;
        case BuildTool::Plants:
            // Kind is a 0..2 pick rendered as a slider, for the same reason the
            // rest of this rail is sliders: one row shape, one interaction.
            add("Kind",    &s.plantKind,    0.0f, 2.0f, "");
            add("Density", &s.plantDensity, 0.1f, 4.0f, "x");
            break;
        case BuildTool::Prop:
            add("Kind", &s.propKind, 0.0f, 4.0f, "");
            break;
        case BuildTool::Ladder:
            add("Height",   &s.height,  0.50f, 20.0f, "m");
            add("Rung gap", &s.rungGap, 0.10f,  1.0f, "m");
            break;
        case BuildTool::Reduce:
            add("Keep", &s.keepPercent, 5.0f, 95.0f, "%");
            break;
        default:
            break;
    }

    const u32 written = std::min(count, maxFields);
    for (u32 i = 0; i < written; ++i) out[i] = fields[i];
    return written;
}

const char* const* BuildToolModeLabels(BuildTool tool) {
    static const char* kAddCut[2]     = { "Add", "Subtract" };
    static const char* kRaiseLower[2] = { "Raise", "Lower" };
    if (BuildToolCanSubtract(tool)) return kAddCut;
    if (tool == BuildTool::Terrain) return kRaiseLower;
    return nullptr;
}

void CreativeMode::SetTool(BuildTool tool) {
    if (tool >= BuildTool::Count) return;
    m_Tool = tool;
    // A tool with only one mode must not inherit the second one from the tool
    // before it, or the surface shows the cut colour for a tool that only adds.
    if (!ToolHasSecondMode(m_Tool)) m_Subtracting = false;
}

void CreativeMode::SetSubtracting(bool subtracting) {
    m_Subtracting = subtracting && ToolHasSecondMode(m_Tool);
}

BuildToolSettings& CreativeMode::Settings(BuildTool tool) {
    const usize index = static_cast<usize>(tool);
    if (index >= static_cast<usize>(BuildTool::Count)) return m_Settings[0];
    return m_Settings[index];
}

const BuildToolSettings& CreativeMode::Settings(BuildTool tool) const {
    const usize index = static_cast<usize>(tool);
    if (index >= static_cast<usize>(BuildTool::Count)) return m_Settings[0];
    return m_Settings[index];
}

Math::Vector3 CreativeMode::SnapToGrid(const Math::Vector3& point) const {
    if (!m_Snap) return point;
    return Math::Vector3(SnapAxis(point.x, m_GridSize), point.y, SnapAxis(point.z, m_GridSize));
}

bool CreativeMode::BuildBrushes(BuildTool tool,
                                const BuildToolSettings& settings,
                                bool subtract,
                                const Math::Vector3& dragStart,
                                const Math::Vector3& dragEnd,
                                ECS::BrushSolidComponent& out) {
    if (!BuildToolMakesBrushes(tool)) return false;

    const Math::Vector3 delta = dragEnd - dragStart;
    const f32 spanX = std::fabs(delta.x);
    const f32 spanZ = std::fabs(delta.z);

    const Geometry::BrushOp op = subtract ? Geometry::BrushOp::Subtract
                                          : Geometry::BrushOp::Add;

    // Every extent below is a HALF extent, and every one of them is clamped
    // away from zero. A zero half-extent is a degenerate solid: the CSG build
    // produces no faces, the entity renders nothing, and it reads as the tool
    // being broken rather than as the drag being too small.
    constexpr f32 kMinHalf = 0.005f;

    switch (tool) {
        case BuildTool::Wall: {
            const f32 length = HorizontalLength(delta);
            if (length < kCreativeMinDragLength) return false;

            const f32 height = std::max(kMinHalf * 2.0f, settings.height);
            const f32 thickness = std::max(kMinHalf * 2.0f, settings.thickness);

            ECS::BrushSolidComponent::Brush brush;
            brush.shape = ECS::BrushSolidComponent::Shape::Box;
            brush.op = op;
            // The wall stands ON the drag line rather than hanging from it, so
            // the line you drew is the wall's foot, not its middle.
            brush.center = Math::Vector3((dragStart.x + dragEnd.x) * 0.5f,
                                         dragStart.y + height * 0.5f,
                                         (dragStart.z + dragEnd.z) * 0.5f);
            brush.rotation = YawAlong(delta);
            brush.halfExtents = Math::Vector3(length * 0.5f, height * 0.5f, thickness * 0.5f);
            out.brushes.push_back(brush);
            return true;
        }

        case BuildTool::Floor: {
            if (spanX < kCreativeMinDragLength || spanZ < kCreativeMinDragLength) return false;

            const f32 thickness = std::max(kMinHalf * 2.0f, settings.thickness);

            ECS::BrushSolidComponent::Brush brush;
            brush.shape = ECS::BrushSolidComponent::Shape::Box;
            brush.op = op;
            // The slab hangs BELOW its elevation, so "elevation 0" is a floor
            // you stand on at y=0 rather than one you stand inside.
            brush.center = Math::Vector3((dragStart.x + dragEnd.x) * 0.5f,
                                         settings.elevation - thickness * 0.5f,
                                         (dragStart.z + dragEnd.z) * 0.5f);
            brush.halfExtents = Math::Vector3(spanX * 0.5f, thickness * 0.5f, spanZ * 0.5f);
            out.brushes.push_back(brush);
            return true;
        }

        case BuildTool::Stairs: {
            const f32 length = HorizontalLength(delta);
            if (length < kCreativeMinDragLength) return false;

            const f32 rise  = std::max(0.01f, settings.rise);
            const f32 run   = std::max(0.01f, settings.run);
            const f32 width = std::max(kMinHalf * 2.0f, settings.width);

            u32 steps = static_cast<u32>(std::floor(length / run));
            if (steps < 1) steps = 1;
            steps = std::min(steps, kCreativeMaxStairSteps);

            // Unit vector along the drag, in the XZ plane.
            const Math::Vector3 dir(delta.x / length, 0.0f, delta.z / length);
            const Math::Quaternion yaw = YawAlong(delta);

            for (u32 i = 0; i < steps; ++i) {
                // Each tread is a box from the ground up to its own top, rather
                // than a thin slab floating at step height. A stack of solid
                // treads is what gives the run a side to see and a collider a
                // character can walk up without falling through the gaps.
                const f32 top = rise * static_cast<f32>(i + 1);
                const f32 alongCentre = run * (static_cast<f32>(i) + 0.5f);

                ECS::BrushSolidComponent::Brush tread;
                tread.shape = ECS::BrushSolidComponent::Shape::Box;
                tread.op = op;
                tread.center = Math::Vector3(dragStart.x + dir.x * alongCentre,
                                             dragStart.y + top * 0.5f,
                                             dragStart.z + dir.z * alongCentre);
                tread.rotation = yaw;
                tread.halfExtents = Math::Vector3(run * 0.5f, top * 0.5f, width * 0.5f);
                out.brushes.push_back(tread);
            }
            return true;
        }

        case BuildTool::Brush: {
            if (spanX < kCreativeMinDragLength || spanZ < kCreativeMinDragLength) return false;

            const f32 height = std::max(kMinHalf * 2.0f, settings.height);

            ECS::BrushSolidComponent::Brush brush;
            brush.op = op;
            brush.center = Math::Vector3((dragStart.x + dragEnd.x) * 0.5f,
                                         dragStart.y + height * 0.5f,
                                         (dragStart.z + dragEnd.z) * 0.5f);

            const u32 sides = static_cast<u32>(settings.sides + 0.5f);
            if (sides > 4) {
                // A prism is round, so it takes ONE radius: the drag describes a
                // rectangle and the smaller half-span is the one that fits
                // inside it. Taking the larger would make the shape overflow the
                // box the person just dragged.
                brush.shape = ECS::BrushSolidComponent::Shape::Prism;
                brush.radius = std::max(kMinHalf, std::min(spanX, spanZ) * 0.5f);
                brush.halfHeight = height * 0.5f;
                brush.sides = std::min(sides, 256u);
            } else {
                brush.shape = ECS::BrushSolidComponent::Shape::Box;
                brush.halfExtents = Math::Vector3(spanX * 0.5f, height * 0.5f, spanZ * 0.5f);
            }
            out.brushes.push_back(brush);
            return true;
        }

        default:
            return false;
    }
}

bool CreativeMode::PlanPlacement(BuildTool tool,
                                 const BuildToolSettings& settings,
                                 const Math::Vector3& dragStart,
                                 const Math::Vector3& dragEnd,
                                 ToolPlacement& out) {
    const Math::Vector3 delta = dragEnd - dragStart;
    const f32 spanX = std::fabs(delta.x);
    const f32 spanZ = std::fabs(delta.z);
    const f32 midX = (dragStart.x + dragEnd.x) * 0.5f;
    const f32 midZ = (dragStart.z + dragEnd.z) * 0.5f;

    switch (tool) {
        case BuildTool::Water: {
            // A water plane needs both dimensions for the same reason a floor
            // does: a rectangle with one side of zero has no surface to draw.
            if (spanX < kCreativeMinDragLength || spanZ < kCreativeMinDragLength) return false;

            out = ToolPlacement{};
            out.origin = Math::Vector3(midX, settings.elevation, midZ);
            out.halfExtents = Math::Vector3(spanX * 0.5f, 0.0f, spanZ * 0.5f);
            return true;
        }

        case BuildTool::Plants: {
            // Both dimensions, same as Water: a patch with one side of zero has
            // no area to scatter into, and would produce a volume that looks
            // placed and grows nothing.
            if (spanX < kCreativeMinDragLength || spanZ < kCreativeMinDragLength) return false;

            out = ToolPlacement{};
            // Sits on the ground the drag happened on, not at an authored
            // elevation -- plants grow where you dragged them.
            out.origin = Math::Vector3(midX, dragStart.y, midZ);
            out.halfExtents = Math::Vector3(spanX * 0.5f, 0.0f, spanZ * 0.5f);
            return true;
        }

        case BuildTool::Prop: {
            // The only tool here that does NOT need a drag. These are ready-made
            // objects at their own size; there is nothing for a gesture to
            // describe, so a click is the whole interaction and a drag simply
            // moves where it lands.
            //
            // Never refused: a click with no span is the intended use, and
            // returning false for it would make the tool appear inert.
            out = ToolPlacement{};
            out.origin = dragEnd;
            out.halfExtents = Math::Vector3(0.0f, 0.0f, 0.0f);
            return true;
        }

        case BuildTool::Ladder: {
            const f32 height = std::max(0.25f, settings.height);

            // The long horizontal axis of the drag is the ladder's width; the
            // other is fixed thin. A ladder as deep as it is wide is a crate,
            // and a drag straight down a wall gives almost no span on one axis
            // anyway, so taking both from the gesture makes a click unusable.
            const f32 halfWide = std::max(kCreativeLadderMinHalf,
                                          std::max(spanX, spanZ) * 0.5f);

            out = ToolPlacement{};
            out.origin = Math::Vector3(midX, dragStart.y + height * 0.5f, midZ);
            out.halfExtents = (spanX >= spanZ)
                ? Math::Vector3(halfWide, height * 0.5f, kCreativeLadderThin)
                : Math::Vector3(kCreativeLadderThin, height * 0.5f, halfWide);

            // Rungs are what makes the climb volume visible. The gap is the
            // authored one, and the count is whatever fits inside the height
            // without a rung sitting on the very top or bottom edge.
            const f32 gap = std::max(0.05f, settings.rungGap);
            const f32 fit = std::floor(height / gap);
            out.rungs = (fit >= 1.0f) ? static_cast<u32>(std::min(fit, 512.0f)) : 1u;
            return true;
        }

        case BuildTool::Terrain: {
            // Sculpting is a press, not a drag, so only the point under the
            // cursor when it went down matters here. A TerrainComponent's grid
            // runs from its transform out to +X/+Z rather than straddling it, so
            // the origin is the corner that puts the press point in the middle
            // -- otherwise a fresh terrain appears entirely off to one side of
            // the first stroke and the stroke lands on nothing.
            const f32 half = static_cast<f32>(kCreativeTerrainGrid) * kCreativeTerrainCell * 0.5f;
            out = ToolPlacement{};
            out.origin = Math::Vector3(dragStart.x - half, 0.0f, dragStart.z - half);
            out.halfExtents = Math::Vector3(half, 0.0f, half);
            return true;
        }

        default:
            // Wall/Floor/Stairs/Brush belong to BuildBrushes, and Reduce has no
            // footprint at all -- it acts on whatever mesh is under the cursor.
            return false;
    }
}

namespace {

// The brush's own horizontal axes. Unit vectors, because a rotation preserves
// length -- so a projection onto one of them is a distance in metres and needs
// no normalising.
struct BrushAxes {
    Math::Vector3 x;
    Math::Vector3 z;
};

BrushAxes AxesOf(const ECS::BrushSolidComponent::Brush& brush) {
    return BrushAxes{ brush.rotation.Rotate(Math::Vector3(1.0f, 0.0f, 0.0f)),
                      brush.rotation.Rotate(Math::Vector3(0.0f, 0.0f, 1.0f)) };
}

f32 Dot(const Math::Vector3& a, const Math::Vector3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Move one edge along `axis` to `target`, leaving the edge on the other side
// alone. `half` is the brush's half extent on that axis and is updated; the
// centre slides to stay halfway between the two edges.
bool SlideEdge(Math::Vector3& centre, f32& half, const Math::Vector3& axis,
               const Math::Vector3& target, bool movingMaxSide) {
    // The edge that does NOT move. Keeping it as a POINT rather than a distance
    // is what preserves the centre's other two components for free.
    const Math::Vector3 anchor = movingMaxSide ? (centre - axis * half)
                                               : (centre + axis * half);
    // Signed width from the anchor toward the dragged side.
    const f32 width = movingMaxSide ? Dot(target - anchor, axis)
                                    : Dot(anchor - target, axis);
    if (width < kCreativeMinBrushExtent) return false;   // collapsed, or dragged inside out

    half = width * 0.5f;
    centre = movingMaxSide ? (anchor + axis * half) : (anchor - axis * half);
    return true;
}

bool GripTouchesX(BrushGrip g) {
    switch (g) {
        case BrushGrip::MinX: case BrushGrip::MaxX:
        case BrushGrip::MinXMinZ: case BrushGrip::MaxXMinZ:
        case BrushGrip::MinXMaxZ: case BrushGrip::MaxXMaxZ: return true;
        default: return false;
    }
}
bool GripTouchesZ(BrushGrip g) {
    switch (g) {
        case BrushGrip::MinZ: case BrushGrip::MaxZ:
        case BrushGrip::MinXMinZ: case BrushGrip::MaxXMinZ:
        case BrushGrip::MinXMaxZ: case BrushGrip::MaxXMaxZ: return true;
        default: return false;
    }
}
bool GripIsMaxX(BrushGrip g) {
    return g == BrushGrip::MaxX || g == BrushGrip::MaxXMinZ || g == BrushGrip::MaxXMaxZ;
}
bool GripIsMaxZ(BrushGrip g) {
    return g == BrushGrip::MaxZ || g == BrushGrip::MinXMaxZ || g == BrushGrip::MaxXMaxZ;
}

// A prism is round: it has one radius rather than two independent half extents,
// so every grip drives the same number.
f32& RadialExtent(ECS::BrushSolidComponent::Brush& brush) { return brush.radius; }

} // namespace

bool BrushGripIsCorner(BrushGrip grip) {
    return grip == BrushGrip::MinXMinZ || grip == BrushGrip::MaxXMinZ ||
           grip == BrushGrip::MinXMaxZ || grip == BrushGrip::MaxXMaxZ;
}

Math::Vector3 BrushGripPosition(const ECS::BrushSolidComponent::Brush& brush, BrushGrip grip) {
    const BrushAxes axes = AxesOf(brush);
    const bool prism = (brush.shape == ECS::BrushSolidComponent::Shape::Prism);
    const f32 hx = prism ? brush.radius : brush.halfExtents.x;
    const f32 hz = prism ? brush.radius : brush.halfExtents.z;

    Math::Vector3 p = brush.center;
    if (GripTouchesX(grip)) p = p + axes.x * (GripIsMaxX(grip) ? hx : -hx);
    if (GripTouchesZ(grip)) p = p + axes.z * (GripIsMaxZ(grip) ? hz : -hz);
    return p;
}

bool ResizeBrushByGrip(ECS::BrushSolidComponent::Brush& brush, BrushGrip grip,
                       const Math::Vector3& worldPoint) {
    if (grip >= BrushGrip::Count) return false;
    const BrushAxes axes = AxesOf(brush);

    if (brush.shape == ECS::BrushSolidComponent::Shape::Prism) {
        // One radius, so the far side cannot be held: the centre stays put and
        // the radius follows the cursor. Anything else would make a round shape
        // drift sideways as it grew, which is not what a handle on its rim
        // promises.
        const Math::Vector3 d = worldPoint - brush.center;
        const f32 r = std::sqrt(Dot(d, axes.x) * Dot(d, axes.x) + Dot(d, axes.z) * Dot(d, axes.z));
        if (r < kCreativeMinBrushExtent * 0.5f) return false;
        RadialExtent(brush) = r;
        return true;
    }

    // A corner drives both axes, and BOTH have to survive: a corner drag that
    // collapsed one axis and kept the other would leave the brush half-resized
    // from a single gesture, which no undo entry describes honestly.
    Math::Vector3 centre = brush.center;
    Math::Vector3 half = brush.halfExtents;

    if (GripTouchesX(grip) &&
        !SlideEdge(centre, half.x, axes.x, worldPoint, GripIsMaxX(grip))) {
        return false;
    }
    if (GripTouchesZ(grip) &&
        !SlideEdge(centre, half.z, axes.z, worldPoint, GripIsMaxZ(grip))) {
        return false;
    }

    brush.center = centre;
    brush.halfExtents = half;
    return true;
}

std::vector<Math::Vector3> CreativeMode::SamplePath(const std::vector<Math::Vector3>& points,
                                                    const std::vector<f32>& bows,
                                                    u32 segmentsPerBow) {
    std::vector<Math::Vector3> out;
    if (points.size() < 2) {
        if (points.size() == 1) out.push_back(points[0]);
        return out;
    }

    const u32 segs = std::clamp(segmentsPerBow, kCreativePathSegmentsMin, kCreativePathSegmentsMax);
    out.push_back(points[0]);

    const usize spans = points.size() - 1;
    for (usize i = 0; i < spans; ++i) {
        const Math::Vector3 a = points[i];
        const Math::Vector3 b = points[i + 1];
        const f32 bow = (i < bows.size()) ? bows[i] : 0.0f;

        if (std::fabs(bow) < kCreativePathMinBow) {
            out.push_back(b);          // straight: one segment, nothing to sample
            continue;
        }

        // A quadratic through a control point pushed off the midpoint along the
        // span's normal. Not a true arc, and deliberately the same curve the
        // mockup used, so what was agreed there is what gets built.
        const Math::Vector3 d = b - a;
        const f32 len = HorizontalLength(d);
        if (len < 1e-4f) { out.push_back(b); continue; }

        const Math::Vector3 n(-d.z / len, 0.0f, d.x / len);
        const Math::Vector3 mid((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, (a.z + b.z) * 0.5f);
        const Math::Vector3 ctrl = mid + n * (bow * 2.0f);

        for (u32 s = 1; s <= segs; ++s) {
            const f32 t = static_cast<f32>(s) / static_cast<f32>(segs);
            const f32 it = 1.0f - t;
            out.push_back(Math::Vector3(
                it * it * a.x + 2.0f * it * t * ctrl.x + t * t * b.x,
                it * it * a.y + 2.0f * it * t * ctrl.y + t * t * b.y,
                it * it * a.z + 2.0f * it * t * ctrl.z + t * t * b.z));
        }
    }
    return out;
}

u32 CreativeMode::CountPathBrushes(const std::vector<Math::Vector3>& points,
                                   const std::vector<f32>& bows,
                                   u32 segmentsPerBow) {
    const std::vector<Math::Vector3> line = SamplePath(points, bows, segmentsPerBow);
    return (line.size() < 2) ? 0u : static_cast<u32>(line.size() - 1);
}

bool CreativeMode::BuildPathBrushes(const std::vector<Math::Vector3>& points,
                                    const std::vector<f32>& bows,
                                    u32 segmentsPerBow,
                                    const BuildToolSettings& settings,
                                    bool subtract,
                                    ECS::BrushSolidComponent& out) {
    const std::vector<Math::Vector3> line = SamplePath(points, bows, segmentsPerBow);
    if (line.size() < 2) return false;

    const f32 height = std::max(0.01f, settings.height);
    const f32 thickness = std::max(0.01f, settings.thickness);
    const Geometry::BrushOp op = subtract ? Geometry::BrushOp::Subtract
                                          : Geometry::BrushOp::Add;

    // How far a segment must run PAST a joint so the corner fills in.
    //
    // Two boxes meeting at an angle leave a wedge-shaped notch on the outside of
    // the bend: each one stops at the shared point, and neither covers the gap
    // between their outer faces. Mitring is the standard answer, and for a box
    // whose length is what we control it reduces to extending each segment by
    //     (thickness / 2) * tan(turn / 2)
    // which is zero on a straight run and exactly half the thickness at a right
    // angle. The extensions overlap inside the corner, and that costs nothing:
    // these are union brushes, so overlap is free and only the GAP is visible.
    //
    // This is the borrowed idea from Oskar Stalberg's building tools (Townscaper,
    // Brick Block): a person draws the line they mean and the system works out
    // the corner piece. Making someone place a corner brush by hand to close a
    // seam the tool created is the tool handing back its own homework.
    auto mitre = [&](const Math::Vector3& incoming, const Math::Vector3& outgoing) {
        const f32 li = HorizontalLength(incoming), lo = HorizontalLength(outgoing);
        if (li < 1e-4f || lo < 1e-4f) return 0.0f;
        const f32 c = std::clamp((incoming.x * outgoing.x + incoming.z * outgoing.z) / (li * lo),
                                 -1.0f, 1.0f);
        // tan(turn/2) from the half-angle identity, which stays stable near a
        // straight run where the angle itself is noisy.
        if (c <= -0.999f) return thickness * 4.0f;      // doubling back; just cap it
        const f32 tanHalf = std::sqrt((1.0f - c) / (1.0f + c));
        // Capped, because tan runs away as the corner sharpens and an unbounded
        // extension would shoot a wall off across the level.
        return std::min(thickness * 0.5f * tanHalf, thickness * 4.0f);
    };

    bool any = false;
    const usize segCount = line.size() - 1;
    for (usize i = 0; i < segCount; ++i) {
        const Math::Vector3 a = line[i];
        const Math::Vector3 b = line[i + 1];
        const Math::Vector3 d = b - a;
        const f32 len = HorizontalLength(d);
        // A sampled curve can hand back a repeated point where a span was
        // degenerate. A zero-length wall has no faces, so it is skipped rather
        // than added as an invisible brush that still costs a CSG operand.
        if (len < 1e-4f) continue;

        // Only at interior joints. The two ends of the whole path stay exactly
        // where they were clicked, or the wall would grow past its own corners.
        const f32 extendStart = (i > 0) ? mitre(a - line[i - 1], d) : 0.0f;
        const f32 extendEnd   = (i + 1 < segCount) ? mitre(d, line[i + 2] - b) : 0.0f;

        const Math::Vector3 dir(d.x / len, 0.0f, d.z / len);
        const f32 fullLength = len + extendStart + extendEnd;
        // The centre slides by half the DIFFERENCE, so a segment extended at one
        // end only does not drift at the other.
        const Math::Vector3 mid((a.x + b.x) * 0.5f, 0.0f, (a.z + b.z) * 0.5f);
        const Math::Vector3 centre = mid + dir * ((extendEnd - extendStart) * 0.5f);

        ECS::BrushSolidComponent::Brush brush;
        brush.shape = ECS::BrushSolidComponent::Shape::Box;
        brush.op = op;
        // Stands ON the segment, the same convention as the Wall tool: the line
        // you drew is the wall's foot, not its middle.
        brush.center = Math::Vector3(centre.x, a.y + height * 0.5f, centre.z);
        brush.rotation = YawAlong(d);
        brush.halfExtents = Math::Vector3(fullLength * 0.5f, height * 0.5f, thickness * 0.5f);
        out.brushes.push_back(brush);
        any = true;
    }
    return any;
}

bool CreativeMode::GroundHit(const Math::Vector3& rayOrigin,
                             const Math::Vector3& rayDirection,
                             Math::Vector3& out) {
    // Level blockout happens on the ground, so the build plane is y = 0.
    //
    // The threshold is a USABLE angle, not merely a non-parallel one. A ray a
    // fraction of a degree off the horizontal does intersect the plane -- half a
    // kilometre away -- and the old 1e-4 epsilon accepted it. Every tool then
    // worked exactly as written and felt broken: near the horizon one pixel of
    // mouse movement is tens of metres of ground, so a short drag produced a
    // wall the length of the level, and the terrain brush's ring flattened into
    // a sliver pointing at somewhere you were not looking.
    //
    // sin(6 degrees) ~= 0.105. Below that the answer is "not a build point",
    // which the caller already knows how to say.
    constexpr f32 kMinGrazeSin = 0.105f;
    if (std::fabs(rayDirection.y) < kMinGrazeSin) return false;

    const f32 t = -rayOrigin.y / rayDirection.y;
    if (t <= 0.0f) return false;   // the plane is behind the camera

    out = Math::Vector3(rayOrigin.x + rayDirection.x * t,
                        0.0f,
                        rayOrigin.z + rayDirection.z * t);
    return true;
}

void CreativeMode::BuildLadderVisual(const ToolPlacement& placement,
                                     ECS::BrushSolidComponent& out) {
    // Nothing to stand in front of the climb volume: the rungs are there to be
    // looked at, and a solid ladder would be a wall between the character and
    // the thing that makes them climb.
    out.generateCollider = false;

    const bool alongX  = placement.halfExtents.x >= placement.halfExtents.z;
    const f32 halfWide = alongX ? placement.halfExtents.x : placement.halfExtents.z;
    const f32 halfThin = kCreativeLadderThin;
    const f32 halfH    = std::max(0.05f, placement.halfExtents.y);
    const f32 railHalf = std::min(0.04f, halfWide * 0.25f);
    const f32 railOff  = std::max(railHalf, halfWide - railHalf);

    auto addBox = [&](const Math::Vector3& centre, const Math::Vector3& half) {
        ECS::BrushSolidComponent::Brush b;
        b.shape = ECS::BrushSolidComponent::Shape::Box;
        b.op = Geometry::BrushOp::Add;
        b.center = centre;
        b.halfExtents = half;
        out.brushes.push_back(b);
    };

    // Two uprights at the edges of the width.
    for (int side = -1; side <= 1; side += 2) {
        const f32 o = railOff * static_cast<f32>(side);
        addBox(alongX ? Math::Vector3(o, 0.0f, 0.0f) : Math::Vector3(0.0f, 0.0f, o),
               alongX ? Math::Vector3(railHalf, halfH, halfThin)
                      : Math::Vector3(halfThin, halfH, railHalf));
    }

    // Rungs, evenly spaced inside the height. Half a gap in from each end keeps
    // the bottom rung off the floor and the top one below the ledge.
    const u32 rungs = std::max(1u, placement.rungs);
    const f32 gap = (halfH * 2.0f) / static_cast<f32>(rungs);
    for (u32 i = 0; i < rungs; ++i) {
        const f32 y = -halfH + gap * (static_cast<f32>(i) + 0.5f);
        addBox(Math::Vector3(0.0f, y, 0.0f),
               alongX ? Math::Vector3(railOff, railHalf, halfThin * 0.6f)
                      : Math::Vector3(halfThin * 0.6f, railHalf, railOff));
    }
}

} // namespace Editor
} // namespace Enjin
