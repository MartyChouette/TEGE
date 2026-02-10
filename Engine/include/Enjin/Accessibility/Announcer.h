#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <deque>

namespace Enjin {
namespace Accessibility {

// Priority levels for announcements
enum class AnnouncePriority : u8 {
    Low = 0,      // Status changes (mode switches, settings)
    Normal,       // Standard actions (entity created, component added)
    High,         // Errors and warnings
    Critical      // Must be announced immediately (data loss, crashes)
};

// A queued accessibility announcement
struct Announcement {
    std::string text;
    AnnouncePriority priority = AnnouncePriority::Normal;
    f32 timeRemaining = 3.0f;
};

// Accessibility announcer for screen reader support groundwork
// Queues text announcements that can be:
// 1. Displayed as a visual status bar in the editor
// 2. Forwarded to OS accessibility APIs (future)
// 3. Logged to the console for assistive tech
class ENJIN_API AccessibilityAnnouncer {
public:
    AccessibilityAnnouncer() = default;
    ~AccessibilityAnnouncer() = default;

    // Announce an action or status change
    void Announce(const std::string& text, AnnouncePriority priority = AnnouncePriority::Normal);

    // Update timers, remove expired
    void Update(f32 dt);

    // Render status bar at bottom of screen (when enabled)
    void RenderStatusBar();

    // Get current announcement text (for screen readers)
    const std::string& GetCurrentText() const;

    // Clear all announcements
    void Clear();

    // Settings
    bool enabled = false;             // Show visual status bar
    bool logToConsole = false;        // Also log announcements to console
    f32 displayDuration = 3.0f;       // How long each announcement shows

private:
    std::deque<Announcement> m_Queue;
    std::string m_EmptyString;
};

} // namespace Accessibility
} // namespace Enjin
