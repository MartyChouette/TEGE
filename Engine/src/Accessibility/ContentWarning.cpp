#include "Enjin/Accessibility/ContentWarning.h"
#include <imgui.h>

namespace Enjin {
namespace Accessibility {

void ContentWarningSystem::SetSceneFlags(const SceneContentFlags& flags) {
    m_CurrentFlags = flags;
    m_Dismissed = false;
    m_Visible = HasWarnings();
}

void ContentWarningSystem::Dismiss() {
    m_Visible = false;
    m_Dismissed = true;
}

bool ContentWarningSystem::HasWarnings() const {
    return static_cast<u32>(m_CurrentFlags.flags) != 0 || !m_CurrentFlags.customWarnings.empty();
}

std::string ContentWarningSystem::GetWarningText(ContentWarningType type) const {
    switch (type) {
        case ContentWarningType::FlashingLights: return "Flashing Lights";
        case ContentWarningType::RapidMotion:    return "Rapid Motion";
        case ContentWarningType::Violence:       return "Violence";
        case ContentWarningType::Heights:        return "Heights / Vertigo";
        case ContentWarningType::LoudSounds:     return "Loud Sounds";
        case ContentWarningType::Spiders:        return "Spiders / Insects";
        case ContentWarningType::Gore:           return "Gore";
        case ContentWarningType::Drowning:       return "Drowning / Underwater";
        default: return "";
    }
}

void ContentWarningSystem::RenderWarningOverlay(u32 viewportWidth, u32 viewportHeight) {
    if (!m_Visible || m_Dismissed) return;

    f32 screenW = static_cast<f32>(viewportWidth);
    f32 screenH = static_cast<f32>(viewportHeight);

    // Semi-transparent background
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    drawList->AddRectFilled(
        ImVec2(0, 0), ImVec2(screenW, screenH),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.85f)));

    f32 centerX = screenW * 0.5f;
    f32 y = screenH * 0.25f;

    // Title
    const char* title = "Content Warning";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    drawList->AddText(ImVec2(centerX - titleSize.x * 0.5f, y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.9f, 0.3f, 1.0f)), title);
    y += 40.0f;

    // Subtitle
    const char* subtitle = "This scene contains the following content:";
    ImVec2 subSize = ImGui::CalcTextSize(subtitle);
    drawList->AddText(ImVec2(centerX - subSize.x * 0.5f, y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.8f, 0.8f, 0.8f, 1.0f)), subtitle);
    y += 30.0f;

    ImU32 textColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    // List built-in warning types
    ContentWarningType allTypes[] = {
        ContentWarningType::FlashingLights, ContentWarningType::RapidMotion,
        ContentWarningType::Violence, ContentWarningType::Heights,
        ContentWarningType::LoudSounds, ContentWarningType::Spiders,
        ContentWarningType::Gore, ContentWarningType::Drowning
    };

    for (auto type : allTypes) {
        if (HasWarning(m_CurrentFlags.flags, type)) {
            std::string text = "- " + GetWarningText(type);
            ImVec2 textSz = ImGui::CalcTextSize(text.c_str());
            drawList->AddText(ImVec2(centerX - textSz.x * 0.5f, y), textColor, text.c_str());
            y += 24.0f;
        }
    }

    // Custom warnings
    for (const auto& custom : m_CurrentFlags.customWarnings) {
        std::string text = "- " + custom;
        ImVec2 textSz = ImGui::CalcTextSize(text.c_str());
        drawList->AddText(ImVec2(centerX - textSz.x * 0.5f, y), textColor, text.c_str());
        y += 24.0f;
    }

    y += 20.0f;

    // Dismiss instruction
    const char* dismiss = "Press any key or click to continue";
    ImVec2 dismissSize = ImGui::CalcTextSize(dismiss);
    drawList->AddText(ImVec2(centerX - dismissSize.x * 0.5f, y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.6f, 0.6f, 1.0f)), dismiss);

    // Auto-dismiss on any input
    ImGuiIO& io = ImGui::GetIO();
    if (io.MouseClicked[0] || io.MouseClicked[1]) {
        Dismiss();
    }
    // Check a broad range of named keys for dismiss
    for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(k))) {
            Dismiss();
            break;
        }
    }
}

} // namespace Accessibility
} // namespace Enjin
