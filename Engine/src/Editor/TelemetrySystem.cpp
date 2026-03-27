#include "Enjin/Editor/TelemetrySystem.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Networking/HTTPClient.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <filesystem>

namespace Enjin {
namespace Editor {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ── Timestamp helper ─────────────────────────────────────────────────

std::string TelemetrySystem::CurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    struct tm buf;
#ifdef _WIN32
    localtime_s(&buf, &t);
#else
    localtime_r(&t, &buf);
#endif
    char out[64];
    std::strftime(out, sizeof(out), "%Y-%m-%dT%H:%M:%S", &buf);
    return std::string(out);
}

// ── Session lifecycle ────────────────────────────────────────────────

void TelemetrySystem::BeginSession() {
    m_Current = TelemetrySession{};
    m_Current.startTime = CurrentTimestamp();
    m_SessionStart = std::chrono::steady_clock::now();
    m_SessionActive = true;

    ENJIN_LOG_INFO(Editor, "TelemetrySystem: session started at %s", m_Current.startTime.c_str());
}

void TelemetrySystem::EndSession() {
    if (!m_SessionActive) return;

    // Compute session duration
    auto elapsed = std::chrono::steady_clock::now() - m_SessionStart;
    m_Current.durationMinutes = std::chrono::duration<f32, std::ratio<60>>(elapsed).count();
    m_SessionActive = false;

    // Merge current session into aggregate
    m_Aggregate.totalSessions++;
    m_Aggregate.totalMinutes += m_Current.durationMinutes;
    m_Aggregate.totalScenesLoaded += m_Current.scenesLoaded;
    m_Aggregate.totalBuilds += m_Current.buildsRun;
    m_Aggregate.totalCrashes += m_Current.crashCount;
    m_Aggregate.totalBugReports += m_Current.bugReportsSubmitted;
    m_Aggregate.totalPlayModeEnters += m_Current.playModeEnters;

    // Merge panel open counts
    for (const auto& [name, count] : m_Current.panelsOpened) {
        m_Aggregate.panelOpenCounts[name] += count;
    }

    // Merge feature use counts
    for (const auto& [name, count] : m_Current.featuresUsed) {
        m_Aggregate.featureUseCounts[name] += count;
    }

    // Update date range
    if (m_Aggregate.firstSessionDate.empty()) {
        m_Aggregate.firstSessionDate = m_Current.startTime;
    }
    m_Aggregate.lastSessionDate = m_Current.startTime;

    Save();

    ENJIN_LOG_INFO(Editor, "TelemetrySystem: session ended (%.1f min, %u total sessions)",
                   m_Current.durationMinutes, m_Aggregate.totalSessions);
}

// ── Tracking methods ─────────────────────────────────────────────────

void TelemetrySystem::TrackSceneLoaded()    { m_Current.scenesLoaded++; }
void TelemetrySystem::TrackBuildRun()       { m_Current.buildsRun++; }
void TelemetrySystem::TrackCrash()          { m_Current.crashCount++; }
void TelemetrySystem::TrackBugReport()      { m_Current.bugReportsSubmitted++; }
void TelemetrySystem::TrackUndo()           { m_Current.undoCount++; }
void TelemetrySystem::TrackRedo()           { m_Current.redoCount++; }
void TelemetrySystem::TrackPlayModeEnter()  { m_Current.playModeEnters++; }

void TelemetrySystem::TrackPanelOpened(const std::string& panelName) {
    m_Current.panelsOpened[panelName]++;
}

void TelemetrySystem::TrackFeatureUsed(const std::string& featureName) {
    m_Current.featuresUsed[featureName]++;
}

// ── JSON serialization helpers ───────────────────────────────────────

static json AggregateToJson(const TelemetryAggregate& a) {
    json j;
    j["totalSessions"] = a.totalSessions;
    j["totalMinutes"] = a.totalMinutes;
    j["totalScenesLoaded"] = a.totalScenesLoaded;
    j["totalBuilds"] = a.totalBuilds;
    j["totalCrashes"] = a.totalCrashes;
    j["totalBugReports"] = a.totalBugReports;
    j["totalPlayModeEnters"] = a.totalPlayModeEnters;
    j["panelOpenCounts"] = a.panelOpenCounts;
    j["featureUseCounts"] = a.featureUseCounts;
    j["firstSessionDate"] = a.firstSessionDate;
    j["lastSessionDate"] = a.lastSessionDate;
    return j;
}

static TelemetryAggregate AggregateFromJson(const json& j) {
    TelemetryAggregate a;
    if (j.contains("totalSessions"))      a.totalSessions = j["totalSessions"].get<u32>();
    if (j.contains("totalMinutes"))       a.totalMinutes = j["totalMinutes"].get<f32>();
    if (j.contains("totalScenesLoaded"))  a.totalScenesLoaded = j["totalScenesLoaded"].get<u32>();
    if (j.contains("totalBuilds"))        a.totalBuilds = j["totalBuilds"].get<u32>();
    if (j.contains("totalCrashes"))       a.totalCrashes = j["totalCrashes"].get<u32>();
    if (j.contains("totalBugReports"))    a.totalBugReports = j["totalBugReports"].get<u32>();
    if (j.contains("totalPlayModeEnters")) a.totalPlayModeEnters = j["totalPlayModeEnters"].get<u32>();
    if (j.contains("panelOpenCounts") && j["panelOpenCounts"].is_object()) {
        for (auto& [key, val] : j["panelOpenCounts"].items()) {
            a.panelOpenCounts[key] = val.get<u32>();
        }
    }
    if (j.contains("featureUseCounts") && j["featureUseCounts"].is_object()) {
        for (auto& [key, val] : j["featureUseCounts"].items()) {
            a.featureUseCounts[key] = val.get<u32>();
        }
    }
    if (j.contains("firstSessionDate"))  a.firstSessionDate = j["firstSessionDate"].get<std::string>();
    if (j.contains("lastSessionDate"))   a.lastSessionDate = j["lastSessionDate"].get<std::string>();
    return a;
}

// ── Persistence ──────────────────────────────────────────────────────

std::string TelemetrySystem::GetDefaultPath() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) return std::string(appData) + "\\enjin\\telemetry.json";
    return "telemetry.json";
#else
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.enjin/telemetry.json";
    return "telemetry.json";
#endif
}

void TelemetrySystem::Save(const std::string& path) {
    std::string filePath = path.empty() ? GetDefaultPath() : path;

    // Ensure parent directory exists
    try {
        fs::path parentDir = fs::path(filePath).parent_path();
        if (!parentDir.empty()) {
            fs::create_directories(parentDir);
        }
    } catch (...) {
        ENJIN_LOG_ERROR(Editor, "TelemetrySystem: failed to create directory for %s", filePath.c_str());
        return;
    }

    json root;
    root["version"] = 1;
    root["aggregate"] = AggregateToJson(m_Aggregate);

    std::ofstream file(filePath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Editor, "TelemetrySystem: failed to write %s", filePath.c_str());
        return;
    }
    file << root.dump(2);
    file.close();

    ENJIN_LOG_INFO(Editor, "TelemetrySystem: saved aggregate (%u sessions) to %s",
                   m_Aggregate.totalSessions, filePath.c_str());
}

void TelemetrySystem::Load(const std::string& path) {
    std::string filePath = path.empty() ? GetDefaultPath() : path;

    std::ifstream file(filePath);
    if (!file.is_open()) return; // No data yet, start fresh

    json root;
    try {
        file >> root;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "TelemetrySystem: failed to parse %s: %s", filePath.c_str(), e.what());
        return;
    }
    file.close();

    if (root.contains("aggregate")) {
        m_Aggregate = AggregateFromJson(root["aggregate"]);
    }

    ENJIN_LOG_INFO(Editor, "TelemetrySystem: loaded aggregate (%u sessions) from %s",
                   m_Aggregate.totalSessions, filePath.c_str());
}

// ── Discord webhook upload ───────────────────────────────────────────

bool TelemetrySystem::UploadToDiscord(const std::string& webhookUrl) {
    if (webhookUrl.empty()) {
        ENJIN_LOG_ERROR(Editor, "TelemetrySystem: Discord webhook URL is empty");
        return false;
    }

    const auto& a = m_Aggregate;

    // Build embed fields
    json fields = json::array();

    fields.push_back({{"name", "Sessions"}, {"value", std::to_string(a.totalSessions)}, {"inline", true}});

    char minutesBuf[64];
    snprintf(minutesBuf, sizeof(minutesBuf), "%.1f min (%.1f hrs)", a.totalMinutes, a.totalMinutes / 60.0f);
    fields.push_back({{"name", "Total Time"}, {"value", std::string(minutesBuf)}, {"inline", true}});

    fields.push_back({{"name", "Scenes Loaded"}, {"value", std::to_string(a.totalScenesLoaded)}, {"inline", true}});
    fields.push_back({{"name", "Builds"}, {"value", std::to_string(a.totalBuilds)}, {"inline", true}});
    fields.push_back({{"name", "Play Mode Enters"}, {"value", std::to_string(a.totalPlayModeEnters)}, {"inline", true}});
    fields.push_back({{"name", "Crashes"}, {"value", std::to_string(a.totalCrashes)}, {"inline", true}});
    fields.push_back({{"name", "Bug Reports"}, {"value", std::to_string(a.totalBugReports)}, {"inline", true}});

    // Top 5 most-used features
    if (!a.featureUseCounts.empty()) {
        // Sort by count descending
        std::vector<std::pair<std::string, u32>> sorted(a.featureUseCounts.begin(), a.featureUseCounts.end());
        std::sort(sorted.begin(), sorted.end(), [](const auto& l, const auto& r) { return l.second > r.second; });

        std::string featureList;
        usize limit = sorted.size() < 5 ? sorted.size() : 5;
        for (usize i = 0; i < limit; ++i) {
            featureList += sorted[i].first + ": " + std::to_string(sorted[i].second) + "\n";
        }
        fields.push_back({{"name", "Top Features"}, {"value", featureList}, {"inline", false}});
    }

    // Top 5 most-opened panels
    if (!a.panelOpenCounts.empty()) {
        std::vector<std::pair<std::string, u32>> sorted(a.panelOpenCounts.begin(), a.panelOpenCounts.end());
        std::sort(sorted.begin(), sorted.end(), [](const auto& l, const auto& r) { return l.second > r.second; });

        std::string panelList;
        usize limit = sorted.size() < 5 ? sorted.size() : 5;
        for (usize i = 0; i < limit; ++i) {
            panelList += sorted[i].first + ": " + std::to_string(sorted[i].second) + "\n";
        }
        fields.push_back({{"name", "Top Panels"}, {"value", panelList}, {"inline", false}});
    }

    // Build the embed
    json embed;
    embed["title"] = "Enjin Telemetry Summary";
    embed["color"] = 0x5865F2; // Discord blurple
    embed["fields"] = fields;

    std::string footer = "Engine " + std::string(ENJIN_VERSION_STRING);
    if (!a.firstSessionDate.empty()) {
        footer += " | " + a.firstSessionDate + " to " + a.lastSessionDate;
    }
    embed["footer"] = {{"text", footer}};

    json payload;
    payload["embeds"] = json::array({embed});

    auto response = Networking::HTTPClient::Post(webhookUrl, payload.dump(),
        {{"Content-Type", "application/json"}});

    if (response.success && response.statusCode >= 200 && response.statusCode < 300) {
        ENJIN_LOG_INFO(Editor, "TelemetrySystem: uploaded aggregate to Discord");
        return true;
    }

    ENJIN_LOG_ERROR(Editor, "TelemetrySystem: Discord upload failed (HTTP %d): %s",
                    response.statusCode, response.error.c_str());
    return false;
}

} // namespace Editor
} // namespace Enjin
