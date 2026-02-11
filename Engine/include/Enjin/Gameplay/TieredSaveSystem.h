#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Gameplay/SaveBackend.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace Enjin {
namespace Gameplay {

// Info about a single save slot (lightweight metadata)
struct SaveSlotInfo {
    u32 slotIndex = 0;
    std::string displayName;
    std::string sceneName;
    std::string timestamp;
    f32 playTime = 0.0f;
    bool isEmpty = true;
    bool isAutoSave = false;
};

// Auto-save configuration
struct AutoSaveConfig {
    bool enabled = false;
    bool onSceneTransition = true;
    bool onTimedInterval = false;
    f32 intervalSeconds = 300.0f;  // 5 minutes
    bool onCheckpoint = true;
    u32 autoSaveSlotCount = 3;    // Rotating auto-save slots (slot 17-19)
};

// 3-tier save system: SceneState, RunState, MetaProgression
class ENJIN_API TieredSaveSystem {
public:
    static constexpr u32 MAX_SLOTS = 20;
    static constexpr u32 MANUAL_SLOTS = 17;       // Slots 0-16
    static constexpr u32 AUTO_SAVE_SLOT_START = 17; // Slots 17-19

    TieredSaveSystem();
    ~TieredSaveSystem() = default;

    // Slot operations
    bool SaveToSlot(u32 slot, ECS::World* world, const std::string& sceneName);
    bool LoadFromSlot(u32 slot, ECS::World* world);
    bool DeleteSlot(u32 slot);
    SaveSlotInfo GetSlotInfo(u32 slot) const;
    std::vector<SaveSlotInfo> GetAllSlots() const;

    // Meta-progression (separate file, never deleted by slot operations)
    bool SaveMeta();
    bool LoadMeta();
    void SetMetaFloat(const std::string& key, f32 value);
    f32 GetMetaFloat(const std::string& key, f32 fallback = 0.0f) const;
    void SetMetaInt(const std::string& key, i32 value);
    i32 GetMetaInt(const std::string& key, i32 fallback = 0) const;
    void SetMetaBool(const std::string& key, bool value);
    bool GetMetaBool(const std::string& key, bool fallback = false) const;
    void SetMetaString(const std::string& key, const std::string& value);
    std::string GetMetaString(const std::string& key, const std::string& fallback = "") const;

    // Meta data accessors (for debug panel)
    const std::unordered_map<std::string, f32>& GetMetaFloats() const { return m_MetaFloats; }
    const std::unordered_map<std::string, i32>& GetMetaInts() const { return m_MetaInts; }
    const std::unordered_map<std::string, bool>& GetMetaBools() const { return m_MetaBools; }
    const std::unordered_map<std::string, std::string>& GetMetaStrings() const { return m_MetaStrings; }

    // Checkpoints & auto-save
    void Checkpoint(ECS::World* world, const std::string& sceneName);
    void ConfigureAutoSave(const AutoSaveConfig& config);
    const AutoSaveConfig& GetAutoSaveConfig() const { return m_AutoSaveConfig; }
    AutoSaveConfig& GetAutoSaveConfig() { return m_AutoSaveConfig; }
    void Update(f32 deltaTime, ECS::World* world, const std::string& currentScene);

    // Scene transition hook
    void OnSceneTransition(const std::string& fromScene, const std::string& toScene,
                           ECS::World* world);

    // Backend management
    void SetBackend(std::shared_ptr<ISaveBackend> backend);
    void SetCloudBackend(std::shared_ptr<ISaveBackend> cloud);
    void SyncToCloud();
    void SyncFromCloud();
    ISaveBackend* GetBackend() const { return m_LocalBackend.get(); }
    ISaveBackend* GetCloudBackend() const { return m_CloudBackend.get(); }

    // Track play time
    void AddPlayTime(f32 deltaTime) { m_SessionPlayTime += deltaTime; }
    f32 GetSessionPlayTime() const { return m_SessionPlayTime; }

    // Current scene tracking for auto-save
    void SetCurrentScene(const std::string& scene) { m_CurrentScene = scene; }
    const std::string& GetCurrentScene() const { return m_CurrentScene; }

private:
    std::shared_ptr<ISaveBackend> m_LocalBackend;
    std::shared_ptr<ISaveBackend> m_CloudBackend;

    // Meta-progression (always loaded)
    std::unordered_map<std::string, f32> m_MetaFloats;
    std::unordered_map<std::string, i32> m_MetaInts;
    std::unordered_map<std::string, bool> m_MetaBools;
    std::unordered_map<std::string, std::string> m_MetaStrings;

    // Auto-save state
    AutoSaveConfig m_AutoSaveConfig;
    f32 m_AutoSaveTimer = 0.0f;
    u32 m_AutoSaveRotation = 0;  // Rotating index within auto-save slots

    // Session state
    f32 m_SessionPlayTime = 0.0f;
    std::string m_CurrentScene;

    // Per-scene state cache (scene name -> JSON data for SceneState entities)
    std::unordered_map<std::string, std::string> m_SceneStateCache;

    // Helpers
    std::string GetSlotKey(u32 slot) const;
    std::string GetMetaKey() const;
    std::string BuildSaveJson(u32 slot, ECS::World* world, const std::string& sceneName);
    bool ApplySaveJson(const std::string& jsonStr, ECS::World* world);
    void CollectEntitiesByTier(ECS::World* world, ECS::PersistenceTier tier,
                               std::string& outJson);
    void CacheCurrentSceneState(ECS::World* world, const std::string& sceneName);
    u32 GetNextAutoSaveSlot();
};

} // namespace Gameplay
} // namespace Enjin
