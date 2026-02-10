#include "Enjin/Editor/CommandPalette.h"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace Enjin {
namespace Editor {

void CommandPalette::RegisterCommand(const PaletteCommand& cmd) {
    m_Commands.push_back(cmd);
}

void CommandPalette::ClearCommands() {
    m_Commands.clear();
    m_Filtered.clear();
}

void CommandPalette::Open() {
    m_Open = true;
    m_JustOpened = true;
    m_SearchBuf[0] = '\0';
    m_SelectedIndex = 0;
    UpdateFilter();
}

void CommandPalette::Close() {
    m_Open = false;
    m_SearchBuf[0] = '\0';
    m_SelectedIndex = 0;
}

bool CommandPalette::Render() {
    if (!m_Open) return false;

    bool executed = false;

    // Center the palette at top of screen
    ImGuiIO& io = ImGui::GetIO();
    f32 paletteW = std::min(600.0f, io.DisplaySize.x * 0.6f);
    f32 paletteX = (io.DisplaySize.x - paletteW) * 0.5f;
    f32 paletteY = 60.0f;
    ImGui::SetNextWindowPos(ImVec2(paletteX, paletteY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(paletteW, 0.0f), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.5f, 0.8f, 0.5f));

    if (ImGui::Begin("##CommandPalette", nullptr, flags)) {
        // Search input
        ImGui::PushItemWidth(-1);
        if (m_JustOpened) {
            ImGui::SetKeyboardFocusHere();
            m_JustOpened = false;
        }
        bool searchChanged = ImGui::InputText("##PaletteSearch", m_SearchBuf, sizeof(m_SearchBuf),
                                               ImGuiInputTextFlags_AutoSelectAll);
        ImGui::PopItemWidth();

        if (searchChanged) {
            UpdateFilter();
            m_SelectedIndex = 0;
        }

        // Keyboard navigation
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            m_SelectedIndex = std::min(m_SelectedIndex + 1, static_cast<i32>(m_Filtered.size()) - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            m_SelectedIndex = std::max(m_SelectedIndex - 1, 0);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            Close();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) && !m_Filtered.empty() &&
            m_SelectedIndex >= 0 && m_SelectedIndex < static_cast<i32>(m_Filtered.size())) {
            const auto& cmd = m_Commands[m_Filtered[m_SelectedIndex]];
            if (cmd.action) {
                cmd.action();
                m_LastExecuted = cmd.name;
                executed = true;
            }
            Close();
        }

        // Results list
        ImGui::Separator();
        f32 maxListH = std::min(400.0f, io.DisplaySize.y - paletteY - 100.0f);
        ImGui::BeginChild("##PaletteResults", ImVec2(0, maxListH), false);

        if (m_Filtered.empty()) {
            ImGui::TextDisabled("No matching commands");
        }

        for (i32 i = 0; i < static_cast<i32>(m_Filtered.size()); ++i) {
            const auto& cmd = m_Commands[m_Filtered[i]];
            bool isSelected = (i == m_SelectedIndex);

            ImGui::PushID(i);

            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.35f, 0.55f, 0.7f));
            }

            if (ImGui::Selectable("##cmd", isSelected, ImGuiSelectableFlags_SpanAllColumns)) {
                if (cmd.action) {
                    cmd.action();
                    m_LastExecuted = cmd.name;
                    executed = true;
                }
                Close();
            }

            if (isSelected) {
                ImGui::PopStyleColor();
                // Auto-scroll to selected
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                    ImGui::SetScrollHereY(0.5f);
                }
            }

            // Draw command info on same line
            ImGui::SameLine(12.0f);

            // Category tag
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.7f, 0.9f, 0.8f));
            ImGui::Text("[%s]", cmd.category.c_str());
            ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::Text("%s", cmd.name.c_str());

            // Shortcut on the right
            if (!cmd.shortcut.empty()) {
                f32 shortcutW = ImGui::CalcTextSize(cmd.shortcut.c_str()).x;
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - shortcutW + ImGui::GetCursorPosX());
                ImGui::TextDisabled("%s", cmd.shortcut.c_str());
            }

            ImGui::PopID();
        }

        ImGui::EndChild();

        // Status bar
        ImGui::Separator();
        ImGui::TextDisabled("%zu / %zu commands", m_Filtered.size(), m_Commands.size());
        ImGui::SameLine();
        ImGui::TextDisabled(" | Up/Down to navigate, Enter to execute, Esc to close");
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    // Close if clicked outside
    if (m_Open && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
        Close();
    }

    return executed;
}

void CommandPalette::UpdateFilter() {
    m_Filtered.clear();
    std::string query = m_SearchBuf;

    if (query.empty()) {
        // Show all commands
        for (usize i = 0; i < m_Commands.size(); ++i) {
            m_Filtered.push_back(i);
        }
        return;
    }

    // Convert query to lowercase
    for (auto& c : query) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // Score and sort
    struct ScoredResult {
        usize index;
        i32 score;
    };
    std::vector<ScoredResult> scored;

    for (usize i = 0; i < m_Commands.size(); ++i) {
        i32 nameScore = FuzzyScore(m_Commands[i].name, query);
        i32 catScore = FuzzyScore(m_Commands[i].category, query);
        i32 descScore = FuzzyScore(m_Commands[i].description, query);
        i32 bestScore = std::max({nameScore, catScore, descScore});

        if (bestScore > 0) {
            // Boost name matches
            if (nameScore == bestScore) bestScore += 10;
            scored.push_back({ i, bestScore });
        }
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
        [](const ScoredResult& a, const ScoredResult& b) { return a.score > b.score; });

    for (const auto& s : scored) {
        m_Filtered.push_back(s.index);
    }
}

bool CommandPalette::FuzzyMatch(const std::string& text, const std::string& query) const {
    return FuzzyScore(text, query) > 0;
}

i32 CommandPalette::FuzzyScore(const std::string& text, const std::string& query) const {
    if (query.empty()) return 1;
    if (text.empty()) return 0;

    // Lowercase comparison
    std::string lowerText = text;
    for (auto& c : lowerText) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::string lowerQuery = query;
    for (auto& c : lowerQuery) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // Exact substring match (highest score)
    auto pos = lowerText.find(lowerQuery);
    if (pos != std::string::npos) {
        i32 score = 100;
        if (pos == 0) score += 50; // Prefix match bonus
        return score;
    }

    // Fuzzy character-by-character match
    usize qi = 0;
    i32 score = 0;
    bool prevMatched = false;

    for (usize ti = 0; ti < lowerText.size() && qi < lowerQuery.size(); ++ti) {
        if (lowerText[ti] == lowerQuery[qi]) {
            score += prevMatched ? 5 : 3; // Consecutive matches worth more
            // Word boundary bonus
            if (ti == 0 || lowerText[ti - 1] == ' ' || lowerText[ti - 1] == '_' ||
                (std::islower(static_cast<unsigned char>(text[ti - 1])) &&
                 std::isupper(static_cast<unsigned char>(text[ti])))) {
                score += 10;
            }
            prevMatched = true;
            ++qi;
        } else {
            prevMatched = false;
        }
    }

    // All query chars matched?
    return (qi == lowerQuery.size()) ? score : 0;
}

} // namespace Editor
} // namespace Enjin
