#include "Enjin/Accessibility/SubtitleSystem.h"
#include <imgui.h>
#include <algorithm>
#include <cfloat>

namespace Enjin {
namespace Accessibility {

void SubtitleSystem::ShowSubtitle(const std::string& text, const std::string& speaker,
                                   const Math::Vector3& speakerColor, f32 duration) {
    if (!m_Config.enabled) return;

    SubtitleEntry entry;
    entry.text = text;
    entry.speaker = speaker;
    entry.speakerColor = speakerColor;
    entry.duration = duration;
    entry.timeRemaining = duration;
    entry.isCaption = false;
    m_Entries.push_back(entry);

    // Trim to max visible
    while (m_Entries.size() > m_Config.maxVisibleLines) {
        m_Entries.pop_front();
    }
}

void SubtitleSystem::ShowCaption(const std::string& text, SubtitleDirection direction, f32 duration) {
    if (!m_Config.captionsEnabled) return;

    SubtitleEntry entry;
    entry.text = text;
    entry.duration = duration;
    entry.timeRemaining = duration;
    entry.isCaption = true;
    entry.direction = direction;
    m_Entries.push_back(entry);

    // Trim to max visible
    while (m_Entries.size() > m_Config.maxVisibleLines) {
        m_Entries.pop_front();
    }
}

void SubtitleSystem::Update(f32 dt) {
    for (auto& entry : m_Entries) {
        entry.timeRemaining -= dt;
    }

    // Remove expired entries
    while (!m_Entries.empty() && m_Entries.front().timeRemaining <= 0.0f) {
        m_Entries.pop_front();
    }
}

void SubtitleSystem::RenderOverlay(u32 viewportWidth, u32 viewportHeight) {
    if (m_Entries.empty()) return;
    if (!m_Config.enabled && !m_Config.captionsEnabled) return;
    // T-L8: Guard against zero viewport dimensions
    if (viewportWidth == 0 || viewportHeight == 0) return;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    if (!drawList) return;

    f32 screenW = static_cast<f32>(viewportWidth);
    f32 screenH = static_cast<f32>(viewportHeight);
    f32 scale = m_Config.fontSize / 24.0f;

    // Calculate subtitle area
    f32 maxWidth = screenW * 0.7f;
    f32 padding = 8.0f * scale;
    f32 lineHeight = m_Config.fontSize + 4.0f;
    f32 totalHeight = static_cast<f32>(m_Entries.size()) * lineHeight + padding * 2.0f;

    f32 startY = screenH * m_Config.positionY - totalHeight;
    f32 centerX = screenW * 0.5f;

    // Background
    ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(
        ImVec4(0.0f, 0.0f, 0.0f, m_Config.backgroundOpacity));

    f32 bgLeft = centerX - maxWidth * 0.5f - padding;
    f32 bgRight = centerX + maxWidth * 0.5f + padding;
    f32 bgTop = startY - padding;
    f32 bgBottom = startY + totalHeight;

    drawList->AddRectFilled(
        ImVec2(bgLeft, bgTop),
        ImVec2(bgRight, bgBottom),
        bgColor, 4.0f);

    // Render each entry
    f32 y = startY;
    for (const auto& entry : m_Entries) {
        // Fade out in last 0.5 seconds
        // T-M6: Guard against divide-by-zero if fadeDuration is very small
        f32 alpha = 1.0f;
        if (entry.timeRemaining < 0.5f) {
            alpha = std::max(0.0f, entry.timeRemaining) / 0.5f;
        }

        std::string line;

        // Direction indicator for captions
        if (entry.isCaption && m_Config.showDirectionIndicators && entry.direction != SubtitleDirection::None) {
            switch (entry.direction) {
                case SubtitleDirection::Left:    line += "< "; break;
                case SubtitleDirection::Right:   line += "> "; break;
                case SubtitleDirection::Above:   line += "^ "; break;
                case SubtitleDirection::Below:   line += "v "; break;
                case SubtitleDirection::Behind:  line += "~ "; break;
                default: break;
            }
        }

        // Render at the CONFIGURED size. The layout always scaled with
        // fontSize but the glyphs were drawn with the size-less AddText —
        // the Subtitle Size setting moved the box and never the text.
        ImFont* font = ImGui::GetFont();
        f32 fontSize = m_Config.fontSize;

        // Caption brackets
        if (entry.isCaption) {
            line += "[" + entry.text + "]";
        } else {
            // Speaker name
            if (m_Config.showSpeakerNames && !entry.speaker.empty()) {
                // Draw speaker name in their color
                ImU32 speakerCol = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(entry.speakerColor.x, entry.speakerColor.y, entry.speakerColor.z, alpha));
                std::string speakerText = entry.speaker + ": ";

                ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, speakerText.c_str());
                f32 fullLineW = textSize.x + font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, entry.text.c_str()).x;
                f32 lineX = centerX - fullLineW * 0.5f;

                drawList->AddText(font, fontSize, ImVec2(lineX, y), speakerCol, speakerText.c_str());

                // Then draw dialogue text in white
                ImU32 textCol = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, alpha));
                drawList->AddText(font, fontSize, ImVec2(lineX + textSize.x, y), textCol, entry.text.c_str());

                y += lineHeight;
                continue;
            }
            line += entry.text;
        }

        // Center the text
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, line.c_str());
        f32 textX = centerX - textSize.x * 0.5f;

        ImU32 textCol = ImGui::ColorConvertFloat4ToU32(
            ImVec4(1.0f, 1.0f, 1.0f, alpha));

        // Drop shadow for readability
        ImU32 shadowCol = ImGui::ColorConvertFloat4ToU32(
            ImVec4(0.0f, 0.0f, 0.0f, alpha * 0.8f));
        drawList->AddText(font, fontSize, ImVec2(textX + 1.0f, y + 1.0f), shadowCol, line.c_str());
        drawList->AddText(font, fontSize, ImVec2(textX, y), textCol, line.c_str());

        y += lineHeight;
    }
}

void SubtitleSystem::Clear() {
    m_Entries.clear();
}

} // namespace Accessibility
} // namespace Enjin
