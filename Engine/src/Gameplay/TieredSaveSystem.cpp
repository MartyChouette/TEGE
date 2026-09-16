#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"
#include <unordered_map>
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/ECS/Components/Gameplay.h"
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
        if (entityJson.empty()) continue;
        auto parsed = json::parse(entityJson, nullptr, false);
        if (parsed.is_discarded()) continue;   // GP-H1

        // THE STABLE ID IS WRITTEN HERE, NOT BY THE SERIALIZER.
        //
        // SerializeEntityToString deliberately omits it: its other caller is
        // copy/paste, and a pasted entity carrying the original's stable id
        // would put two entities in the scene claiming the same identity --
        // which is its own class of bug and cost a day elsewhere this week.
        //
        // A save needs the opposite. Runtime Entity ids are generational and do
        // not survive a load, so the stable id is the ONLY join key between a
        // record and the entity it belongs to. So the save system adds it, and
        // the clipboard still does not have it.
        auto* sid = world->GetComponent<ECS::StableIdComponent>(entity);
        if (!sid || sid->id == 0) {
            // Opted into persistence with no identity: it can be written and
            // can never be matched on load. Silence here would look exactly
            // like a save that works.
            ENJIN_LOG_WARN(Editor,
                "TieredSaveSystem: entity %llu has a SaveDataComponent but no stable id, "
                "so nothing it stores can be restored. Give it a StableIdComponent "
                "(the editor adds one when an entity is created in a scene).",
                static_cast<unsigned long long>(entity));
            continue;
        }
        parsed["stableId"] = json::object({{"id", sid->id}});
        entitiesArr.push_back(std::move(parsed));
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
    saveJson["version"] = kSaveFormatVersion;
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

    // NO FULL SCENE DUMP. (ENG-001, S1.)
    //
    // This used to write the ENTIRE level into every slot:
    //   saveJson["sceneData"] = serializer.SaveToString();
    //
    // Three things were wrong with it. A 5.7 MB level times twenty slots is
    // over 100 MB of saves for a one-level demo. The save became the AUTHORITY
    // on the level, so shipping a patch that moved a building or fixed a
    // collider was silently reverted by every existing save -- the hardest
    // class of bug to explain to a player. And it made the tier design
    // pointless: runState and sceneStates are collected twenty lines above and
    // were then rendered redundant by a dump of everything.
    //
    // A save is a DELTA. The level on disk is the authority on the level; the
    // save is the authority on what the player changed about it.

    return saveJson.dump(2);
}

// ---------------------------------------------------------------------------
// Apply save JSON (restore)
// ---------------------------------------------------------------------------

// Apply a tier's entity records onto the live world, matched by stable id.
//
// (ENG-001, S3.) Runtime Entity ids are generational and are not stable across
// a load, so they must never be the join key. StableIdComponent already exists,
// is already serialized, and already survives a round trip -- it is the key.
//
// A record whose entity is gone is LOGGED AND SKIPPED, not an error: deleting an
// authored entity from a level must not make older saves unloadable, and the
// alternative is telling a player their save is corrupt because a designer
// removed a crate.
u32 TieredSaveSystem::ApplyEntityRecords(ECS::World* world, const std::string& recordsJson,
                                         const char* tierName) {
    if (!world || recordsJson.empty()) return 0;
    // Records travel as a string, the way CollectEntitiesByTier already hands
    // them over, so the public header stays free of the JSON library.
    json records = json::parse(recordsJson, nullptr, false);
    if (records.is_discarded() || !records.is_array()) return 0;

    // One pass to index what is live, rather than a scan per record.
    std::unordered_map<u64, ECS::Entity> byStableId;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::StableIdComponent>()) {
        auto* sid = world->GetComponent<ECS::StableIdComponent>(e);
        if (sid && sid->id != 0) byStableId[sid->id] = e;
    }

    u32 applied = 0, missing = 0, unidentified = 0;
    for (const auto& rec : records) {
        if (!rec.is_object()) continue;

        u64 stableId = 0;
        if (rec.contains("stableId")) {
            const auto& sj = rec["stableId"];
            if (sj.is_object() && sj.contains("id")) stableId = sj["id"].get<u64>();
            else if (sj.is_number_unsigned()) stableId = sj.get<u64>();
        }
        if (stableId == 0) {
            ++unidentified;
            continue;
        }

        auto it = byStableId.find(stableId);
        if (it == byStableId.end()) {
            ++missing;
            continue;
        }
        if (Scene::SceneSerializer::ApplyEntityComponents(world, it->second, rec.dump()) > 0) {
            ++applied;
        }
    }

    if (missing > 0 || unidentified > 0) {
        ENJIN_LOG_WARN(Editor,
            "TieredSaveSystem: %s restored %u record(s); %u named an entity that is no "
            "longer in the level (skipped), %u carried no stable id and could not be "
            "matched to anything.",
            tierName, applied, missing, unidentified);
    } else {
        ENJIN_LOG_INFO(Editor, "TieredSaveSystem: %s restored %u record(s)", tierName, applied);
    }
    return applied;
}

bool TieredSaveSystem::ApplySaveJson(const std::string& jsonStr, ECS::World* world) {
    if (!world) return false;

    try {
        json saveJson = json::parse(jsonStr, nullptr, false);
        if (saveJson.is_discarded()) return false;

        // VERSION GATE. (ENG-001, S5.)
        //
        // BuildSaveJson has always written a version and nothing has ever read
        // it, so a file from a future build was parsed as though it were this
        // one -- every key it did not recognise silently absent, every key that
        // changed meaning silently misread. Refusing is the only honest answer
        // to a format this build does not know.
        const i32 fileVersion = saveJson.value("version", 1);
        if (fileVersion > kSaveFormatVersion) {
            ENJIN_LOG_ERROR(Editor,
                "TieredSaveSystem: save is format version %d and this build understands "
                "up to %d. Refusing to load it rather than guessing at what changed.",
                fileVersion, kSaveFormatVersion);
            return false;
        }

        // A version-2 save carries the whole level inside itself, which is the
        // thing S1 removed. It cannot be applied as a delta, because its
        // records were written against a world it also contains.
        if (saveJson.contains("sceneData")) {
            ENJIN_LOG_ERROR(Editor,
                "TieredSaveSystem: this save embeds a full scene dump (format %d). That "
                "format is no longer loadable: a save is now a delta applied over the "
                "level on disk, so the level is whatever the game currently ships. "
                "Start a new game.",
                fileVersion);
            return false;
        }

        // RUN STATE, WHICH WAS WRITE-ONLY. (ENG-001, S2.)
        //
        // BuildSaveJson has always written runState.entities. ApplySaveJson read
        // sceneData, playTime, sceneStates and sceneName, and never read
        // runState back. The tier documented as holding health, inventory and
        // quest progress was written to disk on every save and restored from it
        // never. It appeared to work only because the full scene dump happened
        // to contain those entities anyway -- so removing the dump without this
        // would have stopped run state persisting entirely.
        if (saveJson.contains("runState") && saveJson["runState"].contains("entities")) {
            ApplyEntityRecords(world, saveJson["runState"]["entities"].dump(), "runState");
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
        if (j.is_discarded()) {
            // There IS data here, it just did not parse. Saying "empty" would
            // invite the player to overwrite it.
            info.isCorrupt = true;
            info.isEmpty = false;
            info.displayName = "Damaged save";
            ENJIN_LOG_WARN(Game, "Save slot %u could not be parsed (%zu bytes on disk)",
                           slot, data.size());
            return info;
        }

        info.isEmpty = false;
        if (j.contains("displayName")) info.displayName = j["displayName"].get<std::string>();
        if (j.contains("sceneName")) info.sceneName = j["sceneName"].get<std::string>();
        if (j.contains("timestamp")) info.timestamp = j["timestamp"].get<std::string>();
        if (j.contains("playTime")) info.playTime = j["playTime"].get<f32>();
    } catch (const std::exception& e) {
        info.isCorrupt = true;
        info.isEmpty = false;
        info.displayName = "Damaged save";
        ENJIN_LOG_WARN(Game, "Save slot %u could not be read: %s", slot, e.what());
    }

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

bool TieredSaveSystem::OnCheckpointReached(ECS::World* world, const std::string& sceneName) {
    if (!world) return false;
    // Both switches apply: the master enable, and the per-trigger one. This is the
    // automatic path, and an author who turned either off asked for no save here.
    if (!m_AutoSaveConfig.enabled || !m_AutoSaveConfig.onCheckpoint) return false;

    const u32 slot = GetNextAutoSaveSlot();
    SaveToSlot(slot, world, sceneName);
    ENJIN_LOG_INFO(Editor, "TieredSaveSystem: Auto-saved at checkpoint (slot %u)", slot);
    return true;
}

void TieredSaveSystem::ConfigureAutoSave(const AutoSaveConfig& config) {
    m_AutoSaveConfig = config;
    m_AutoSaveTimer = 0.0f;
}

u32 TieredSaveSystem::GetNextAutoSaveSlot() {
    u32 slotCount = (m_AutoSaveConfig.autoSaveSlotCount > 0) ? m_AutoSaveConfig.autoSaveSlotCount : 1;
    u32 slot = AUTO_SAVE_SLOT_START + (m_AutoSaveRotation % slotCount);
    m_AutoSaveRotation++;
    return slot;
}

void TieredSaveSystem::ApplyConfigFromWorld(ECS::World* world) {
    if (!world) return;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::SaveSystemComponent>()) {
        const auto* c = world->GetComponent<ECS::SaveSystemComponent>(e);
        if (!c) continue;
        m_AutoSaveConfig.enabled = c->autoSaveEnabled;
        m_AutoSaveConfig.onSceneTransition = c->autoSaveOnSceneTransition;
        m_AutoSaveConfig.onTimedInterval = c->autoSaveOnInterval;
        m_AutoSaveConfig.onCheckpoint = c->autoSaveOnCheckpoint;
        m_AutoSaveConfig.autoSaveSlotCount = c->autoSaveSlotCount;

        // A zero interval would save every frame, which is a disk-shredder
        // rather than a setting. Clamped and said out loud ONCE, because a
        // believable wrong value is worse than an absurd one: silently treating
        // 0 as "never" would look like auto-save being broken again.
        f32 interval = c->autoSaveIntervalSeconds;
        if (m_AutoSaveConfig.onTimedInterval && interval < kMinAutoSaveInterval) {
            static bool warned = false;
            if (!warned) {
                warned = true;
                ENJIN_LOG_WARN(Editor,
                    "SaveSystemComponent.autoSaveIntervalSeconds is %.2f; clamping to %.0fs. "
                    "A shorter interval saves faster than a save takes.",
                    interval, kMinAutoSaveInterval);
            }
            interval = kMinAutoSaveInterval;
        }
        m_AutoSaveConfig.intervalSeconds = interval;
        break;   // first game manager wins
    }
}

void TieredSaveSystem::Update(f32 deltaTime, ECS::World* world, const std::string& currentScene) {
    m_SessionPlayTime += deltaTime;
    m_CurrentScene = currentScene;

    ApplyConfigFromWorld(world);

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
