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
        case BuildTool::Brush:   return "Brush";
        case BuildTool::Water:   return "Water";
        case BuildTool::Terrain: return "Terrain";
        case BuildTool::Ladder:  return "Ladder";
        case BuildTool::Reduce:  return "Reduce";
        default:                    return "Unknown";
    }
}

const char* BuildToolVerb(BuildTool tool) {
    switch (tool) {
        case BuildTool::Wall:
            return "Click, drag, click. Height and thickness stay put between walls.";
        case BuildTool::Floor:
            return "Drag a region. Fills to the grid, snaps to the walls already there.";
        case BuildTool::Stairs:
            return "Drag the run. Treads and a collider come with it.";
        case BuildTool::Brush:
            return "A convex solid. Subtract one from a wall and you have a doorway.";
        case BuildTool::Water:
            return "Drag out a volume. Depth follows the drag.";
        case BuildTool::Terrain:
            return "Sculpt by dragging: raise, lower, smooth, flatten.";
        case BuildTool::Ladder:
            return "Place against a wall. Climbing is already wired into the controller.";
        case BuildTool::Reduce:
            return "Point at a model and cut its triangles. Undo puts it straight back.";
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
    // Terrain has Raise/Lower, which is its own pair rather than add/subtract;
    // Ladder and Reduce have no second mode at all. Offering the toggle on
    // those would be a switch that does nothing.
    return BuildToolMakesBrushes(tool);
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
        case BuildTool::Stairs:
            add("Rise",  &s.rise,  0.05f, 0.45f, "m");
            add("Run",   &s.run,   0.10f, 0.60f, "m");
            add("Width", &s.width, 0.40f, 6.00f, "m");
            break;
        case BuildTool::Brush: {
            add("Height", &s.height, 0.10f, 20.0f, "m");
            break;
        }
        case BuildTool::Water:
            add("Depth",      &s.depth,     0.10f, 20.0f, "m");
            add("Wave scale", &s.waveScale, 0.00f,  2.0f, "");
            break;
        case BuildTool::Terrain:
            add("Radius",   &s.radius,   0.50f, 40.0f, "m");
            add("Strength", &s.strength, 0.05f,  2.0f, "");
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
    // A tool that cannot cut must not inherit a cutting state from the one
    // before it, or the surface shows the cut colour for a tool that only adds.
    if (!BuildToolCanSubtract(m_Tool)) m_Subtracting = false;
}

void CreativeMode::SetSubtracting(bool subtracting) {
    m_Subtracting = subtracting && BuildToolCanSubtract(m_Tool);
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

            if (settings.sides > 4) {
                // A prism is round, so it takes ONE radius: the drag describes a
                // rectangle and the smaller half-span is the one that fits
                // inside it. Taking the larger would make the shape overflow the
                // box the person just dragged.
                brush.shape = ECS::BrushSolidComponent::Shape::Prism;
                brush.radius = std::max(kMinHalf, std::min(spanX, spanZ) * 0.5f);
                brush.halfHeight = height * 0.5f;
                brush.sides = std::min(settings.sides, 256u);
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

} // namespace Editor
} // namespace Enjin
