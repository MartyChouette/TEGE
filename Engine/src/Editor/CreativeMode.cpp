#include "Enjin/Editor/CreativeMode.h"
#include "Enjin/Geometry/VoxelEdit.h"

#include <cctype>

#include <algorithm>
#include <cmath>
#include <utility>

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

// --------------------------------------------------------------------------
// Cave
// --------------------------------------------------------------------------
//
// What is left here is the geometry the TERRAIN needs to know about: how far
// the carve reaches sideways and how high it reaches, so the surface above it
// can be opened exactly where it breaks through.
//
// The shape of the carve itself lives in Geometry::VoxelEdit, because it is a
// distance field now rather than a list of brushes. The prism builder that used
// to live here is gone: it described a pipe, and nothing calls it.

namespace {

constexpr f32 kCaveMinBore = 0.5f;

f32 CaveBore(const BuildToolSettings& s) { return std::max(s.radius, kCaveMinBore); }

} // namespace

f32 CreativeMode::CaveOuterRadius(const BuildToolSettings& s) {
    return CaveBore(s) + std::max(0.0f, s.roughness);
}

f32 CreativeMode::CaveTopY(const BuildToolSettings& s, const Math::Vector3& start) {
    // Floor on the drag, so the axis is one bore up and the roof is one more.
    // Roughness pushes the wall outwards on top of that.
    return start.y + 2.0f * CaveBore(s) + std::max(0.0f, s.roughness);
}

f32 CreativeMode::CaveDistanceToAxisXZ(const Math::Vector3& start, const Math::Vector3& end,
                                       f32 px, f32 pz) {
    const f32 ax = end.x - start.x;
    const f32 az = end.z - start.z;
    const f32 len2 = ax * ax + az * az;
    if (len2 < 1e-8f) {
        const f32 dx = px - start.x, dz = pz - start.z;
        return std::sqrt(dx * dx + dz * dz);
    }
    // Clamped to the SEGMENT, not the infinite line: a passage does not carry
    // on past the end of the drag, and an unclamped projection would punch the
    // terrain open in a stripe running off to the horizon.
    f32 t = ((px - start.x) * ax + (pz - start.z) * az) / len2;
    t = std::max(0.0f, std::min(1.0f, t));
    const f32 cx = start.x + ax * t;
    const f32 cz = start.z + az * t;
    const f32 dx = px - cx, dz = pz - cz;
    return std::sqrt(dx * dx + dz * dz);
}

bool BuildToolFromName(const char* name, BuildTool& out) {
    if (!name || !*name) return false;
    for (u8 i = 0; i < static_cast<u8>(BuildTool::Count); ++i) {
        const BuildTool t = static_cast<BuildTool>(i);
        const char* n = BuildToolName(t);
        usize k = 0;
        for (;; ++k) {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(n[k])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(name[k])));
            if (a != b) break;
            if (a == '\0') { out = t; return true; }
        }
    }
    return false;
}

const char* BuildToolName(BuildTool tool) {
    switch (tool) {
        case BuildTool::Wall:    return "Wall";
        case BuildTool::Floor:   return "Floor";
        case BuildTool::Stairs:  return "Stairs";
        case BuildTool::Path:    return "Path";
        case BuildTool::Roof:    return "Roof";
        case BuildTool::Door:    return "Door";
        case BuildTool::Paint:   return "Paint";
        case BuildTool::Brush:   return "Brush";
        case BuildTool::Water:   return "Water";
        case BuildTool::Plants:  return "Plants";
        case BuildTool::Prop:    return "Prop";
        case BuildTool::Terrain: return "Terrain";
        case BuildTool::Cave:    return "Cave";
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
        case BuildTool::Path:
        case BuildTool::Roof:
        case BuildTool::Door:    return 0;   // structure
        case BuildTool::Brush:
        case BuildTool::Water:
        case BuildTool::Plants:
        case BuildTool::Cave:
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
        case BuildTool::Roof:
            return "Drag over a room. Sits at wall height; the ridge runs the long way.";
        case BuildTool::Door:
            return "Click the side of a wall. It is cut there, and a door swings in it.";
        case BuildTool::Paint:
            return "Pick a colour, then click what you built. Hold and sweep to do several.";
        case BuildTool::Brush:
            return "A convex solid. Subtract one from a wall and you have a doorway.";
        case BuildTool::Water:
            return "Drag a rectangle. Swimmable has depth to swim in; Surface is the top only.";
        case BuildTool::Terrain:
            return "Drag over the ground to raise or lower it. Makes a terrain if there is none.";
        case BuildTool::Cave:
            return "Dig where you point, into the ground or into a wall you have already "
                   "opened. Brush picks what the drag means. Fill puts rock back.";
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
        case BuildTool::Roof:
        case BuildTool::Brush:
            return true;
        default:
            return false;
    }
}

bool BuildToolCanSubtract(BuildTool tool) {
    // A roof is brushes, and two of them are already cuts: the slopes are made
    // by taking wedges off a block. Subtracting the whole thing from something
    // else would turn those cuts inside out, so it has no second mode.
    if (tool == BuildTool::Roof) return false;
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
    // A pick from a named list. The range is derived from the list, so a name
    // added to it is selectable without touching a second number.
    auto choice = [&](const char* label, f32* value, const char* const* names, u32 n) {
        if (count >= kBuildMaxFields || n == 0) return;
        BuildField f{label, value, 0.0f, static_cast<f32>(n - 1), ""};
        f.kind = BuildFieldKind::Choice;
        f.choices = names;
        f.choiceCount = n;
        // A value left over from a longer list (or typed past the end by a
        // script) is brought back in range here, where every reader passes.
        *value = std::clamp(std::round(*value), 0.0f, static_cast<f32>(n - 1));
        fields[count++] = f;
    };

    static const char* const kWaterKinds[]   = { "Surface", "Swimmable" };
    static const char* const kPlantKinds[]   = { "Grass", "Shrubs", "Trees" };
    static const char* const kRoofKinds[]    = { "Flat", "Shed", "Gable" };
    static const char* const kOpeningKinds[] = { "Doorway", "Door", "Window" };

    switch (tool) {
        case BuildTool::Wall:
            add("Height",    &s.height,    kCreativeWallHeightMin, kCreativeWallHeightMax, "m");
            add("Thickness", &s.thickness, kCreativeWallThicknessMin, kCreativeWallThicknessMax, "m");
            break;
        case BuildTool::Floor:
            add("Thickness", &s.thickness, 0.05f,  2.0f, "m");
            add("Elevation", &s.elevation, -20.0f, 20.0f, "m");
            break;
        case BuildTool::Path:
            add("Height",    &s.height,    kCreativeWallHeightMin, kCreativeWallHeightMax, "m");
            add("Thickness", &s.thickness, kCreativeWallThicknessMin, kCreativeWallThicknessMax, "m");
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
            choice("Kind", &s.waterKind, kWaterKinds, 2);
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
        case BuildTool::Cave:
            // Brush first, because it changes what the other three mean.
            // Bore is the space you walk through; Rough is how far the wall
            // wanders from a perfect tube -- the difference between a cave and
            // a drainpipe; Depth is how far a Shaft sinks or a Ramp descends.
            {
                // Named from VoxelBrushName, so the rail and the carve cannot
                // call the same brush two things.
                static const char* names[static_cast<usize>(Geometry::VoxelBrush::Count)];
                for (u8 i = 0; i < static_cast<u8>(Geometry::VoxelBrush::Count); ++i) {
                    names[i] = Geometry::VoxelBrushName(static_cast<Geometry::VoxelBrush>(i));
                }
                choice("Brush", &s.caveBrush, names,
                       static_cast<u32>(Geometry::VoxelBrush::Count));
            }
            add("Bore",  &s.radius,    0.75f, 12.0f, "m");
            add("Depth", &s.caveDepth, 1.00f, 30.0f, "m");
            add("Rough", &s.roughness, 0.00f,  1.5f, "m");
            break;
        case BuildTool::Plants:
            // Kind is a 0..2 pick rendered as a slider, for the same reason the
            // rest of this rail is sliders: one row shape, one interaction.
            choice("Kind", &s.plantKind, kPlantKinds, 3);
            add("Density", &s.plantDensity, 0.1f, 4.0f, "x");
            break;
        case BuildTool::Prop: {
            u32 n = 0;
            const RailProp* props = RailProps(n);
            static const char* names[64];
            n = std::min<u32>(n, 64);
            for (u32 i = 0; i < n; ++i) names[i] = props[i].name;
            choice("Kind", &s.propKind, names, n);
            break;
        }
        case BuildTool::Roof:
            choice("Kind", &s.roofKind, kRoofKinds, 3);
            add("Height", &s.roofBase, 0.0f, 20.0f, "m");
            if (static_cast<int>(s.roofKind + 0.5f) == static_cast<int>(RoofKind::Flat)) {
                // A flat roof has no slope to set, and a Pitch box that did
                // nothing would be the switch-that-does-nothing again.
                add("Thickness", &s.thickness, 0.05f, 1.0f, "m");
            } else {
                add("Pitch", &s.pitch, kCreativeRoofPitchMin, kCreativeRoofPitchMax, "deg");
            }
            add("Overhang", &s.overhang, 0.0f, 2.0f, "m");
            break;
        case BuildTool::Door:
            choice("Kind", &s.openingKind, kOpeningKinds, 3);
            if (static_cast<int>(s.openingKind + 0.5f) == static_cast<int>(OpeningKind::Window)) {
                add("Width",  &s.windowWidth,  0.30f, 6.0f, "m");
                add("Height", &s.windowHeight, 0.30f, 4.0f, "m");
                add("Sill",   &s.sill,         0.10f, 4.0f, "m");
            } else {
                add("Width",  &s.doorWidth,  0.50f, 6.0f, "m");
                add("Height", &s.doorHeight, 1.00f, 6.0f, "m");
            }
            break;
        case BuildTool::Paint: {
            BuildField f{"Colour", &s.paintColour, 0.0f,
                         static_cast<f32>(kCreativePaintColourCount - 1), ""};
            f.kind = BuildFieldKind::Swatch;
            f.choiceCount = kCreativePaintColourCount;
            s.paintColour = std::clamp(std::round(s.paintColour), 0.0f, f.maxValue);
            if (count < kBuildMaxFields) fields[count++] = f;

            static const char* names[kCreativePaintFinishCount];
            for (u32 i = 0; i < kCreativePaintFinishCount; ++i) names[i] = kCreativePaintFinishes[i].name;
            choice("Finish", &s.paintFinish, names, kCreativePaintFinishCount);
            break;
        }
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
    // Fill is the eraser for the hole mask. Without it the mask had a writer
    // and nothing that could take a cell back: a tunnel that opened more of the
    // hillside than you wanted could only be undone whole, tunnel and all.
    static const char* kDigFill[2]    = { "Dig", "Fill" };
    if (BuildToolCanSubtract(tool)) return kAddCut;
    if (tool == BuildTool::Terrain) return kRaiseLower;
    if (tool == BuildTool::Cave)    return kDigFill;
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

        case BuildTool::Roof: {
            if (spanX < kCreativeMinDragLength || spanZ < kCreativeMinDragLength) return false;

            const f32 over = std::max(0.0f, settings.overhang);
            const f32 hx = spanX * 0.5f + over;
            const f32 hz = spanZ * 0.5f + over;
            const f32 cx = (dragStart.x + dragEnd.x) * 0.5f;
            const f32 cz = (dragStart.z + dragEnd.z) * 0.5f;
            const f32 base = settings.roofBase;

            int kind = static_cast<int>(settings.roofKind + 0.5f);
            kind = std::max(0, std::min(kind, static_cast<int>(RoofKind::Count) - 1));

            if (kind == static_cast<int>(RoofKind::Flat)) {
                const f32 thickness = std::max(kMinHalf * 2.0f, settings.thickness);
                ECS::BrushSolidComponent::Brush slab;
                slab.shape = ECS::BrushSolidComponent::Shape::Box;
                // Sits ON its height, the opposite of a floor: the number is
                // where the walls stop, and the roof starts there.
                slab.center = Math::Vector3(cx, base + thickness * 0.5f, cz);
                slab.halfExtents = Math::Vector3(hx, thickness * 0.5f, hz);
                out.brushes.push_back(slab);
                return true;
            }

            // A sloped roof is a block with the corners taken off.
            //
            // Brush CSG has boxes and prisms and no wedge, and a three-sided
            // prism is equilateral, which fixes the pitch at 60 degrees. So the
            // slope is made the way a doorway is: an Add block the size of the
            // whole roof, and one big Subtract box per slope, turned so its
            // underside lies exactly on the slope. That gives any pitch, closed
            // gable ends, and a solid the collider can stand on.
            //
            // The ridge runs along the LONGER side of the drag, which is the
            // way a real roof spans: across the short dimension.
            const bool ridgeAlongX = spanX >= spanZ;
            const f32 halfLength = ridgeAlongX ? hx : hz;
            const f32 halfSpan   = ridgeAlongX ? hz : hx;

            const f32 pitchDeg = std::clamp(settings.pitch, kCreativeRoofPitchMin, kCreativeRoofPitchMax);
            const f32 pitch = pitchDeg * 0.01745329252f;
            const bool gable = (kind == static_cast<int>(RoofKind::Gable));
            // A gable climbs half the span to the ridge; a shed climbs all of it.
            const f32 run  = gable ? halfSpan : halfSpan * 2.0f;
            const f32 rise = run * std::tan(pitch);
            const f32 fascia = kCreativeRoofFascia;

            ECS::BrushSolidComponent::Brush body;
            body.shape = ECS::BrushSolidComponent::Shape::Box;
            body.center = Math::Vector3(cx, base + (fascia + rise) * 0.5f, cz);
            body.halfExtents = ridgeAlongX
                ? Math::Vector3(halfLength, (fascia + rise) * 0.5f, halfSpan)
                : Math::Vector3(halfSpan, (fascia + rise) * 0.5f, halfLength);
            out.brushes.push_back(body);

            // Half the cutter's size. Only has to be bigger than what it cuts.
            const f32 reach = run + rise + 1.0f;
            const int slopes = gable ? 2 : 1;
            for (int i = 0; i < slopes; ++i) {
                const f32 side = (i == 0) ? 1.0f : -1.0f;
                // The slope runs from the eave at `side * halfSpan` up to the
                // ridge, `run` further in. Its outward normal leans to `side`.
                const f32 midAcross = side * (halfSpan - run * 0.5f);
                const f32 midY = base + fascia + rise * 0.5f;
                const f32 nAcross = side * std::sin(pitch);
                const f32 nY = std::cos(pitch);

                ECS::BrushSolidComponent::Brush cutter;
                cutter.shape = ECS::BrushSolidComponent::Shape::Box;
                cutter.op = Geometry::BrushOp::Subtract;
                if (ridgeAlongX) {
                    cutter.center = Math::Vector3(cx, midY + nY * reach, cz + midAcross + nAcross * reach);
                    // About X by a: local +Y goes to (0, cos a, sin a).
                    cutter.rotation = Math::Quaternion::FromEuler(Math::Vector3(side * pitch, 0.0f, 0.0f));
                    cutter.halfExtents = Math::Vector3(halfLength + 1.0f, reach, reach);
                } else {
                    cutter.center = Math::Vector3(cx + midAcross + nAcross * reach, midY + nY * reach, cz);
                    // About Z by b: local +Y goes to (-sin b, cos b, 0).
                    cutter.rotation = Math::Quaternion::FromEuler(Math::Vector3(0.0f, 0.0f, -side * pitch));
                    cutter.halfExtents = Math::Vector3(reach, reach, halfLength + 1.0f);
                }
                out.brushes.push_back(cutter);
            }
            return true;
        }

        default:
            return false;
    }
}

// --------------------------------------------------------------------------
// Prop table
// --------------------------------------------------------------------------

const RailProp* RailProps(u32& count) {
    // Names are what the chip on the surface says, so they are short: the
    // options column is two chips wide.
    static const RailProp kProps[] = {
        {"Ball",        PropKind::Ball,       nullptr, nullptr, 0.0f},
        {"Light",       PropKind::Light,      nullptr, nullptr, 0.0f},
        {"Physics Box", PropKind::PhysicsBox, nullptr, nullptr, 0.0f},
        {"Barrel",      PropKind::Barrel,     nullptr, nullptr, 0.0f},
        {"Spawn Point", PropKind::SpawnPoint, nullptr, nullptr, 0.0f},
        // From the Entity menu. The lift is half the thing's height where its
        // origin is its centre, and nothing where its origin is its foot.
        {"Player 3rd",   PropKind::Count, "Player Character", "Third Person", 0.80f},
        {"Player 1st",   PropKind::Count, "Player Character", "First Person", 0.80f},
        {"Door",         PropKind::Count, "Gameplay", "Door",         0.00f},
        {"Trigger Zone", PropKind::Count, "Gameplay", "Trigger Zone", 1.00f},
        {"Save Point",   PropKind::Count, "Gameplay", "Save Point",   0.10f},
        // A rope hangs DOWN from its entity, so it starts overhead.
        {"Rope",         PropKind::Count, "Gameplay", "Rope",         4.00f},
        {"Sound",        PropKind::Count, "", "Audio Source",         1.00f},
        {"Particles",    PropKind::Count, "", "Particle Emitter",     0.10f},
    };
    count = static_cast<u32>(sizeof(kProps) / sizeof(kProps[0]));
    return kProps;
}

// --------------------------------------------------------------------------
// Door: an opening cut where you click
// --------------------------------------------------------------------------

bool RayHitBoxBrush(const ECS::BrushSolidComponent::Brush& brush,
                    const Math::Vector3& origin, const Math::Vector3& direction,
                    BrushRayHit& out) {
    if (brush.shape != ECS::BrushSolidComponent::Shape::Box) return false;

    // Into the box's own frame, where it is axis-aligned and the test is three
    // pairs of planes.
    const Math::Quaternion inv = brush.rotation.Inverse();
    const Math::Vector3 o = inv.Rotate(origin - brush.center);
    const Math::Vector3 d = inv.Rotate(direction);
    const Math::Vector3& h = brush.halfExtents;

    f32 tNear = -1e30f, tFar = 1e30f;
    u8 nearAxis = 0;
    for (usize a = 0; a < 3; ++a) {
        if (std::fabs(d[a]) < 1e-8f) {
            // Parallel to this pair of faces: either between them all the way,
            // or never.
            if (o[a] < -h[a] || o[a] > h[a]) return false;
            continue;
        }
        f32 t0 = (-h[a] - o[a]) / d[a];
        f32 t1 = ( h[a] - o[a]) / d[a];
        if (t0 > t1) std::swap(t0, t1);
        if (t0 > tNear) { tNear = t0; nearAxis = static_cast<u8>(a); }
        tFar = std::min(tFar, t1);
        if (tNear > tFar) return false;
    }
    // Behind the origin, or the origin is inside: there is no face in front
    // to put a door in.
    if (tNear <= 0.0f) return false;

    out.t = tNear;
    out.point = origin + direction * tNear;
    out.axis = nearAxis;
    return true;
}

bool PointInsideBoxBrush(const ECS::BrushSolidComponent::Brush& brush, const Math::Vector3& point) {
    if (brush.shape != ECS::BrushSolidComponent::Shape::Box) return false;
    const Math::Vector3 p = brush.rotation.Inverse().Rotate(point - brush.center);
    const Math::Vector3& h = brush.halfExtents;
    return std::fabs(p.x) <= h.x && std::fabs(p.y) <= h.y && std::fabs(p.z) <= h.z;
}

bool FindOpeningTarget(const ECS::BrushSolidComponent& solid,
                       const Math::Vector3& origin, const Math::Vector3& direction,
                       usize& brushIndex, BrushRayHit& hit) {
    bool found = false;
    for (usize i = 0; i < solid.brushes.size(); ++i) {
        const auto& b = solid.brushes[i];
        if (!b.enabled || b.op != Geometry::BrushOp::Add) continue;

        BrushRayHit h;
        if (!RayHitBoxBrush(b, origin, direction, h)) continue;
        if (found && h.t >= hit.t) continue;

        // Nudged a hair into the wall before asking whether a cut owns that
        // spot, because the point is ON the face and a cut that overruns the
        // face by its overcut would otherwise always claim it.
        const Math::Vector3 inside = h.point + direction * (1e-3f / std::max(1e-6f,
            std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z)));
        bool inACut = false;
        for (const auto& c : solid.brushes) {
            if (!c.enabled || c.op != Geometry::BrushOp::Subtract) continue;
            if (PointInsideBoxBrush(c, inside)) { inACut = true; break; }
        }
        if (inACut) continue;

        hit = h;
        brushIndex = i;
        found = true;
    }
    return found;
}

bool PlanOpening(const ECS::BrushSolidComponent::Brush& wall, const BrushRayHit& hit,
                 const BuildToolSettings& s, f32 gridSize, OpeningPlan& out) {
    if (wall.shape != ECS::BrushSolidComponent::Shape::Box) return false;
    if (hit.axis == 1) return false;   // the top or the underside

    const usize through = hit.axis;          // 0 or 2
    const usize along = 2 - through;
    const Math::Vector3& half = wall.halfExtents;
    const f32 halfAlong = half[along];
    const f32 halfY = half.y;

    int kind = static_cast<int>(s.openingKind + 0.5f);
    kind = std::max(0, std::min(kind, static_cast<int>(OpeningKind::Count) - 1));
    const bool window = (kind == static_cast<int>(OpeningKind::Window));

    const f32 width = std::max(0.2f, window ? s.windowWidth : s.doorWidth);
    if (halfAlong * 2.0f < width + kCreativeOpeningJamb * 2.0f) return false;

    // Where along the wall, in its own frame.
    const Math::Vector3 local = wall.rotation.Inverse().Rotate(hit.point - wall.center);
    f32 a = local[along];
    if (gridSize > 0.001f) {
        // From the wall's END, not its middle. A 5 m wall drawn on a 1 m grid
        // has its middle at 2.5, and snapping from there would put every
        // opening half a cell off the grid the wall itself sits on.
        a = -halfAlong + std::round((a + halfAlong) / gridSize) * gridSize;
    }
    const f32 limit = halfAlong - kCreativeOpeningJamb - width * 0.5f;
    a = std::max(-limit, std::min(limit, a));

    const f32 foot = -halfY;
    f32 y0, y1;      // the cut, with its overcut
    f32 open0, open1; // the opening you see
    if (window) {
        open0 = foot + std::max(0.0f, s.sill);
        open1 = std::min(open0 + std::max(0.2f, s.windowHeight), halfY - kCreativeOpeningLintel);
        if (open1 - open0 < 0.2f) return false;
        y0 = open0;
        y1 = open1;
    } else {
        open0 = foot;
        open1 = foot + std::max(0.2f, s.doorHeight);
        // Out through the floor line, so the threshold is not a coplanar face.
        y0 = foot - kCreativeOpeningOvercut;
        if (open1 >= halfY - kCreativeOpeningLintel) {
            // The wall is no taller than the door: the gap goes all the way up
            // rather than leaving a lintel too thin to see.
            open1 = halfY;
            y1 = halfY + kCreativeOpeningOvercut;
        } else {
            y1 = open1;
        }
    }

    Math::Vector3 centre(0.0f, (y0 + y1) * 0.5f, 0.0f);
    centre[along] = a;
    Math::Vector3 cutHalf(0.0f, (y1 - y0) * 0.5f, 0.0f);
    cutHalf[along] = width * 0.5f;
    cutHalf[through] = half[through] + kCreativeOpeningOvercut;

    out = OpeningPlan{};
    out.cut.shape = ECS::BrushSolidComponent::Shape::Box;
    out.cut.op = Geometry::BrushOp::Subtract;
    out.cut.center = wall.center + wall.rotation.Rotate(centre);
    out.cut.rotation = wall.rotation;
    out.cut.halfExtents = cutHalf;

    Math::Vector3 hinge(0.0f, open0, 0.0f);
    hinge[along] = a - width * 0.5f;
    out.hinge = wall.center + wall.rotation.Rotate(hinge);
    // A door panel is built along its own +X. When the opening runs along the
    // wall's Z, turn it a quarter about Y so +X lands on +Z.
    out.rotation = (along == 0)
        ? wall.rotation
        : wall.rotation * Math::Quaternion::FromEuler(Math::Vector3(0.0f, -1.57079632679f, 0.0f));
    out.width = width;
    out.height = open1 - open0;
    out.wallThickness = half[through] * 2.0f;
    return true;
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
            // cursor when it went down matters here.
            //
            // The terrain mesh is CENTRED on its transform, so the press point
            // is the transform -- no corner arithmetic. This used to subtract a
            // half-extent, on the belief that the grid runs from the transform
            // out to +X/+Z. It does not: MeshFactory::CreateTerrain centres it.
            // The brush and the raycast believed the same wrong thing, which is
            // how a stroke landed half a terrain from the cursor while the
            // brush ring drew in exactly the right place.
            const f32 half = static_cast<f32>(kCreativeTerrainGrid) * kCreativeTerrainCell * 0.5f;
            out = ToolPlacement{};
            out.origin = Math::Vector3(dragStart.x, 0.0f, dragStart.z);
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

// --------------------------------------------------------------------------
// Shape handles
// --------------------------------------------------------------------------

namespace {

// The span's sideways direction, the same one the Path tool bows along, so a
// bow authored while drawing and a bow dragged afterwards mean the same thing.
bool SpanNormal(const Math::Vector3& a, const Math::Vector3& b, Math::Vector3& n) {
    const Math::Vector3 d = b - a;
    const f32 len = HorizontalLength(d);
    if (len < 1e-4f) return false;
    n = Math::Vector3(-d.z / len, 0.0f, d.x / len);
    return true;
}

Math::Vector3 SpanMid(const Math::Vector3& a, const Math::Vector3& b) {
    return Math::Vector3((a.x + b.x) * 0.5f, a.y, (a.z + b.z) * 0.5f);
}

} // namespace

bool ShapeHandleIsRound(const ShapeHandle& handle) {
    switch (handle.kind) {
        case ShapeHandleKind::WallPoint:
        case ShapeHandleKind::OutlinePoint: return true;
        case ShapeHandleKind::RectGrip:
        case ShapeHandleKind::BoxGrip:
            return BrushGripIsCorner(static_cast<BrushGrip>(handle.index));
        default: return false;
    }
}

void WallPathHandles(const ECS::WallPathComponent& path, const Math::Vector3& origin,
                     std::vector<ShapeHandle>& out) {
    for (usize i = 0; i < path.points.size(); ++i) {
        out.push_back({ShapeHandleKind::WallPoint, static_cast<i32>(i), origin + path.points[i]});
    }
    for (usize i = 0; i + 1 < path.points.size(); ++i) {
        const Math::Vector3 a = path.points[i], b = path.points[i + 1];
        Math::Vector3 n;
        if (!SpanNormal(a, b, n)) continue;
        const f32 bow = (i < path.bows.size()) ? path.bows[i] : 0.0f;
        out.push_back({ShapeHandleKind::WallBow, static_cast<i32>(i), origin + SpanMid(a, b) + n * bow});
    }
}

void OutlineHandles(const ECS::BoundaryPolygonComponent& outline, const Math::Vector3& origin,
                    std::vector<ShapeHandle>& out) {
    const usize n = outline.points.size();
    if (n < 3) return;
    auto world = [&](const Math::Vector2& p) {
        return Math::Vector3(origin.x + p.x, origin.y, origin.z + p.y);
    };
    for (usize i = 0; i < n; ++i) {
        out.push_back({ShapeHandleKind::OutlinePoint, static_cast<i32>(i), world(outline.points[i])});
    }
    for (usize i = 0; i < n; ++i) {
        const Math::Vector2 m = (outline.points[i] + outline.points[(i + 1) % n]) * 0.5f;
        out.push_back({ShapeHandleKind::OutlineEdge, static_cast<i32>(i), world(m)});
    }
}

void RectHandles(const Math::Vector3& centre, f32 width, f32 depth, std::vector<ShapeHandle>& out) {
    ECS::BrushSolidComponent::Brush box;
    box.center = centre;
    box.halfExtents = Math::Vector3(width * 0.5f, 0.0f, depth * 0.5f);
    for (u8 g = 0; g < static_cast<u8>(BrushGrip::Count); ++g) {
        out.push_back({ShapeHandleKind::RectGrip, static_cast<i32>(g),
                       BrushGripPosition(box, static_cast<BrushGrip>(g))});
    }
}

bool DragWallPathHandle(ECS::WallPathComponent& path, const ECS::WallPathComponent& start,
                        const ShapeHandle& handle, const Math::Vector3& origin,
                        const Math::Vector3& worldPoint) {
    const Math::Vector3 local = worldPoint - origin;
    const usize i = static_cast<usize>(handle.index);

    if (handle.kind == ShapeHandleKind::WallPoint) {
        if (handle.index < 0 || i >= start.points.size()) return false;
        std::vector<Math::Vector3> next = start.points;
        // Height stays where the wall stands; the ground under the cursor only
        // says where on the floor plan the corner goes.
        next[i] = Math::Vector3(local.x, start.points[i].y, local.z);
        // A corner dropped onto its neighbour leaves a span of no length, and a
        // wall of no length is nothing to rebuild.
        auto tooClose = [&](usize j) {
            return j < next.size() && HorizontalLength(next[j] - next[i]) < kCreativeMinDragLength;
        };
        if ((i > 0 && tooClose(i - 1)) || tooClose(i + 1)) return false;
        path.points = std::move(next);
        return true;
    }

    if (handle.kind == ShapeHandleKind::WallBow) {
        if (handle.index < 0 || i + 1 >= start.points.size()) return false;
        const Math::Vector3 a = start.points[i], b = start.points[i + 1];
        Math::Vector3 n;
        if (!SpanNormal(a, b, n)) return false;
        const Math::Vector3 mid = SpanMid(a, b);
        f32 bow = (local.x - mid.x) * n.x + (local.z - mid.z) * n.z;
        // Snaps back to straight near the line, so a span that was only nudged
        // stays one straight wall instead of a slight curve cut into segments.
        if (std::fabs(bow) < kCreativePathMinBow) bow = 0.0f;
        path.bows = start.bows;
        path.bows.resize(start.points.size() - 1, 0.0f);
        path.bows[i] = bow;
        return true;
    }
    return false;
}

bool DragOutlineHandle(ECS::BoundaryPolygonComponent& outline,
                       const ECS::BoundaryPolygonComponent& start,
                       const ShapeHandle& handle, const Math::Vector3& origin,
                       const Math::Vector3& worldPoint) {
    const usize n = start.points.size();
    if (n < 3 || handle.index < 0 || static_cast<usize>(handle.index) >= n) return false;
    const usize i = static_cast<usize>(handle.index);
    std::vector<Math::Vector2> next = start.points;

    if (handle.kind == ShapeHandleKind::OutlinePoint) {
        next[i] = Math::Vector2(worldPoint.x - origin.x, worldPoint.z - origin.z);
    } else if (handle.kind == ShapeHandleKind::OutlineEdge) {
        // The whole edge moves with the cursor, so a straight shore is pulled
        // out as a straight shore. Measured from where the handle was when it
        // was pressed: the edge's midpoint at the start of the drag.
        const Math::Vector2 d(worldPoint.x - handle.position.x, worldPoint.z - handle.position.z);
        next[i] = start.points[i] + d;
        next[(i + 1) % n] = start.points[(i + 1) % n] + d;
    } else {
        return false;
    }

    // Refuse a ring with no area left: the surface builder would draw nothing
    // and the pond would vanish under the cursor mid-drag.
    f32 area = 0.0f;
    for (usize k = 0; k < n; ++k) {
        const auto& p = next[k]; const auto& q = next[(k + 1) % n];
        area += p.x * q.y - q.x * p.y;
    }
    if (std::fabs(area) * 0.5f < kCreativeMinBrushExtent * kCreativeMinBrushExtent) return false;

    outline.points = std::move(next);
    outline.dirty = true;
    return true;
}

bool DragRectGrip(Math::Vector3& centre, f32& width, f32& depth,
                  BrushGrip grip, const Math::Vector3& worldPoint) {
    ECS::BrushSolidComponent::Brush box;
    box.center = centre;
    box.halfExtents = Math::Vector3(width * 0.5f, 0.5f, depth * 0.5f);
    const Math::Vector3 flat(worldPoint.x, centre.y, worldPoint.z);
    if (!ResizeBrushByGrip(box, grip, flat)) return false;
    centre = Math::Vector3(box.center.x, centre.y, box.center.z);
    width = box.halfExtents.x * 2.0f;
    depth = box.halfExtents.z * 2.0f;
    return true;
}

bool InsertWallPoint(ECS::WallPathComponent& path, usize span) {
    if (span + 1 >= path.points.size() || path.points.size() >= kCreativePathMaxPoints) return false;
    const Math::Vector3 a = path.points[span], b = path.points[span + 1];
    // On the curve when the span is bowed. The new corner lands on the wall that
    // is already there; it gives the wall somewhere new to bend without moving it.
    Math::Vector3 at = SpanMid(a, b);
    Math::Vector3 n;
    const f32 bow = (span < path.bows.size()) ? path.bows[span] : 0.0f;
    if (SpanNormal(a, b, n)) at = at + n * bow;
    path.bows.resize(path.points.size() - 1, 0.0f);
    path.points.insert(path.points.begin() + static_cast<std::ptrdiff_t>(span + 1), at);
    path.bows[span] = 0.0f;
    path.bows.insert(path.bows.begin() + static_cast<std::ptrdiff_t>(span + 1), 0.0f);
    return true;
}

bool RemoveWallPoint(ECS::WallPathComponent& path, usize point) {
    if (path.points.size() <= 2 || point >= path.points.size()) return false;
    path.bows.resize(path.points.size() - 1, 0.0f);
    path.points.erase(path.points.begin() + static_cast<std::ptrdiff_t>(point));
    // Removing an end drops its span. Removing a corner joins its two spans
    // into one, which starts straight.
    const usize drop = (point == 0) ? 0 : point - 1;
    path.bows.erase(path.bows.begin() + static_cast<std::ptrdiff_t>(drop));
    if (point > 0 && point < path.points.size() && drop < path.bows.size()) path.bows[drop] = 0.0f;
    return true;
}

bool InsertOutlinePoint(ECS::BoundaryPolygonComponent& outline, usize edge) {
    const usize n = outline.points.size();
    if (n < 3 || edge >= n || n >= 4096) return false;
    const Math::Vector2 m = (outline.points[edge] + outline.points[(edge + 1) % n]) * 0.5f;
    outline.points.insert(outline.points.begin() + static_cast<std::ptrdiff_t>(edge + 1), m);
    outline.dirty = true;
    return true;
}

bool RemoveOutlinePoint(ECS::BoundaryPolygonComponent& outline, usize point) {
    if (outline.points.size() <= 3 || point >= outline.points.size()) return false;
    outline.points.erase(outline.points.begin() + static_cast<std::ptrdiff_t>(point));
    outline.dirty = true;
    return true;
}

bool RebuildWallPath(ECS::WallPathComponent& path, ECS::BrushSolidComponent& solid) {
    BuildToolSettings s;
    s.height = path.height;
    s.thickness = path.thickness;
    ECS::BrushSolidComponent built;
    if (!CreativeMode::BuildPathBrushes(path.points, path.bows, path.segmentsPerBow, s, false, built)) {
        return false;
    }
    const usize keepFrom = std::min<usize>(path.builtBrushes, solid.brushes.size());
    std::vector<ECS::BrushSolidComponent::Brush> next = std::move(built.brushes);
    const usize made = next.size();
    next.insert(next.end(), solid.brushes.begin() + static_cast<std::ptrdiff_t>(keepFrom),
                solid.brushes.end());
    solid.brushes = std::move(next);
    solid.dirty = true;
    path.builtBrushes = static_cast<u32>(made);
    return true;
}

bool ResizeWallPath(ECS::WallPathComponent& path, ECS::BrushSolidComponent& solid,
                    f32 height, f32 thickness) {
    ECS::WallPathComponent next = path;
    next.height = std::clamp(height, kCreativeWallHeightMin, kCreativeWallHeightMax);
    next.thickness = std::clamp(thickness, kCreativeWallThicknessMin, kCreativeWallThicknessMax);
    ECS::BrushSolidComponent rebuilt = solid;
    if (!RebuildWallPath(next, rebuilt)) return false;
    path = std::move(next);
    solid = std::move(rebuilt);
    return true;
}

bool RecoverWallPath(const ECS::BrushSolidComponent& solid, ECS::WallPathComponent& out) {
    // The wall is the leading run of Add brushes; anything after it must be a
    // cut (a doorway), which a rebuild keeps. An Add after a cut is not a
    // drawn wall with a door in it, and is refused.
    usize n = 0;
    while (n < solid.brushes.size() && solid.brushes[n].op == Geometry::BrushOp::Add) ++n;
    for (usize i = n; i < solid.brushes.size(); ++i) {
        if (solid.brushes[i].op != Geometry::BrushOp::Subtract) return false;
    }
    // One box is left alone. A one-segment wall and a plank or door slab made
    // with the Box tool are the same brush, and as a box it keeps the grips that
    // change its height and thickness; recovered as a wall it would lose them.
    if (n < 2 || n > kCreativePathMaxPoints) return false;

    struct Seg { Math::Vector3 a, b; };   // foot ends, XZ plus foot height
    std::vector<Seg> segs;
    segs.reserve(n);
    const f32 height = solid.brushes[0].halfExtents.y * 2.0f;
    const f32 thickness = solid.brushes[0].halfExtents.z * 2.0f;

    for (usize k = 0; k < n; ++k) {
        const auto& br = solid.brushes[k];
        if (br.shape != ECS::BrushSolidComponent::Shape::Box) return false;
        // Upright: turned about Y only, or it was never stood on a drawn line.
        const Math::Vector3 up = br.rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
        if (up.y < 0.999f) return false;
        const Math::Vector3 h = br.halfExtents;
        // Longer than it is thick, taller than it is thick, and one wall's
        // thickness throughout. A floor fails the second, a block the first.
        if (!(h.x > h.z && h.y > h.z)) return false;
        if (std::fabs(h.y * 2.0f - height) > 0.01f || std::fabs(h.z * 2.0f - thickness) > 0.01f) return false;
        const Math::Vector3 axis = br.rotation.Rotate(Math::Vector3(1.0f, 0.0f, 0.0f));
        const Math::Vector3 foot(br.center.x, br.center.y - h.y, br.center.z);
        segs.push_back({foot - axis * h.x, foot + axis * h.x});
    }

    // Orient each segment so it runs away from the one before it.
    auto d2 = [](const Math::Vector3& p, const Math::Vector3& q) {
        const f32 dx = p.x - q.x, dz = p.z - q.z;
        return dx * dx + dz * dz;
    };
    if (n > 1) {
        const f32 keep = std::min(d2(segs[0].b, segs[1].a), d2(segs[0].b, segs[1].b));
        const f32 flip = std::min(d2(segs[0].a, segs[1].a), d2(segs[0].a, segs[1].b));
        if (flip < keep) std::swap(segs[0].a, segs[0].b);
        for (usize i = 1; i < n; ++i) {
            if (d2(segs[i].b, segs[i - 1].b) < d2(segs[i].a, segs[i - 1].b)) std::swap(segs[i].a, segs[i].b);
        }
    }

    ECS::WallPathComponent path;
    path.height = height;
    path.thickness = thickness;
    path.points.push_back(segs[0].a);
    for (usize i = 0; i + 1 < n; ++i) {
        // Where the two centre lines cross. Parallel lines (a straight run
        // split in two) meet halfway between the ends that face each other.
        const Math::Vector3 p = segs[i].a, r = segs[i].b - segs[i].a;
        const Math::Vector3 q = segs[i + 1].a, s = segs[i + 1].b - segs[i + 1].a;
        const f32 denom = r.x * s.z - r.z * s.x;
        Math::Vector3 joint;
        if (std::fabs(denom) < 1e-6f * (HorizontalLength(r) * HorizontalLength(s) + 1e-6f)) {
            joint = (segs[i].b + segs[i + 1].a) * 0.5f;
        } else {
            const f32 t = ((q.x - p.x) * s.z - (q.z - p.z) * s.x) / denom;
            joint = p + r * t;
        }
        joint.y = segs[i].a.y;
        path.points.push_back(joint);
    }
    path.points.push_back(segs[n - 1].b);
    path.bows.assign(path.points.size() - 1, 0.0f);

    // The proof: the recovered line has to build the walls that are there.
    ECS::BrushSolidComponent rebuilt;
    path.builtBrushes = 0;
    if (!RebuildWallPath(path, rebuilt) || rebuilt.brushes.size() != n) return false;
    for (usize i = 0; i < n; ++i) {
        const auto& x = rebuilt.brushes[i];
        const auto& y = solid.brushes[i];
        const Math::Vector3 dc = x.center - y.center;
        const Math::Vector3 dh = x.halfExtents - y.halfExtents;
        if (std::fabs(dc.x) > 0.01f || std::fabs(dc.y) > 0.01f || std::fabs(dc.z) > 0.01f ||
            std::fabs(dh.x) > 0.01f || std::fabs(dh.y) > 0.01f || std::fabs(dh.z) > 0.01f) return false;
        // Rotation: the long axes must agree up to direction.
        const Math::Vector3 ax = x.rotation.Rotate(Math::Vector3(1.0f, 0.0f, 0.0f));
        const Math::Vector3 ay = y.rotation.Rotate(Math::Vector3(1.0f, 0.0f, 0.0f));
        if (std::fabs(ax.x * ay.x + ax.z * ay.z) < 0.9999f) return false;
    }

    path.builtBrushes = static_cast<u32>(n);
    out = std::move(path);
    return true;
}

} // namespace Editor
} // namespace Enjin
