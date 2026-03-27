#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <unordered_map>
#include <chrono>

namespace Enjin {
namespace Editor {

// ── Per-session usage metrics ────────────────────────────────────────

struct TelemetrySession {
    std::string startTime;          // ISO 8601
    f32 durationMinutes = 0.0f;
    u32 scenesLoaded = 0;
    u32 buildsRun = 0;
    u32 crashCount = 0;
    u32 bugReportsSubmitted = 0;
    u32 undoCount = 0;
    u32 redoCount = 0;
    u32 playModeEnters = 0;
    std::unordered_map<std::string, u32> panelsOpened;
    std::unordered_map<std::string, u32> featuresUsed;
};

// ── Aggregate metrics across all sessions ────────────────────────────

struct TelemetryAggregate {
    u32 totalSessions = 0;
    f32 totalMinutes = 0.0f;
    u32 totalScenesLoaded = 0;
    u32 totalBuilds = 0;
    u32 totalCrashes = 0;
    u32 totalBugReports = 0;
    u32 totalPlayModeEnters = 0;
    std::unordered_map<std::string, u32> panelOpenCounts;
    std::unordered_map<std::string, u32> featureUseCounts;
    std::string firstSessionDate;
    std::string lastSessionDate;
};

// ── Telemetry system ─────────────────────────────────────────────────

class ENJIN_API TelemetrySystem {
public:
    // Session lifecycle
    void BeginSession();
    void EndSession();  // Aggregates current into totals, saves

    // Tracking methods — call these from editor code as events occur
    void TrackSceneLoaded();
    void TrackBuildRun();
    void TrackCrash();
    void TrackBugReport();
    void TrackUndo();
    void TrackRedo();
    void TrackPlayModeEnter();
    void TrackPanelOpened(const std::string& panelName);
    void TrackFeatureUsed(const std::string& featureName);

    // Persistence
    void Save(const std::string& path = "");
    void Load(const std::string& path = "");
    static std::string GetDefaultPath();

    // Accessors
    const TelemetrySession& GetCurrentSession() const { return m_Current; }
    const TelemetryAggregate& GetAggregate() const { return m_Aggregate; }

    // Discord webhook upload of aggregate summary
    bool UploadToDiscord(const std::string& webhookUrl);

private:
    TelemetrySession m_Current;
    TelemetryAggregate m_Aggregate;
    std::chrono::steady_clock::time_point m_SessionStart;
    bool m_SessionActive = false;

    // Timestamp helper (ISO 8601 local time)
    static std::string CurrentTimestamp();
};

} // namespace Editor
} // namespace Enjin
