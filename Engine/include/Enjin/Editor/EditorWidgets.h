#pragma once

// Shared editor widget helpers — the visual-identity building blocks every
// panel uses so the editor reads as ONE designed tool. Start here before
// hand-rolling one-off widget styling in a panel.

#include "Enjin/GUI/ImGuiLayer.h"
#include <imgui.h>

namespace Enjin {
namespace Editor {
namespace UI {

// Right edge of the content region as an X in window-local coordinates: the
// value SameLine() and SetCursorPosX() take. This is what the obsolete
// ImGui::GetContentRegionMax().x and GetWindowContentRegionMax().x returned.
inline float ContentRegionRightX() {
    return ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - ImGui::GetWindowPos().x;
}

// CollapsingHeader in the section typeface (H2). Drop-in replacement: same
// ID, same return, context menus attached to the item keep working. This is
// what gives panels a type hierarchy instead of walls of body text.
inline bool SectionHeader(const char* label, ImGuiTreeNodeFlags flags = 0) {
    ImFont* f = GUI::ImGuiLayer::SectionFont();
    if (f) ImGui::PushFont(f, f->LegacySize);
    bool open = ImGui::CollapsingHeader(label, flags);
    if (f) ImGui::PopFont();
    return open;
}

// Inline section label (the TextDisabled("Events") pattern, upgraded): small
// caps-feel divider text in the section typeface with breathing room.
inline void SectionLabel(const char* label) {
    ImGui::Spacing();
    ImFont* f = GUI::ImGuiLayer::SectionFont();
    if (f) ImGui::PushFont(f, f->LegacySize);
    ImGui::TextDisabled("%s", label);
    if (f) ImGui::PopFont();
}

} // namespace UI
} // namespace Editor
} // namespace Enjin
