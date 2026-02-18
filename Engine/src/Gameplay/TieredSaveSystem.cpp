#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

namespace Enjin {
namespace Gameplay {

TieredSaveSystem::TieredSaveSystem() {
    m_LocalBackend = std::make_shared<LocalSaveBackend>();
}

// ---------------------------------------------------------------------------
// Slot key helpers
// ---------------------------------------------------------------------------

std::string TieredSaveSystem::GetSlotKey(u32 slot) const {
    if (slot >= AUTO_SAVE_SLOT_START) {
        return "slot_" + std::to_string(slot) + "_auto.enjsave";
    }
    return std::string("slot_") + (slot < 10 ? "0" : "") + std::to_string(slot) + ".enjsave";
}

std::string TieredSaveSystem::GetMetaKey() const {
    return "meta.enjsave";
}

// ---------------------------------------------------------------------------
// Timestamp helper
// ---------------------------------------------------------------------------

static std::string GetTimestamp() {
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
    return oss.str();
}

// ---------------------------------------------------------------------------
// Collect entities by tier (serialize SaveDataComponent entities of given tier)
// ---------------------------------------------------------------------------

void TieredSaveSystem::CollectEntitiesByTier(ECS::World* world, ECS::PersistenceTier tier,
                                              std::string& outJson) {
    if (!world) { outJson = "[]"; return; }

    json entitiesArr = json::array();
    auto entities = world->GetEntitiesWithComponent<ECS::SaveDataComponent>();

    for (auto entity : entities) {
        auto* sd = world->GetComponent<ECS::SaveDataComponent>(entity);
        if (!sd || sd->tier != tier) continue;

        // Serialize this entity using the SceneSerializer per-entity method
        std::string entityJson = Scene::SceneSerializer::SerializeEntityToString(world, entity);
        if (!entityJson.empty()) {
            auto parsed = json::parse(entityJson, nullptr, false);
            if (!parsed.is_discarded()) entitiesArr.push_back(std::move(parsed)); // GP-H1
        }
    }
    outJson = entitiesArr.dump();
}

// ---------------------------------------------------------------------------
// Cache current scene's SceneState entities
// ---------------------------------------------------------------------------

void TieredSaveSystem::CacheCurrentSceneState(ECS::World* world, const std::string& sceneName) {
    if (sceneName.empty() || !world) return;
    std::string data;
    CollectEntitiesByTier(world, ECS::PersistenceTier::SceneState, data);
    m_SceneStateCache[sceneName] = data;
}

// ---------------------------------------------------------------------------
// Build save JSON
// ---------------------------------------------------------------------------

std::string TieredSaveSystem::BuildSaveJson(u32 slot, ECS::World* world,
                                             const std::string& sceneName) {
    json saveJson;
    saveJson["version"] = 2;
    saveJson["slotIndex"] = slot;
    saveJson["displayName"] = (slot >= AUTO_SAVE_SLOT_START)
        ? "Auto Save " + std::to_string(slot - AUTO_SAVE_SLOT_START + 1)
        : "Save " + std::to_string(slot + 1);
    saveJson["sceneName"] = sceneName;
    saveJson["timestamp"] = GetTimestamp();
    saveJson["playTime"] = m_SessionPlayTime;

    // RunState: serialize all RunState-tier entities + full scene data
    {
        std::string runData;
        CollectEntitiesByTier(world, ECS::PersistenceTier::RunState, runData);
        auto parsed = json::parse(runData, nullptr, false);
        saveJson["runState"]["entities"] = parsed.is_discarded() ? json::array() : std::move(parsed); // GP-H2
    }

    // Cache current scene's SceneState before saving
    CacheCurrentSceneState(world, sceneName);

    // SceneStates: per-scene SceneState entity data
    {
        json sceneStates = json::object();
        for (const auto& [scene, data] : m_SceneStateCache) {
            auto parsed = json::parse(data, nullptr, false);
            sceneStates[scene]["entities"] = parsed.is_discarded() ? json::array() : std::move(parsed); // GP-H3
        }
        saveJson["sceneStates"] = sceneStates;
    }

    // Full scene data for complete restore
    {
        Scene::SceneSerializer serializer(world);
        saveJson["sceneData"] = serializer.SaveToString();
    }

    return saveJson.dump(2);
}

// ---------------------------------------------------------------------------
// Apply save JSON (restore)
// ---------------------------------------------------------------------------

bool TieredSaveSystem::ApplySaveJson(const std::string& jsonStr, ECS::World* world) {
    if (!world) return false;

    try {
        json saveJson = json::parse(jsonStr, nullptr, false);
        if (saveJson.is_discarded()) return false;

        // Restore full scene
        if (saveJson.contains("sceneData")) {
            std::string sceneData = saveJson["sceneData"].get<std::string>();
            Scene::SceneSerializer serializer(world);
            auto result = serializer.LoadFromString(sceneData, true);
            if (!result.success) {
                ENJIN_LOG_ERROR(Editor, "TieredSaveSystem: Failed to restore scene: %s",
                                result.error.c_str());
                return false;
            }
        }

        // Restore play time
        if (saveJson.contains("playTime")) {
            m_SessionPlayTime = saveJson["playTime"].get<f32>();
        }

        // Restore scene state cache
        m_SceneStateCache.clear();
        if (saveJson.contains("sceneStates") && saveJson["sceneStates"].is_object()) {
            for (auto& [scene, data] : saveJson["sceneStates"].items()) {
                if (data.contains("entities")) {
                    m_SceneStateCache[scene] = data["entities"].dump();
                }
            }
        }

        // Set current scene
        if (saveJson.contains("sceneName")) {
            m_CurrentScene = saveJson["sceneName"].get<std::string>();
        }

        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "TieredSaveSystem: Load failed: %s", e.what());
        return false;
    }
}

// ---------------------------------------------------------------------------
// Slot operations
// ---------------------------------------------------------------------------

bool TieredSaveSystem::SaveToSlot(u32 slot, ECS::World* world, const std::string& sceneName) {
    if (!world || slot >= MAX_SLOTS || !m_LocalBackend) return false;

    std::string key = GetSlotKey(slot);
    std::string data = BuildSaveJson(slot, world, sceneName);

    if (!m_LocalBackend->Write(key, data)) {
        ENJIN_LOG_ERROR(Editor, "TieredSaveSystem: Failed to write slot %u", slot);
        return false;
    }

    ENJIN_LOG_INFO(Editor, "TieredSaveSystem: Saved to slot %u (%s)", slot, sceneName.c_str());
    return true;
}

bool TieredSaveSystem::LoadFromSlot(u32 slot, ECS::World* world) {
    if (!world || slot >= MAX_SLOTS || !m_LocalBackend) return false;

    std::string key = GetSlotKey(slot);
    std::string data;
    if (!m_LocalBackend->Read(key, data)) {
        ENJIN_LOG_WARN(Editor, "TieredSaveSystem: Slot %u not found", slot);
        return false;
    }

    if (!ApplySaveJson(data, world)) return false;

    ENJIN_LOG_INFO(Editor, "TieredSaveSystem: Loaded slot %u", slot);
    return true;
}

bool TieredSaveSystem::DeleteSlot(u32 slot) {
    if (slot >= MAX_SLOTS || !m_LocalBackend) return false;
    return m_LocalBackend->Delete(GetSlotKey(slot));
}

SaveSlotInfo TieredSaveSystem::GetSlotInfo(u32 slot) const {
    SaveSlotInfo info;
    info.slotIndex = slot;
    info.isAutoSave = (slot >= AUTO_SAVE_SLOT_START);
    if (slot >= MAX_SLOTS || !m_LocalBackend) return info;

    std::string key = GetSlotKey(slot);
    std::string data;
    if (!m_LocalBackend->Read(key, data)) return info;

    try {
        json j = json::parse(data, nullptr, false);
        if (j.is_discarded()) return info;

        info.isEmpty = false;
        if (j.contains("displayName")) info.displayName = j["displayName"].get<std::string>();
        if (j.contains("sceneName")) info.sceneName = j["sceneName"].get<std::string>();
        if (j.contains("timestamp")) info.timestamp = j["timestamp"].get<std::string>();
        if (j.contains("playTime")) info.playTime = j["playTime"].get<f32>();
    } catch (...) {}

    return info;
}

std::vector<SaveSlotInfo> TieredSaveSystem::GetAllSlots() const {
    std::vector<SaveSlotInfo> slots;
    slots.reserve(MAX_SLOTS);
    for (u32 i = 0; i < MAX_SLOTS; ++i) {
        slots.push_back(GetSlotInfo(i));
    }
    return slots;
}

// ---------------------------------------------------------------------------
// Meta-progression
// ---------------------------------------------------------------------------

bool TieredSaveSystem::SaveMeta() {
    if (!m_LocalBackend) return false;

    json j;
    j["version"] = 1;

    json floats = json::object();
    for (const auto& [k, v] : m_MetaFloats) floats[k] = v;
    j["floats"] = floats;

    json ints = json::object();
    for (const auto& [k, v] : m_MetaInts) ints[k] = v;
    j["ints"] = ints;

    json bools = json::object();
    for (const auto& [k, v] : m_MetaBools) bools[k] = v;
    j["bools"] = bools;

    json strings = json::object();
    for (const auto& [k, v] : m_MetaStrings) strings[k] = v;
    j["strings"] = strings;

    return m_LocalBackend->Write(GetMetaKey(), j.dump(2));
}

bool TieredSaveSystem::LoadMeta() {
    if (!m_LocalBackend) return false;

    std::string data;
    if (!m_LocalBackend->Read(GetMetaKey(), data)) return false;

    try {
        json j = json::parse(data, nullptr, false);
        if (j.is_discarded()) return false;

        m_MetaFloats.clear();
        m_MetaInts.clear();
        m_MetaBools.clear();
        m_MetaStrings.clear();

        if (j.contains("floats") && j["floats"].is_object()) {
            for (auto& [k, v] : j["floats"].items()) m_MetaFloats[k] = v.get<f32>();
        }
        if (j.contains("ints") && j["ints"].is_object()) {
            for (auto& [k, v] : j["ints"].items()) m_MetaInts[k] = v.get<i32>();
        }
        if (j.contains("bools") && j["bools"].is_object()) {
            for (auto& [k, v] : j["bools"].items()) m_MetaBools[k] = v.get<bool>();
        }
        if (j.contains("strings") && j["strings"].is_object()) {
            for (auto& [k, v] : j["strings"].items()) m_MetaStrings[k] = v.get<std::string>();
        }

        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "TieredSaveSystem: LoadMeta failed: %s", e.what());
        return false;
    }
}

void TieredSaveSystem::SetMetaFloat(const std::string& key, f32 value) { m_MetaFloats[key] = value; }
f32 TieredSaveSystem::GetMetaFloat(const std::string& key, f32 fallback) const {
    auto it = m_MetaFloats.find(key);
    return it != m_MetaFloats.end() ? it->second : fallback;
}
void TieredSaveSystem::SetMetaInt(const std::string& key, i32 value) { m_MetaInts[key] = value; }
i32 TieredSaveSystem::GetMetaInt(const std::string& key, i32 fallback) const {
    auto it = m_MetaInts.find(key);
    return it != m_MetaInts.end() ? it->second : fallback;
}
void TieredSaveSystem::SetMetaBool(const std::string& key, bool value) { m_MetaBools[key] = value; }
bool TieredSaveSystem::GetMetaBool(const std::string& key, bool fallback) const {
    auto it = m_MetaBools.find(key);
    return it != m_MetaBools.end() ? it->second : fallback;
}
void TieredSaveSystem::SetMetaString(const std::string& key, const std::string& value) { m_MetaStrings[key] = value; }
std::string TieredSaveSystem::GetMetaString(const std::string& key, const std::string& fallback) const {
    auto it = m_MetaStrings.find(key);
    return it != m_MetaStrings.end() ? it->second : fallback;
}

// ---------------------------------------------------------------------------
// Checkpoints & auto-save
// ---------------------------------------------------------------------------

void TieredSaveSystem::Checkpoint(ECS::World* world, const std::string& sceneName) {
    if (!world) return;
    u32 slot = GetNextAutoSaveSlot();
    SaveToSlot(slot, world, sceneName);
    ENJIN_LOG_INFO(Editor, "TieredSaveSystem: Checkpoint saved to auto-slot %u", slot);
}

void TieredSaveSystem::ConfigureAutoSave(const AutoSaveConfig& config) {
    m_AutoSaveConfig = config;
    m_AutoSaveTimer = 0.0f;
}

u32 TieredSaveSystem::GetNextAutoSaveSlot() {
    u32 slot = AUTO_SAVE_SLOT_START + (m_AutoSaveRotation % m_AutoSaveConfig.autoSaveSlotCount);
    m_AutoSaveRotation++;
    return slot;
}

void TieredSaveSystem::Update(f32 deltaTime, ECS::World* world, const std::string& currentScene) {
    m_SessionPlayTime += deltaTime;
    m_CurrentScene = currentScene;

    if (!m_AutoSaveConfig.enabled || !world) return;

    if (m_AutoSaveConfig.onTimedInterval) {
        m_AutoSaveTimer += deltaTime;
        if (m_AutoSaveTimer >= m_AutoSaveConfig.intervalSeconds) {
            m_AutoSaveTimer = 0.0f;
            u32 slot = GetNextAutoSaveSlot();
            SaveToSlot(slot, world, currentScene);
            ENJIN_LOG_INFO(Editor, "TieredSaveSystem: Auto-saved to slot %u (timed)", slot);
        }
    }
}

void TieredSaveSystem::OnSceneTransition(const std::string& fromScene, const std::string& toScene,
                                          ECS::World* world) {
    if (!world) return;

    // Cache SceneState entities from the scene we're leaving
    CacheCurrentSceneState(world, fromScene);

    // Auto-save on scene transition if configured
    if (m_AutoSaveConfig.enabled && m_AutoSaveConfig.onSceneTransition) {
        u32 slot = GetNextAutoSaveSlot();
        SaveToSlot(slot, world, fromScene);
        ENJIN_LOG_INFO(Editor, "TieredSaveSystem: Auto-saved on transition %s -> %s (slot %u)",
                        fromScene.c_str(), toScene.c_str(), slot);
    }

    m_CurrentScene = toScene;
}

// ---------------------------------------------------------------------------
// Cloud sync
// ---------------------------------------------------------------------------

void TieredSaveSystem::SetBackend(std::shared_ptr<ISaveBackend> backend) {
    m_LocalBackend = backend;
}

void TieredSaveSystem::SetCloudBackend(std::shared_ptr<ISaveBackend> cloud) {
    m_CloudBackend = cloud;
}

void TieredSaveSystem::SyncToCloud() {
    if (!m_CloudBackend || !m_LocalBackend) return;

    // Sync all existing slots + meta to cloud
    for (u32 i = 0; i < MAX_SLOTS; ++i) {
        std::string key = GetSlotKey(i);
        std::string data;
        if (m_LocalBackend->Read(key, data)) {
            m_CloudBackend->Write(key, data);
        }
    }

    // Sync meta
    std::string metaData;
    if (m_LocalBackend->Read(GetMetaKey(), metaData)) {
        m_CloudBackend->Write(GetMetaKey(), metaData);
    }

    ENJIN_LOG_INFO(Editor, "TieredSaveSystem: Synced saves to cloud (%s)",
                    m_CloudBackend->GetName().c_str());
}

void TieredSaveSystem::SyncFromCloud() {
    if (!m_CloudBackend || !m_LocalBackend) return;

    for (u32 i = 0; i < MAX_SLOTS; ++i) {
        std::string key = GetSlotKey(i);
        std::string data;
        if (m_CloudBackend->Read(key, data)) {
            m_LocalBackend->Write(key, data);
        }
    }

    std::string metaData;
    if (m_CloudBackend->Read(GetMetaKey(), metaData)) {
        m_LocalBackend->Write(GetMetaKey(), metaData);
    }

    // Reload meta into memory
    LoadMeta();

    ENJIN_LOG_INFO(Editor, "TieredSaveSystem: Synced saves from cloud (%s)",
                    m_CloudBackend->GetName().c_str());
}

} // namespace Gameplay
} // namespace Enjin
