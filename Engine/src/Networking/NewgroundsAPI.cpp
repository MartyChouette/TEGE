#include "Enjin/Networking/NewgroundsAPI.h"
#include "Enjin/Networking/HTTPClient.h"
#include "Enjin/Logging/Log.h"
#include <sstream>

namespace Enjin {
namespace Networking {

static const std::string NG_GATEWAY = "https://newgrounds.io/gateway_v3.php";

// ============================================================================
// Initialization & Session
// ============================================================================

void NewgroundsAPI::Initialize(const std::string& appId, const std::string& encryptionKey) {
    m_AppId = appId;
    m_EncryptionKey = encryptionKey;
    m_Initialized = true;
    m_Connected = false;
    ENJIN_LOG_INFO(Network, "Newgrounds API initialized for app: %s", appId.c_str());
}

void NewgroundsAPI::StartSession() {
    if (!m_Initialized) return;

    std::string result = CallComponent("App.startSession", "{}");
    if (result.empty()) return;

    // Parse session from result
    // Response: {"success":true,"app_id":"...","result":{"data":{"session":{"id":"...","passport_url":"..."}}}}
    m_Session.id = ExtractString(result, "id");
    m_PassportUrl = ExtractString(result, "passport_url");

    if (!m_Session.id.empty()) {
        ENJIN_LOG_INFO(Network, "NG session started: %s", m_Session.id.c_str());
        if (!m_PassportUrl.empty()) {
            ENJIN_LOG_INFO(Network, "NG passport URL: %s", m_PassportUrl.c_str());
        }
    }
}

bool NewgroundsAPI::CheckSession() {
    if (!m_Initialized || m_Session.id.empty()) return false;

    std::string result = CallComponent("App.checkSession", "{}");
    if (result.empty()) return false;

    // Check if user is logged in
    m_Session.userName = ExtractString(result, "name");
    m_Session.userId = ExtractInt(result, "id");
    m_Session.expired = ExtractBool(result, "expired");

    m_Connected = !m_Session.userName.empty() && !m_Session.expired;

    if (m_Connected) {
        ENJIN_LOG_INFO(Network, "NG connected as: %s (id: %d)",
                       m_Session.userName.c_str(), m_Session.userId);
    }

    return m_Connected;
}

// ============================================================================
// Medals
// ============================================================================

std::vector<NGMedal> NewgroundsAPI::GetMedals() {
    std::vector<NGMedal> medals;
    if (!m_Initialized) return medals;

    std::string result = CallComponent("Medal.getList", "{}");
    if (result.empty()) return medals;

    // Parse medals array — simplified parsing for the JSON response
    // Each medal: {"id":N,"name":"...","description":"...","icon":"...","value":N,"unlocked":bool}
    std::string search = result;
    std::string medalStart = "\"id\":";
    size_t pos = 0;

    while ((pos = search.find(medalStart, pos)) != std::string::npos) {
        NGMedal medal;
        // Extract from this position
        std::string chunk = search.substr(pos, 500);  // Grab a reasonable chunk
        medal.id = ExtractInt(chunk, "id");
        medal.name = ExtractString(chunk, "name");
        medal.description = ExtractString(chunk, "description");
        medal.iconUrl = ExtractString(chunk, "icon");
        medal.value = ExtractInt(chunk, "value");
        medal.unlocked = ExtractBool(chunk, "unlocked");

        if (medal.id > 0) {
            medals.push_back(medal);
        }
        pos += medalStart.size();
    }

    return medals;
}

bool NewgroundsAPI::UnlockMedal(i32 medalId) {
    if (!m_Initialized) return false;

    std::string params = "{\"id\":" + std::to_string(medalId) + "}";
    std::string result = CallComponent("Medal.unlock", params);

    bool success = result.find("\"success\":true") != std::string::npos;
    if (success) {
        ENJIN_LOG_INFO(Network, "NG medal unlocked: %d", medalId);
    }
    return success;
}

// ============================================================================
// Scoreboards
// ============================================================================

std::vector<NGScoreBoard> NewgroundsAPI::GetScoreBoards() {
    std::vector<NGScoreBoard> boards;
    if (!m_Initialized) return boards;

    std::string result = CallComponent("ScoreBoard.getBoards", "{}");
    if (result.empty()) return boards;

    std::string search = result;
    size_t pos = 0;
    std::string idStart = "\"id\":";

    while ((pos = search.find(idStart, pos)) != std::string::npos) {
        std::string chunk = search.substr(pos, 300);
        NGScoreBoard board;
        board.id = ExtractInt(chunk, "id");
        board.name = ExtractString(chunk, "name");
        if (board.id > 0) {
            boards.push_back(board);
        }
        pos += idStart.size();
    }

    return boards;
}

bool NewgroundsAPI::PostScore(i32 boardId, i32 value) {
    if (!m_Initialized) return false;

    std::string params = "{\"id\":" + std::to_string(boardId) +
                         ",\"value\":" + std::to_string(value) + "}";
    std::string result = CallComponent("ScoreBoard.postScore", params);

    bool success = result.find("\"success\":true") != std::string::npos;
    if (success) {
        ENJIN_LOG_INFO(Network, "NG score posted: %d to board %d", value, boardId);
    }
    return success;
}

std::vector<NGScoreEntry> NewgroundsAPI::GetScores(i32 boardId, i32 limit,
                                                    const std::string& period) {
    std::vector<NGScoreEntry> scores;
    if (!m_Initialized) return scores;

    // N12: Validate period against known values to prevent JSON injection
    if (period != "D" && period != "W" && period != "M" && period != "Y" && period != "A") {
        ENJIN_LOG_WARN(Network, "Invalid scoreboard period: %s", period.c_str());
        return scores;
    }
    std::string params = "{\"id\":" + std::to_string(boardId) +
                         ",\"limit\":" + std::to_string(limit) +
                         ",\"period\":\"" + period + "\"}";
    std::string result = CallComponent("ScoreBoard.getScores", params);
    if (result.empty()) return scores;

    std::string search = result;
    size_t pos = 0;

    while ((pos = search.find("\"user\":", pos)) != std::string::npos) {
        std::string chunk = search.substr(pos, 500);
        NGScoreEntry entry;
        entry.user = ExtractString(chunk, "name");
        entry.value = ExtractInt(chunk, "value");
        entry.formattedValue = ExtractString(chunk, "formatted_value");
        entry.tag = ExtractString(chunk, "tag");
        if (!entry.user.empty()) {
            scores.push_back(entry);
        }
        pos += 7;
    }

    return scores;
}

// ============================================================================
// Cloud Saves
// ============================================================================

bool NewgroundsAPI::SaveSlot(i32 slotId, const std::string& data) {
    if (!m_Initialized) return false;

    // Escape the data string for JSON
    std::string escaped;
    for (char c : data) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
        }
    }

    std::string params = "{\"id\":" + std::to_string(slotId) +
                         ",\"data\":\"" + escaped + "\"}";
    std::string result = CallComponent("CloudSave.setData", params);
    return result.find("\"success\":true") != std::string::npos;
}

std::string NewgroundsAPI::LoadSlot(i32 slotId) {
    if (!m_Initialized) return "";

    std::string params = "{\"id\":" + std::to_string(slotId) + "}";
    std::string result = CallComponent("CloudSave.loadSlot", params);
    return ExtractString(result, "data");
}

std::vector<NGSaveSlot> NewgroundsAPI::GetSaveSlots() {
    std::vector<NGSaveSlot> slots;
    if (!m_Initialized) return slots;

    std::string result = CallComponent("CloudSave.loadSlots", "{}");
    if (result.empty()) return slots;

    // Parse slots
    size_t pos = 0;
    while ((pos = result.find("\"id\":", pos)) != std::string::npos) {
        std::string chunk = result.substr(pos, 500);
        NGSaveSlot slot;
        slot.id = ExtractInt(chunk, "id");
        slot.datetime = ExtractString(chunk, "datetime");
        slot.size = ExtractInt(chunk, "size");
        if (slot.id > 0) {
            slots.push_back(slot);
        }
        pos += 5;
    }

    return slots;
}

// ============================================================================
// Internal: API Call
// ============================================================================

// S5: JSON string escape helper to prevent injection via untrusted values
static std::string JsonEscapeString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += c;
                }
                break;
        }
    }
    return out;
}

std::string NewgroundsAPI::CallComponent(const std::string& component,
                                          const std::string& parameters) {
    // Build NG.io request JSON — S5: escape interpolated strings
    std::string safeAppId = JsonEscapeString(m_AppId);
    std::string safeComponent = JsonEscapeString(component);

    std::ostringstream json;
    json << "{\"app_id\":\"" << safeAppId << "\"";

    if (!m_Session.id.empty()) {
        std::string safeSessionId = JsonEscapeString(m_Session.id);
        json << ",\"session_id\":\"" << safeSessionId << "\"";
    }

    json << ",\"call\":{\"component\":\"" << safeComponent << "\"";
    if (parameters != "{}") {
        json << ",\"parameters\":" << parameters;
    }
    json << "}}";

    std::string body = json.str();
    auto response = HTTPClient::Post(NG_GATEWAY, body);

    if (!response.success) {
        ENJIN_LOG_ERROR(Network, "NG API call failed: %s - %s",
                        component.c_str(), response.error.c_str());
        return "";
    }

    return response.body;
}

// ============================================================================
// Simple JSON Extraction (no external dependency)
// ============================================================================

std::string NewgroundsAPI::ExtractString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos += search.size();
    auto end = json.find('"', pos);
    if (end == std::string::npos) return "";

    // Handle escaped quotes
    // S-M8: Add iteration limit to prevent infinite loop on malformed input
    constexpr size_t MAX_ITERATIONS = 1000000;
    size_t iterations = 0;
    while (end > 0 && json[end - 1] == '\\') {
        end = json.find('"', end + 1);
        if (end == std::string::npos) return "";
        if (++iterations > MAX_ITERATIONS) {
            ENJIN_LOG_WARN(Network, "ExtractString iteration limit reached for key '%s'", key.c_str());
            return "";
        }
    }

    // S-L4: Basic JSON unescaping for common escape sequences
    std::string raw = json.substr(pos, end - pos);
    std::string result;
    result.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
            switch (raw[i + 1]) {
                case '\\': result += '\\'; ++i; break;
                case '"':  result += '"';  ++i; break;
                case 'n':  result += '\n'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                default:   result += raw[i]; break;
            }
        } else {
            result += raw[i];
        }
    }
    return result;
}

i32 NewgroundsAPI::ExtractInt(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) return 0;

    pos += search.size();
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    std::string numStr;
    while (pos < json.size() && ((json[pos] >= '0' && json[pos] <= '9') || json[pos] == '-')) {
        numStr += json[pos++];
    }
    if (numStr.empty()) return 0;
    // N15: Use strtol instead of stoi to avoid uncaught out_of_range exception
    char* endPtr = nullptr;
    long val = std::strtol(numStr.c_str(), &endPtr, 10);
    if (endPtr == numStr.c_str() || val > INT_MAX || val < INT_MIN) return 0;
    return static_cast<i32>(val);
}

bool NewgroundsAPI::ExtractBool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":";
    auto pos = json.find(search);
    if (pos == std::string::npos) return false;

    pos += search.size();
    while (pos < json.size() && json[pos] == ' ') pos++;

    return json.substr(pos, 4) == "true";
}

} // namespace Networking
} // namespace Enjin
