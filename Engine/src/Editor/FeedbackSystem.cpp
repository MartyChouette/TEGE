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
    root["version"] = 2;
    root["nextBugId"] = m_NextBugId;
    root["nextFeedbackId"] = m_NextFeedbackId;

    // GitHub config
    json gh;
    gh["owner"] = m_GitHubConfig.owner;
    gh["repo"] = m_GitHubConfig.repo;
    gh["token"] = m_GitHubConfig.token;
    gh["enabled"] = m_GitHubConfig.enabled;
    root["github"] = gh;

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

    // GitHub config
    if (root.contains("github")) {
        const auto& gh = root["github"];
        if (gh.contains("owner"))   m_GitHubConfig.owner = gh["owner"].get<std::string>();
        if (gh.contains("repo"))    m_GitHubConfig.repo = gh["repo"].get<std::string>();
        if (gh.contains("token"))   m_GitHubConfig.token = gh["token"].get<std::string>();
        if (gh.contains("enabled")) m_GitHubConfig.enabled = gh["enabled"].get<bool>();
    }

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

// ── GitHub Issues API ────────────────────────────────────────────────

std::string FeedbackManager::GitHubApiUrl(const std::string& path) const {
    return "https://api.github.com/repos/" + m_GitHubConfig.owner + "/" + m_GitHubConfig.repo + path;
}

std::unordered_map<std::string, std::string> FeedbackManager::GitHubHeaders() const {
    return {
        {"Accept", "application/vnd.github+json"},
        {"Authorization", "Bearer " + m_GitHubConfig.token},
        {"Content-Type", "application/json"},
        {"User-Agent", "TEGE-Engine"},
        {"X-GitHub-Api-Version", "2022-11-28"}
    };
}

std::string FeedbackManager::FormatBugReportAsMarkdown(const BugReport& report) {
    std::string md;
    md += "**Type:** " + std::string(ReportTypeLabel(report.type)) + "  \n";
    md += "**Severity:** " + std::string(ReportSeverityLabel(report.severity)) + "  \n\n";

    if (!report.description.empty()) {
        md += "## Description\n" + report.description + "\n\n";
    }
    if (!report.stepsToReproduce.empty()) {
        md += "## Steps to Reproduce\n" + report.stepsToReproduce + "\n\n";
    }
    if (!report.expectedBehavior.empty()) {
        md += "## Expected Behavior\n" + report.expectedBehavior + "\n\n";
    }
    if (!report.actualBehavior.empty()) {
        md += "## Actual Behavior\n" + report.actualBehavior + "\n\n";
    }

    // Diagnostics
    const auto& d = report.diagnostics;
    md += "## Diagnostics\n";
    md += "| Metric | Value |\n|---|---|\n";
    if (!d.engineVersion.empty()) md += "| Engine | " + d.engineVersion + " |\n";
    if (!d.platform.empty())      md += "| Platform | " + d.platform + " |\n";
    if (!d.gpuName.empty())       md += "| GPU | " + d.gpuName + " |\n";
    if (d.fps > 0)        md += "| FPS | " + std::to_string(static_cast<i32>(d.fps)) + " |\n";
    if (d.frameTimeMs > 0) md += "| Frame Time | " + std::to_string(d.frameTimeMs).substr(0, 5) + " ms |\n";
    md += "| Draw Calls | " + std::to_string(d.drawCalls) + " |\n";
    md += "| Entities | " + std::to_string(d.entityCount) + " |\n";
    md += "| Triangles | " + std::to_string(d.triangleCount) + " |\n";
    if (d.ramProcess > 0) {
        md += "| RAM (Process) | " + std::to_string(d.ramProcess / (1024 * 1024)) + " MB |\n";
    }
    if (!d.scenePath.empty()) md += "| Scene | " + d.scenePath + " |\n";
    md += "\n";

    // Console log tail (collapsible)
    if (!d.consoleLogTail.empty()) {
        md += "<details><summary>Console Log (last " + std::to_string(d.consoleLogTail.size()) + " lines)</summary>\n\n```\n";
        for (const auto& line : d.consoleLogTail) {
            md += line + "\n";
        }
        md += "```\n</details>\n\n";
    }

    md += "\n---\n*Submitted from TEGE Editor*\n";
    return md;
}

bool FeedbackManager::SubmitBugReportToGitHub(u64 id) {
    if (!IsGitHubConfigured()) {
        ENJIN_LOG_ERROR(Editor, "FeedbackManager: GitHub not configured (missing token)");
        return false;
    }

    BugReport* report = GetBugReport(id);
    if (!report) return false;

    // Build labels based on type and severity
    json labels = json::array();
    labels.push_back("bug");
    labels.push_back(std::string("type:") + ReportTypeLabel(report->type));
    labels.push_back(std::string("severity:") + ReportSeverityLabel(report->severity));

    json body;
    body["title"] = "[Bug] " + report->title;
    body["body"] = FormatBugReportAsMarkdown(*report);
    body["labels"] = labels;

    auto response = Networking::HTTPClient::Post(
        GitHubApiUrl("/issues"), body.dump(), GitHubHeaders());

    if (response.success && response.statusCode >= 200 && response.statusCode < 300) {
        report->status = ReportStatus::Submitted;
        report->updatedAt = CurrentTimestamp();

        // Parse the issue number from the response
        try {
            auto respJson = json::parse(response.body);
            if (respJson.contains("number")) {
                i32 issueNum = respJson["number"].get<i32>();
                ENJIN_LOG_INFO(Editor, "FeedbackManager: bug report #%llu submitted as GitHub issue #%d", id, issueNum);
            }
        } catch (...) {}

        return true;
    }

    ENJIN_LOG_ERROR(Editor, "FeedbackManager: GitHub issue creation failed (%d): %s",
                    response.statusCode, response.error.c_str());
    return false;
}

bool FeedbackManager::SubmitCrashReportToGitHub(const std::string& crashText) {
    if (!IsGitHubConfigured()) {
        ENJIN_LOG_ERROR(Editor, "FeedbackManager: GitHub not configured (missing token)");
        return false;
    }

    // Extract a summary line from the crash text for the title
    std::string title = "[Crash] Engine crash";
    // Try to find the exception line for a better title
    auto pos = crashText.find("Exception:");
    if (pos != std::string::npos) {
        auto end = crashText.find('\n', pos);
        if (end != std::string::npos && end - pos < 120) {
            title = "[Crash] " + crashText.substr(pos, end - pos);
        }
    }

    json labels = json::array();
    labels.push_back("bug");
    labels.push_back("type:Crash");
    labels.push_back("severity:Critical");
    labels.push_back("auto-submitted");

    std::string body = "## Automatic Crash Report\n\n";
    body += "This issue was automatically submitted by the TEGE crash handler.\n\n";
    body += "```\n" + crashText + "\n```\n\n";
    body += "---\n*Auto-submitted from TEGE Editor crash handler*\n";

    json issueBody;
    issueBody["title"] = title;
    issueBody["body"] = body;
    issueBody["labels"] = labels;

    auto response = Networking::HTTPClient::Post(
        GitHubApiUrl("/issues"), issueBody.dump(), GitHubHeaders());

    if (response.success && response.statusCode >= 200 && response.statusCode < 300) {
        ENJIN_LOG_INFO(Editor, "FeedbackManager: crash report submitted to GitHub Issues");
        return true;
    }

    ENJIN_LOG_ERROR(Editor, "FeedbackManager: crash report GitHub submission failed (%d): %s",
                    response.statusCode, response.error.c_str());
    return false;
}

bool FeedbackManager::FetchGitHubIssues(bool includeClosedRecent) {
    if (!IsGitHubConfigured()) {
        ENJIN_LOG_ERROR(Editor, "FeedbackManager: GitHub not configured (missing token)");
        return false;
    }

    // Fetch open issues, sorted by most recently updated
    std::string url = GitHubApiUrl("/issues?state=open&sort=updated&direction=desc&per_page=50");

    auto response = Networking::HTTPClient::Get(url, GitHubHeaders());
    if (!response.success || response.statusCode < 200 || response.statusCode >= 300) {
        ENJIN_LOG_ERROR(Editor, "FeedbackManager: failed to fetch GitHub issues (%d): %s",
                        response.statusCode, response.error.c_str());
        return false;
    }

    m_GitHubIssues.clear();
    try {
        auto issues = json::parse(response.body);
        if (!issues.is_array()) return false;

        for (const auto& j : issues) {
            // Skip pull requests (GitHub API returns them mixed with issues)
            if (j.contains("pull_request")) continue;

            GitHubIssue issue;
            issue.number = j.value("number", 0);
            issue.title = j.value("title", "");
            issue.body = j.value("body", "");
            issue.state = j.value("state", "open");
            issue.createdAt = j.value("created_at", "");
            issue.updatedAt = j.value("updated_at", "");
            issue.htmlUrl = j.value("html_url", "");

            if (j.contains("user") && j["user"].contains("login")) {
                issue.authorLogin = j["user"]["login"].get<std::string>();
            }
            if (j.contains("labels")) {
                for (const auto& label : j["labels"]) {
                    if (label.contains("name"))
                        issue.labels.push_back(label["name"].get<std::string>());
                }
            }
            m_GitHubIssues.push_back(std::move(issue));
        }
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "FeedbackManager: failed to parse GitHub issues: %s", e.what());
        return false;
    }

    // Optionally fetch recently closed issues too
    if (includeClosedRecent) {
        std::string closedUrl = GitHubApiUrl("/issues?state=closed&sort=updated&direction=desc&per_page=20");
        auto closedResp = Networking::HTTPClient::Get(closedUrl, GitHubHeaders());
        if (closedResp.success && closedResp.statusCode >= 200 && closedResp.statusCode < 300) {
            try {
                auto closedIssues = json::parse(closedResp.body);
                if (closedIssues.is_array()) {
                    for (const auto& j : closedIssues) {
                        if (j.contains("pull_request")) continue;
                        GitHubIssue issue;
                        issue.number = j.value("number", 0);
                        issue.title = j.value("title", "");
                        issue.body = j.value("body", "");
                        issue.state = j.value("state", "closed");
                        issue.createdAt = j.value("created_at", "");
                        issue.updatedAt = j.value("updated_at", "");
                        issue.htmlUrl = j.value("html_url", "");
                        if (j.contains("user") && j["user"].contains("login"))
                            issue.authorLogin = j["user"]["login"].get<std::string>();
                        if (j.contains("labels")) {
                            for (const auto& label : j["labels"])
                                if (label.contains("name"))
                                    issue.labels.push_back(label["name"].get<std::string>());
                        }
                        m_GitHubIssues.push_back(std::move(issue));
                    }
                }
            } catch (...) {}
        }
    }

    ENJIN_LOG_INFO(Editor, "FeedbackManager: fetched %zu GitHub issues", m_GitHubIssues.size());
    return true;
}

// ── Discord webhook submission ──────────────────────────────────────

bool FeedbackManager::SubmitBugReportToDiscord(u64 id, const std::string& webhookUrl,
                                                const std::vector<u8>& screenshotPng) {
    if (webhookUrl.empty()) {
        ENJIN_LOG_ERROR(Editor, "FeedbackManager: Discord webhook URL is empty");
        return false;
    }

    BugReport* report = GetBugReport(id);
    if (!report) {
        ENJIN_LOG_ERROR(Editor, "FeedbackManager: bug report #%llu not found", id);
        return false;
    }

    // Build the Discord message content
    std::string content = "**Bug Report**\n";
    content += "**Title:** " + report->title + "\n";
    content += "**Type:** " + std::string(ReportTypeLabel(report->type)) + "\n";
    content += "**Severity:** " + std::string(ReportSeverityLabel(report->severity)) + "\n";

    if (!report->description.empty()) {
        content += "**Description:** " + report->description + "\n";
    }
    if (!report->stepsToReproduce.empty()) {
        content += "**Steps to Reproduce:**\n" + report->stepsToReproduce + "\n";
    }
    if (!report->expectedBehavior.empty()) {
        content += "**Expected:** " + report->expectedBehavior + "\n";
    }
    if (!report->actualBehavior.empty()) {
        content += "**Actual:** " + report->actualBehavior + "\n";
    }

    // System info
    auto& diag = report->diagnostics;
    content += "\n**System Info:**\n";
    if (!diag.gpuName.empty()) content += "GPU: " + diag.gpuName + "\n";
    content += "Platform: " + diag.platform + "\n";
    content += "Engine: " + diag.engineVersion + "\n";
    char fpsLine[128];
    snprintf(fpsLine, sizeof(fpsLine), "FPS: %.1f | Frame: %.2fms | Draw Calls: %u | Tris: %u",
             diag.fps, diag.frameTimeMs, diag.drawCalls, diag.triangleCount);
    content += std::string(fpsLine) + "\n";
    char memLine[128];
    snprintf(memLine, sizeof(memLine), "RAM: %.1f MB | VRAM: %.1f MB",
             diag.ramProcess / (1024.0f * 1024.0f),
             diag.vramAllocated / (1024.0f * 1024.0f));
    content += std::string(memLine) + "\n";
    content += "Entities: " + std::to_string(diag.entityCount) + "\n";
    if (!diag.scenePath.empty()) content += "Scene: " + diag.scenePath + "\n";

    // Console log tail
    if (!diag.consoleLogTail.empty()) {
        content += "\n**Console Log (last " + std::to_string(diag.consoleLogTail.size()) + " lines):**\n```\n";
        for (auto& line : diag.consoleLogTail) {
            content += line + "\n";
        }
        content += "```\n";
    }

    // Discord has a 2000 character limit for content; truncate if needed
    if (content.size() > 1950) {
        content = content.substr(0, 1947) + "...";
    }

    content += "\n*Reported at " + report->diagnostics.timestamp + "*";

    // If we have a screenshot, use multipart upload; otherwise just JSON POST
    Networking::HTTPResponse response;

    if (!screenshotPng.empty()) {
        // Discord multipart: payload_json field + file attachment
        json payload;
        payload["content"] = content;

        std::unordered_map<std::string, std::string> fields;
        fields["payload_json"] = payload.dump();

        std::vector<Networking::HTTPClient::MultipartFile> files;
        Networking::HTTPClient::MultipartFile screenshot;
        screenshot.fieldName = "files[0]";
        screenshot.fileName = "screenshot.png";
        screenshot.contentType = "image/png";
        screenshot.data = screenshotPng;
        files.push_back(std::move(screenshot));

        response = Networking::HTTPClient::PostMultipart(webhookUrl, fields, files);
    } else {
        // Simple JSON POST
        json payload;
        payload["content"] = content;
        response = Networking::HTTPClient::Post(webhookUrl, payload.dump(),
            {{"Content-Type", "application/json"}});
    }

    if (response.success || (response.statusCode >= 200 && response.statusCode < 300)) {
        report->status = ReportStatus::Submitted;
        report->updatedAt = CurrentTimestamp();
        ENJIN_LOG_INFO(Editor, "FeedbackManager: bug report #%llu sent to Discord", id);
        return true;
    }

    ENJIN_LOG_ERROR(Editor, "FeedbackManager: Discord webhook failed (HTTP %d): %s",
                    response.statusCode, response.error.c_str());
    return false;
}

} // namespace Editor
} // namespace Enjin
