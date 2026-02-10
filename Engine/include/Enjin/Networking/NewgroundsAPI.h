#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <functional>

namespace Enjin {
namespace Networking {

// Medal (achievement) data
struct NGMedal {
    i32 id = 0;
    std::string name;
    std::string description;
    std::string iconUrl;
    i32 value = 0;
    bool unlocked = false;
};

// Score entry
struct NGScoreEntry {
    std::string user;
    i32 value = 0;
    std::string formattedValue;
    std::string tag;
};

// Scoreboard info
struct NGScoreBoard {
    i32 id = 0;
    std::string name;
};

// Session info
struct NGSession {
    std::string id;
    std::string userName;
    i32 userId = 0;
    bool expired = false;
};

// Cloud save slot
struct NGSaveSlot {
    i32 id = 0;
    std::string data;
    std::string datetime;
    i32 size = 0;
};

// Newgrounds.io API client
class ENJIN_API NewgroundsAPI {
public:
    NewgroundsAPI() = default;

    // Initialize with app credentials
    void Initialize(const std::string& appId, const std::string& encryptionKey);

    // Session management
    void StartSession();
    bool CheckSession();
    bool IsConnected() const { return m_Connected; }
    const NGSession& GetSession() const { return m_Session; }
    std::string GetPassportUrl() const { return m_PassportUrl; }

    // Medals (achievements)
    std::vector<NGMedal> GetMedals();
    bool UnlockMedal(i32 medalId);

    // Scoreboards
    std::vector<NGScoreBoard> GetScoreBoards();
    bool PostScore(i32 boardId, i32 value);
    std::vector<NGScoreEntry> GetScores(i32 boardId, i32 limit = 10,
                                         const std::string& period = "A");

    // Cloud saves
    bool SaveSlot(i32 slotId, const std::string& data);
    std::string LoadSlot(i32 slotId);
    std::vector<NGSaveSlot> GetSaveSlots();

    // App info
    std::string GetAppId() const { return m_AppId; }

private:
    // Call an NG.io component and return the JSON response body
    std::string CallComponent(const std::string& component,
                              const std::string& parameters = "{}");

    // Parse a JSON string for a key's string value (simple parser)
    static std::string ExtractString(const std::string& json, const std::string& key);
    static i32 ExtractInt(const std::string& json, const std::string& key);
    static bool ExtractBool(const std::string& json, const std::string& key);

    std::string m_AppId;
    std::string m_EncryptionKey;
    NGSession m_Session;
    std::string m_PassportUrl;
    bool m_Connected = false;
    bool m_Initialized = false;
};

} // namespace Networking
} // namespace Enjin
