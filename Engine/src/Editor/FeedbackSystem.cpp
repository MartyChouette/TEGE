#include "Enjin/Editor/FeedbackSystem.h"
#include "Enjin/Core/Version.h"
#include "Enjin/Networking/HTTPClient.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <filesystem>

namespace Enjin {
namespace Editor {

using json = nlohmann::json;
namespace fs = std::filesystem;

// ── Timestamp helper ─────────────────────────────────────────────────

std::string FeedbackManager::CurrentTimestamp() {
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

bool FeedbackManager::CaseInsensitiveContains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    std::string h = haystack, n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    return h.find(n) != std::string::npos;
}

// ── DiagnosticSnapshot::Capture ──────────────────────────────────────

DiagnosticSnapshot DiagnosticSnapshot::Capture(
    const PerformanceMetrics& perf,
    f32 fps,
    f32 frameTimeMs,
    u32 entityCount,
    const std::string& scenePath,
    const std::vector<std::string>& consoleLog,
    u32 selectedCount,
    const std::string& sceneJson)
{
    DiagnosticSnapshot snap;
    snap.engineVersion = ENJIN_VERSION_STRING;
#ifdef _WIN32
    snap.platform = "Windows";
#elif defined(__linux__)
    snap.platform = "Linux";
#elif defined(__APPLE__)
    snap.platform = "macOS";
#else
    snap.platform = "Unknown";
#endif
    snap.gpuName = ""; // populated by caller if available

    snap.ramTotal = perf.totalPhysicalMemory;
    snap.ramAvailable = perf.availablePhysicalMemory;
    snap.ramProcess = perf.processMemoryBytes;
    snap.vramTotal = perf.gpuTotalBytes;
    snap.vramAllocated = perf.gpuAllocatedBytes;

    snap.fps = fps;
    snap.frameTimeMs = frameTimeMs;
    snap.drawCalls = perf.drawCallCount;
    snap.entityCount = entityCount;
    snap.triangleCount = perf.triangleCount;
    snap.selectedCount = selectedCount;

    snap.scenePath = scenePath;
    snap.sceneJson = sceneJson;

    // Last 50 console log lines
    usize start = consoleLog.size() > 50 ? consoleLog.size() - 50 : 0;
    for (usize i = start; i < consoleLog.size(); ++i) {
        snap.consoleLogTail.push_back(consoleLog[i]);
    }

    snap.timestamp = FeedbackManager::CurrentTimestamp();
    return snap;
}

// ── JSON serialization: DiagnosticSnapshot ───────────────────────────

static json DiagnosticToJson(const DiagnosticSnapshot& d) {
    json j;
    j["engineVersion"] = d.engineVersion;
    j["platform"] = d.platform;
    j["gpuName"] = d.gpuName;
    j["ramTotal"] = d.ramTotal;
    j["ramAvailable"] = d.ramAvailable;
    j["ramProcess"] = d.ramProcess;
    j["vramTotal"] = d.vramTotal;
    j["vramAllocated"] = d.vramAllocated;
    j["fps"] = d.fps;
    j["frameTimeMs"] = d.frameTimeMs;
    j["drawCalls"] = d.drawCalls;
    j["entityCount"] = d.entityCount;
    j["triangleCount"] = d.triangleCount;
    j["selectedCount"] = d.selectedCount;
    j["scenePath"] = d.scenePath;
    if (!d.sceneJson.empty()) j["sceneJson"] = d.sceneJson;
    j["consoleLogTail"] = d.consoleLogTail;
    j["timestamp"] = d.timestamp;
    return j;
}

static DiagnosticSnapshot DiagnosticFromJson(const json& j) {
    DiagnosticSnapshot d;
    if (j.contains("engineVersion")) d.engineVersion = j["engineVersion"].get<std::string>();
    if (j.contains("platform"))      d.platform = j["platform"].get<std::string>();
    if (j.contains("gpuName"))       d.gpuName = j["gpuName"].get<std::string>();
    if (j.contains("ramTotal"))      d.ramTotal = j["ramTotal"].get<usize>();
    if (j.contains("ramAvailable"))  d.ramAvailable = j["ramAvailable"].get<usize>();
    if (j.contains("ramProcess"))    d.ramProcess = j["ramProcess"].get<usize>();
    if (j.contains("vramTotal"))     d.vramTotal = j["vramTotal"].get<usize>();
    if (j.contains("vramAllocated")) d.vramAllocated = j["vramAllocated"].get<usize>();
    if (j.contains("fps"))           d.fps = j["fps"].get<f32>();
    if (j.contains("frameTimeMs"))   d.frameTimeMs = j["frameTimeMs"].get<f32>();
    if (j.contains("drawCalls"))     d.drawCalls = j["drawCalls"].get<u32>();
    if (j.contains("entityCount"))   d.entityCount = j["entityCount"].get<u32>();
    if (j.contains("triangleCount")) d.triangleCount = j["triangleCount"].get<u32>();
    if (j.contains("selectedCount")) d.selectedCount = j["selectedCount"].get<u32>();
    if (j.contains("scenePath"))     d.scenePath = j["scenePath"].get<std::string>();
    if (j.contains("sceneJson"))     d.sceneJson = j["sceneJson"].get<std::string>();
    if (j.contains("consoleLogTail")) {
        for (auto& line : j["consoleLogTail"])
            d.consoleLogTail.push_back(line.get<std::string>());
    }
    if (j.contains("timestamp")) d.timestamp = j["timestamp"].get<std::string>();
    return d;
}

// ── JSON serialization: BugReport ────────────────────────────────────

static json BugReportToJson(const BugReport& r) {
    json j;
    j["id"] = r.id;
    j["type"] = static_cast<i32>(r.type);
    j["severity"] = static_cast<i32>(r.severity);
    j["status"] = static_cast<i32>(r.status);
    j["title"] = r.title;
    j["description"] = r.description;
    j["stepsToReproduce"] = r.stepsToReproduce;
    j["expectedBehavior"] = r.expectedBehavior;
    j["actualBehavior"] = r.actualBehavior;
    j["diagnostics"] = DiagnosticToJson(r.diagnostics);
    j["attachmentPaths"] = r.attachmentPaths;
    j["tags"] = r.tags;
    j["createdAt"] = r.createdAt;
    j["updatedAt"] = r.updatedAt;
    return j;
}

static BugReport BugReportFromJson(const json& j) {
    BugReport r;
    if (j.contains("id"))               r.id = j["id"].get<u64>();
    if (j.contains("type"))             r.type = static_cast<ReportType>(j["type"].get<i32>());
    if (j.contains("severity"))         r.severity = static_cast<ReportSeverity>(j["severity"].get<i32>());
    if (j.contains("status"))           r.status = static_cast<ReportStatus>(j["status"].get<i32>());
    if (j.contains("title"))            r.title = j["title"].get<std::string>();
    if (j.contains("description"))      r.description = j["description"].get<std::string>();
    if (j.contains("stepsToReproduce")) r.stepsToReproduce = j["stepsToReproduce"].get<std::string>();
    if (j.contains("expectedBehavior")) r.expectedBehavior = j["expectedBehavior"].get<std::string>();
    if (j.contains("actualBehavior"))   r.actualBehavior = j["actualBehavior"].get<std::string>();
    if (j.contains("diagnostics"))      r.diagnostics = DiagnosticFromJson(j["diagnostics"]);
    if (j.contains("attachmentPaths")) {
        for (auto& p : j["attachmentPaths"])
            r.attachmentPaths.push_back(p.get<std::string>());
    }
    if (j.contains("tags")) {
        for (auto& t : j["tags"])
            r.tags.push_back(t.get<std::string>());
    }
    if (j.contains("createdAt")) r.createdAt = j["createdAt"].get<std::string>();
    if (j.contains("updatedAt")) r.updatedAt = j["updatedAt"].get<std::string>();
    return r;
}

// ── JSON serialization: FeedbackEntry ────────────────────────────────

static json FeedbackToJson(const FeedbackEntry& f) {
    json j;
    j["id"] = f.id;
    j["type"] = static_cast<i32>(f.type);
    j["priority"] = static_cast<i32>(f.priority);
    j["satisfaction"] = static_cast<i32>(f.satisfaction);
    j["title"] = f.title;
    j["description"] = f.description;
    j["category"] = f.category;
    j["includeDiagnostics"] = f.includeDiagnostics;
    if (f.includeDiagnostics) {
        j["diagnostics"] = DiagnosticToJson(f.diagnostics);
    }
    j["createdAt"] = f.createdAt;
    j["updatedAt"] = f.updatedAt;
    return j;
}

static FeedbackEntry FeedbackFromJson(const json& j) {
    FeedbackEntry f;
    if (j.contains("id"))                 f.id = j["id"].get<u64>();
    if (j.contains("type"))               f.type = static_cast<FeedbackType>(j["type"].get<i32>());
    if (j.contains("priority"))           f.priority = static_cast<FeedbackPriority>(j["priority"].get<i32>());
    if (j.contains("satisfaction"))        f.satisfaction = static_cast<SatisfactionRating>(j["satisfaction"].get<i32>());
    if (j.contains("title"))              f.title = j["title"].get<std::string>();
    if (j.contains("description"))        f.description = j["description"].get<std::string>();
    if (j.contains("category"))           f.category = j["category"].get<std::string>();
    if (j.contains("includeDiagnostics")) f.includeDiagnostics = j["includeDiagnostics"].get<bool>();
    if (j.contains("diagnostics"))        f.diagnostics = DiagnosticFromJson(j["diagnostics"]);
    if (j.contains("createdAt"))          f.createdAt = j["createdAt"].get<std::string>();
    if (j.contains("updatedAt"))          f.updatedAt = j["updatedAt"].get<std::string>();
    return f;
}

// ── Bug report CRUD ──────────────────────────────────────────────────

u64 FeedbackManager::CreateBugReport() {
    BugReport report;
    report.id = m_NextBugId++;
    report.createdAt = CurrentTimestamp();
    report.updatedAt = report.createdAt;
    u64 id = report.id;
    m_BugReports.push_back(std::move(report));
    return id;
}

void FeedbackManager::SaveBugReport(const BugReport& report) {
    for (auto& r : m_BugReports) {
        if (r.id == report.id) {
            r = report;
            r.updatedAt = CurrentTimestamp();
            return;
        }
    }
    // If not found, add it
    m_BugReports.push_back(report);
    m_BugReports.back().updatedAt = CurrentTimestamp();
    if (report.id >= m_NextBugId) m_NextBugId = report.id + 1;
}

void FeedbackManager::DeleteBugReport(u64 id) {
    m_BugReports.erase(
        std::remove_if(m_BugReports.begin(), m_BugReports.end(),
                        [id](const BugReport& r) { return r.id == id; }),
        m_BugReports.end());
}

BugReport* FeedbackManager::GetBugReport(u64 id) {
    for (auto& r : m_BugReports)
        if (r.id == id) return &r;
    return nullptr;
}

// ── Feedback CRUD ────────────────────────────────────────────────────

u64 FeedbackManager::CreateFeedback() {
    FeedbackEntry entry;
    entry.id = m_NextFeedbackId++;
    entry.createdAt = CurrentTimestamp();
    entry.updatedAt = entry.createdAt;
    u64 id = entry.id;
    m_FeedbackEntries.push_back(std::move(entry));
    return id;
}

void FeedbackManager::SaveFeedback(const FeedbackEntry& entry) {
    for (auto& f : m_FeedbackEntries) {
        if (f.id == entry.id) {
            f = entry;
            f.updatedAt = CurrentTimestamp();
            return;
        }
    }
    m_FeedbackEntries.push_back(entry);
    m_FeedbackEntries.back().updatedAt = CurrentTimestamp();
    if (entry.id >= m_NextFeedbackId) m_NextFeedbackId = entry.id + 1;
}

void FeedbackManager::DeleteFeedback(u64 id) {
    m_FeedbackEntries.erase(
        std::remove_if(m_FeedbackEntries.begin(), m_FeedbackEntries.end(),
                        [id](const FeedbackEntry& f) { return f.id == id; }),
        m_FeedbackEntries.end());
}

FeedbackEntry* FeedbackManager::GetFeedback(u64 id) {
    for (auto& f : m_FeedbackEntries)
        if (f.id == id) return &f;
    return nullptr;
}

// ── Persistence ──────────────────────────────────────────────────────

std::string FeedbackManager::GetDefaultDirectory() {
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData) return std::string(appData) + "\\enjin\\feedback\\";
    return "feedback\\";
#else
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.config/enjin/feedback/";
    return "feedback/";
#endif
}

void FeedbackManager::SaveAll(const std::string& dir) {
    std::string directory = dir.empty() ? GetDefaultDirectory() : dir;
    try {
        fs::create_directories(directory);
    } catch (...) {
        ENJIN_LOG_ERROR(Editor, "FeedbackManager: failed to create directory: %s", directory.c_str());
        return;
    }

    json root;
    root["version"] = 1;
    root["nextBugId"] = m_NextBugId;
    root["nextFeedbackId"] = m_NextFeedbackId;

    json bugs = json::array();
    for (const auto& r : m_BugReports) bugs.push_back(BugReportToJson(r));
    root["bugReports"] = bugs;

    json feedback = json::array();
    for (const auto& f : m_FeedbackEntries) feedback.push_back(FeedbackToJson(f));
    root["feedbackEntries"] = feedback;

    std::string filePath = directory + "feedback_data.json";
    std::ofstream file(filePath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Editor, "FeedbackManager: failed to write %s", filePath.c_str());
        return;
    }
    file << root.dump(2);
    file.close();
    ENJIN_LOG_INFO(Editor, "FeedbackManager: saved %zu bugs, %zu feedback to %s",
                   m_BugReports.size(), m_FeedbackEntries.size(), filePath.c_str());
}

void FeedbackManager::LoadAll(const std::string& dir) {
    std::string directory = dir.empty() ? GetDefaultDirectory() : dir;
    std::string filePath = directory + "feedback_data.json";

    std::ifstream file(filePath);
    if (!file.is_open()) return; // No data yet, that's fine

    json root;
    try {
        file >> root;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "FeedbackManager: failed to parse %s: %s", filePath.c_str(), e.what());
        return;
    }
    file.close();

    if (root.contains("nextBugId"))      m_NextBugId = root["nextBugId"].get<u64>();
    if (root.contains("nextFeedbackId")) m_NextFeedbackId = root["nextFeedbackId"].get<u64>();

    m_BugReports.clear();
    if (root.contains("bugReports")) {
        for (const auto& j : root["bugReports"])
            m_BugReports.push_back(BugReportFromJson(j));
    }

    m_FeedbackEntries.clear();
    if (root.contains("feedbackEntries")) {
        for (const auto& j : root["feedbackEntries"])
            m_FeedbackEntries.push_back(FeedbackFromJson(j));
    }

    ENJIN_LOG_INFO(Editor, "FeedbackManager: loaded %zu bugs, %zu feedback from %s",
                   m_BugReports.size(), m_FeedbackEntries.size(), filePath.c_str());
}

// ── Remote submission ────────────────────────────────────────────────

bool FeedbackManager::SubmitBugReport(u64 id, const std::string& endpoint) {
    BugReport* report = GetBugReport(id);
    if (!report) return false;

    json body = BugReportToJson(*report);
    auto response = Networking::HTTPClient::Post(endpoint, body.dump(),
        {{"Content-Type", "application/json"}});

    if (response.success && response.statusCode >= 200 && response.statusCode < 300) {
        report->status = ReportStatus::Submitted;
        report->updatedAt = CurrentTimestamp();
        ENJIN_LOG_INFO(Editor, "FeedbackManager: bug report #%llu submitted successfully", id);
        return true;
    }

    ENJIN_LOG_ERROR(Editor, "FeedbackManager: failed to submit bug report #%llu: %s",
                    id, response.error.c_str());
    return false;
}

bool FeedbackManager::SubmitFeedback(u64 id, const std::string& endpoint) {
    FeedbackEntry* entry = GetFeedback(id);
    if (!entry) return false;

    json body = FeedbackToJson(*entry);
    auto response = Networking::HTTPClient::Post(endpoint, body.dump(),
        {{"Content-Type", "application/json"}});

    if (response.success && response.statusCode >= 200 && response.statusCode < 300) {
        ENJIN_LOG_INFO(Editor, "FeedbackManager: feedback #%llu submitted successfully", id);
        return true;
    }

    ENJIN_LOG_ERROR(Editor, "FeedbackManager: failed to submit feedback #%llu: %s",
                    id, response.error.c_str());
    return false;
}

// ── Filter / Search ──────────────────────────────────────────────────

std::vector<BugReport*> FeedbackManager::FilterBugReports(i32 statusFilter, i32 severityFilter) {
    std::vector<BugReport*> result;
    for (auto& r : m_BugReports) {
        if (statusFilter >= 0 && static_cast<i32>(r.status) != statusFilter) continue;
        if (severityFilter >= 0 && static_cast<i32>(r.severity) != severityFilter) continue;
        result.push_back(&r);
    }
    return result;
}

std::vector<BugReport*> FeedbackManager::SearchBugReports(const std::string& query) {
    std::vector<BugReport*> result;
    if (query.empty()) {
        for (auto& r : m_BugReports) result.push_back(&r);
        return result;
    }
    for (auto& r : m_BugReports) {
        if (CaseInsensitiveContains(r.title, query) ||
            CaseInsensitiveContains(r.description, query))
            result.push_back(&r);
    }
    return result;
}

std::vector<FeedbackEntry*> FeedbackManager::SearchFeedback(const std::string& query) {
    std::vector<FeedbackEntry*> result;
    if (query.empty()) {
        for (auto& f : m_FeedbackEntries) result.push_back(&f);
        return result;
    }
    for (auto& f : m_FeedbackEntries) {
        if (CaseInsensitiveContains(f.title, query) ||
            CaseInsensitiveContains(f.description, query))
            result.push_back(&f);
    }
    return result;
}

// ── Stats ────────────────────────────────────────────────────────────

usize FeedbackManager::GetOpenBugReports() const {
    usize count = 0;
    for (const auto& r : m_BugReports) {
        if (r.status != ReportStatus::Resolved && r.status != ReportStatus::Closed)
            ++count;
    }
    return count;
}

// ── Export ────────────────────────────────────────────────────────────

bool FeedbackManager::ExportBugReportAsJson(u64 id, const std::string& path) {
    BugReport* report = GetBugReport(id);
    if (!report) return false;

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << BugReportToJson(*report).dump(2);
    file.close();
    ENJIN_LOG_INFO(Editor, "FeedbackManager: exported bug report #%llu to %s", id, path.c_str());
    return true;
}

bool FeedbackManager::ExportAllAsJson(const std::string& path) {
    json root;
    root["exportTimestamp"] = CurrentTimestamp();

    json bugs = json::array();
    for (const auto& r : m_BugReports) bugs.push_back(BugReportToJson(r));
    root["bugReports"] = bugs;

    json feedback = json::array();
    for (const auto& f : m_FeedbackEntries) feedback.push_back(FeedbackToJson(f));
    root["feedbackEntries"] = feedback;

    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << root.dump(2);
    file.close();
    ENJIN_LOG_INFO(Editor, "FeedbackManager: exported all data to %s", path.c_str());
    return true;
}

} // namespace Editor
} // namespace Enjin
