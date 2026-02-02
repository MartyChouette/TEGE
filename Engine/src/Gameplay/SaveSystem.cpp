#include "Enjin/Gameplay/SaveSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace Enjin {
namespace Gameplay {

std::string SaveSystem::GetSaveDirectory() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) return std::string(appdata) + "\\enjin\\saves\\";
    return "saves\\";
#elif defined(__APPLE__)
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/Library/Application Support/enjin/saves/";
    return "saves/";
#else
    const char* home = std::getenv("HOME");
    if (home) return std::string(home) + "/.config/enjin/saves/";
    return "saves/";
#endif
}

std::string SaveSystem::GetSlotPath(i32 slot) const {
    return GetSaveDirectory() + "slot_" + std::to_string(slot) + ".json";
}

std::string SaveSystem::GetQuickSavePath() const {
    return GetSaveDirectory() + "quicksave.json";
}

bool SaveSystem::SaveToSlot(i32 slot, ECS::World* world, const std::string& sceneName) {
    if (!world || slot < 0 || slot >= MAX_SLOTS) return false;

    std::string dir = GetSaveDirectory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");

    // Serialize scene using SceneSerializer
    Scene::SceneSerializer serializer(world);
    std::string sceneData = serializer.SaveToString();

    try {
        json saveJson;
        saveJson["slotIndex"] = slot;
        saveJson["sceneName"] = sceneName;
        saveJson["timestamp"] = oss.str();
        saveJson["displayName"] = "Save " + std::to_string(slot + 1);
        saveJson["sceneData"] = sceneData;

        std::ofstream file(GetSlotPath(slot));
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Editor, "Failed to write save slot %d", slot);
            return false;
        }
        file << saveJson.dump(2);
        file.close();

        ENJIN_LOG_INFO(Editor, "Saved to slot %d: %s", slot, sceneName.c_str());
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "Save failed: %s", e.what());
        return false;
    }
}

bool SaveSystem::LoadFromSlot(i32 slot, ECS::World* world) {
    if (!world || slot < 0 || slot >= MAX_SLOTS) return false;

    std::string path = GetSlotPath(slot);
    if (!std::filesystem::exists(path)) return false;

    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        json saveJson;
        file >> saveJson;
        file.close();

        if (!saveJson.contains("sceneData")) return false;

        std::string sceneData = saveJson["sceneData"].get<std::string>();
        Scene::SceneSerializer serializer(world);
        auto result = serializer.LoadFromString(sceneData, true);

        ENJIN_LOG_INFO(Editor, "Loaded save slot %d", slot);
        return result.success;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "Load failed: %s", e.what());
        return false;
    }
}

bool SaveSystem::DeleteSlot(i32 slot) {
    if (slot < 0 || slot >= MAX_SLOTS) return false;
    std::string path = GetSlotPath(slot);
    if (!std::filesystem::exists(path)) return true;
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

SaveSlot SaveSystem::GetSlotInfo(i32 slot) const {
    SaveSlot info;
    info.slotIndex = slot;
    if (slot < 0 || slot >= MAX_SLOTS) return info;

    std::string path = GetSlotPath(slot);
    if (!std::filesystem::exists(path)) return info;

    try {
        std::ifstream file(path);
        if (!file.is_open()) return info;
        json j;
        file >> j;
        file.close();

        info.isEmpty = false;
        if (j.contains("sceneName")) info.sceneName = j["sceneName"].get<std::string>();
        if (j.contains("timestamp")) info.timestamp = j["timestamp"].get<std::string>();
        if (j.contains("playTime")) info.playTime = j["playTime"].get<f32>();
        if (j.contains("displayName")) info.displayName = j["displayName"].get<std::string>();
    } catch (const std::exception& e) {
        ENJIN_LOG_WARN(Editor, "Error reading save slot metadata '%s': %s", path.c_str(), e.what());
    } catch (...) {
        ENJIN_LOG_WARN(Editor, "Unknown error reading save slot metadata '%s'", path.c_str());
    }

    return info;
}

std::vector<SaveSlot> SaveSystem::GetAllSlots() const {
    std::vector<SaveSlot> slots;
    for (i32 i = 0; i < MAX_SLOTS; ++i) {
        slots.push_back(GetSlotInfo(i));
    }
    return slots;
}

bool SaveSystem::QuickSave(ECS::World* world, const std::string& sceneName) {
    if (!world) return false;
    std::string dir = GetSaveDirectory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    Scene::SceneSerializer serializer(world);
    std::string sceneData = serializer.SaveToString();

    try {
        json saveJson;
        saveJson["sceneName"] = sceneName;
        saveJson["sceneData"] = sceneData;

        std::ofstream file(GetQuickSavePath());
        if (!file.is_open()) return false;
        file << saveJson.dump(2);
        file.close();

        ENJIN_LOG_INFO(Editor, "Quick saved: %s", sceneName.c_str());
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "Quick save failed: %s", e.what());
        return false;
    } catch (...) {
        ENJIN_LOG_ERROR(Editor, "Quick save failed: unknown error");
        return false;
    }
}

bool SaveSystem::QuickLoad(ECS::World* world) {
    if (!world) return false;
    std::string path = GetQuickSavePath();
    if (!std::filesystem::exists(path)) return false;

    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        json saveJson;
        file >> saveJson;
        file.close();

        if (!saveJson.contains("sceneData")) return false;

        Scene::SceneSerializer serializer(world);
        auto result = serializer.LoadFromString(saveJson["sceneData"].get<std::string>(), true);
        ENJIN_LOG_INFO(Editor, "Quick loaded");
        return result.success;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "Quick load failed: %s", e.what());
        return false;
    } catch (...) {
        ENJIN_LOG_ERROR(Editor, "Quick load failed: unknown error");
        return false;
    }
}

} // namespace Gameplay
} // namespace Enjin
