#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Enjin {
namespace Editor {

// Information about a single entity lock
struct EntityLockInfo {
    std::string user;       // Username who locked the entity
    std::string machine;    // Machine identifier
    u64 timestamp = 0;      // Unix timestamp when locked
};

// Scene-level lock information
struct SceneLockInfo {
    std::string scenePath;                                 // Scene this lock file corresponds to
    std::string lockId;                                    // Unique lock session identifier
    std::unordered_map<u64, EntityLockInfo> entityLocks;   // Entity ID -> lock info
    std::string sceneLockedBy;                             // If non-empty, entire scene is locked by this user
    std::string sceneLockedMachine;
    u64 sceneLockTimestamp = 0;
};

// Advisory lock manager for collaborative scene editing
// Reads/writes .enjinlock JSON sidecar files alongside scene files
class ENJIN_API SceneLockManager {
public:
    SceneLockManager();
    ~SceneLockManager();

    // Set the current scene path (called when opening a scene)
    void SetScenePath(const std::string& scenePath);
    const std::string& GetScenePath() const { return m_ScenePath; }

    // Refresh locks from disk (call periodically or on focus)
    void Refresh();

    // Entity-level locking
    bool LockEntity(ECS::Entity entity);
    bool UnlockEntity(ECS::Entity entity);
    bool IsEntityLocked(ECS::Entity entity) const;
    bool IsEntityLockedByOther(ECS::Entity entity) const;
    const EntityLockInfo* GetEntityLockInfo(ECS::Entity entity) const;

    // Scene-level locking
    bool LockScene();
    bool UnlockScene();
    bool IsSceneLocked() const;
    bool IsSceneLockedByOther() const;
    const std::string& GetSceneLockedBy() const { return m_LockInfo.sceneLockedBy; }

    // Get all locked entity IDs
    std::unordered_set<u64> GetLockedEntityIds() const;

    // Get current user/machine identifiers
    const std::string& GetCurrentUser() const { return m_CurrentUser; }
    const std::string& GetCurrentMachine() const { return m_CurrentMachine; }

    // Release all locks held by this user on the current scene
    void ReleaseAllLocks();

    // Break a stale entity lock (admin action — removes regardless of owner)
    bool BreakEntityLock(ECS::Entity entity);

    // Break the scene-level lock (admin action)
    bool BreakSceneLock();

    // Check if a lock is stale (older than threshold in seconds, default 1 hour)
    bool IsEntityLockStale(ECS::Entity entity, u64 thresholdSeconds = 3600) const;
    bool IsSceneLockStale(u64 thresholdSeconds = 3600) const;

    // Get total lock counts
    usize GetEntityLockCount() const { return m_LockInfo.entityLocks.size(); }
    usize GetMyEntityLockCount() const;
    usize GetOtherEntityLockCount() const;

private:
    bool LoadLockFile();
    bool SaveLockFile();
    std::string GetLockFilePath() const;
    u64 GetTimestamp() const;

    std::string m_ScenePath;
    std::string m_CurrentUser;
    std::string m_CurrentMachine;
    SceneLockInfo m_LockInfo;
};

} // namespace Editor
} // namespace Enjin
