#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Editor/PerformanceStats.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Enjin {
namespace Editor {

// ── Report / Feedback enums ──────────────────────────────────────────

enum class ReportType : u8 {
    Bug = 0,
    Crash,
    Performance,
    Visual,
    Audio,
    Other
};

enum class ReportSeverity : u8 {
    Low = 0,
    Medium,
    High,
    Critical
};

enum class ReportStatus : u8 {
    Draft = 0,
    Submitted,
    Acknowledged,
    Resolved,
    Closed
};

enum class FeedbackType : u8 {
    General = 0,
    FeatureRequest,
    Usability,
    Documentation,
    Praise
};

enum class FeedbackPriority : u8 {
    Low = 0,
    Medium,
    High
};

enum class SatisfactionRating : u8 {
    None = 0,
    VeryPoor = 1,
    Poor = 2,
    Average = 3,
    Good = 4,
    Excellent = 5
};

// ── Helper label functions ───────────────────────────────────────────

inline const char* ReportTypeLabel(ReportType t) {
    switch (t) {
        case ReportType::Bug:         return "Bug";
        case ReportType::Crash:       return "Crash";
        case ReportType::Performance: return "Performance";
        case ReportType::Visual:      return "Visual";
        case ReportType::Audio:       return "Audio";
        case ReportType::Other:       return "Other";
    }
    return "Unknown";
}

inline const char* ReportSeverityLabel(ReportSeverity s) {
    switch (s) {
        case ReportSeverity::Low:      return "Low";
        case ReportSeverity::Medium:   return "Medium";
        case ReportSeverity::High:     return "High";
        case ReportSeverity::Critical: return "Critical";
    }
    return "Unknown";
}

inline const char* ReportStatusLabel(ReportStatus s) {
    switch (s) {
        case ReportStatus::Draft:        return "Draft";
        case ReportStatus::Submitted:    return "Submitted";
        case ReportStatus::Acknowledged: return "Acknowledged";
        case ReportStatus::Resolved:     return "Resolved";
        case ReportStatus::Closed:       return "Closed";
    }
    return "Unknown";
}

inline const char* FeedbackTypeLabel(FeedbackType t) {
    switch (t) {
        case FeedbackType::General:        return "General";
        case FeedbackType::FeatureRequest: return "Feature Request";
        case FeedbackType::Usability:      return "Usability";
        case FeedbackType::Documentation:  return "Documentation";
        case FeedbackType::Praise:         return "Praise";
    }
    return "Unknown";
}

inline const char* FeedbackPriorityLabel(FeedbackPriority p) {
    switch (p) {
        case FeedbackPriority::Low:    return "Low";
        case FeedbackPriority::Medium: return "Medium";
        case FeedbackPriority::High:   return "High";
    }
    return "Unknown";
}

// ── Diagnostic snapshot ──────────────────────────────────────────────

struct DiagnosticSnapshot {
    std::string engineVersion;
    std::string platform;
    std::string gpuName;

    usize ramTotal = 0;
    usize ramAvailable = 0;
    usize ramProcess = 0;
    usize vramTotal = 0;
    usize vramAllocated = 0;

    f32 fps = 0.0f;
    f32 frameTimeMs = 0.0f;
    u32 drawCalls = 0;
    u32 entityCount = 0;
    u32 triangleCount = 0;
    u32 selectedCount = 0;

    std::string scenePath;
    std::string sceneJson;
    std::vector<std::string> consoleLogTail; // last 50 lines
    std::string timestamp; // ISO 8601

    static DiagnosticSnapshot Capture(
        const PerformanceMetrics& perf,
        f32 fps,
        f32 frameTimeMs,
        u32 entityCount,
        const std::string& scenePath,
        const std::vector<std::string>& consoleLog,
        u32 selectedCount,
        const std::string& sceneJson = "");
};

// ── Bug report ───────────────────────────────────────────────────────

struct BugReport {
    u64 id = 0;
    ReportType type = ReportType::Bug;
    ReportSeverity severity = ReportSeverity::Medium;
    ReportStatus status = ReportStatus::Draft;

    std::string title;
    std::string description;
    std::string stepsToReproduce;
    std::string expectedBehavior;
    std::string actualBehavior;

    DiagnosticSnapshot diagnostics;
    std::vector<std::string> attachmentPaths;
    std::vector<std::string> tags;

    std::string createdAt;
    std::string updatedAt;
};

// ── Feedback entry ───────────────────────────────────────────────────

struct FeedbackEntry {
    u64 id = 0;
    FeedbackType type = FeedbackType::General;
    FeedbackPriority priority = FeedbackPriority::Medium;
    SatisfactionRating satisfaction = SatisfactionRating::None;

    std::string title;
    std::string description;
    std::string category;

    DiagnosticSnapshot diagnostics;
    bool includeDiagnostics = false;

    std::string createdAt;
    std::string updatedAt;
};

// ── GitHub issue (fetched from remote) ──────────────────────────────

struct GitHubIssue {
    i32 number = 0;
    std::string title;
    std::string body;
    std::string state;       // "open" or "closed"
    std::string createdAt;
    std::string updatedAt;
    std::string htmlUrl;
    std::vector<std::string> labels;
    std::string authorLogin;
};

// ── GitHub settings ─────────────────────────────────────────────────

struct GitHubConfig {
    std::string owner = "MartyChouette";
    std::string repo = "TEGE";
    std::string token;   // Personal access token (stored in config, not in code)
    bool enabled = true;
};

// ── Feedback manager ─────────────────────────────────────────────────

class ENJIN_API FeedbackManager {
public:
    // Bug report CRUD — returns the new report's ID (safe against vector reallocation)
    u64 CreateBugReport();
    void SaveBugReport(const BugReport& report);
    void DeleteBugReport(u64 id);
    std::vector<BugReport>& GetBugReports() { return m_BugReports; }
    const std::vector<BugReport>& GetBugReports() const { return m_BugReports; }
    BugReport* GetBugReport(u64 id);

    // Feedback CRUD — returns the new entry's ID (safe against vector reallocation)
    u64 CreateFeedback();
    void SaveFeedback(const FeedbackEntry& entry);
    void DeleteFeedback(u64 id);
    std::vector<FeedbackEntry>& GetFeedbackEntries() { return m_FeedbackEntries; }
    const std::vector<FeedbackEntry>& GetFeedbackEntries() const { return m_FeedbackEntries; }
    FeedbackEntry* GetFeedback(u64 id);

    // Persistence
    void SaveAll(const std::string& dir = "");
    void LoadAll(const std::string& dir = "");
    static std::string GetDefaultDirectory();

    // Remote submission (generic endpoint)
    bool SubmitBugReport(u64 id, const std::string& endpoint);
    bool SubmitFeedback(u64 id, const std::string& endpoint);

    // GitHub Issues API integration
    GitHubConfig& GetGitHubConfig() { return m_GitHubConfig; }
    const GitHubConfig& GetGitHubConfig() const { return m_GitHubConfig; }
    bool SubmitBugReportToGitHub(u64 id);
    bool SubmitCrashReportToGitHub(const std::string& crashText);
    bool FetchGitHubIssues(bool includeClosedRecent = false);
    const std::vector<GitHubIssue>& GetGitHubIssues() const { return m_GitHubIssues; }
    bool IsGitHubConfigured() const { return !m_GitHubConfig.token.empty() && m_GitHubConfig.enabled; }

    // Filter / Search
    std::vector<BugReport*> FilterBugReports(i32 statusFilter, i32 severityFilter);
    std::vector<BugReport*> SearchBugReports(const std::string& query);
    std::vector<FeedbackEntry*> SearchFeedback(const std::string& query);

    // Stats
    usize GetTotalBugReports() const { return m_BugReports.size(); }
    usize GetOpenBugReports() const;
    usize GetTotalFeedback() const { return m_FeedbackEntries.size(); }

    // Export
    bool ExportBugReportAsJson(u64 id, const std::string& path);
    bool ExportAllAsJson(const std::string& path);

    // Utility
    static std::string CurrentTimestamp();
    static bool CaseInsensitiveContains(const std::string& haystack, const std::string& needle);

private:
    std::vector<BugReport> m_BugReports;
    std::vector<FeedbackEntry> m_FeedbackEntries;
    u64 m_NextBugId = 1;
    u64 m_NextFeedbackId = 1;

    // GitHub integration
    GitHubConfig m_GitHubConfig;
    std::vector<GitHubIssue> m_GitHubIssues;

    // Build a GitHub Issues API URL
    std::string GitHubApiUrl(const std::string& path = "") const;
    // Build auth headers for GitHub API
    std::unordered_map<std::string, std::string> GitHubHeaders() const;
    // Format a bug report as a GitHub issue body (markdown)
    static std::string FormatBugReportAsMarkdown(const BugReport& report);
};

} // namespace Editor
} // namespace Enjin
