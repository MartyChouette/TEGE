#include "Enjin/Accessibility/AlternativeInput.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Accessibility {

void AlternativeInputManager::Update(f32 dt) {
    if (m_SwitchConfig.enabled && m_SwitchConfig.scanningMode) {
        UpdateSwitchScanning(dt);
    }
    if (m_EyeConfig.enabled) {
        UpdateEyeTracking(dt);
    }
}

void AlternativeInputManager::RenderOverlay() {
    if (m_SwitchConfig.enabled && m_SwitchConfig.scanningMode) {
        RenderScanHighlight();
    }
    if (m_EyeConfig.enabled && m_EyeConfig.showGazeIndicator) {
        RenderGazeIndicator();
    }
}

void AlternativeInputManager::RegisterScanTarget(const ScanTarget& target) {
    m_ScanTargets.push_back(target);
}

void AlternativeInputManager::ClearScanTargets() {
    m_ScanTargets.clear();
    m_ScanIndex = -1;
}

void AlternativeInputManager::UpdateGazePosition(f32 x, f32 y) {
    m_GazeX = x;
    m_GazeY = y;
}

void AlternativeInputManager::UpdatePressure(f32 pressure) {
    m_CurrentPressure = pressure;
    // Detect sip (negative) and puff (positive)
    m_SipActive = pressure < -m_SipPuffConfig.sipThreshold;
    m_PuffActive = pressure > m_SipPuffConfig.puffThreshold;
}

void AlternativeInputManager::UpdateHeadRotation(f32 yaw, f32 pitch) {
    m_HeadYaw = yaw;
    m_HeadPitch = pitch;
}

bool AlternativeInputManager::IsAnyAlternativeInputActive() const {
    return m_SwitchConfig.enabled || m_EyeConfig.enabled ||
           m_SipPuffConfig.enabled || m_HeadConfig.enabled;
}

// ============================================================================
// Switch Scanning
// ============================================================================

void AlternativeInputManager::UpdateSwitchScanning(f32 dt) {
    if (m_ScanTargets.empty()) return;

    m_ScanTimer += dt;
    if (m_ScanTimer >= m_SwitchConfig.scanSpeed) {
        m_ScanTimer = 0.0f;

        // Clear previous focus
        if (m_ScanIndex >= 0 && m_ScanIndex < static_cast<i32>(m_ScanTargets.size())) {
            m_ScanTargets[m_ScanIndex].isFocused = false;
        }

        // Advance to next element
        if (m_ScanForward) {
            m_ScanIndex++;
            if (m_ScanIndex >= static_cast<i32>(m_ScanTargets.size())) {
                if (m_SwitchConfig.autoScanReverse) {
                    m_ScanForward = false;
                    m_ScanIndex = static_cast<i32>(m_ScanTargets.size()) - 2;
                } else {
                    m_ScanIndex = 0;
                }
            }
        } else {
            m_ScanIndex--;
            if (m_ScanIndex < 0) {
                m_ScanForward = true;
                m_ScanIndex = 1;
            }
        }

        // Clamp
        m_ScanIndex = std::clamp(m_ScanIndex, 0, static_cast<i32>(m_ScanTargets.size()) - 1);

        // Set new focus
        if (m_ScanIndex >= 0 && m_ScanIndex < static_cast<i32>(m_ScanTargets.size())) {
            m_ScanTargets[m_ScanIndex].isFocused = true;
        }
    }

    // Check for activation (ImGui key check for mapped switch)
    bool selectPressed = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space);
    if (m_SwitchConfig.switchCount >= 2) {
        // Two-switch mode: separate select and next
        bool nextPressed = ImGui::IsKeyPressed(ImGuiKey_Tab);
        if (nextPressed) {
            m_ScanTimer = m_SwitchConfig.scanSpeed; // Force advance on next update
        }
    }

    if (selectPressed && m_ScanIndex >= 0 && m_ScanIndex < static_cast<i32>(m_ScanTargets.size())) {
        auto& target = m_ScanTargets[m_ScanIndex];
        if (target.onActivate) {
            target.onActivate();
        }
    }
}

// ============================================================================
// Eye Tracking
// ============================================================================

void AlternativeInputManager::UpdateEyeTracking(f32 dt) {
    // Smooth gaze position
    f32 alpha = m_EyeConfig.smoothingFactor;
    m_SmoothedGazeX = m_SmoothedGazeX * (1.0f - alpha) + m_GazeX * alpha;
    m_SmoothedGazeY = m_SmoothedGazeY * (1.0f - alpha) + m_GazeY * alpha;

    // Find scan target under gaze
    i32 gazeTarget = -1;
    for (i32 i = 0; i < static_cast<i32>(m_ScanTargets.size()); ++i) {
        const auto& t = m_ScanTargets[i];
        if (m_SmoothedGazeX >= t.x && m_SmoothedGazeX <= t.x + t.w &&
            m_SmoothedGazeY >= t.y && m_SmoothedGazeY <= t.y + t.h) {
            gazeTarget = i;
            break;
        }
    }

    // Dwell detection
    if (gazeTarget >= 0) {
        if (gazeTarget == m_GazeDwellTarget) {
            m_GazeDwellTimer += dt;
            if (m_GazeDwellTimer >= m_EyeConfig.dwellTime) {
                // Activate!
                auto& target = m_ScanTargets[gazeTarget];
                if (target.onActivate) {
                    target.onActivate();
                }
                m_GazeDwellTimer = 0.0f;
            }
        } else {
            m_GazeDwellTarget = gazeTarget;
            m_GazeDwellTimer = 0.0f;
        }
    } else {
        m_GazeDwellTarget = -1;
        m_GazeDwellTimer = 0.0f;
    }
}

// ============================================================================
// Overlay Rendering
// ============================================================================

void AlternativeInputManager::RenderScanHighlight() {
    if (m_ScanIndex < 0 || m_ScanIndex >= static_cast<i32>(m_ScanTargets.size())) return;

    const auto& target = m_ScanTargets[m_ScanIndex];
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Highlight border around focused element
    f32 pad = 3.0f;
    ImU32 borderColor = IM_COL32(80, 180, 255, 220);
    dl->AddRect(
        ImVec2(target.x - pad, target.y - pad),
        ImVec2(target.x + target.w + pad, target.y + target.h + pad),
        borderColor, 4.0f, 0, 3.0f);

    // Label above
    if (!target.label.empty()) {
        ImVec2 textSize = ImGui::CalcTextSize(target.label.c_str());
        f32 labelX = target.x + (target.w - textSize.x) * 0.5f;
        f32 labelY = target.y - textSize.y - pad - 4.0f;
        dl->AddRectFilled(
            ImVec2(labelX - 4.0f, labelY - 2.0f),
            ImVec2(labelX + textSize.x + 4.0f, labelY + textSize.y + 2.0f),
            IM_COL32(20, 20, 30, 200), 3.0f);
        dl->AddText(ImVec2(labelX, labelY), IM_COL32(80, 180, 255, 255), target.label.c_str());
    }
}

void AlternativeInputManager::RenderGazeIndicator() {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    f32 r = m_EyeConfig.gazeIndicatorSize;

    // Outer ring
    dl->AddCircle(ImVec2(m_SmoothedGazeX, m_SmoothedGazeY), r,
                  IM_COL32(100, 200, 255, 120), 24, 2.0f);

    // Inner dot
    dl->AddCircleFilled(ImVec2(m_SmoothedGazeX, m_SmoothedGazeY), 3.0f,
                        IM_COL32(100, 200, 255, 200), 12);

    // Dwell progress ring
    if (m_GazeDwellTarget >= 0 && m_GazeDwellTimer > 0.0f) {
        f32 progress = m_GazeDwellTimer / m_EyeConfig.dwellTime;
        f32 endAngle = -1.5708f + progress * 6.2832f; // -PI/2 to -PI/2 + 2PI
        // Draw arc segments
        i32 segments = static_cast<i32>(progress * 24.0f);
        for (i32 i = 0; i < segments; ++i) {
            f32 a0 = -1.5708f + (static_cast<f32>(i) / 24.0f) * 6.2832f;
            f32 a1 = -1.5708f + (static_cast<f32>(i + 1) / 24.0f) * 6.2832f;
            if (a1 > endAngle) a1 = endAngle;
            dl->AddLine(
                ImVec2(m_SmoothedGazeX + std::cos(a0) * r, m_SmoothedGazeY + std::sin(a0) * r),
                ImVec2(m_SmoothedGazeX + std::cos(a1) * r, m_SmoothedGazeY + std::sin(a1) * r),
                IM_COL32(255, 200, 50, 220), 3.0f);
        }
    }
}

} // namespace Accessibility
} // namespace Enjin
