#include "Enjin/Scene/GameSaveSystem.h"

#include <fstream>
#include <filesystem>
#include <system_error>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <ShlObj.h>
#pragma comment(lib, "shell32.lib")
#elif defined(__APPLE__)
#include <pwd.h>
#include <unistd.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace Enjin::Scene
{
    static std::string GetCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};

#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    std::string GameSaveSystem::GetSaveDirectory()
    {
        std::string basePath;

#ifdef _WIN32
        char* appData = nullptr;
        size_t len = 0;
        if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData != nullptr)
        {
            basePath = std::string(appData);
            free(appData);
        }
        else
        {
            basePath = "C:/Users/Default/AppData/Roaming";
        }
        basePath += "/enjin/saves/";
#elif defined(__APPLE__)
        const char* home = getenv("HOME");
        if (!home)
        {
            struct passwd* pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        basePath = std::string(home) + "/Library/Application Support/enjin/saves/";
#else
        // Save files are user data, so they belong under XDG_DATA_HOME
        // (~/.local/share), which is what Platform::GetAppUserDataDirectory
        // already returns for everything else the engine writes. They used to go
        // to XDG_CONFIG_HOME (~/.config), which is for configuration and is what
        // a "reset my settings" tool or a dotfile manager will happily delete.
        //
        // Existing saves are not abandoned: if the old location has a directory
        // and the new one does not, we keep reading and writing the old one. A
        // player who already has saves keeps them; a new install gets the
        // correct path.
        const char* home = getenv("HOME");
        if (!home)
        {
            struct passwd* pw = getpwuid(getuid());
            home = pw ? pw->pw_dir : "/tmp";
        }
        const char* dataDir = getenv("XDG_DATA_HOME");
        basePath = dataDir ? (std::string(dataDir) + "/enjin/saves/")
                           : (std::string(home) + "/.local/share/enjin/saves/");

        const char* configDir = getenv("XDG_CONFIG_HOME");
        const std::string legacy = configDir
            ? (std::string(configDir) + "/enjin/saves/")
            : (std::string(home) + "/.config/enjin/saves/");
        std::error_code ec;
        if (!std::filesystem::exists(basePath, ec) && std::filesystem::exists(legacy, ec))
        {
            basePath = legacy;
        }
#endif

        return basePath;
    }

    std::string GameSaveSystem::GetSlotPath(u32 slot) const
    {
        return GetSaveDirectory() + "save_" + std::to_string(slot) + ".json";
    }

    nlohmann::json GameSaveSystem::DataToJson(const GameSaveData& data) const
    {
        nlohmann::json j;

        j["scenePath"] = data.scenePath;
        j["playerPosition"] = {
            {"x", data.playerPosition.x},
            {"y", data.playerPosition.y},
            {"z", data.playerPosition.z}
        };
        j["playerHealth"] = data.playerHealth;
        j["playTime"] = data.playTime;
        j["saveTimestamp"] = data.saveTimestamp;

        nlohmann::json floatArr = nlohmann::json::array();
        for (const auto& [name, value] : data.floatVars)
        {
            floatArr.push_back({{"name", name}, {"value", value}});
        }
        j["floatVars"] = floatArr;

        nlohmann::json intArr = nlohmann::json::array();
        for (const auto& [name, value] : data.intVars)
        {
            intArr.push_back({{"name", name}, {"value", value}});
        }
        j["intVars"] = intArr;

        nlohmann::json boolArr = nlohmann::json::array();
        for (const auto& [name, value] : data.boolVars)
        {
            boolArr.push_back({{"name", name}, {"value", value}});
        }
        j["boolVars"] = boolArr;

        nlohmann::json stringArr = nlohmann::json::array();
        for (const auto& [name, value] : data.stringVars)
        {
            stringArr.push_back({{"name", name}, {"value", value}});
        }
        j["stringVars"] = stringArr;

        return j;
    }

    GameSaveData GameSaveSystem::JsonToData(const nlohmann::json& j) const
    {
        GameSaveData data;

        if (j.contains("scenePath"))
            data.scenePath = j["scenePath"].get<std::string>();

        if (j.contains("playerPosition"))
        {
            const auto& pos = j["playerPosition"];
            data.playerPosition.x = pos.value("x", 0.0f);
            data.playerPosition.y = pos.value("y", 0.0f);
            data.playerPosition.z = pos.value("z", 0.0f);
        }

        if (j.contains("playerHealth") && j["playerHealth"].is_number())
            data.playerHealth = j["playerHealth"].get<f32>();

        if (j.contains("playTime") && j["playTime"].is_number())
            data.playTime = j["playTime"].get<f32>();

        if (j.contains("saveTimestamp") && j["saveTimestamp"].is_string())
            data.saveTimestamp = j["saveTimestamp"].get<std::string>();

        // N13: Validate entry fields and types before access to prevent exceptions on malformed saves
        if (j.contains("floatVars") && j["floatVars"].is_array())
        {
            for (const auto& entry : j["floatVars"])
            {
                if (!entry.contains("name") || !entry["name"].is_string()) continue;
                if (!entry.contains("value") || !entry["value"].is_number()) continue;
                data.floatVars.emplace_back(
                    entry["name"].get<std::string>(),
                    entry["value"].get<f32>()
                );
            }
        }

        if (j.contains("intVars") && j["intVars"].is_array())
        {
            for (const auto& entry : j["intVars"])
            {
                if (!entry.contains("name") || !entry["name"].is_string()) continue;
                if (!entry.contains("value") || !entry["value"].is_number_integer()) continue;
                data.intVars.emplace_back(
                    entry["name"].get<std::string>(),
                    entry["value"].get<i32>()
                );
            }
        }

        if (j.contains("boolVars") && j["boolVars"].is_array())
        {
            for (const auto& entry : j["boolVars"])
            {
                if (!entry.contains("name") || !entry["name"].is_string()) continue;
                if (!entry.contains("value") || !entry["value"].is_boolean()) continue;
                data.boolVars.emplace_back(
                    entry["name"].get<std::string>(),
                    entry["value"].get<bool>()
                );
            }
        }

        if (j.contains("stringVars") && j["stringVars"].is_array())
        {
            for (const auto& entry : j["stringVars"])
            {
                if (!entry.contains("name") || !entry["name"].is_string()) continue;
                if (!entry.contains("value") || !entry["value"].is_string()) continue;
                data.stringVars.emplace_back(
                    entry["name"].get<std::string>(),
                    entry["value"].get<std::string>()
                );
            }
        }

        return data;
    }

    bool GameSaveSystem::SaveToSlot(u32 slot, const GameSaveData& data)
    {
        if (slot >= MAX_SLOTS)
            return false;

        std::string dirPath = GetSaveDirectory();
        std::filesystem::path dir(dirPath);

        if (!std::filesystem::exists(dir))
        {
            std::error_code ec;
            if (!std::filesystem::create_directories(dir, ec))
                return false;
        }

        GameSaveData saveData = data;
        saveData.saveTimestamp = GetCurrentTimestamp();

        nlohmann::json j = DataToJson(saveData);

        // Atomic file save: write to temp file, then rename
        std::string filePath = GetSlotPath(slot);
        std::string tmpPath = filePath + ".tmp";
        {
            std::ofstream file(tmpPath);
            if (!file.is_open())
                return false;

            file << j.dump(4);
            file.close();

            if (!file.good()) {
                std::error_code ec;
                std::filesystem::remove(tmpPath, ec);
                return false;
            }
        }

        {
            std::error_code ec;
            std::filesystem::rename(tmpPath, filePath, ec);
            if (ec) {
                std::filesystem::remove(tmpPath, ec);
                return false;
            }
        }

        return true;
    }

    bool GameSaveSystem::LoadFromSlot(u32 slot, GameSaveData& outData)
    {
        if (slot >= MAX_SLOTS)
            return false;

        std::string filePath = GetSlotPath(slot);

        if (!std::filesystem::exists(filePath))
            return false;

        std::ifstream file(filePath);
        if (!file.is_open())
            return false;

        nlohmann::json j;
        try
        {
            file >> j;
        }
        catch (const nlohmann::json::parse_error&)
        {
            return false;
        }

        outData = JsonToData(j);
        return true;
    }

    bool GameSaveSystem::DeleteSlot(u32 slot)
    {
        if (slot >= MAX_SLOTS)
            return false;

        std::string filePath = GetSlotPath(slot);

        if (!std::filesystem::exists(filePath))
            return false;

        std::error_code ec;
        return std::filesystem::remove(filePath, ec);
    }

    SaveSlotInfo GameSaveSystem::GetSlotInfo(u32 slot) const
    {
        SaveSlotInfo info;
        info.slotIndex = slot;

        if (slot >= MAX_SLOTS)
            return info;

        std::string filePath = GetSlotPath(slot);

        if (!std::filesystem::exists(filePath))
            return info;

        std::ifstream file(filePath);
        if (!file.is_open())
            return info;

        nlohmann::json j;
        try
        {
            file >> j;
        }
        catch (const nlohmann::json::parse_error&)
        {
            return info;
        }

        info.occupied = true;

        if (j.contains("saveTimestamp") && j["saveTimestamp"].is_string())
            info.timestamp = j["saveTimestamp"].get<std::string>();

        if (j.contains("scenePath") && j["scenePath"].is_string())
            info.scenePath = j["scenePath"].get<std::string>();

        if (j.contains("playTime") && j["playTime"].is_number())
            info.playTime = j["playTime"].get<f32>();

        info.displayName = "Slot " + std::to_string(slot);
        if (!info.scenePath.empty())
        {
            std::filesystem::path scenePath(info.scenePath);
            info.displayName += " - " + scenePath.stem().string();
        }

        return info;
    }

    std::vector<SaveSlotInfo> GameSaveSystem::GetAllSlots() const
    {
        std::vector<SaveSlotInfo> slots;
        slots.reserve(MAX_SLOTS);

        for (u32 i = 0; i < MAX_SLOTS; ++i)
        {
            slots.push_back(GetSlotInfo(i));
        }

        return slots;
    }

} // namespace Enjin::Scene
