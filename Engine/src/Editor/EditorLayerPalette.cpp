// The Scene Palette editor.
//
// Palette cycling is only worth having if a person can author it, so this is the
// authoring surface: pick a preset, see the table, define the runs that rotate,
// and watch it animate before committing. Without this the feature would be
// reachable only from code, which is the bar the golden rule sets.
//
// The preview is drawn with ImDrawList rather than an uploaded texture, the same
// choice the Cookie Creator makes: no GPU image lifetime to manage for something
// that exists only while a window is open.

#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/EditorWidgets.h"
#include "Enjin/Renderer/PaletteCycle.h"
#include "Enjin/ECS/Systems/RenderSystem.h"

#include <imgui.h>

#include <cmath>
#include <cstddef>

namespace Enjin {
namespace Editor {

namespace {

// The swapchain is B8G8R8A8_SRGB and ImGui writes vertex colours straight
// through, so an authored colour has to be converted to linear or it renders
// about three times lighter than it is. Same conversion the creative surface uses.
ImU32 Authored(const Renderer::PaletteColor& c) {
    auto lin = [](u8 v) {
        const f32 s = static_cast<f32>(v) / 255.0f;
        const f32 l = (s <= 0.04045f) ? (s / 12.92f)
                                      : std::pow((s + 0.055f) / 1.055f, 2.4f);
        return static_cast<u8>(l * 255.0f + 0.5f);
    };
    return IM_COL32(lin(c.r), lin(c.g), lin(c.b), 255);
}

// One row of swatches. Returns the index under the cursor, or -1.
i32 DrawPaletteStrip(const Renderer::Palette& p, f32 width, f32 height, i32 markFirst, i32 markCount) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const u32 n = p.count > 0 ? p.count : 1;
    const f32 w = width / static_cast<f32>(n);

    i32 hovered = -1;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    for (u32 i = 0; i < n; ++i) {
        const ImVec2 a(origin.x + static_cast<f32>(i) * w, origin.y);
        // +1 on the far edge: without it fractional widths leave hairline gaps
        // and the strip reads as a grid rather than a continuous table.
        const ImVec2 b(a.x + w + 1.0f, a.y + height);
        dl->AddRectFilled(a, b, Authored(p.colors[i]));
        if (mouse.x >= a.x && mouse.x < a.x + w && mouse.y >= a.y && mouse.y < b.y) {
            hovered = static_cast<i32>(i);
        }
    }

    // Bracket the run being edited, so "which entries move" is visible rather
    // than something you hold in your head while typing numbers.
    if (markCount > 1 && markFirst >= 0 && static_cast<u32>(markFirst) < n) {
        const f32 x0 = origin.x + static_cast<f32>(markFirst) * w;
        const f32 x1 = x0 + static_cast<f32>(markCount) * w;
        dl->AddRect(ImVec2(x0, origin.y - 2.0f), ImVec2(x1, origin.y + height + 2.0f),
                    IM_COL32(120, 190, 255, 255), 0.0f, 0, 2.0f);
    }
    dl->AddRect(origin, ImVec2(origin.x + width, origin.y + height),
                IM_COL32(90, 95, 105, 255));
    ImGui::Dummy(ImVec2(width, height));
    return hovered;
}

} // namespace

void EditorLayer::DrawSettingsSection_ScenePalette() {
    if (!UI::SectionHeader("Scene Palette (indexed colour + cycling)")) return;
    if (!m_RenderSystem) { ImGui::TextDisabled("No render system."); return; }

    const f32 s = ImGui::GetIO().FontGlobalScale;

    ImGui::TextWrapped("A palette-indexed material stores an INDEX in its base colour texture's "
                       "red channel instead of a colour, and this table supplies the colours. "
                       "Rotating a run of the table animates every surface using it, for the cost "
                       "of rewriting a few entries -- no matter how much of the screen is moving.");
    ImGui::Spacing();

    Renderer::Palette palette = m_RenderSystem->GetScenePalette();
    std::vector<Renderer::PaletteCycleRange> cycles = m_RenderSystem->GetPaletteCycles();
    bool changed = false;

    // --- Presets, which are the fastest way to see the effect at all.
    static int s_Preset = 0;
    const char* presetNames[] = { "Water", "Fire", "Rain", "Aurora" };
    ImGui::PushItemWidth(160.0f * s);
    ImGui::Combo("Preset", &s_Preset, presetNames, IM_ARRAYSIZE(presetNames));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Load preset")) {
        Renderer::MakePalettePreset(static_cast<Renderer::PalettePreset>(s_Preset), palette, cycles);
        changed = true;
    }
    if (palette.count > 0) {
        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            palette = Renderer::Palette{};
            cycles.clear();
            changed = true;
        }
    }

    if (palette.count == 0) {
        ImGui::TextDisabled("No palette. Load a preset to start.");
        if (changed) m_RenderSystem->SetScenePalette(palette, cycles);
        return;
    }

    // --- The table itself.
    ImGui::Spacing();
    static int s_Selected = 0;
    if (s_Selected >= static_cast<int>(palette.count)) s_Selected = 0;

    const int markFirst = cycles.empty() ? -1 : static_cast<int>(cycles[0].first);
    const int markCount = cycles.empty() ? 0 : static_cast<int>(cycles[0].count);
    const i32 hovered = DrawPaletteStrip(palette, 420.0f * s, 34.0f * s, markFirst, markCount);
    if (hovered >= 0) {
        ImGui::SetTooltip("Index %d", hovered);
        if (ImGui::IsMouseClicked(0)) s_Selected = hovered;
    }
    ImGui::TextDisabled("%u colours. The blue bracket is the first cycling run.", palette.count);

    // --- Edit one entry. A palette is authored colour by colour.
    ImGui::PushItemWidth(220.0f * s);
    int sel = s_Selected;
    if (ImGui::SliderInt("Entry", &sel, 0, static_cast<int>(palette.count) - 1)) s_Selected = sel;
    Renderer::PaletteColor& pc = palette.colors[static_cast<u32>(s_Selected)];
    f32 col[3] = { pc.r / 255.0f, pc.g / 255.0f, pc.b / 255.0f };
    if (ImGui::ColorEdit3("Colour", col)) {
        pc.r = static_cast<u8>(col[0] * 255.0f + 0.5f);
        pc.g = static_cast<u8>(col[1] * 255.0f + 0.5f);
        pc.b = static_cast<u8>(col[2] * 255.0f + 0.5f);
        changed = true;
    }
    ImGui::PopItemWidth();

    // --- The runs. This is the authored unit: a slice of the table and a speed.
    ImGui::Spacing();
    ImGui::SeparatorText("Cycling runs");
    ImGui::TextDisabled("Entries inside a run rotate. Everything else stays put.");

    for (usize i = 0; i < cycles.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        Renderer::PaletteCycleRange& r = cycles[i];

        bool en = r.enabled;
        if (ImGui::Checkbox("##en", &en)) { r.enabled = en; changed = true; }
        ImGui::SameLine();

        ImGui::PushItemWidth(90.0f * s);
        int first = static_cast<int>(r.first);
        int count = static_cast<int>(r.count);
        if (ImGui::DragInt("first", &first, 0.2f, 0, static_cast<int>(palette.count) - 1)) {
            r.first = static_cast<u32>(first < 0 ? 0 : first); changed = true;
        }
        ImGui::SameLine();
        if (ImGui::DragInt("count", &count, 0.2f, 0, static_cast<int>(palette.count))) {
            r.count = static_cast<u32>(count < 0 ? 0 : count); changed = true;
        }
        ImGui::SameLine();
        if (ImGui::DragFloat("speed", &r.speed, 0.1f, -60.0f, 60.0f, "%.1f/s")) changed = true;
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::SmallButton("x")) {
            cycles.erase(cycles.begin() + static_cast<std::ptrdiff_t>(i));
            changed = true;
            ImGui::PopID();
            break;
        }
        // Clamped as authored, so a run can never index past the table.
        Renderer::ClampCycleRange(r, palette.count);
        ImGui::PopID();
    }

    if (ImGui::Button("Add run")) {
        Renderer::PaletteCycleRange r;
        r.first = 1;
        r.count = palette.count > 1 ? palette.count - 1 : 1;
        r.speed = 6.0f;
        Renderer::ClampCycleRange(r, palette.count);
        cycles.push_back(r);
        changed = true;
    }

    // --- Live preview: the authored table beside what it looks like right now.
    ImGui::Spacing();
    ImGui::SeparatorText("Now");
    Renderer::Palette animated;
    Renderer::ApplyPaletteCycles(palette, cycles, static_cast<f32>(ImGui::GetTime()), animated);
    DrawPaletteStrip(animated, 420.0f * s, 22.0f * s, -1, 0);
    ImGui::TextDisabled("Cycling as it will render. Assign a material's Palette Indexed flag to use it.");

    if (changed) m_RenderSystem->SetScenePalette(palette, cycles);
}

} // namespace Editor
} // namespace Enjin
