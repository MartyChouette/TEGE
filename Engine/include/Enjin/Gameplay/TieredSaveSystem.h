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
    // There is a slot here and it could not be read. NOT the same as empty, and
    // it used to render as empty: a player whose save was corrupt or locked saw
    // "(empty)", and the menu hid Load behind !isEmpty while still offering
    // Save -- so the UI invited them to overwrite a recoverable save.
    bool isCorrupt = false;
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
    // Floor for a timed auto-save. Below this a save starts before the last
    // one finished.
    static constexpr f32 kMinAutoSaveInterval = 5.0f;

    // Save format this build writes and is willing to read.
    //
    // 3 dropped the embedded full-scene dump: a save is a delta over the level
    // on disk, not a copy of it. A version above this is refused rather than
    // parsed as though it were this one. (ENG-001.)
    static constexpr i32 kSaveFormatVersion = 3;

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

    // Adopt the auto-save block from a SaveSystemComponent in the world, if
    // one is there. Called at the top of Update, so every runtime that already
    // ticks this system gets it and no caller changes.
    //
    // AutoSaveConfig and SaveSystemComponent's auto-save fields are the same
    // six settings written twice, and NOTHING connected them: ConfigureAutoSave
    // had no callers anywhere, so `enabled` sat at its default of false and
    // Update returned on the first line. Timed auto-save has never run in a
    // shipped game, and the component field that would switch it on was read by
    // nobody (2026-09-16).
    //
    // The component WINS while it exists, every frame. That matters if anything
    // ever calls ConfigureAutoSave as well: authored configuration beats code,
    // because the person editing the scene can see it.
    void ApplyConfigFromWorld(ECS::World* world);
    const AutoSaveConfig& GetAutoSaveConfig() const { return m_AutoSaveConfig; }
    AutoSaveConfig& GetAutoSaveConfig() { return m_AutoSaveConfig; }
    void Update(f32 deltaTime, ECS::World* world, const std::string& currentScene);

    // Scene transition hook
    // A checkpoint the GAME reached, as opposed to one a script asked for.
    //
    // Checkpoint() below is an explicit command -- a script calling
    // SaveGame_Checkpoint() means it, and honouring a config switch there would be
    // silently ignoring an instruction. This is the other half: the player walked
    // into a Checkpoint goal zone, and whether that saves is exactly what the
    // "On Checkpoint" switch is for.
    //
    // That switch had no consumer at all. It sat in the Save Debug panel with
    // onSceneTransition and onTimedInterval, both of which work, so it read as the
    // third member of a working set.
    //
    // Returns whether it saved, so a caller can say so rather than guessing.
    bool OnCheckpointReached(ECS::World* world, const std::string& sceneName);

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
    // Apply one tier's records onto the live world, matched by StableIdComponent.
    // Returns how many were applied; missing entities are skipped, not fatal.
    u32 ApplyEntityRecords(ECS::World* world, const std::string& recordsJson,
                           const char* tierName);

    bool ApplySaveJson(const std::string& jsonStr, ECS::World* world);
    void CollectEntitiesByTier(ECS::World* world, ECS::PersistenceTier tier,
                               std::string& outJson);
    void CacheCurrentSceneState(ECS::World* world, const std::string& sceneName);
    u32 GetNextAutoSaveSlot();
};

} // namespace Gameplay
} // namespace Enjin
