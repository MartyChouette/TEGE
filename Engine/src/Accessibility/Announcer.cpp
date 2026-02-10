#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>
#include <algorithm>

namespace Enjin {
namespace Accessibility {

void AccessibilityAnnouncer::Announce(const std::string& text, AnnouncePriority priority) {
    Announcement ann;
    ann.text = text;
    ann.priority = priority;
    ann.timeRemaining = displayDuration;

    // High/Critical announcements go to front of queue
    if (priority >= AnnouncePriority::High) {
        m_Queue.push_front(ann);
    } else {
        m_Queue.push_back(ann);
    }

    // Limit queue size
    while (m_Queue.size() > 5) {
        m_Queue.pop_back();
    }

    if (logToConsole) {
        const char* priorityStr = "";
        switch (priority) {
            case AnnouncePriority::Low: priorityStr = "INFO"; break;
            case AnnouncePriority::Normal: priorityStr = "ACTION"; break;
            case AnnouncePriority::High: priorityStr = "WARNING"; break;
            case AnnouncePriority::Critical: priorityStr = "CRITICAL"; break;
        }
        ENJIN_LOG_INFO(Editor, "[Announce/%s] %s", priorityStr, text.c_str());
    }
}

void AccessibilityAnnouncer::Update(f32 dt) {
    for (auto& ann : m_Queue) {
        ann.timeRemaining -= dt;
    }

    m_Queue.erase(
        std::remove_if(m_Queue.begin(), m_Queue.end(),
            [](const Announcement& a) { return a.timeRemaining <= 0.0f; }),
        m_Queue.end());
}

void AccessibilityAnnouncer::RenderStatusBar() {
    if (!enabled || m_Queue.empty()) return;

    const auto& current = m_Queue.front();

    // Draw at bottom of screen
    ImGuiIO& io = ImGui::GetIO();
    f32 barHeight = ImGui::GetTextLineHeightWithSpacing() + 8.0f;
    ImVec2 barPos(0.0f, io.DisplaySize.y - barHeight);
    ImVec2 barEnd(io.DisplaySize.x, io.DisplaySize.y);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Background
    f32 alpha = std::min(1.0f, current.timeRemaining / 0.5f);
    ImU32 bgCol = IM_COL32(20, 20, 30, static_cast<u8>(200 * alpha));
    drawList->AddRectFilled(barPos, barEnd, bgCol);

    // Priority color indicator
    ImU32 indicatorCol;
    switch (current.priority) {
        case AnnouncePriority::Low:      indicatorCol = IM_COL32(100, 180, 255, static_cast<u8>(255 * alpha)); break;
        case AnnouncePriority::Normal:   indicatorCol = IM_COL32(100, 220, 100, static_cast<u8>(255 * alpha)); break;
        case AnnouncePriority::High:     indicatorCol = IM_COL32(255, 180, 50, static_cast<u8>(255 * alpha)); break;
        case AnnouncePriority::Critical: indicatorCol = IM_COL32(255, 80, 80, static_cast<u8>(255 * alpha)); break;
    }
    drawList->AddRectFilled(barPos, ImVec2(barPos.x + 4.0f, barEnd.y), indicatorCol);

    // Text
    ImU32 textCol = IM_COL32(220, 220, 220, static_cast<u8>(255 * alpha));
    drawList->AddText(ImVec2(barPos.x + 12.0f, barPos.y + 4.0f), textCol, current.text.c_str());
}

const std::string& AccessibilityAnnouncer::GetCurrentText() const {
    if (m_Queue.empty()) return m_EmptyString;
    return m_Queue.front().text;
}

void AccessibilityAnnouncer::Clear() {
    m_Queue.clear();
}

} // namespace Accessibility
} // namespace Enjin
