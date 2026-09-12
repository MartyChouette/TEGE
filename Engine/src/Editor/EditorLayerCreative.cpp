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
#include "Enjin/Editor/ScenePlacement.h"
#include <cfloat>
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/Editor/CreativeMode.h"
#include "Enjin/Editor/ScenePicker.h"
#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/BrushSolid.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Ladder.h"
#include "Enjin/ECS/Systems/BrushSolidSystem.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/MeshSimplifier.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace Enjin {
namespace Editor {

namespace {

// The swapchain is B8G8R8A8_SRGB and ImGui writes its vertex colours straight
// through, so the hardware converts them as though they were already linear and
// everything lands visibly lighter than it was authored: #1b212b arrives on
// screen as roughly RGB(91,101,114), which is what made the first pass look
// washed out and grey rather than like the palette it was designed in.
//
// So the palette is written in sRGB, the way it was chosen, and converted here.
// Measured, not guessed: the sampled pixel matched the sRGB transfer curve.
inline f32 SrgbToLinear(f32 c) {
    return (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline ImU32 Authored(u8 r, u8 g, u8 b, u8 a = 255) {
    auto conv = [](u8 v) {
        return static_cast<u8>(SrgbToLinear(static_cast<f32>(v) / 255.0f) * 255.0f + 0.5f);
    };
    // Alpha is not a colour channel and is not transfer-encoded.
    return IM_COL32(conv(r), conv(g), conv(b), a);
}

// The palette, settled in the mockup rather than in C++ at five minutes a
// rebuild. Cool blue-slate ground, warm brass for tools, and a cool cyan kept
// strictly for cutting -- so adding and subtracting can never look alike.
const ImU32 kGround  = Authored(0x14, 0x18, 0x1f);
const ImU32 kSurface = Authored(0x1b, 0x21, 0x2b);
const ImU32 kRail    = Authored(0x0f, 0x13, 0x1a);
const ImU32 kLine    = Authored(0x2a, 0x32, 0x40);
const ImU32 kInk     = Authored(0xe9, 0xe6, 0xe0);
const ImU32 kMuted   = Authored(0x83, 0x8d, 0x9c);
const ImU32 kDim     = Authored(0x5d, 0x66, 0x74);
const ImU32 kAccent  = Authored(0xd8, 0x97, 0x3c);
const ImU32 kCut     = Authored(0x5a, 0xa9, 0xc9);
const ImU32 kAccentSoft = Authored(0xd8, 0x97, 0x3c, 0x18);
const ImU32 kCutSoft    = Authored(0x5a, 0xa9, 0xc9, 0x1a);
const ImU32 kOk         = Authored(0x7f, 0xb0, 0x69);
const ImU32 kOnAccent   = Authored(0x17, 0x14, 0x0d);

// A separator is drawn before a tool whose group differs from the one above.
// The grouping itself is shared with the height budget, which has to count the
// same separators this draws.
u8 ToolGroup(BuildTool tool) { return BuildToolGroup(tool); }

// Tool glyphs, drawn rather than shipped as an icon font: eight shapes is less
// work than an atlas, and they scale with the surface.
void DrawToolIcon(ImDrawList* dl, BuildTool tool, ImVec2 c, ImU32 col, f32 ui) {
    const f32 r = 10.0f * ui;
    const f32 t = std::max(1.0f, 1.6f * ui);
    switch (tool) {
        case BuildTool::Wall: {   // a standing slab
            dl->AddRect(ImVec2(c.x - r * 0.7f, c.y - r), ImVec2(c.x + r * 0.7f, c.y + r), col, 0, 0, t);
            dl->AddLine(ImVec2(c.x - r * 0.7f, c.y), ImVec2(c.x + r * 0.7f, c.y), col, t * 0.6f);
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
        case BuildTool::Plants: {  // three sprigs on a ground line
            // Drawn rather than lettered, like every other tool on this rail.
            // A blank button is what a new tool gets by default here, and a rail
            // of shapes with one gap in it reads as a bug in the rail.
            dl->AddLine(ImVec2(c.x - r, c.y + r * 0.8f),
                        ImVec2(c.x + r, c.y + r * 0.8f), col, t);
            for (int i = 0; i < 3; ++i) {
                const f32 x = c.x + (static_cast<f32>(i) - 1.0f) * r * 0.62f;
                const f32 h = (i == 1) ? r * 1.5f : r * 1.1f;   // the middle one taller
                const ImVec2 base(x, c.y + r * 0.8f);
                dl->AddLine(base, ImVec2(x, base.y - h), col, t);
                // A leaf either side, so it reads as a plant and not a fence.
                dl->AddLine(ImVec2(x, base.y - h * 0.55f),
                            ImVec2(x - r * 0.34f, base.y - h * 0.85f), col, t);
                dl->AddLine(ImVec2(x, base.y - h * 0.55f),
                            ImVec2(x + r * 0.34f, base.y - h * 0.85f), col, t);
            }
            break;
        }
        case BuildTool::Prop: {   // a crate with a ball resting on it
            // A pair rather than one shape, because this tool is five different
            // things and no single silhouette is honest about that. Two objects
            // reads as "objects"; a barrel would read as "barrel".
            dl->AddRect(ImVec2(c.x - r * 0.9f, c.y + r * 0.05f),
                        ImVec2(c.x + r * 0.15f, c.y + r * 0.95f), col, 0, 0, t);
            dl->AddCircle(ImVec2(c.x + r * 0.5f, c.y + r * 0.5f), r * 0.42f, col, 0, t);
            break;
        }
        case BuildTool::Edit: {   // a rectangle with grips on its edges
            dl->AddRect(ImVec2(c.x - r * 0.62f, c.y - r * 0.5f),
                        ImVec2(c.x + r * 0.62f, c.y + r * 0.5f), col, 0, 0, t);
            const f32 g = 2.6f * (r / 10.0f);
            const ImVec2 grips[4] = {
                ImVec2(c.x - r * 0.62f, c.y), ImVec2(c.x + r * 0.62f, c.y),
                ImVec2(c.x, c.y - r * 0.5f),  ImVec2(c.x, c.y + r * 0.5f) };
            for (int i = 0; i < 4; ++i) {
                dl->AddRectFilled(ImVec2(grips[i].x - g, grips[i].y - g),
                                  ImVec2(grips[i].x + g, grips[i].y + g), col);
            }
            break;
        }
        case BuildTool::Reduce: {  // a shape collapsing to fewer points
            const ImVec2 p[4] = { ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y),
                                  ImVec2(c.x, c.y + r), ImVec2(c.x - r, c.y) };
            dl->AddPolyline(p, 4, col, ImDrawFlags_Closed, t);
            dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), col, t * 0.6f);
            break;
        }
        default: break;
    }
}

// The inverse of ScenePicker::ScreenToRay, and it has to stay the inverse: NDC
// Y runs DOWN here, matching screen Y, because that is what the picker assumes
// on Vulkan. Flipping one and not the other puts every projected overlay on the
// wrong side of the horizon.
//
// `imgMin` is the top-left of the viewport IMAGE, not of the window, and is
// passed in for the same reason the overlay's rect is: read from the members it
// is stale whenever the Scene panel did not draw.
bool ProjectToViewport(const Renderer::Camera* camera, const Math::Vector3& world,
                       const ImVec2& imgMin, f32 viewW, f32 viewH, ImVec2& out) {
    if (!camera || viewW <= 0.0f || viewH <= 0.0f) return false;

    const Math::Matrix4 viewProj = camera->GetViewProjectionMatrix();
    const Math::Vector4 clip = viewProj * Math::Vector4(world.x, world.y, world.z, 1.0f);
    if (clip.w <= 0.001f) return false;   // at or behind the eye

    const f32 ndcX = clip.x / clip.w;
    const f32 ndcY = clip.y / clip.w;
    const f32 ndcZ = clip.z / clip.w;
    if (ndcZ < 0.0f || ndcZ > 1.0f) return false;

    out = ImVec2(imgMin.x + (ndcX + 1.0f) * 0.5f * viewW,
                 imgMin.y + (ndcY + 1.0f) * 0.5f * viewH);
    return true;
}

// How much bigger everything on this surface is drawn.
//
// The editor's UI scale is ImGui's io.FontGlobalScale, and that reaches
// ImGui::Text and ImGui::GetFontSize -- but NOT ImDrawList::AddText with an
// explicit size, which is every single label on this surface. Left alone the
// surface half-scales: headings grow with the setting while the rail labels,
// verbs, units and hints stay 11 pixels, and at 190% the scene name is drawn
// straight through the middle of the word "Creative" (measured, 2026-09-07).
// Every number in the layout is authored at 100% and multiplied by this.
f32 CreativeUIScale() {
    const f32 s = ImGui::GetIO().FontGlobalScale;
    return (s > 0.01f) ? s : 1.0f;
}

// What the surface needs vertically, per unit of scale: the header, the rail
// with its two group separators, and the footer.
f32 CreativeNaturalHeight() {
    // Counted, not assumed: a tool added in a new band adds a separator too, and
    // a budget that missed it would push the footer down by exactly that much.
    u32 separators = 0;
    for (u8 i = 1; i < static_cast<u8>(BuildTool::Count); ++i) {
        if (BuildToolGroup(static_cast<BuildTool>(i)) !=
            BuildToolGroup(static_cast<BuildTool>(i - 1))) ++separators;
    }
    return 44.0f                                                   // header
         + 8.0f + static_cast<f32>(BuildTool::Count) * 54.0f       // rail
         + static_cast<f32>(separators) * 10.0f
         + 78.0f;                                                  // footer
}

// The scale the surface can actually be drawn at in a window this tall.
//
// The surface has a fixed list of things on it and no say in the window height.
// Eight rail tools at the editor's 190% text setting want about 1100 pixels
// before the header and footer are counted, which is more than a 900-tall
// window has, and the piece that would go missing is the FOOTER -- where Play
// lives, the one button this whole mode exists to put within reach of someone
// who has not opened a settings window. A surface that looks finished because
// the only missing part is the part below the fold is the worst kind of broken.
// So the scale comes down until everything fits.
f32 CreativeFittedScale(f32 availableHeight) {
    const f32 ui = CreativeUIScale();
    const f32 natural = CreativeNaturalHeight();
    if (natural * ui <= availableHeight || availableHeight <= 0.0f) return ui;
    // Never below legible. Under this the surface stops fitting again, and a
    // footer pinned to the bottom is still better than one off the screen.
    return std::max(0.7f, std::min(ui, availableHeight / natural));
}

} // namespace

f32 EditorLayer::CreativeSurfaceWidthPx() const {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const f32 avail = vp ? (vp->Size.y - ImGui::GetFrameHeight()) : 0.0f;
    return kCreativeSurfaceWidth * CreativeFittedScale(avail);
}

void EditorLayer::DrawCreativeSurface() {
    // Cleared FIRST so that leaving creative mode leaves no stale rectangle
    // behind. A rect that outlives the thing it describes is worse than no rect:
    // it would tell injected input to avoid a region that is now open ground.
    m_CreativeSurfaceMinX = m_CreativeSurfaceMinY = 0.0f;
    m_CreativeSurfaceMaxX = m_CreativeSurfaceMaxY = 0.0f;

    if (!m_Creative.IsActive()) return;

    // Below the main menu bar, not over it. The first version used the viewport
    // work area, which does not account for the editor's menu bar, so the
    // surface painted straight across File / Edit / View.
    const f32 menuBarH = ImGui::GetFrameHeight();
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const f32 surfaceH = vp->Size.y - menuBarH;

    // One spacing scale for the whole surface. The first version picked numbers
    // per element and the result collided: labels sat on top of their own value
    // boxes, and the rail clipped "Terrain" to "errain". Everything below is
    // derived from these, and every string is drawn at an EXPLICIT size so the
    // whole surface shrinks and grows as one piece.
    const f32 ui = CreativeFittedScale(surfaceH);
    const f32 kPad       = 14.0f * ui;   // surface edge -> content
    const f32 kToolH     = 54.0f * ui;
    const f32 kLabelH    = 18.0f * ui;   // a small label and the gap under it
    const f32 kBoxH      = 26.0f * ui;   // a value box
    const f32 kTrackH    =  3.0f * ui;
    const f32 kRowGap    = 12.0f * ui;   // between one field and the next
    const f32 kSmallText = 11.0f * ui;   // rail labels, units, hints
    const f32 kBodyText  = 17.0f * ui;   // headings and values
    const f32 kFooterH   = 78.0f * ui;
    const f32 kHeaderH   = std::max(44.0f * ui, kBodyText + kSmallText + 18.0f * ui);

    const f32 surfaceW = kCreativeSurfaceWidth * ui;
    const f32 railW    = kCreativeRailWidth * ui;

    const ImU32 tint     = m_Creative.IsSubtracting() ? kCut : kAccent;
    const ImU32 tintSoft = m_Creative.IsSubtracting() ? kCutSoft : kAccentSoft;

    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + menuBarH));
    ImGui::SetNextWindowSize(ImVec2(surfaceW, surfaceH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##CreativeSurface", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const ImVec2 o = ImGui::GetWindowPos();
        const f32 h = ImGui::GetWindowSize().y;
        const f32 optX  = o.x + railW;

        // Record where the surface is, so injected input can aim past it.
        // Recorded HERE rather than computed by the caller, because the size
        // depends on the fitted scale this function derives.
        m_CreativeSurfaceMinX = o.x;
        m_CreativeSurfaceMinY = o.y;
        m_CreativeSurfaceMaxX = o.x + surfaceW;
        m_CreativeSurfaceMaxY = o.y + h;
        const f32 innerW = (kCreativeOptionsWidth * ui) - kPad * 2.0f;

        auto text = [&](f32 size, const ImVec2& at, ImU32 col, const char* str, f32 wrap = 0.0f) {
            dl->AddText(font, size, at, col, str, nullptr, wrap);
        };
        auto measure = [&](f32 size, const char* str, f32 wrap = 0.0f) {
            return font->CalcTextSizeA(size, FLT_MAX, wrap, str);
        };

        // Paint the whole surface rather than inheriting the editor's window
        // colour, which is what made this read as a discoloured ImGui panel
        // instead of as its own thing.
        dl->AddRectFilled(o, ImVec2(o.x + surfaceW, o.y + h), kSurface);
        dl->AddRectFilled(ImVec2(o.x, o.y), ImVec2(o.x + railW, o.y + h), kRail);
        dl->AddLine(ImVec2(o.x + railW, o.y), ImVec2(o.x + railW, o.y + h), kLine, 1.0f);
        dl->AddLine(ImVec2(o.x + surfaceW, o.y), ImVec2(o.x + surfaceW, o.y + h), kLine, 1.0f);

        // --- header: identity, and the way back out -------------------------
        dl->AddRectFilled(o, ImVec2(o.x + surfaceW, o.y + kHeaderH), kRail);
        dl->AddLine(ImVec2(o.x, o.y + kHeaderH), ImVec2(o.x + surfaceW, o.y + kHeaderH), kLine, 1.0f);
        dl->AddRectFilled(o, ImVec2(o.x + 3.0f * ui, o.y + kHeaderH), kAccent);

        const f32 titleY = o.y + 5.0f * ui;
        // The mode's own name, not a hardcoded "Creative". Tutorial uses this
        // same surface, and a header that called it Creative while a walkthrough
        // ran over it would be the surface disagreeing with the menu you used to
        // get here.
        text(kBodyText, ImVec2(o.x + kPad, titleY), kInk, EditorModeName(m_EditorMode));

        const std::string sceneLabel =
            m_CurrentScenePath.empty() ? std::string("unsaved scene")
                                       : std::filesystem::path(m_CurrentScenePath).filename().string();
        text(kSmallText, ImVec2(o.x + kPad, titleY + kBodyText + 2.0f * ui), kDim, sceneLabel.c_str());

        {
            const char* leave = "Full Editor";
            const ImVec2 ls = measure(kSmallText, leave);
            const f32 bw = ls.x + 16.0f * ui;
            const f32 bh = ls.y + 12.0f * ui;
            const ImVec2 bMin(o.x + surfaceW - bw - kPad, o.y + (kHeaderH - bh) * 0.5f);
            const ImVec2 bMax(bMin.x + bw, bMin.y + bh);
            ImGui::SetCursorScreenPos(bMin);
            ImGui::PushID(900);
            if (ImGui::InvisibleButton("##leave", ImVec2(bw, bh))) {
                SetEditorMode(EditorMode::Developer);
            }
            const bool hov = ImGui::IsItemHovered();
            ImGui::PopID();
            dl->AddRect(bMin, bMax, hov ? kInk : kLine, 3.0f, 0, 1.0f);
            text(kSmallText, ImVec2(bMin.x + 8.0f * ui, bMin.y + 6.0f * ui),
                 hov ? kInk : kMuted, leave);
            if (hov) ImGui::SetTooltip("Back to the full editor, with every panel (Ctrl+B)");
        }

        // --- rail -----------------------------------------------------------
        f32 y = o.y + kHeaderH + 8.0f * ui;
        u8 lastGroup = ToolGroup(BuildTool::Wall);

        for (u8 i = 0; i < static_cast<u8>(BuildTool::Count); ++i) {
            const BuildTool t = static_cast<BuildTool>(i);
            if (ToolGroup(t) != lastGroup) {
                y += 5.0f * ui;
                dl->AddLine(ImVec2(o.x + 12.0f * ui, y), ImVec2(o.x + railW - 12.0f * ui, y), kLine, 1.0f);
                y += 5.0f * ui;
                lastGroup = ToolGroup(t);
            }

            ImGui::SetCursorScreenPos(ImVec2(o.x, y));
            ImGui::PushID(i);
            const bool pressed = ImGui::InvisibleButton("##tool", ImVec2(railW, kToolH));
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();

            const bool on = (m_Creative.GetTool() == t);
            if (on) {
                dl->AddRectFilled(ImVec2(o.x, y), ImVec2(o.x + railW, y + kToolH), tintSoft);
                dl->AddRectFilled(ImVec2(o.x, y), ImVec2(o.x + 2.0f * ui, y + kToolH), tint);
            } else if (hovered) {
                dl->AddRectFilled(ImVec2(o.x, y), ImVec2(o.x + railW, y + kToolH),
                                  Authored(0xff, 0xff, 0xff, 10));
            }

            const ImU32 col = on ? tint : (hovered ? kInk : kDim);
            // The label sits under the icon, so the icon is centred in the space
            // ABOVE the caption rather than in the whole cell -- centring it in
            // the cell walks it down into its own caption as the scale goes up.
            const f32 labelH = kSmallText + 3.0f * ui;
            DrawToolIcon(dl, t, ImVec2(o.x + railW * 0.5f, y + (kToolH - labelH) * 0.5f), col, ui);

            // Sized explicitly: at body size "Terrain" is wider than the rail
            // and was rendering as "errain".
            const char* name = BuildToolName(t);
            const ImVec2 ns = measure(kSmallText, name);
            text(kSmallText, ImVec2(o.x + (railW - ns.x) * 0.5f, y + kToolH - labelH), col, name);

            if (pressed) m_Creative.SetTool(t);
            if (hovered) ImGui::SetTooltip("%s", BuildToolVerb(t));
            y += kToolH;
        }
        const f32 railBottom = y;

        // --- options ---------------------------------------------------------
        const BuildTool tool = m_Creative.GetTool();
        f32 oy = o.y + kHeaderH + kPad;

        text(kBodyText, ImVec2(optX + kPad, oy), kInk, BuildToolName(tool));
        oy += kBodyText + 5.0f * ui;

        const char* verb = BuildToolVerb(tool);
        text(kSmallText, ImVec2(optX + kPad, oy), kMuted, verb, innerW);
        oy += measure(kSmallText, verb, innerW).y + kRowGap + 4.0f * ui;

        BuildField fields[kBuildMaxFields];
        const u32 count = BuildToolFields(tool, m_Creative.CurrentSettings(), fields, kBuildMaxFields);

        for (u32 f = 0; f < count; ++f) {
            BuildField& fd = fields[f];
            if (!fd.value) continue;

            text(kSmallText, ImVec2(optX + kPad, oy), kDim, fd.label);
            oy += kLabelH;

            const ImVec2 bMin(optX + kPad, oy);
            const ImVec2 bMax(optX + kPad + innerW, oy + kBoxH);
            ImGui::SetCursorScreenPos(bMin);
            ImGui::PushID(static_cast<int>(f) + 100);
            ImGui::InvisibleButton("##field", ImVec2(innerW, kBoxH));
            const bool held = ImGui::IsItemActive();
            const bool hov  = ImGui::IsItemHovered();
            if (held) {
                const f32 span = fd.maxValue - fd.minValue;
                // Divided by the scale so a drag moves the same amount of VALUE
                // per pixel however large the box is drawn.
                *fd.value = std::clamp(
                    *fd.value + ImGui::GetIO().MouseDelta.x * span * 0.005f / ui,
                    fd.minValue, fd.maxValue);
            }
            ImGui::PopID();
            if (hov || held) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

            dl->AddRectFilled(bMin, bMax, kGround, 3.0f);
            dl->AddRect(bMin, bMax, held ? tint : (hov ? kMuted : kLine), 3.0f, 0, 1.0f);

            // A drag lands on 6.37 sides as easily as on 6, so an integral
            // field is snapped as well as printed whole. Showing "6.00 sides"
            // and quietly rounding it invites someone to go looking for what
            // the fraction did.
            const bool integral = BuildToolSettings::FieldIsIntegral(
                fd.value, m_Creative.CurrentSettings());
            if (integral) *fd.value = std::round(*fd.value);

            char buf[48];
            std::snprintf(buf, sizeof(buf), integral ? "%.0f" : "%.2f",
                          static_cast<double>(*fd.value));
            text(kBodyText, ImVec2(bMin.x + 9.0f * ui, bMin.y + (kBoxH - kBodyText) * 0.5f), kInk, buf);
            if (fd.unit[0] != 0) {
                const ImVec2 us = measure(kSmallText, fd.unit);
                text(kSmallText, ImVec2(bMax.x - us.x - 9.0f * ui, bMin.y + (kBoxH - us.y) * 0.5f),
                     kDim, fd.unit);
            }
            oy += kBoxH + 5.0f * ui;

            const f32 frac = std::clamp((*fd.value - fd.minValue) /
                                        std::max(0.0001f, fd.maxValue - fd.minValue), 0.0f, 1.0f);
            dl->AddRectFilled(ImVec2(optX + kPad, oy),
                              ImVec2(optX + kPad + innerW, oy + kTrackH), kLine, 2.0f);
            dl->AddRectFilled(ImVec2(optX + kPad, oy),
                              ImVec2(optX + kPad + innerW * frac, oy + kTrackH), tint, 2.0f);
            oy += kTrackH + kRowGap;
        }

        // --- add / subtract ---------------------------------------------------
        if (const char* const* modes = BuildToolModeLabels(tool)) {
            text(kSmallText, ImVec2(optX + kPad, oy), kDim, "Mode");
            oy += kLabelH;

            const f32 half = (innerW - 6.0f * ui) * 0.5f;
            for (int m = 0; m < 2; ++m) {
                const ImVec2 bMin(optX + kPad + static_cast<f32>(m) * (half + 6.0f * ui), oy);
                const ImVec2 bMax(bMin.x + half, oy + kBoxH);
                ImGui::SetCursorScreenPos(bMin);
                ImGui::PushID(m + 200);
                if (ImGui::InvisibleButton("##mode", ImVec2(half, kBoxH))) {
                    m_Creative.SetSubtracting(m == 1);
                }
                const bool hov = ImGui::IsItemHovered();
                ImGui::PopID();

                const bool sel = ((m == 1) == m_Creative.IsSubtracting());
                const ImU32 edge = sel ? (m == 1 ? kCut : kAccent) : (hov ? kMuted : kLine);
                dl->AddRectFilled(bMin, bMax, sel ? (m == 1 ? kCutSoft : kAccentSoft) : kGround, 3.0f);
                dl->AddRect(bMin, bMax, edge, 3.0f, 0, 1.0f);
                const ImVec2 ts = measure(kSmallText, modes[m]);
                text(kSmallText, ImVec2(bMin.x + (half - ts.x) * 0.5f, bMin.y + (kBoxH - ts.y) * 0.5f),
                     sel ? edge : kMuted, modes[m]);
            }
            oy += kBoxH + kRowGap;
        }

        // --- grid + snap ------------------------------------------------------
        dl->AddLine(ImVec2(optX + kPad, oy), ImVec2(optX + kPad + innerW, oy), kLine, 1.0f);
        oy += 10.0f * ui;

        // The grid size itself, not just whether it is on. Everything a tool
        // places snaps to this, and until now it could only be READ off the
        // surface -- the number was printed and there was no way to change it
        // without leaving creative mode. Four chips rather than a slider,
        // because a blockout grid is a handful of round numbers and a slider is
        // a way to miss all of them.
        text(kSmallText, ImVec2(optX + kPad, oy), kDim, "Grid");
        oy += kLabelH;

        const f32 chipGap = 5.0f * ui;
        const f32 chipW = (innerW - chipGap * static_cast<f32>(kCreativeGridChoiceCount - 1))
                          / static_cast<f32>(kCreativeGridChoiceCount);
        for (u32 c = 0; c < kCreativeGridChoiceCount; ++c) {
            const f32 choice = kCreativeGridChoices[c];
            const ImVec2 bMin(optX + kPad + static_cast<f32>(c) * (chipW + chipGap), oy);
            const ImVec2 bMax(bMin.x + chipW, oy + kBoxH);
            ImGui::SetCursorScreenPos(bMin);
            ImGui::PushID(static_cast<int>(c) + 400);
            if (ImGui::InvisibleButton("##grid", ImVec2(chipW, kBoxH))) {
                m_Creative.SetGridSize(choice);
                // Choosing a grid says you want the grid. Setting a size that
                // then did nothing because snapping was off would read as the
                // button being broken.
                m_Creative.SetSnapEnabled(true);
            }
            const bool hov = ImGui::IsItemHovered();
            ImGui::PopID();

            const bool sel = std::fabs(m_Creative.GetGridSize() - choice) < 0.001f &&
                             m_Creative.IsSnapEnabled();
            dl->AddRectFilled(bMin, bMax, sel ? tintSoft : kGround, 3.0f);
            dl->AddRect(bMin, bMax, sel ? tint : (hov ? kMuted : kLine), 3.0f, 0, 1.0f);

            char label[16];
            std::snprintf(label, sizeof(label), "%g", static_cast<double>(choice));
            const ImVec2 ts = measure(kSmallText, label);
            text(kSmallText, ImVec2(bMin.x + (chipW - ts.x) * 0.5f, bMin.y + (kBoxH - ts.y) * 0.5f),
                 sel ? tint : kMuted, label);
            if (hov) ImGui::SetTooltip("Snap everything to a %g m grid", static_cast<double>(choice));
        }
        oy += kBoxH + 6.0f * ui;

        const f32 snapH = kSmallText + 9.0f * ui;
        ImGui::SetCursorScreenPos(ImVec2(optX + kPad, oy));
        ImGui::PushID(300);
        if (ImGui::InvisibleButton("##snap", ImVec2(innerW, snapH))) {
            m_Creative.SetSnapEnabled(!m_Creative.IsSnapEnabled());
        }
        const bool snapHov = ImGui::IsItemHovered();
        ImGui::PopID();
        char grid[80];
        std::snprintf(grid, sizeof(grid), "Snapping: %s",
                      m_Creative.IsSnapEnabled() ? "on" : "off");
        text(kSmallText, ImVec2(optX + kPad, oy + 4.0f * ui),
             m_Creative.IsSnapEnabled() ? (snapHov ? kInk : kMuted) : kDim, grid);
        if (snapHov) {
            ImGui::SetTooltip("Click to turn grid snapping %s",
                              m_Creative.IsSnapEnabled() ? "off" : "on");
        }

        // --- footer: the one instruction, then play ---------------------------
        //
        // Pinned to the bottom, but never on top of the content above it. What
        // lives here is Play, the one button this whole mode exists to put in
        // reach of someone who has not opened a settings window, so it is worth
        // being explicit that it cannot be pushed off or drawn over: the fitted
        // scale keeps the rail short enough to leave room, and this max() is
        // what holds if it ever cannot.
        const f32 fy = std::max(o.y + h - kFooterH,
                                std::max(oy, railBottom) + kRowGap * 2.0f);
        dl->AddLine(ImVec2(optX + kPad, fy - 12.0f * ui),
                    ImVec2(optX + kPad + innerW, fy - 12.0f * ui), kLine, 1.0f);
        // The instruction follows the tool: Terrain and Reduce are not drags,
        // and telling someone to drag on the ground while Reduce is armed sends
        // them hunting for a bug that is not there.
        const char* instruction = "Drag on the ground to build.";
        if (tool == BuildTool::Terrain)     instruction = "Drag over the ground to sculpt it.";
        else if (tool == BuildTool::Reduce) instruction = "Click a model to cut its triangles.";
        else if (tool == BuildTool::Edit)   instruction = "Click something, then drag a handle.";
        else if (tool == BuildTool::Path)   instruction = "Click corners. Enter finishes, Esc cancels.";
        text(kSmallText, ImVec2(optX + kPad, fy), kDim, instruction, innerW);

        {
            const bool playing = !m_PlayMode.IsStopped();
            const f32 btnH = 34.0f * ui;
            const ImVec2 bMin(optX + kPad, fy + 24.0f * ui);
            const ImVec2 bMax(optX + kPad + innerW, bMin.y + btnH);
            ImGui::SetCursorScreenPos(bMin);
            ImGui::PushID(901);
            if (ImGui::InvisibleButton("##play", ImVec2(innerW, btnH))) {
                if (playing) m_PlayMode.Stop(); else m_PlayMode.Play();
            }
            const bool hov = ImGui::IsItemHovered();
            ImGui::PopID();

            dl->AddRectFilled(bMin, bMax, playing ? kOk : kAccent, 4.0f);
            if (hov) dl->AddRect(bMin, bMax, kInk, 4.0f, 0, 1.0f);
            const char* label = playing ? "Stop" : "Play";
            const ImVec2 ls = measure(kBodyText, label);
            text(kBodyText, ImVec2(bMin.x + (innerW - ls.x) * 0.5f, bMin.y + (btnH - ls.y) * 0.5f),
                 kOnAccent, label);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

bool EditorLayer::CreativeGroundPoint(f32 screenX, f32 screenY, f32 viewW, f32 viewH,
                                      Math::Vector3& out) const {
    if (!m_Camera) return false;
    // Screen to ray is the camera's business; ray to build plane is the tool's,
    // and lives with the rest of the geometry so it can be tested without one.
    const Ray ray = ScenePicker::ScreenToRay(m_Camera, screenX, screenY, viewW, viewH);
    return CreativeMode::GroundHit(ray.origin, ray.direction, out);
}

// Advance the walkthrough. Tutorial mode only.
//
// Reads the world and the rail rather than being driven by them: a step
// completes because the thing it asked for is true, not because some other code
// remembered to tell it. That is what lets you wander off, build three other
// things, and come back to a tutorial that is still on the right step.
void EditorLayer::TickWalkthrough() {
    if (m_EditorMode != EditorMode::Tutorial) return;

    WalkthroughContext ctx;
    ctx.world = m_World;
    ctx.currentTool = m_Creative.GetTool();
    ctx.isPlaying = !m_PlayMode.IsStopped();

    if (m_Walkthrough.Update(ctx)) {
        // Once, on the frame it completes.
        if (m_Announcer.enabled) {
            m_Announcer.Announce(m_Walkthrough.IsFinished()
                                     ? "Walkthrough complete"
                                     : m_Walkthrough.Current().instruction.c_str(),
                                 Accessibility::AnnouncePriority::Low);
        }
    }
}

// The guide: what to do next and why, over the viewport, in Tutorial mode.
//
// Top-right, because the bottom edge already carries the tool name and the
// brush/triangle readout, and the top-left has the gizmo buttons and the
// shading row. Drawn with the same hand-rolled surface the rest of creative
// mode uses, at the editor's UI scale -- ImGui's FontGlobalScale does not reach
// ImDrawList::AddText, so every size here carries the scale itself.
void EditorLayer::DrawGuide(const ImVec2& imgMin, const ImVec2& imgMax) {
    if (m_EditorMode != EditorMode::Tutorial || !m_World) return;
    if (imgMax.x - imgMin.x < 32.0f || imgMax.y - imgMin.y < 32.0f) return;

    const f32 ui = m_EditorSettings.uiScale;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();

    const f32 kHead = 15.0f * ui;
    const f32 kBody = 12.0f * ui;
    const f32 pad   = 12.0f * ui;
    const f32 cardW = 300.0f * ui;

    const auto& step = m_Walkthrough.Current();
    const bool done = m_Walkthrough.IsFinished();

    const char* heading = done ? "That is the whole tour" : step.instruction.c_str();
    const char* body = done
        ? "Everything here is the same editor. Switch to Developer from the "
          "View menu when you want the panels."
        : step.guidance.c_str();

    // Measured, then boxed. Wrapping is done by ImGui's own measurement rather
    // than a character count, because the text is authored at 100% and the card
    // is not.
    const ImVec2 headSize = font->CalcTextSizeA(kHead, FLT_MAX, cardW - pad * 2.0f, heading);
    const ImVec2 bodySize = font->CalcTextSizeA(kBody, FLT_MAX, cardW - pad * 2.0f, body);

    const f32 barH = 4.0f * ui;
    const f32 cardH = pad + headSize.y + 6.0f * ui + bodySize.y + 10.0f * ui + barH + pad;

    const ImVec2 c0(imgMax.x - cardW - 14.0f * ui, imgMin.y + 14.0f * ui);
    const ImVec2 c1(c0.x + cardW, c0.y + cardH);

    dl->AddRectFilled(c0, c1, IM_COL32(22, 25, 33, 235), 6.0f * ui);
    dl->AddRect(c0, c1, IM_COL32(90, 150, 210, 180), 6.0f * ui, 0, 1.5f * ui);

    f32 y = c0.y + pad;
    dl->AddText(font, kHead, ImVec2(c0.x + pad, y), IM_COL32(235, 240, 255, 255),
                heading, nullptr, cardW - pad * 2.0f);
    y += headSize.y + 6.0f * ui;
    dl->AddText(font, kBody, ImVec2(c0.x + pad, y), IM_COL32(170, 180, 205, 255),
                body, nullptr, cardW - pad * 2.0f);
    y += bodySize.y + 10.0f * ui;

    // Progress as a bar rather than a number alone: "3 of 7" tells you where you
    // are, the bar tells you how much is left without reading.
    const usize total = m_Walkthrough.StepCount();
    const usize at = done ? total : m_Walkthrough.CurrentStep();
    const f32 frac = total ? static_cast<f32>(at) / static_cast<f32>(total) : 1.0f;
    const ImVec2 b0(c0.x + pad, y);
    const ImVec2 b1(c1.x - pad, y + barH);
    dl->AddRectFilled(b0, b1, IM_COL32(60, 66, 82, 255), barH * 0.5f);
    if (frac > 0.0f) {
        dl->AddRectFilled(b0, ImVec2(b0.x + (b1.x - b0.x) * frac, b1.y),
                          IM_COL32(90, 170, 240, 255), barH * 0.5f);
    }

    char prog[48];
    std::snprintf(prog, sizeof(prog), "%zu of %zu", at, total);
    const ImVec2 ps = font->CalcTextSizeA(kBody * 0.9f, FLT_MAX, 0.0f, prog);
    dl->AddText(font, kBody * 0.9f, ImVec2(c1.x - pad - ps.x, b0.y - ps.y - 3.0f * ui),
                IM_COL32(140, 150, 175, 255), prog);
}

void EditorLayer::DrawCreativeOverlay(const ImVec2& imgMin, const ImVec2& imgMax) {
    // The guide first, because it is the one thing in Tutorial mode that has to
    // be there whether or not the rest of the overlay has anything to say.
    DrawGuide(imgMin, imgMax);

    if (!m_Creative.IsActive() || !m_World) return;
    if (!m_PlayMode.IsStopped()) return;   // nothing to author while the game runs

    // The rect is passed in rather than read from the members, because the
    // members are only correct on the frames the Scene image actually drew and
    // the overlay landed hundreds of pixels off when it read a stale one.
    const f32 x0 = imgMin.x, x1 = imgMax.x;
    const f32 y0 = imgMin.y, y1 = imgMax.y;

    // These are only written while the Scene panel actually draws its image. If
    // the Game View tab is the active one, or the panel is collapsed, they keep
    // whatever they held before -- initially (0,0,0,0). Drawing against that put
    // the first-run hint in the top-left corner of the SCREEN, across the menu
    // bar and the tab strip. A degenerate or unset rect means there is no
    // viewport to draw over, so there is nothing to say.
    if (x1 - x0 < 32.0f || y1 - y0 < 32.0f) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // What the scene is made of, counted from the brush solids themselves so
    // the number moves the instant a drag commits. That is the "how do they
    // know it worked" answer: two counters that visibly change.
    usize brushCount = 0, triCount = 0;
    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::BrushSolidComponent>()) {
        if (auto* solid = m_World->GetComponent<ECS::BrushSolidComponent>(e)) {
            brushCount += solid->brushes.size();
            triCount += solid->lastTriangleCount;
        }
    }

    ImFont* font = ImGui::GetFont();
    // Same reason as the surface: an explicit size ignores the editor's UI
    // scale, so the readouts have to carry it themselves or they stay 11 pixels
    // while everything around them grows.
    const f32 ui = CreativeUIScale();
    const f32 kSmall = 11.0f * ui;

    // Along the BOTTOM edge. The top-left of the viewport already carries the
    // gizmo buttons and the shading-mode row, and the first version painted
    // straight over them.
    const BuildTool tool = m_Creative.GetTool();
    const ImU32 tint = m_Creative.IsSubtracting() ? kCut : kAccent;

    char toolText[96];
    std::snprintf(toolText, sizeof(toolText), "%s%s", BuildToolName(tool),
                  m_Creative.IsSubtracting() ? "  -  subtracting" : "");
    dl->AddText(font, kSmall, ImVec2(x0 + 14.0f * ui, y1 - kSmall - 13.0f * ui), tint, toolText);

    // The drag preconditions, in one word, next to the tool.
    //
    // Added because a report of "it does not work" could not be told apart from
    // a report of "it works and I am pointing at the wrong thing" -- and I could
    // not reproduce either, having no way to drive a mouse. This makes the next
    // report data rather than a guess: whichever word is showing when a click
    // does nothing is the reason it did nothing.
    {
        const char* state = "ready";
        ImU32 stateCol = kDim;
        Math::Vector3 probeGround;
        if (!m_PlayMode.IsStopped()) {
            state = "playing - press Stop to build";
            stateCol = kMuted;
        } else if (!m_EditorViewportHovered) {
            state = "cursor not over the viewport";
            stateCol = kMuted;
        } else if (!CreativeGroundPoint(ImGui::GetIO().MousePos.x - x0,
                                        ImGui::GetIO().MousePos.y - y0,
                                        x1 - x0, y1 - y0, probeGround)) {
            // Computed here rather than passed in: the overlay is the only place
            // that knows the viewport rect is current this frame, which is the
            // same reason the rest of these readouts live here.
            state = "no ground under the cursor";
            stateCol = kMuted;
        } else if (m_BuildDragging) {
            state = "dragging";
            stateCol = kAccent;
        }
        const f32 tw = font->CalcTextSizeA(kSmall, FLT_MAX, 0.0f, toolText).x;
        dl->AddText(font, kSmall,
                    ImVec2(x0 + 14.0f * ui + tw + 12.0f * ui, y1 - kSmall - 13.0f * ui),
                    stateCol, state);
    }

    // Terrain and Reduce work on triangle counts rather than brush counts, and a
    // readout that only ever says "brushes 0" while a hillside grows under the
    // cursor is a readout that answers the wrong question.
    usize meshTris = 0;
    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::MeshComponent>()) {
        if (auto* mesh = m_World->GetComponent<ECS::MeshComponent>(e)) {
            meshTris += mesh->indices.size() / 3;
        }
    }

    char readout[160];
    std::snprintf(readout, sizeof(readout),
                  "grid %.2f m   snap %s   brushes %zu   brush tris %zu   scene tris %zu",
                  static_cast<double>(m_Creative.GetGridSize()),
                  m_Creative.IsSnapEnabled() ? "on" : "off", brushCount, triCount, meshTris);
    const ImVec2 rs = font->CalcTextSizeA(kSmall, FLT_MAX, 0.0f, readout);
    dl->AddText(font, kSmall, ImVec2(x1 - rs.x - 14.0f * ui, y1 - kSmall - 13.0f * ui), kDim, readout);

    // Empty state. Someone opening creative mode for the first time is looking
    // at an empty grid, and the single most useful thing on screen is the one
    // sentence that gets them from nothing to a wall.
    // Not while the ground is out of reach: the drag handler puts its own
    // message in the middle of the viewport for that case, and telling someone
    // to "drag on the ground" on top of "you cannot reach the ground" is worse
    // than saying nothing. Tested at the centre of the image, which is the same
    // question the drag asks under the cursor.
    Math::Vector3 centreHit;
    const bool groundInReach = CreativeGroundPoint((x1 - x0) * 0.5f, (y1 - y0) * 0.5f,
                                                   x1 - x0, y1 - y0, centreHit);

    if (brushCount == 0 && meshTris == 0 && groundInReach) {
        const char* line1 = "Pick a tool on the left, then drag on the ground.";
        const char* line2 = "Wall and Floor are the two to start with.";
        // Sized and spaced off the same number, because the fixed +/-10 offsets
        // this used to have were less than one line tall at the editor's scale
        // and drew the second sentence through the first. Also clamped to the
        // viewport width: over a narrow Scene pane the first line ran out past
        // both edges.
        const f32 line = std::min(17.0f * ui, (x1 - x0) * 0.045f);
        const ImVec2 s1 = font->CalcTextSizeA(line, FLT_MAX, 0.0f, line1);
        const ImVec2 s2 = font->CalcTextSizeA(line, FLT_MAX, 0.0f, line2);
        const f32 cx = (x0 + x1) * 0.5f;
        const f32 cy = (y0 + y1) * 0.5f;
        dl->AddText(font, line, ImVec2(cx - s1.x * 0.5f, cy - line - 2.0f), kMuted, line1);
        dl->AddText(font, line, ImVec2(cx - s2.x * 0.5f, cy + 4.0f), kDim, line2);
    }
}

// Abandon whatever gesture is in flight, without committing it.
//
// Every exit from the drag handler that is not a mouse release has to come
// through here. ImGui calls io.ClearInputMouse() when the window loses focus,
// and that zeroes DownDuration AND DownDurationPrev -- so a button held at the
// moment you Alt-Tab away never produces an IsMouseReleased, and a flag cleared
// only in the release branch stays set forever. What that looked like: start a
// wall, Alt-Tab, come back, and the next click anywhere commits a wall stretching
// from the abandoned anchor; or start a terrain stroke, Alt-Tab, and every later
// stroke keeps deforming the first terrain because the press guard is
// `pressed && !m_BrushActive` and never re-targets.
void EditorLayer::CancelCreativeGesture() {
    m_BuildDragging = false;
    // A half-clicked path is a gesture in flight like any other. Left standing
    // it would keep drawing over the viewport after the tool changed, and the
    // next Enter would build a wall nobody was still drawing.
    m_CreativePathPoints.clear();
    m_CreativePathBows.clear();
    m_CreativePathBow = -1;
    if (m_BrushActive && m_CreativeTerrainTarget != ECS::INVALID_ENTITY) {
        // The stroke did happen, so it stays on the terrain -- but it becomes
        // its own undo step rather than being silently merged into the next one.
        if (auto* terrain = m_World ? m_World->GetComponent<ECS::TerrainComponent>(m_CreativeTerrainTarget)
                                    : nullptr) {
            if (m_TerrainUndoHeightmapSnapshot != terrain->heightmap ||
                m_TerrainUndoSplatmapSnapshot != terrain->splatmap) {
                m_UndoRedo.Execute(std::make_unique<TerrainSculptCommand>(
                    m_World, m_CreativeTerrainTarget,
                    std::move(m_TerrainUndoHeightmapSnapshot), terrain->heightmap,
                    std::move(m_TerrainUndoSplatmapSnapshot), terrain->splatmap));
                MarkDirty();
            }
        }
    }
    m_TerrainUndoHeightmapSnapshot.clear();
    m_TerrainUndoSplatmapSnapshot.clear();
    m_BrushActive = false;
    m_CreativeTerrainTarget = ECS::INVALID_ENTITY;
}

void EditorLayer::HandleBuildDrag() {
    // Each of these is a way to leave mid-gesture: Ctrl+B out of creative mode,
    // hitting Play, or the Scene panel going behind another dock tab.
    if (!m_Creative.IsActive() || !m_PlayMode.IsStopped()) {
        CancelCreativeGesture();
        return;
    }

    const f32 vpW = m_EditorViewportImageMaxX - m_EditorViewportImageMinX;
    const f32 vpH = m_EditorViewportImageMaxY - m_EditorViewportImageMinY;
    if (vpW <= 0.0f || vpH <= 0.0f) {
        CancelCreativeGesture();
        return;
    }

    // The button is not down and we still think a gesture is running: the
    // release was eaten (focus loss), so end it here instead of waiting for one
    // that is never coming.
    //
    // The release edge has to be excluded, and leaving it out cost every
    // press-drag-release tool in the mode.
    //
    // On the frame a drag ends normally, IsMouseDown is ALREADY false -- that is
    // what a release is -- while IsMouseReleased is true for that one frame. So
    // this ran on every successful release, cancelled the gesture, and the
    // commit below never saw m_BuildDragging set. Wall, Floor, Stairs, Brush,
    // Water, Ladder and Prop all dragged out a live preview, showed the length
    // in metres, and then built nothing on release. Path, Terrain, Reduce and
    // Edit return before this point and so kept working, which is why the mode
    // looked half-alive rather than broken: "none of the items really drag and
    // drop", with the four that are not drags fine.
    //
    // A release that was really eaten leaves the button up with no edge at all,
    // which is what this now tests for.
    if ((m_BuildDragging || m_BrushActive) &&
        GestureWasAbandoned(ImGui::IsMouseDown(ImGuiMouseButton_Left),
                            ImGui::IsMouseReleased(ImGuiMouseButton_Left))) {
        CancelCreativeGesture();
    }

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const f32 localX = mouse.x - m_EditorViewportImageMinX;
    const f32 localY = mouse.y - m_EditorViewportImageMinY;

    Math::Vector3 ground;
    const bool onGround = CreativeGroundPoint(localX, localY, vpW, vpH, ground);
    if (onGround) ground = m_Creative.SnapToGrid(ground);
    m_CreativeOnGroundThisFrame = onGround;

    // Say when the ground is out of reach, instead of doing nothing.
    //
    // Every tool but Reduce works on the y = 0 build plane, and
    // CreativeGroundPoint refuses a ray that runs parallel to it or points away
    // rather than inventing a placement at infinity. That refusal was silent:
    // you dragged and nothing happened, which reads as a broken tool. The case
    // that matters is a 2D scene, where the editor camera looks along -Z with
    // the ground plane edge-on and NO drag can ever land -- but it is the same
    // answer for a camera that has been orbited flat or dropped below the floor,
    // so the check is geometric rather than a scene-type test.
    // Why a click will not build, said out loud.
    //
    // A build tool that silently does nothing is indistinguishable from a broken
    // one, and that is exactly how this was reported: "none of the items really
    // drag and drop". Three things have to be true before a press starts a drag
    // -- the cursor over the viewport image, a usable angle onto the ground, and
    // play stopped -- and when one is not, the surface now names it instead of
    // ignoring the click.
    if (m_EditorViewportHovered && !onGround && m_Creative.GetTool() != BuildTool::Reduce) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const f32 ui = CreativeUIScale();
        const f32 size = 13.0f * ui;
        const char* why = "Too flat an angle to build. Orbit down, or press Home to frame the ground.";
        const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, why);
        const f32 cx = (m_EditorViewportImageMinX + m_EditorViewportImageMaxX) * 0.5f;
        const f32 cy = (m_EditorViewportImageMinY + m_EditorViewportImageMaxY) * 0.5f;
        const ImVec2 bMin(cx - ts.x * 0.5f - 12.0f * ui, cy - ts.y * 0.5f - 7.0f * ui);
        const ImVec2 bMax(cx + ts.x * 0.5f + 12.0f * ui, cy + ts.y * 0.5f + 7.0f * ui);
        dl->AddRectFilled(bMin, bMax, Authored(0x14, 0x18, 0x1f, 0xdd), 4.0f);
        dl->AddRect(bMin, bMax, kLine, 4.0f, 0, 1.0f);
        dl->AddText(font, size, ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), kMuted, why);
    }

    // Two tools are not press-drag-release and are handled on their own terms.
    // Terrain paints continuously while the button is held, and Reduce is a
    // single click on a model with no footprint at all. Forcing either into the
    // drag shape would mean sculpting only on mouse-up, which is not sculpting.
    switch (m_Creative.GetTool()) {
        case BuildTool::Terrain:
            // A brush drag left running when the tool changed would commit on
            // the next release under a tool that never started it.
            if (m_BuildDragging) CancelCreativeGesture();
            HandleCreativeTerrain(ground, onGround, localX, localY, vpW, vpH);
            return;
        case BuildTool::Reduce:
            if (m_BuildDragging || m_BrushActive) CancelCreativeGesture();
            HandleCreativeReduce(localX, localY, vpW, vpH);
            return;
        case BuildTool::Edit:
            if (m_BuildDragging || m_BrushActive) CancelCreativeGesture();
            HandleCreativeEdit(localX, localY, vpW, vpH, ground, onGround);
            return;
        case BuildTool::Path:
            if (m_BuildDragging || m_BrushActive) CancelCreativeGesture();
            HandleCreativePath(vpW, vpH, ground, onGround);
            return;
        default:
            if (m_BrushActive) CancelCreativeGesture();
            break;
    }

    // A press only starts a drag when it lands in the viewport. Without the
    // hover check, clicking a tool in the rail would also begin building.
    if (!m_BuildDragging) {
        if (m_EditorViewportHovered && onGround && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_BuildDragging = true;
            m_BuildDragStart = ground;
        }
        return;
    }

    // Mid-drag: show what the release will make, in the tool's own colour.
    // This is the "how do they know it worked" answer starting before the thing
    // exists.
    if (onGround) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const ImU32 tint = m_Creative.IsSubtracting() ? kCut : kAccent;
        const BuildTool tool = m_Creative.GetTool();
        const f32 ui = CreativeUIScale();
        const f32 arm = 8.0f * ui;
        dl->AddLine(ImVec2(mouse.x - arm, mouse.y), ImVec2(mouse.x + arm, mouse.y), tint, 1.0f);
        dl->AddLine(ImVec2(mouse.x, mouse.y - arm), ImVec2(mouse.x, mouse.y + arm), tint, 1.0f);

        char buf[128];
        const Math::Vector3 d = ground - m_BuildDragStart;
        const f32 length = std::sqrt(d.x * d.x + d.z * d.z);
        // Walls and stairs run along the drag, so their useful number is a
        // length; floors and brushes cover a region, so theirs is an area. A
        // ladder is dragged along the wall it leans on, so it is a length too --
        // and it carries its height, because the height is the number that
        // decides whether the climb reaches the ledge.
        if (tool == BuildTool::Ladder) {
            std::snprintf(buf, sizeof(buf), "%.2f m wide, %.2f m tall",
                          static_cast<double>(std::max(std::fabs(d.x), std::fabs(d.z))),
                          static_cast<double>(m_Creative.CurrentSettings().height));
        } else if (tool == BuildTool::Wall || tool == BuildTool::Stairs) {
            std::snprintf(buf, sizeof(buf), "%.2f m", static_cast<double>(length));
        } else {
            std::snprintf(buf, sizeof(buf), "%.2f x %.2f m",
                          static_cast<double>(std::fabs(d.x)), static_cast<double>(std::fabs(d.z)));
        }
        dl->AddText(ImGui::GetFont(), 13.0f * ui, ImVec2(mouse.x + 12.0f * ui, mouse.y + 10.0f * ui),
                    tint, buf);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        m_BuildDragging = false;
        if (onGround) CommitCreativeDrag(m_BuildDragStart, ground);
    }
}

// Which solid a cut belongs to.
//
// Answering this from the gesture rather than from the selection is what makes
// "cut a doorway in that wall" ONE action. Creative mode hides the Hierarchy and
// turns off viewport picking (a left click in the viewport is a drag, not a
// selection), so a Subtract that demanded a prior selection could only ever cut
// the thing you had just built.
//
// The selected solid still wins when the cut touches it, so a deliberate choice
// is never overridden by whatever else happens to be nearby.
static ECS::Entity FindSolidToCut(ECS::World* world, ECS::Entity preferred,
                                  const ECS::BrushSolidComponent& cut) {
    if (!world || cut.brushes.empty()) return ECS::INVALID_ENTITY;

    // The cut's own bounds. Prism brushes are bounded by their radius on the
    // two horizontal axes and their half height on Y, so both shapes reduce to
    // a half extent and there is no need to build the geometry to ask this.
    AABB cutBox;
    for (const auto& b : cut.brushes) {
        const Math::Vector3 half =
            (b.shape == ECS::BrushSolidComponent::Shape::Prism)
                ? Math::Vector3(b.radius, b.halfHeight, b.radius)
                : b.halfExtents;
        // Rotated brushes bound conservatively by their longest half extent:
        // an over-large box can only make this MORE willing to find a target,
        // and the alternative is transforming eight corners to save nothing.
        const f32 r = std::max(half.x, std::max(half.y, half.z));
        cutBox.Expand(b.center - Math::Vector3(r, r, r));
        cutBox.Expand(b.center + Math::Vector3(r, r, r));
    }

    auto overlaps = [&](ECS::Entity e) {
        const AABB box = ScenePicker::CalculateEntityAABB(world, e);
        if (box.min.x > box.max.x) return false;   // no geometry to bound
        return cutBox.min.x <= box.max.x && cutBox.max.x >= box.min.x &&
               cutBox.min.y <= box.max.y && cutBox.max.y >= box.min.y &&
               cutBox.min.z <= box.max.z && cutBox.max.z >= box.min.z;
    };

    if (preferred != ECS::INVALID_ENTITY &&
        world->HasComponent<ECS::BrushSolidComponent>(preferred) && overlaps(preferred)) {
        return preferred;
    }

    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::BrushSolidComponent>()) {
        if (e == preferred) continue;
        if (overlaps(e)) return e;
    }

    // Nothing was hit. Fall back to a deliberate selection anyway: someone who
    // selected a solid and dragged past it meant that solid, and a cut that
    // lands outside is a mistake they can see and undo.
    if (preferred != ECS::INVALID_ENTITY &&
        world->HasComponent<ECS::BrushSolidComponent>(preferred)) {
        return preferred;
    }
    return ECS::INVALID_ENTITY;
}

void EditorLayer::CommitCreativeDrag(const Math::Vector3& start, const Math::Vector3& end) {
    if (!m_World) return;

    const BuildTool tool = m_Creative.GetTool();

    // Water and Ladder place a component instead of building brushes, so they
    // never reach BuildBrushes at all.
    if (!BuildToolMakesBrushes(tool)) {
        PlaceCreativeComponent(tool, start, end);
        return;
    }

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
        const ECS::Entity cutInto = FindSolidToCut(m_World, m_PrimarySelected, solid);
        if (cutInto == ECS::INVALID_ENTITY) {
            ENJIN_LOG_WARN(Editor, "Nothing to cut: drag across something you have built");
            return;
        }
        auto* target = m_World->GetComponent<ECS::BrushSolidComponent>(cutInto);
        if (!target) return;

        const auto before = target->brushes;
        for (const auto& brush : solid.brushes) target->brushes.push_back(brush);
        target->dirty = true;
        ECS::BrushSolidSystem::Rebuild(m_World, cutInto);

        m_UndoRedo.Execute(std::make_unique<PropertyEditCommand<std::vector<ECS::BrushSolidComponent::Brush>>>(
            "Cut Brush", before, target->brushes,
            [world = m_World, e = cutInto](const std::vector<ECS::BrushSolidComponent::Brush>& v) {
                if (auto* s = world->GetComponent<ECS::BrushSolidComponent>(e)) {
                    s->brushes = v;
                    s->dirty = true;
                    ECS::BrushSolidSystem::Rebuild(world, e);
                }
            }));
        // Selecting what was cut is the feedback: the outline moves to the wall
        // that changed, so a cut that landed in the wrong solid is visible
        // rather than something you find later.
        SelectEntity(cutInto);
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
    FinishCreativePlacement(entity);

    ENJIN_LOG_INFO(Editor, "Creative: placed %s (%zu brush%s)", BuildToolName(tool),
                   solid.brushes.size(), solid.brushes.size() == 1 ? "" : "es");
}

// A placement is one undo step. Every creative gesture went straight into the
// world with no command behind it, so a stray drag could only be cleaned up by
// finding the entity in a hierarchy the mode deliberately hides. Snapshotted
// AFTER the components are on, because the command serializes the entity as it
// finds it and a snapshot taken too early redoes an empty entity.
void EditorLayer::FinishCreativePlacement(ECS::Entity entity) {
    if (!m_World || entity == ECS::INVALID_ENTITY) return;
    m_UndoRedo.Execute(std::make_unique<FullCreateEntityCommand>(
        m_World, entity,
        [this](ECS::Entity restored) { SelectEntity(restored); }));
    MarkDirty();
}

// ---------------------------------------------------------------------------
// Water and Ladder: place a component, sized by the drag
// ---------------------------------------------------------------------------

ECS::Entity EditorLayer::PlaceCreativeComponent(BuildTool tool,
                                                const Math::Vector3& start,
                                                const Math::Vector3& end) {
    if (!m_World) return ECS::INVALID_ENTITY;

    ToolPlacement plan;
    if (!CreativeMode::PlanPlacement(tool, m_Creative.CurrentSettings(), start, end, plan)) {
        return ECS::INVALID_ENTITY;
    }

    const BuildToolSettings& s = m_Creative.CurrentSettings();
    ECS::Entity entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(entity, BuildToolName(tool));
    auto& xf = m_World->AddComponent<ECS::TransformComponent>(entity);

    if (tool == BuildTool::Water && s.waterKind >= 0.5f) {
        // A swimmable body of water: the component ControllerSystem reads to
        // put a character into a swim state. Playground's Pool is one of these.
        //
        // WaterVolume's transform position IS the surface level, and its
        // halfExtents.y is how far the water reaches below it -- so the surface
        // goes where the Surface slider says and the body hangs underneath.
        xf.position = Math::Vector3(plan.origin.x, s.elevation, plan.origin.z);

        auto& wv = m_World->AddComponent<ECS::WaterVolumeComponent>(entity);
        wv.halfExtents = Math::Vector3(plan.halfExtents.x,
                                       std::max(0.25f, s.waterDepth),
                                       plan.halfExtents.z);
        wv.waterType = ECS::WaterType::Lake;
        wv.waveHeight = s.waveScale;

        if (auto* nc = m_World->GetComponent<ECS::NameComponent>(entity)) {
            nc->name = "Water (swimmable)";
        }

        SelectEntity(entity);
        RecordLayerCreate(entity);
        FinishCreativePlacement(entity);
        ENJIN_LOG_INFO(Editor, "Creative: placed swimmable water (%.2f x %.2f m, %.2f m deep)",
                       static_cast<double>(plan.halfExtents.x * 2.0f),
                       static_cast<double>(plan.halfExtents.z * 2.0f),
                       static_cast<double>(wv.halfExtents.y));
        return entity;
    }

    if (tool == BuildTool::Water) {
        // Water3D builds its surface mesh in WORLD space around
        // settings.position, and the entity transform is applied to that mesh on
        // top of it. So the placement goes in the settings and the transform
        // stays at the origin -- putting it in both moves the water twice as far
        // as the drag, out from under the basin it was dragged into.
        xf.position = Math::Vector3(0.0f, 0.0f, 0.0f);

        auto& water = m_World->AddComponent<ECS::Water3DComponent>(entity);
        water.settings.position = plan.origin;
        water.settings.width = plan.halfExtents.x * 2.0f;
        water.settings.depth = plan.halfExtents.z * 2.0f;
        water.settings.waveHeight = s.waveScale;
        // Tessellation follows the size instead of staying at the 2 m default,
        // which gives a small pond a single quad and no visible waves at all.
        // The lower bound is 0.5 to match the one the LOADER enforces
        // (DeserializeWater3DComponent clamps it) -- authoring 0.25 here would
        // mean a pond that quietly changed shape the first time it was reopened.
        water.settings.tileSize = std::clamp(
            std::min(water.settings.width, water.settings.depth) / 16.0f, 0.5f, 4.0f);
        // Flat water is a still pool, not a broken wave setting, so the style
        // follows the number rather than contradicting it.
        water.settings.style = (s.waveScale > 0.0f) ? Effects::WaterStyle::VertexWave
                                                    : Effects::WaterStyle::Flat;

        SelectEntity(entity);
        RecordLayerCreate(entity);
        FinishCreativePlacement(entity);
        ENJIN_LOG_INFO(Editor, "Creative: placed Water (%.2f x %.2f m at y %.2f)",
                       static_cast<double>(water.settings.width),
                       static_cast<double>(water.settings.depth),
                       static_cast<double>(plan.origin.y));
        return entity;
    }

    if (tool == BuildTool::Plants) {
        // Grass, shrubs or trees, scattered inside the dragged patch.
        //
        // These were only ever reachable from the older Build palette -- View >
        // Build Palette (Creative), which is off by default -- so the engine's
        // whole vegetation capability sat behind a window you had to already
        // know existed. Same components, same density curve as that palette
        // uses, so a patch dragged here and a patch dragged there are the same
        // object; this is the discoverable way in, not a second implementation.
        xf.position = plan.origin;

        const f32 hx = plan.halfExtents.x;
        const f32 hz = plan.halfExtents.z;

        // The density curve and the component choice live in ScenePlacement,
        // which the Build Palette calls too. They used to be duplicated here,
        // which meant the two surfaces agreed only because the same numbers had
        // been typed twice.
        const int kindIndex = static_cast<int>(s.plantKind + 0.5f);
        const PlantKind kind =
            (kindIndex >= 2) ? PlantKind::Trees
          : (kindIndex == 1) ? PlantKind::Shrubs
                             : PlantKind::Grass;
        const char* kindName = PlantKindName(kind);

        AddPlantVolume(m_World, entity, kind);
        SizePlantVolume(m_World, entity, kind, hx, hz, s.plantDensity);

        // Named for what it is rather than for the tool, because "Plants" in the
        // hierarchy tells you nothing about which of the three you made.
        if (auto* nc = m_World->GetComponent<ECS::NameComponent>(entity)) {
            nc->name = kindName;
        }

        SelectEntity(entity);
        RecordLayerCreate(entity);
        FinishCreativePlacement(entity);
        ENJIN_LOG_INFO(Editor, "Creative: placed %s (%.2f x %.2f m)", kindName,
                       static_cast<double>(hx * 2.0f), static_cast<double>(hz * 2.0f));
        return entity;
    }

    if (tool == BuildTool::Prop) {
        // The five ready-made objects the older Build Palette could place and
        // this rail could not: a ball, a point light, a physics box, a barrel
        // and a spawn point.
        //
        // Same meshes, same components, same vertical offsets as that palette
        // uses. The offsets matter and are not decoration: these all land on a
        // ground hit, so a sphere of radius 0.5 has to rise by 0.5 or it is
        // buried to its equator, and a physics box starts at 3 m so it has
        // somewhere to fall from -- dropping it flush with the floor is a box
        // that never visibly falls.
        xf.position = plan.origin;

        // The rail offers five of the six. Block is left out because the Brush
        // tool already makes boxes, and two buttons for one thing on the same
        // rail is worse than one. Mapped explicitly rather than by arithmetic,
        // so adding a kind to either list cannot silently shift the others.
        static constexpr PropKind kRailProps[] = {
            PropKind::Ball, PropKind::Light, PropKind::PhysicsBox,
            PropKind::Barrel, PropKind::SpawnPoint,
        };
        const int kindIndex = static_cast<int>(s.propKind + 0.5f);
        const PropKind kind = kRailProps[
            (kindIndex < 0) ? 0
          : (kindIndex >= static_cast<int>(sizeof(kRailProps) / sizeof(kRailProps[0])))
                ? static_cast<int>(sizeof(kRailProps) / sizeof(kRailProps[0])) - 1
                : kindIndex];
        const char* name = PropKindName(kind);

        // Meshes, components and the vertical drop live in ScenePlacement, which
        // the Build Palette calls too.
        CreateProp(m_World, entity, kind, plan.origin);

        // Named for the thing, not the tool. "Prop" in the hierarchy tells you
        // nothing about which of the five you placed.
        if (auto* nc = m_World->GetComponent<ECS::NameComponent>(entity)) {
            nc->name = name;
        }

        SelectEntity(entity);
        RecordLayerCreate(entity);
        FinishCreativePlacement(entity);
        ENJIN_LOG_INFO(Editor, "Creative: placed %s at (%.2f, %.2f, %.2f)", name,
                       static_cast<double>(xf.position.x),
                       static_cast<double>(xf.position.y),
                       static_cast<double>(xf.position.z));
        return entity;
    }

    // --- Ladder ---
    xf.position = plan.origin;

    auto& ladder = m_World->AddComponent<ECS::LadderComponent>(entity);
    // Colliders in this engine are world space and entity scale does NOT
    // multiply them, so the half extents go on exactly as they were planned.
    ladder.halfExtents = plan.halfExtents;

    // The rails and rungs live in CreativeMode with the rest of the geometry
    // decisions, so what ships is the thing the tests exercise.
    ECS::BrushSolidComponent solid;
    CreativeMode::BuildLadderVisual(plan, solid);

    m_World->AddComponent<ECS::MaterialComponent>(entity);
    m_World->AddComponent<ECS::BrushSolidComponent>(entity, solid);
    ECS::BrushSolidSystem::Rebuild(m_World, entity);

    SelectEntity(entity);
    RecordLayerCreate(entity);
    FinishCreativePlacement(entity);
    ENJIN_LOG_INFO(Editor, "Creative: placed Ladder (%.2f m, %u rungs)",
                   static_cast<double>(plan.halfExtents.y * 2.0f), plan.rungs);
    return entity;
}

// ---------------------------------------------------------------------------
// Terrain: press and hold to sculpt
// ---------------------------------------------------------------------------

void EditorLayer::HandleCreativeTerrain(const Math::Vector3& ground, bool onGround,
                                        f32 localX, f32 localY, f32 viewW, f32 viewH) {
    if (!m_World || !m_Camera) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const BuildToolSettings& s = m_Creative.CurrentSettings();
    const ImU32 tint = m_Creative.IsSubtracting() ? kCut : kAccent;

    // The brush footprint, drawn as a ring at the cursor. Radius is a world
    // number and the cursor is a screen position, so the ring is projected
    // through the same ground-plane maths the stroke uses rather than guessed at
    // a fixed pixel size -- otherwise it says nothing about how much ground a
    // stroke will actually move.
    if (m_EditorViewportHovered && onGround) {
        ImVec2 ring[32];
        u32 ringCount = 0;
        for (u32 i = 0; i < 32; ++i) {
            const f32 a = (static_cast<f32>(i) / 32.0f) * 6.28318531f;
            const Math::Vector3 p(ground.x + std::cos(a) * s.radius, 0.0f,
                                  ground.z + std::sin(a) * s.radius);
            ImVec2 screen;
            if (!ProjectToViewport(m_Camera, p,
                                   ImVec2(m_EditorViewportImageMinX, m_EditorViewportImageMinY),
                                   viewW, viewH, screen)) { ringCount = 0; break; }
            ring[ringCount++] = screen;
        }
        if (ringCount == 32) dl->AddPolyline(ring, 32, tint, ImDrawFlags_Closed, 1.5f);
        else dl->AddCircle(mouse, 24.0f, tint, 32, 1.5f);
    }

    const bool pressed = m_EditorViewportHovered && onGround &&
                         ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    if (pressed && !m_BrushActive) {
        // Pick the terrain to sculpt, or make one. The selected entity wins so a
        // scene with two terrains stays predictable; otherwise the first one in
        // the world; otherwise this is the first stroke in a scene with no
        // terrain at all, which is exactly the moment a person expects the tool
        // to work rather than to be told to go and add a component.
        ECS::Entity target = ECS::INVALID_ENTITY;
        if (m_PrimarySelected != ECS::INVALID_ENTITY &&
            m_World->HasComponent<ECS::TerrainComponent>(m_PrimarySelected)) {
            target = m_PrimarySelected;
        } else {
            for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::TerrainComponent>()) {
                target = e;
                break;
            }
        }

        if (target == ECS::INVALID_ENTITY) {
            ToolPlacement plan;
            if (!CreativeMode::PlanPlacement(BuildTool::Terrain, s, ground, ground, plan)) return;

            target = m_World->CreateEntity();
            m_World->AddComponent<ECS::NameComponent>(target, "Terrain");
            auto& xf = m_World->AddComponent<ECS::TransformComponent>(target);
            xf.position = plan.origin;
            auto& terrain = m_World->AddComponent<ECS::TerrainComponent>(target);
            terrain.gridWidth = kCreativeTerrainGrid;
            terrain.gridHeight = kCreativeTerrainGrid;
            terrain.cellSize = kCreativeTerrainCell;
            terrain.InitializeFlat(0.0f);
            m_World->AddComponent<ECS::MaterialComponent>(target);

            SelectEntity(target);
            RecordLayerCreate(target);
            FinishCreativePlacement(target);
            ENJIN_LOG_INFO(Editor, "Creative: made a %u x %u m terrain to sculpt",
                           kCreativeTerrainGrid, kCreativeTerrainGrid);
        }

        auto* terrain = m_World->GetComponent<ECS::TerrainComponent>(target);
        if (!terrain) return;

        m_CreativeTerrainTarget = target;
        m_BrushActive = true;
        m_TerrainUndoHeightmapSnapshot = terrain->heightmap;
        m_TerrainUndoSplatmapSnapshot = terrain->splatmap;
    }

    if (m_BrushActive && m_CreativeTerrainTarget != ECS::INVALID_ENTITY &&
        ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        auto* terrain = m_World->GetComponent<ECS::TerrainComponent>(m_CreativeTerrainTarget);
        auto* xf = m_World->GetComponent<ECS::TransformComponent>(m_CreativeTerrainTarget);
        if (terrain) {
            // The one sculpt implementation, driven from the creative numbers
            // rather than reimplemented alongside them. Falloff and the flatten
            // height keep whatever the inspector left them at; the two numbers
            // the surface shows are the two the surface owns.
            m_TerrainBrush.radius = s.radius;
            m_TerrainBrush.strength = s.strength;
            m_TerrainBrush.mode = m_Creative.IsSubtracting() ? TerrainBrushMode::Lower
                                                             : TerrainBrushMode::Raise;

            // Sculpt where the cursor is ON THE TERRAIN, not on the flat build
            // plane: once a hill exists the two are metres apart, and a stroke
            // aimed at a peak lands somewhere on its far side. The ground plane
            // is the fallback for a ray that misses the heightmap entirely.
            Math::Vector3 hit = ground;
            const Ray ray = ScenePicker::ScreenToRay(m_Camera, localX, localY, viewW, viewH);
            Math::Vector3 surfaceHit;
            if (RaycastTerrain(ray, terrain, xf, surfaceHit)) hit = surfaceHit;

            ApplyBrush(terrain, xf, hit, ImGui::GetIO().DeltaTime);
        }
    }

    if (m_BrushActive && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        auto* terrain = m_World->GetComponent<ECS::TerrainComponent>(m_CreativeTerrainTarget);
        if (terrain && (m_TerrainUndoHeightmapSnapshot != terrain->heightmap ||
                        m_TerrainUndoSplatmapSnapshot != terrain->splatmap)) {
            m_UndoRedo.Execute(std::make_unique<TerrainSculptCommand>(
                m_World, m_CreativeTerrainTarget,
                std::move(m_TerrainUndoHeightmapSnapshot), terrain->heightmap,
                std::move(m_TerrainUndoSplatmapSnapshot), terrain->splatmap));
            MarkDirty();
        }
        m_TerrainUndoHeightmapSnapshot.clear();
        m_TerrainUndoSplatmapSnapshot.clear();
        m_BrushActive = false;
        m_CreativeTerrainTarget = ECS::INVALID_ENTITY;
    }
}

// ---------------------------------------------------------------------------
// Reduce: click a model, cut its triangles
// ---------------------------------------------------------------------------

void EditorLayer::HandleCreativeReduce(f32 localX, f32 localY, f32 viewW, f32 viewH) {
    if (!m_World || !m_Camera) return;
    if (!m_EditorViewportHovered) return;

    const ECS::Entity hovered =
        ScenePicker::PickEntity(m_World, m_Camera, localX, localY, viewW, viewH);
    if (hovered == ECS::INVALID_ENTITY) return;

    auto* mesh = m_World->GetComponent<ECS::MeshComponent>(hovered);
    if (!mesh || !mesh->IsValid()) return;

    const f32 ratio = std::clamp(m_Creative.CurrentSettings().keepPercent * 0.01f, 0.05f, 0.95f);
    const usize triCount = mesh->indices.size() / 3;

    // Say what the click will cost before it costs it. Reduce is the one tool
    // here that destroys data instead of adding some, and a decimated mesh
    // cannot be reconstructed -- undo holds the whole old copy because there is
    // no way back from the new one.
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    ImFont* font = ImGui::GetFont();
    const char* name = "model";
    if (auto* nc = m_World->GetComponent<ECS::NameComponent>(hovered)) {
        if (!nc->name.empty()) name = nc->name.c_str();
    }
    char label[160];
    std::snprintf(label, sizeof(label), "%s   %zu -> about %zu triangles", name, triCount,
                  static_cast<usize>(static_cast<f32>(triCount) * ratio));
    const f32 ui = CreativeUIScale();
    const f32 size = 11.0f * ui;
    const ImVec2 ls = font->CalcTextSizeA(size, FLT_MAX, 0.0f, label);
    const ImVec2 boxMin(mouse.x + 14.0f * ui, mouse.y + 8.0f * ui);
    dl->AddRectFilled(boxMin, ImVec2(boxMin.x + ls.x + 12.0f * ui, boxMin.y + ls.y + 8.0f * ui),
                      Authored(0x14, 0x18, 0x1f, 0xe0), 3.0f);
    dl->AddText(font, size, ImVec2(boxMin.x + 6.0f * ui, boxMin.y + 4.0f * ui), kAccent, label);

    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;

    std::vector<ECS::Vertex> oldVertices = mesh->vertices;
    std::vector<u32> oldIndices = mesh->indices;
    std::vector<ECS::MeshComponent::SubMesh> oldSubMeshes = mesh->subMeshes;

    ECS::MeshComponent reduced = Renderer::MeshSimplifier::Simplify(*mesh, ratio);
    if (!reduced.IsValid() || reduced.indices.size() >= oldIndices.size()) {
        // Say so rather than appearing to work. A mesh can refuse to decimate
        // any further, when every edge that is left is a boundary.
        ENJIN_LOG_WARN(Editor, "Creative: %s could not be reduced below %zu triangles",
                       name, triCount);
        return;
    }

    m_UndoRedo.Execute(std::make_unique<MeshEditCommand>(
        m_World, hovered, "Simplify Mesh",
        std::move(oldVertices), std::move(oldIndices), std::move(oldSubMeshes), mesh->source,
        reduced.vertices, reduced.indices, reduced.subMeshes, ECS::MeshComponent::SourceRef{}));
    SelectEntity(hovered);
    MarkDirty();
    ENJIN_LOG_INFO(Editor, "Creative: reduced %s from %zu to %zu triangles", name,
                   triCount, reduced.indices.size() / 3);
}

// ---------------------------------------------------------------------------
// Edit: click to select, drag a grip to resize
// ---------------------------------------------------------------------------

void EditorLayer::HandleCreativeEdit(f32 localX, f32 localY, f32 viewW, f32 viewH,
                                     const Math::Vector3& ground, bool onGround) {
    if (!m_World || !m_Camera) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 imgMin(m_EditorViewportImageMinX, m_EditorViewportImageMinY);
    const f32 ui = CreativeUIScale();
    const f32 grab = 7.0f * ui;          // half the handle's on-screen size

    auto* solid = (m_PrimarySelected != ECS::INVALID_ENTITY)
        ? m_World->GetComponent<ECS::BrushSolidComponent>(m_PrimarySelected)
        : nullptr;

    // --- release ends the gesture, and is where the undo entry is pushed ----
    if (m_CreativeGrip >= 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (solid && solid->brushes != m_CreativeGripStart) {
            m_UndoRedo.Execute(std::make_unique<PropertyEditCommand<std::vector<ECS::BrushSolidComponent::Brush>>>(
                "Resize Brush", m_CreativeGripStart, solid->brushes,
                [world = m_World, e = m_PrimarySelected](const std::vector<ECS::BrushSolidComponent::Brush>& v) {
                    if (auto* t = world->GetComponent<ECS::BrushSolidComponent>(e)) {
                        t->brushes = v;
                        t->dirty = true;
                        ECS::BrushSolidSystem::Rebuild(world, e);
                    }
                }));
            MarkDirty();
        }
        m_CreativeGrip = -1;
        m_CreativeGripStart.clear();
    }

    // --- selection ----------------------------------------------------------
    // The one tool where a viewport click selects. Creative mode turns picking
    // off everywhere else because a left click there is a build drag; without
    // this exception there is no way to reach anything you built, which is the
    // state Edit exists to fix.
    if (m_CreativeGrip < 0 && m_EditorViewportHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ECS::Entity hit =
            ScenePicker::PickEntity(m_World, m_Camera, localX, localY, viewW, viewH);
        if (hit != ECS::INVALID_ENTITY && m_World->HasComponent<ECS::BrushSolidComponent>(hit)) {
            if (hit != m_PrimarySelected) {
                SelectEntity(hit);
                solid = m_World->GetComponent<ECS::BrushSolidComponent>(hit);
                // A fresh selection starts on its first brush. Creative mode
                // makes one brush per placement, so for a wall or a floor that
                // is the only one there is.
                if (solid) solid->gizmoBrush = 0;
            }
        }
    }

    if (!solid || solid->brushes.empty()) {
        if (m_EditorViewportHovered) {
            const f32 size = 12.0f * ui;
            const char* msg = "Click a wall, floor or brush to resize it.";
            ImFont* font = ImGui::GetFont();
            const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, msg);
            const f32 cx = (m_EditorViewportImageMinX + m_EditorViewportImageMaxX) * 0.5f;
            const f32 cy = (m_EditorViewportImageMinY + m_EditorViewportImageMaxY) * 0.5f;
            dl->AddText(font, size, ImVec2(cx - ts.x * 0.5f, cy - ts.y * 0.5f), kMuted, msg);
        }
        return;
    }

    // Which brush the grips belong to. Clamped rather than trusted: gizmoBrush
    // is runtime state the inspector also writes, and a stale index outlives the
    // brush it named when a cut removes one.
    i32 bi = solid->gizmoBrush;
    if (bi < 0 || bi >= static_cast<i32>(solid->brushes.size())) bi = 0;
    auto& brush = solid->brushes[static_cast<usize>(bi)];

    // --- draw the grips, and find the one under the cursor ------------------
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    i32 hovered = -1;
    ImVec2 gripScreen[static_cast<usize>(BrushGrip::Count)];
    bool gripVisible[static_cast<usize>(BrushGrip::Count)] = {};

    for (u8 gi = 0; gi < static_cast<u8>(BrushGrip::Count); ++gi) {
        const BrushGrip g = static_cast<BrushGrip>(gi);
        const Math::Vector3 world = BrushGripPosition(brush, g);
        ImVec2 sp;
        if (!ProjectToViewport(m_Camera, world, imgMin, viewW, viewH, sp)) continue;
        gripScreen[gi] = sp;
        gripVisible[gi] = true;
        if (hovered < 0 &&
            std::fabs(mouse.x - sp.x) <= grab && std::fabs(mouse.y - sp.y) <= grab) {
            hovered = static_cast<i32>(gi);
        }
    }

    for (u8 gi = 0; gi < static_cast<u8>(BrushGrip::Count); ++gi) {
        if (!gripVisible[gi]) continue;
        const BrushGrip g = static_cast<BrushGrip>(gi);
        const bool on = (m_CreativeGrip == static_cast<i32>(gi)) || (hovered == static_cast<i32>(gi));
        const ImVec2 sp = gripScreen[gi];
        const f32 r = grab * (on ? 1.0f : 0.8f);
        // Corners are round and edges are square, so which one you have hold of
        // is readable without moving the mouse to find out.
        if (BrushGripIsCorner(g)) {
            dl->AddCircleFilled(sp, r, kGround, 12);
            dl->AddCircle(sp, r, on ? kInk : kAccent, 12, 1.8f);
        } else {
            dl->AddRectFilled(ImVec2(sp.x - r, sp.y - r), ImVec2(sp.x + r, sp.y + r), kGround);
            dl->AddRect(ImVec2(sp.x - r, sp.y - r), ImVec2(sp.x + r, sp.y + r),
                        on ? kInk : kAccent, 0, 0, 1.8f);
        }
    }

    if (hovered >= 0 || m_CreativeGrip >= 0) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    // --- press on a grip starts the drag ------------------------------------
    if (m_CreativeGrip < 0 && hovered >= 0 && m_EditorViewportHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_CreativeGrip = hovered;
        m_CreativeGripStart = solid->brushes;
    }

    // --- drag ---------------------------------------------------------------
    if (m_CreativeGrip >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (onGround &&
            ResizeBrushByGrip(brush, static_cast<BrushGrip>(m_CreativeGrip), ground)) {
            solid->dirty = true;
            // Rebuilt now rather than on the next system pass, so the wall
            // follows the cursor instead of trailing a frame behind it.
            ECS::BrushSolidSystem::Rebuild(m_World, m_PrimarySelected);
        }

        char buf[96];
        if (brush.shape == ECS::BrushSolidComponent::Shape::Prism) {
            std::snprintf(buf, sizeof(buf), "r %.2f m", static_cast<double>(brush.radius));
        } else {
            std::snprintf(buf, sizeof(buf), "%.2f x %.2f m",
                          static_cast<double>(brush.halfExtents.x * 2.0f),
                          static_cast<double>(brush.halfExtents.z * 2.0f));
        }
        dl->AddText(ImGui::GetFont(), 13.0f * ui,
                    ImVec2(mouse.x + 12.0f * ui, mouse.y + 10.0f * ui), kAccent, buf);
    }
}

// ---------------------------------------------------------------------------
// Path: click corners, bow a span, Enter to build
// ---------------------------------------------------------------------------

void EditorLayer::HandleCreativePath(f32 viewW, f32 viewH,
                                     const Math::Vector3& ground, bool onGround) {
    if (!m_World || !m_Camera) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetFont();
    const ImVec2 imgMin(m_EditorViewportImageMinX, m_EditorViewportImageMinY);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const f32 ui = CreativeUIScale();
    const f32 grab = 7.0f * ui;

    const BuildToolSettings& s = m_Creative.CurrentSettings();
    const u32 segs = static_cast<u32>(s.segments + 0.5f);
    const ImU32 tint = m_Creative.IsSubtracting() ? kCut : kAccent;

    auto project = [&](const Math::Vector3& w, ImVec2& out) {
        return ProjectToViewport(m_Camera, w, imgMin, viewW, viewH, out);
    };

    // --- keys ---------------------------------------------------------------
    // Checked before anything else so a finished path cannot also eat the click
    // that finished it.
    if (!m_CreativePathPoints.empty()) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            m_CreativePathPoints.clear();
            m_CreativePathBows.clear();
            m_CreativePathBow = -1;
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
            m_CreativePathPoints.pop_back();
            if (!m_CreativePathBows.empty()) m_CreativePathBows.pop_back();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
            ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            CommitCreativePath();
            return;
        }
    }

    // --- bow handles, and dragging one --------------------------------------
    // Taken before the click that would otherwise add a point, so pulling a span
    // sideways does not also drop a corner under the cursor.
    i32 hoveredBow = -1;
    for (usize i = 0; i + 1 < m_CreativePathPoints.size(); ++i) {
        const Math::Vector3 a = m_CreativePathPoints[i];
        const Math::Vector3 b = m_CreativePathPoints[i + 1];
        const Math::Vector3 d = b - a;
        const f32 len = std::sqrt(d.x * d.x + d.z * d.z);
        if (len < 1e-4f) continue;
        const Math::Vector3 n(-d.z / len, 0.0f, d.x / len);
        const Math::Vector3 mid((a.x + b.x) * 0.5f, a.y, (a.z + b.z) * 0.5f);
        const f32 bow = (i < m_CreativePathBows.size()) ? m_CreativePathBows[i] : 0.0f;
        const Math::Vector3 handle = mid + n * bow;

        ImVec2 hs, ms;
        if (!project(handle, hs) || !project(mid, ms)) continue;

        const bool held = (m_CreativePathBow == static_cast<i32>(i));
        const bool hov = std::fabs(mouse.x - hs.x) <= grab && std::fabs(mouse.y - hs.y) <= grab;
        if (hov && hoveredBow < 0) hoveredBow = static_cast<i32>(i);

        if (std::fabs(bow) > kCreativePathMinBow) {
            dl->AddLine(ms, hs, kCut, 1.0f);
        }
        dl->AddRectFilled(ImVec2(hs.x - grab, hs.y - grab), ImVec2(hs.x + grab, hs.y + grab), kGround);
        dl->AddRect(ImVec2(hs.x - grab, hs.y - grab), ImVec2(hs.x + grab, hs.y + grab),
                    (held || hov) ? kInk : kCut, 0, 0, 1.8f);
    }

    if (m_CreativePathBow < 0 && hoveredBow >= 0 && m_EditorViewportHovered &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_CreativePathBow = hoveredBow;
    }
    if (m_CreativePathBow >= 0) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        const usize i = static_cast<usize>(m_CreativePathBow);
        if (onGround && i + 1 < m_CreativePathPoints.size()) {
            const Math::Vector3 a = m_CreativePathPoints[i];
            const Math::Vector3 b = m_CreativePathPoints[i + 1];
            const Math::Vector3 d = b - a;
            const f32 len = std::sqrt(d.x * d.x + d.z * d.z);
            if (len > 1e-4f) {
                const Math::Vector3 n(-d.z / len, 0.0f, d.x / len);
                const Math::Vector3 mid((a.x + b.x) * 0.5f, a.y, (a.z + b.z) * 0.5f);
                while (m_CreativePathBows.size() <= i) m_CreativePathBows.push_back(0.0f);
                m_CreativePathBows[i] = (ground.x - mid.x) * n.x + (ground.z - mid.z) * n.z;
            }
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) m_CreativePathBow = -1;
    }

    // --- adding corners ------------------------------------------------------
    if (m_CreativePathBow < 0 && hoveredBow < 0 && m_EditorViewportHovered && onGround &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (m_CreativePathPoints.size() < kCreativePathMaxPoints) {
            m_CreativePathPoints.push_back(ground);
            if (m_CreativePathPoints.size() >= 2) m_CreativePathBows.push_back(0.0f);
        } else {
            ENJIN_LOG_WARN(Editor, "Creative: a path stops at %u corners", kCreativePathMaxPoints);
        }
    }

    if (m_CreativePathPoints.empty()) {
        if (m_EditorViewportHovered && onGround) {
            ImVec2 sp;
            if (project(ground, sp)) {
                dl->AddCircle(sp, grab * 0.7f, tint, 12, 1.6f);
            }
        }
        return;
    }

    // --- the line, exactly as it will be built -------------------------------
    // Including the point under the cursor, so the last span previews before it
    // is committed. Drawn from SamplePath, which is also what builds the wall.
    std::vector<Math::Vector3> preview = m_CreativePathPoints;
    std::vector<f32> previewBows = m_CreativePathBows;
    const bool rubberBand = (m_CreativePathBow < 0 && onGround && m_EditorViewportHovered);
    if (rubberBand) {
        preview.push_back(ground);
        previewBows.push_back(0.0f);
    }

    const std::vector<Math::Vector3> line = CreativeMode::SamplePath(preview, previewBows, segs);
    for (usize i = 0; i + 1 < line.size(); ++i) {
        ImVec2 p0, p1;
        if (project(line[i], p0) && project(line[i + 1], p1)) {
            dl->AddLine(p0, p1, tint, 2.0f);
        }
    }

    // Corners, so it is obvious which points are committed and which is the
    // one still following the cursor.
    for (usize i = 0; i < m_CreativePathPoints.size(); ++i) {
        ImVec2 sp;
        if (!project(m_CreativePathPoints[i], sp)) continue;
        dl->AddCircleFilled(sp, grab * 0.6f, kGround, 12);
        dl->AddCircle(sp, grab * 0.6f, tint, 12, 1.8f);
    }

    // --- what it will cost, while there is still time to change it -----------
    const u32 brushes = CreativeMode::CountPathBrushes(m_CreativePathPoints,
                                                       m_CreativePathBows, segs);
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%zu corner%s   %u brush%s   Enter to finish",
                  m_CreativePathPoints.size(),
                  m_CreativePathPoints.size() == 1 ? "" : "s",
                  brushes, brushes == 1 ? "" : "es");
    dl->AddText(font, 13.0f * ui, ImVec2(mouse.x + 12.0f * ui, mouse.y + 10.0f * ui), tint, buf);
}

void EditorLayer::CommitCreativePath() {
    if (!m_World) return;

    const BuildToolSettings& s = m_Creative.CurrentSettings();
    const u32 segs = static_cast<u32>(s.segments + 0.5f);

    ECS::BrushSolidComponent solid;
    const bool built = CreativeMode::BuildPathBrushes(
        m_CreativePathPoints, m_CreativePathBows, segs, s, m_Creative.IsSubtracting(), solid);

    // Cleared either way: a path that could not build is finished with, and
    // leaving it on screen after Enter would look like the key did nothing.
    m_CreativePathPoints.clear();
    m_CreativePathBows.clear();
    m_CreativePathBow = -1;
    if (!built) return;

    if (m_Creative.IsSubtracting()) {
        const ECS::Entity cutInto = FindSolidToCut(m_World, m_PrimarySelected, solid);
        if (cutInto == ECS::INVALID_ENTITY) {
            ENJIN_LOG_WARN(Editor, "Nothing to cut: draw the path across something you have built");
            return;
        }
        auto* target = m_World->GetComponent<ECS::BrushSolidComponent>(cutInto);
        if (!target) return;

        const auto before = target->brushes;
        for (const auto& brush : solid.brushes) target->brushes.push_back(brush);
        target->dirty = true;
        ECS::BrushSolidSystem::Rebuild(m_World, cutInto);

        m_UndoRedo.Execute(std::make_unique<PropertyEditCommand<std::vector<ECS::BrushSolidComponent::Brush>>>(
            "Cut Path", before, target->brushes,
            [world = m_World, e = cutInto](const std::vector<ECS::BrushSolidComponent::Brush>& v) {
                if (auto* t = world->GetComponent<ECS::BrushSolidComponent>(e)) {
                    t->brushes = v;
                    t->dirty = true;
                    ECS::BrushSolidSystem::Rebuild(world, e);
                }
            }));
        SelectEntity(cutInto);
        MarkDirty();
        return;
    }

    ECS::Entity entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(entity, "Path");
    m_World->AddComponent<ECS::TransformComponent>(entity);
    m_World->AddComponent<ECS::MaterialComponent>(entity);
    m_World->AddComponent<ECS::BrushSolidComponent>(entity, solid);
    ECS::BrushSolidSystem::Rebuild(m_World, entity);

    SelectEntity(entity);
    RecordLayerCreate(entity);
    FinishCreativePlacement(entity);
    ENJIN_LOG_INFO(Editor, "Creative: placed Path (%zu brushes)", solid.brushes.size());
}

} // namespace Editor
} // namespace Enjin
