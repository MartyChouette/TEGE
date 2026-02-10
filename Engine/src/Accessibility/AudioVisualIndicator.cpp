#include "Enjin/Accessibility/AudioVisualIndicator.h"

#include <imgui.h>
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Accessibility {

void AudioVisualIndicatorSystem::ShowIndicator(const std::string& label, const Math::Vector3& color, f32 duration) {
    AudioIndicator ind;
    ind.label = label;
    ind.color = color;
    ind.lifetime = duration;
    ind.timeRemaining = duration;
    ind.isPulse = false;
    ind.intensity = 1.0f;

    // Replace existing indicator with same label
    for (auto& existing : m_Indicators) {
        if (existing.label == label && !existing.isPulse) {
            existing = ind;
            return;
        }
    }

    m_Indicators.push_back(ind);

    // Limit to 10 indicators
    while (m_Indicators.size() > 10) {
        m_Indicators.pop_front();
    }
}

void AudioVisualIndicatorSystem::ShowPulse(const std::string& label, const Math::Vector3& color) {
    // Check if already exists
    for (auto& ind : m_Indicators) {
        if (ind.label == label && ind.isPulse) {
            ind.color = color;
            return;
        }
    }

    AudioIndicator ind;
    ind.label = label;
    ind.color = color;
    ind.lifetime = 0.0f;
    ind.timeRemaining = 0.0f;
    ind.isPulse = true;
    ind.intensity = 1.0f;
    m_Indicators.push_back(ind);
}

void AudioVisualIndicatorSystem::StopPulse(const std::string& label) {
    m_Indicators.erase(
        std::remove_if(m_Indicators.begin(), m_Indicators.end(),
            [&](const AudioIndicator& ind) { return ind.isPulse && ind.label == label; }),
        m_Indicators.end());
}

void AudioVisualIndicatorSystem::Update(f32 dt) {
    if (!m_Config.enabled) return;

    m_PulsePhase += dt * 3.0f;
    if (m_PulsePhase > 6.2832f) m_PulsePhase -= 6.2832f;

    // Update non-pulse indicators
    for (auto& ind : m_Indicators) {
        if (!ind.isPulse) {
            ind.timeRemaining -= dt;
            // Fade out in last 0.5 seconds
            if (ind.timeRemaining < 0.5f) {
                ind.intensity = std::max(0.0f, ind.timeRemaining / 0.5f);
            }
        } else {
            // Pulsing indicators oscillate
            ind.intensity = 0.5f + 0.5f * std::sin(m_PulsePhase);
        }
    }

    // Remove expired
    m_Indicators.erase(
        std::remove_if(m_Indicators.begin(), m_Indicators.end(),
            [](const AudioIndicator& ind) { return !ind.isPulse && ind.timeRemaining <= 0.0f; }),
        m_Indicators.end());
}

void AudioVisualIndicatorSystem::RenderOverlay(u32 viewportWidth, u32 viewportHeight) {
    if (!m_Config.enabled || m_Indicators.empty()) return;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    f32 x = m_Config.positionX * static_cast<f32>(viewportWidth);
    f32 y = m_Config.positionY * static_cast<f32>(viewportHeight);
    f32 r = m_Config.indicatorSize;
    f32 spacing = r * 3.0f;

    for (usize i = 0; i < m_Indicators.size(); ++i) {
        const auto& ind = m_Indicators[i];
        f32 cy = y + static_cast<f32>(i) * spacing;

        u8 alpha = static_cast<u8>(ind.intensity * 255.0f);
        ImU32 col = IM_COL32(
            static_cast<u8>(ind.color.x * 255.0f),
            static_cast<u8>(ind.color.y * 255.0f),
            static_cast<u8>(ind.color.z * 255.0f),
            alpha);

        // Draw filled circle
        drawList->AddCircleFilled(ImVec2(x, cy), r * ind.intensity, col, 16);

        // Draw ring for pulse indicators
        if (ind.isPulse) {
            f32 ringR = r * (1.0f + 0.3f * ind.intensity);
            drawList->AddCircle(ImVec2(x, cy), ringR, col, 16, 2.0f);
        }

        // Draw label
        if (m_Config.showLabels && !ind.label.empty()) {
            ImU32 textCol = IM_COL32(255, 255, 255, alpha);
            ImVec2 textSize = ImGui::CalcTextSize(ind.label.c_str());
            drawList->AddText(
                ImVec2(x - textSize.x - r - 4.0f, cy - textSize.y * 0.5f),
                textCol, ind.label.c_str());
        }
    }
}

void AudioVisualIndicatorSystem::Clear() {
    m_Indicators.clear();
}

} // namespace Accessibility
} // namespace Enjin
