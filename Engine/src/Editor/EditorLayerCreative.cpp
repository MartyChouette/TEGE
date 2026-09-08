// Creative mode's surface: the hand-drawn half of option B.
//
// Everything here is ImDrawList. ImGui hosts the window and gives us the mouse,
// and we draw. That is the whole trick of option B: a genuinely custom look
// without porting docking, layout persistence, thirty-one panels' worth of
// widgets, ImGuizmo and two render backends, which is what replacing ImGui
// outright would have cost.
//
// The tool logic lives in CreativeMode.cpp and is pure. Nothing in this file
// decides what a gesture builds; it decides what the surface looks like and
// which gesture happened.

#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/CreativeMode.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/BrushSolid.h"
#include "Enjin/ECS/Systems/BrushSolidSystem.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Enjin {
namespace Editor {

namespace {

// The palette, settled in the mockup rather than in C++ at five minutes a
// rebuild. Cool blue-slate ground, warm brass for tools, and a cool cyan kept
// strictly for cutting -- so adding and subtracting can never look alike.
constexpr ImU32 kGround  = IM_COL32(0x14, 0x18, 0x1f, 0xff);
constexpr ImU32 kSurface = IM_COL32(0x1b, 0x21, 0x2b, 0xff);
constexpr ImU32 kRail    = IM_COL32(0x0f, 0x13, 0x1a, 0xff);
constexpr ImU32 kLine    = IM_COL32(0x2a, 0x32, 0x40, 0xff);
constexpr ImU32 kInk     = IM_COL32(0xe9, 0xe6, 0xe0, 0xff);
constexpr ImU32 kMuted   = IM_COL32(0x83, 0x8d, 0x9c, 0xff);
constexpr ImU32 kDim     = IM_COL32(0x5d, 0x66, 0x74, 0xff);
constexpr ImU32 kAccent  = IM_COL32(0xd8, 0x97, 0x3c, 0xff);
constexpr ImU32 kCut     = IM_COL32(0x5a, 0xa9, 0xc9, 0xff);
constexpr ImU32 kAccentSoft = IM_COL32(0xd8, 0x97, 0x3c, 0x18);
constexpr ImU32 kCutSoft    = IM_COL32(0x5a, 0xa9, 0xc9, 0x1a);

constexpr f32 kRailWidth    = 64.0f;
constexpr f32 kOptionsWidth = 232.0f;
constexpr f32 kToolHeight   = 52.0f;

// Where each tool sits in the rail's three groups: structure, volume, object.
// A separator is drawn before a tool whose group differs from the one above.
u8 ToolGroup(BuildTool tool) {
    switch (tool) {
        case BuildTool::Wall:
        case BuildTool::Floor:
        case BuildTool::Stairs:  return 0;
        case BuildTool::Brush:
        case BuildTool::Water:
        case BuildTool::Terrain: return 1;
        default:                    return 2;
    }
}

// Tool glyphs, drawn rather than shipped as an icon font: eight shapes is less
// work than an atlas, and they scale with the surface.
void DrawToolIcon(ImDrawList* dl, BuildTool tool, ImVec2 c, ImU32 col) {
    const f32 r = 10.0f;
    const f32 t = 1.6f;
    switch (tool) {
        case BuildTool::Wall: {   // a standing slab
            dl->AddRect(ImVec2(c.x - r * 0.7f, c.y - r), ImVec2(c.x + r * 0.7f, c.y + r), col, 0, 0, t);
            dl->AddLine(ImVec2(c.x - r * 0.7f, c.y), ImVec2(c.x + r * 0.7f, c.y), col, 1.0f);
            break;
        }
        case BuildTool::Floor: {  // a plan-view diamond
            const ImVec2 p[4] = { ImVec2(c.x, c.y - r * 0.6f), ImVec2(c.x + r, c.y),
                                  ImVec2(c.x, c.y + r * 0.6f), ImVec2(c.x - r, c.y) };
            dl->AddPolyline(p, 4, col, ImDrawFlags_Closed, t);
            break;
        }
        case BuildTool::Stairs: { // a rising step run
            dl->AddLine(ImVec2(c.x - r, c.y + r), ImVec2(c.x - r, c.y + r * 0.3f), col, t);
            dl->AddLine(ImVec2(c.x - r, c.y + r * 0.3f), ImVec2(c.x - r * 0.2f, c.y + r * 0.3f), col, t);
            dl->AddLine(ImVec2(c.x - r * 0.2f, c.y + r * 0.3f), ImVec2(c.x - r * 0.2f, c.y - r * 0.4f), col, t);
            dl->AddLine(ImVec2(c.x - r * 0.2f, c.y - r * 0.4f), ImVec2(c.x + r * 0.6f, c.y - r * 0.4f), col, t);
            dl->AddLine(ImVec2(c.x + r * 0.6f, c.y - r * 0.4f), ImVec2(c.x + r * 0.6f, c.y - r), col, t);
            break;
        }
        case BuildTool::Brush: {  // a box in three-quarter view
            dl->AddRect(ImVec2(c.x - r * 0.85f, c.y - r * 0.35f), ImVec2(c.x + r * 0.5f, c.y + r * 0.8f), col, 0, 0, t);
            dl->AddLine(ImVec2(c.x - r * 0.85f, c.y - r * 0.35f), ImVec2(c.x - r * 0.35f, c.y - r * 0.85f), col, t);
            dl->AddLine(ImVec2(c.x - r * 0.35f, c.y - r * 0.85f), ImVec2(c.x + r, c.y - r * 0.85f), col, t);
            dl->AddLine(ImVec2(c.x + r, c.y - r * 0.85f), ImVec2(c.x + r * 0.5f, c.y - r * 0.35f), col, t);
            break;
        }
        case BuildTool::Water: {  // two wave lines
            for (int row = 0; row < 2; ++row) {
                const f32 y = c.y - r * 0.2f + static_cast<f32>(row) * r * 0.7f;
                ImVec2 pts[7];
                for (int i = 0; i < 7; ++i) {
                    const f32 fx = -r + (2.0f * r) * (static_cast<f32>(i) / 6.0f);
                    pts[i] = ImVec2(c.x + fx, y + std::sin(static_cast<f32>(i) * 1.05f) * r * 0.22f);
                }
                dl->AddPolyline(pts, 7, col, 0, t);
            }
            break;
        }
        case BuildTool::Terrain: { // a ridge line
            const ImVec2 p[5] = { ImVec2(c.x - r, c.y + r * 0.7f), ImVec2(c.x - r * 0.35f, c.y - r * 0.4f),
                                  ImVec2(c.x, c.y + r * 0.15f), ImVec2(c.x + r * 0.45f, c.y - r * 0.7f),
                                  ImVec2(c.x + r, c.y + r * 0.7f) };
            dl->AddPolyline(p, 5, col, 0, t);
            break;
        }
        case BuildTool::Ladder: {  // two rails and three rungs
            dl->AddLine(ImVec2(c.x - r * 0.55f, c.y - r), ImVec2(c.x - r * 0.55f, c.y + r), col, t);
            dl->AddLine(ImVec2(c.x + r * 0.55f, c.y - r), ImVec2(c.x + r * 0.55f, c.y + r), col, t);
            for (int i = 0; i < 3; ++i) {
                const f32 y = c.y - r * 0.55f + static_cast<f32>(i) * r * 0.55f;
                dl->AddLine(ImVec2(c.x - r * 0.55f, y), ImVec2(c.x + r * 0.55f, y), col, t);
            }
            break;
        }
        case BuildTool::Reduce: {  // a shape collapsing to fewer points
            const ImVec2 p[4] = { ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y),
                                  ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y) };
            dl->AddPolyline(p, 4, col, ImDrawFlags_Closed, t);
            dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), col, 1.0f);
            break;
        }
        default: break;
    }
}

} // namespace

void EditorLayer::DrawCreativeSurface() {
    if (!m_Creative.IsActive()) return;

    const ImU32 tint     = m_Creative.IsSubtracting() ? kCut : kAccent;
    const ImU32 tintSoft = m_Creative.IsSubtracting() ? kCutSoft : kAccentSoft;

    ImGuiViewport* vp = ImGui::GetMainViewport();
    const f32 top = vp->WorkPos.y;
    const f32 width = kRailWidth + kOptionsWidth;

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, top));
    ImGui::SetNextWindowSize(ImVec2(width, vp->WorkSize.y));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kSurface);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("##CreativeSurface", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetWindowPos();
        const f32 height = ImGui::GetWindowSize().y;

        // --- rail -----------------------------------------------------------
        dl->AddRectFilled(origin, ImVec2(origin.x + kRailWidth, origin.y + height), kRail);
        dl->AddLine(ImVec2(origin.x + kRailWidth, origin.y),
                    ImVec2(origin.x + kRailWidth, origin.y + height), kLine, 1.0f);

        f32 y = origin.y + 10.0f;
        u8 lastGroup = ToolGroup(BuildTool::Wall);

        for (u8 i = 0; i < static_cast<u8>(BuildTool::Count); ++i) {
            const BuildTool tool = static_cast<BuildTool>(i);

            if (ToolGroup(tool) != lastGroup) {
                y += 6.0f;
                dl->AddLine(ImVec2(origin.x + 10.0f, y), ImVec2(origin.x + kRailWidth - 10.0f, y), kLine, 1.0f);
                y += 6.0f;
                lastGroup = ToolGroup(tool);
            }

            ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
            ImGui::PushID(i);
            const bool pressed = ImGui::InvisibleButton("##tool", ImVec2(kRailWidth, kToolHeight));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();

            const bool active = (m_Creative.GetTool() == tool);
            if (active) {
                dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + kRailWidth, y + kToolHeight), tintSoft);
                // A 2px bar on the leading edge, which is what makes the active
                // tool readable at a glance without a box around it.
                dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + 2.0f, y + kToolHeight), tint);
            } else if (hovered) {
                dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + kRailWidth, y + kToolHeight),
                                  IM_COL32(255, 255, 255, 8));
            }

            const ImU32 col = active ? tint : (hovered ? kInk : kDim);
            DrawToolIcon(dl, tool, ImVec2(origin.x + kRailWidth * 0.5f, y + 18.0f), col);

            const char* name = BuildToolName(tool);
            const ImVec2 ts = ImGui::CalcTextSize(name);
            dl->AddText(ImVec2(origin.x + (kRailWidth - ts.x) * 0.5f, y + 32.0f), col, name);

            if (pressed) m_Creative.SetTool(tool);
            if (hovered) ImGui::SetTooltip("%s", BuildToolVerb(tool));

            y += kToolHeight;
        }

        // --- options ---------------------------------------------------------
        const f32 ox = origin.x + kRailWidth;
        f32 oy = origin.y + 16.0f;
        const f32 innerW = kOptionsWidth - 30.0f;

        const BuildTool tool = m_Creative.GetTool();
        dl->AddText(ImVec2(ox + 15.0f, oy), kInk, BuildToolName(tool));
        oy += 22.0f;

        // The verb, wrapped. This is the line that tells someone with no AI in
        // the loop what the tool is for, so it is on the surface rather than in
        // a tooltip they have to discover.
        dl->AddText(nullptr, 0.0f, ImVec2(ox + 15.0f, oy), kMuted,
                    BuildToolVerb(tool), nullptr, innerW);
        oy += ImGui::CalcTextSize(BuildToolVerb(tool), nullptr, false, innerW).y + 14.0f;

        BuildField fields[kBuildMaxFields];
        const u32 fieldCount =
            BuildToolFields(tool, m_Creative.CurrentSettings(), fields, kBuildMaxFields);

        for (u32 f = 0; f < fieldCount; ++f) {
            BuildField& field = fields[f];
            if (!field.value) continue;

            dl->AddText(ImVec2(ox + 15.0f, oy), kDim, field.label);
            oy += 16.0f;

            const ImVec2 boxMin(ox + 15.0f, oy);
            const ImVec2 boxMax(ox + 15.0f + innerW, oy + 24.0f);

            ImGui::SetCursorScreenPos(boxMin);
            ImGui::PushID(static_cast<int>(f) + 100);
            ImGui::InvisibleButton("##field", ImVec2(innerW, 24.0f));
            const bool fieldActive = ImGui::IsItemActive();
            if (fieldActive) {
                // Drag anywhere on the row to change it. A hand-drawn surface
                // still has to feel like a control, and this is the cheapest
                // gesture that does: no handle to hit, no text field to open.
                const f32 span = field.maxValue - field.minValue;
                *field.value = std::clamp(*field.value + ImGui::GetIO().MouseDelta.x * span * 0.005f,
                                          field.minValue, field.maxValue);
            }
            ImGui::PopID();

            dl->AddRectFilled(boxMin, boxMax, kGround, 3.0f);
            dl->AddRect(boxMin, boxMax, fieldActive ? tint : kLine, 3.0f, 0, 1.0f);

            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(*field.value));
            dl->AddText(ImVec2(boxMin.x + 9.0f, boxMin.y + 4.0f), kInk, buf);
            if (field.unit[0] != '\0') {
                const ImVec2 us = ImGui::CalcTextSize(field.unit);
                dl->AddText(ImVec2(boxMax.x - us.x - 9.0f, boxMin.y + 4.0f), kDim, field.unit);
            }
            oy += 28.0f;

            // Fill bar: where this value sits in its own range, so the number
            // has a sense of scale without a second widget.
            const f32 frac = std::clamp((*field.value - field.minValue) /
                                        std::max(0.0001f, field.maxValue - field.minValue), 0.0f, 1.0f);
            dl->AddRectFilled(ImVec2(ox + 15.0f, oy), ImVec2(ox + 15.0f + innerW, oy + 3.0f), kLine, 2.0f);
            dl->AddRectFilled(ImVec2(ox + 15.0f, oy), ImVec2(ox + 15.0f + innerW * frac, oy + 3.0f), tint, 2.0f);
            oy += 13.0f;
        }

        // --- mode (add / subtract) -------------------------------------------
        if (const char* const* modes = BuildToolModeLabels(tool)) {
            oy += 4.0f;
            dl->AddText(ImVec2(ox + 15.0f, oy), kDim, "Mode");
            oy += 16.0f;

            const f32 half = (innerW - 5.0f) * 0.5f;
            for (int m = 0; m < 2; ++m) {
                const ImVec2 bMin(ox + 15.0f + static_cast<f32>(m) * (half + 5.0f), oy);
                const ImVec2 bMax(bMin.x + half, oy + 26.0f);

                ImGui::SetCursorScreenPos(bMin);
                ImGui::PushID(m + 200);
                const bool clicked = ImGui::InvisibleButton("##mode", ImVec2(half, 26.0f));
                ImGui::PopID();
                if (clicked) m_Creative.SetSubtracting(m == 1);

                const bool on = (m == 1) == m_Creative.IsSubtracting();
                const ImU32 edge = on ? (m == 1 ? kCut : kAccent) : kLine;
                dl->AddRectFilled(bMin, bMax, kGround, 3.0f);
                dl->AddRect(bMin, bMax, edge, 3.0f, 0, 1.0f);

                const ImVec2 ts = ImGui::CalcTextSize(modes[m]);
                dl->AddText(ImVec2(bMin.x + (half - ts.x) * 0.5f, bMin.y + 5.0f),
                            on ? edge : kMuted, modes[m]);
            }
            oy += 34.0f;
        }

        // --- grid + snap ------------------------------------------------------
        oy += 6.0f;
        dl->AddLine(ImVec2(ox + 15.0f, oy), ImVec2(ox + 15.0f + innerW, oy), kLine, 1.0f);
        oy += 10.0f;

        ImGui::SetCursorScreenPos(ImVec2(ox + 15.0f, oy));
        ImGui::PushID(300);
        if (ImGui::InvisibleButton("##snap", ImVec2(innerW, 20.0f))) {
            m_Creative.SetSnapEnabled(!m_Creative.IsSnapEnabled());
        }
        ImGui::PopID();
        char gridText[96];
        std::snprintf(gridText, sizeof(gridText), "grid %.2f m   snap %s",
                      static_cast<double>(m_Creative.GetGridSize()),
                      m_Creative.IsSnapEnabled() ? "on" : "off");
        dl->AddText(ImVec2(ox + 15.0f, oy + 2.0f), m_Creative.IsSnapEnabled() ? kMuted : kDim, gridText);
        oy += 24.0f;

        dl->AddText(nullptr, 0.0f, ImVec2(ox + 15.0f, oy), kDim,
                    "Drag in the viewport to build. Options never leave this column, "
                    "so nothing covers what you are making.", nullptr, innerW);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

bool EditorLayer::CreativeGroundPoint(f32 screenX, f32 screenY, f32 viewW, f32 viewH,
                                      Math::Vector3& out) const {
    if (!m_Camera) return false;

    const Ray ray = ScenePicker::ScreenToRay(m_Camera, screenX, screenY, viewW, viewH);

    // Intersect the build plane. Level blockout happens on the ground, so the
    // plane is y = 0; a ray running parallel to it (or away from it) has no
    // usable answer and must not be turned into a placement at infinity.
    constexpr f32 kParallelEpsilon = 1e-4f;
    if (std::fabs(ray.direction.y) < kParallelEpsilon) return false;

    const f32 t = -ray.origin.y / ray.direction.y;
    if (t <= 0.0f) return false;   // the plane is behind the camera

    out = Math::Vector3(ray.origin.x + ray.direction.x * t,
                        0.0f,
                        ray.origin.z + ray.direction.z * t);
    return true;
}

void EditorLayer::HandleBuildDrag() {
    if (!m_Creative.IsActive()) return;
    if (!BuildToolMakesBrushes(m_Creative.GetTool())) return;
    if (!m_PlayMode.IsStopped()) return;

    const f32 vpW = m_EditorViewportImageMaxX - m_EditorViewportImageMinX;
    const f32 vpH = m_EditorViewportImageMaxY - m_EditorViewportImageMinY;
    if (vpW <= 0.0f || vpH <= 0.0f) return;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const f32 localX = mouse.x - m_EditorViewportImageMinX;
    const f32 localY = mouse.y - m_EditorViewportImageMinY;

    Math::Vector3 ground;
    const bool onGround = CreativeGroundPoint(localX, localY, vpW, vpH, ground);
    if (onGround) ground = m_Creative.SnapToGrid(ground);

    // A press only starts a drag when it lands in the viewport. Without the
    // hover check, clicking a tool in the rail would also begin building.
    if (!m_BuildDragging) {
        if (m_EditorViewportHovered && onGround && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_BuildDragging = true;
            m_BuildDragStart = ground;
        }
        return;
    }

    // Mid-drag: outline what the release will make, on the ground, in the
    // tool's own colour. This is the "how do they know it worked" answer
    // starting before the thing exists.
    if (onGround) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const ImU32 tint = m_Creative.IsSubtracting() ? kCut : kAccent;
        dl->AddLine(ImVec2(mouse.x - 8.0f, mouse.y), ImVec2(mouse.x + 8.0f, mouse.y), tint, 1.0f);
        dl->AddLine(ImVec2(mouse.x, mouse.y - 8.0f), ImVec2(mouse.x, mouse.y + 8.0f), tint, 1.0f);

        char buf[96];
        const Math::Vector3 d = ground - m_BuildDragStart;
        std::snprintf(buf, sizeof(buf), "%.2f x %.2f m",
                      static_cast<double>(std::fabs(d.x)), static_cast<double>(std::fabs(d.z)));
        dl->AddText(ImVec2(mouse.x + 12.0f, mouse.y + 10.0f), tint, buf);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_BuildDragging = false;
        if (onGround) CommitCreativeDrag(m_BuildDragStart, ground);
    }
}

void EditorLayer::CommitCreativeDrag(const Math::Vector3& start, const Math::Vector3& end) {
    if (!m_World) return;

    const BuildTool tool = m_Creative.GetTool();

    ECS::BrushSolidComponent solid;
    if (!CreativeMode::BuildBrushes(tool, m_Creative.CurrentSettings(),
                                    m_Creative.IsSubtracting(), start, end, solid)) {
        return;
    }

    // Subtracting needs something to cut from. On its own a lone Subtract brush
    // carves air and builds nothing, so it goes onto the SELECTED solid rather
    // than into a new entity of its own -- which is what makes "cut a doorway in
    // that wall" work as one gesture.
    if (m_Creative.IsSubtracting()) {
        if (m_PrimarySelected == ECS::INVALID_ENTITY ||
            !m_World->HasComponent<ECS::BrushSolidComponent>(m_PrimarySelected)) {
            ENJIN_LOG_WARN(Editor, "Nothing to cut: select a brush solid first");
            return;
        }
        auto* target = m_World->GetComponent<ECS::BrushSolidComponent>(m_PrimarySelected);
        if (!target) return;

        const auto before = target->brushes;
        for (const auto& brush : solid.brushes) target->brushes.push_back(brush);
        target->dirty = true;
        ECS::BrushSolidSystem::Rebuild(m_World, m_PrimarySelected);

        m_UndoRedo.Execute(std::make_unique<PropertyEditCommand<std::vector<ECS::BrushSolidComponent::Brush>>>(
            "Cut Brush", before, target->brushes,
            [world = m_World, e = m_PrimarySelected](const std::vector<ECS::BrushSolidComponent::Brush>& v) {
                if (auto* s = world->GetComponent<ECS::BrushSolidComponent>(e)) {
                    s->brushes = v;
                    s->dirty = true;
                    ECS::BrushSolidSystem::Rebuild(world, e);
                }
            }));
        MarkDirty();
        return;
    }

    ECS::Entity entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(entity, BuildToolName(tool));
    m_World->AddComponent<ECS::TransformComponent>(entity);
    m_World->AddComponent<ECS::MaterialComponent>(entity);
    m_World->AddComponent<ECS::BrushSolidComponent>(entity, solid);

    // Built immediately rather than on the next system tick, so the thing you
    // just dragged is on screen when you release the mouse.
    ECS::BrushSolidSystem::Rebuild(m_World, entity);

    SelectEntity(entity);
    RecordLayerCreate(entity);
    MarkDirty();

    ENJIN_LOG_INFO(Editor, "Creative: placed %s (%zu brush%s)", BuildToolName(tool),
                   solid.brushes.size(), solid.brushes.size() == 1 ? "" : "es");
}

} // namespace Editor
} // namespace Enjin
