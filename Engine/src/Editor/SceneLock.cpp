#include "Enjin/Editor/SceneLock.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <cstdlib>

using json = nlohmann::json;

namespace Enjin {
namespace Editor {

SceneLockManager::SceneLockManager() {
    // Determine current user
#ifdef _WIN32
    const char* user = std::getenv("USERNAME");
#else
    const char* user = std::getenv("USER");
#endif
    m_CurrentUser = user ? user : "unknown";

    // Determine machine name
#ifdef _WIN32
    const char* machine = std::getenv("COMPUTERNAME");
#else
    const char* machine = std::getenv("HOSTNAME");
#endif
    m_CurrentMachine = machine ? machine : "unknown";
}

SceneLockManager::~SceneLockManager() {
    ReleaseAllLocks();
}

void SceneLockManager::SetScenePath(const std::string& scenePath) {
    if (m_ScenePath == scenePath) return;

    // Release locks on old scene
    if (!m_ScenePath.empty()) {
        ReleaseAllLocks();
    }

    m_ScenePath = scenePath;
    m_LockInfo = SceneLockInfo{};
    m_LockInfo.scenePath = scenePath;

    if (!scenePath.empty()) {
        LoadLockFile();
    }
}

void SceneLockManager::Refresh() {
    if (m_ScenePath.empty()) return;
    LoadLockFile();
}

bool SceneLockManager::LockEntity(ECS::Entity entity) {
    u64 id = static_cast<u64>(entity);

    // Check if already locked by someone else
    auto it = m_LockInfo.entityLocks.find(id);
    if (it != m_LockInfo.entityLocks.end()) {
        if (it->second.user != m_CurrentUser || it->second.machine != m_CurrentMachine) {
            ENJIN_LOG_WARN(Editor, "Entity %llu is locked by %s@%s",
                id, it->second.user.c_str(), it->second.machine.c_str());
            return false;
        }
        return true; // Already locked by us
    }

    EntityLockInfo lock;
    lock.user = m_CurrentUser;
    lock.machine = m_CurrentMachine;
    lock.timestamp = GetTimestamp();
    m_LockInfo.entityLocks[id] = lock;

    return SaveLockFile();
}

bool SceneLockManager::UnlockEntity(ECS::Entity entity) {
    u64 id = static_cast<u64>(entity);

    auto it = m_LockInfo.entityLocks.find(id);
    if (it == m_LockInfo.entityLocks.end()) return true; // Not locked

    // Only unlock if we own the lock
    if (it->second.user != m_CurrentUser || it->second.machine != m_CurrentMachine) {
        ENJIN_LOG_WARN(Editor, "Cannot unlock entity %llu — locked by %s@%s",
            id, it->second.user.c_str(), it->second.machine.c_str());
        return false;
    }

    m_LockInfo.entityLocks.erase(it);
    return SaveLockFile();
}

bool SceneLockManager::IsEntityLocked(ECS::Entity entity) const {
    return m_LockInfo.entityLocks.count(static_cast<u64>(entity)) > 0;
}

bool SceneLockManager::IsEntityLockedByOther(ECS::Entity entity) const {
    auto it = m_LockInfo.entityLocks.find(static_cast<u64>(entity));
    if (it == m_LockInfo.entityLocks.end()) return false;
    return it->second.user != m_CurrentUser || it->second.machine != m_CurrentMachine;
}

const EntityLockInfo* SceneLockManager::GetEntityLockInfo(ECS::Entity entity) const {
    auto it = m_LockInfo.entityLocks.find(static_cast<u64>(entity));
    if (it == m_LockInfo.entityLocks.end()) return nullptr;
    return &it->second;
}

bool SceneLockManager::LockScene() {
    if (!m_LockInfo.sceneLockedBy.empty()) {
        if (m_LockInfo.sceneLockedBy != m_CurrentUser ||
            m_LockInfo.sceneLockedMachine != m_CurrentMachine) {
            ENJIN_LOG_WARN(Editor, "Scene is locked by %s@%s",
                m_LockInfo.sceneLockedBy.c_str(), m_LockInfo.sceneLockedMachine.c_str());
            return false;
        }
        return true; // Already locked by us
    }

    m_LockInfo.sceneLockedBy = m_CurrentUser;
    m_LockInfo.sceneLockedMachine = m_CurrentMachine;
    m_LockInfo.sceneLockTimestamp = GetTimestamp();
    return SaveLockFile();
}

bool SceneLockManager::UnlockScene() {
    if (m_LockInfo.sceneLockedBy.empty()) return true;

    if (m_LockInfo.sceneLockedBy != m_CurrentUser ||
        m_LockInfo.sceneLockedMachine != m_CurrentMachine) {
        ENJIN_LOG_WARN(Editor, "Cannot unlock scene — locked by %s@%s",
            m_LockInfo.sceneLockedBy.c_str(), m_LockInfo.sceneLockedMachine.c_str());
        return false;
    }

    m_LockInfo.sceneLockedBy.clear();
    m_LockInfo.sceneLockedMachine.clear();
    m_LockInfo.sceneLockTimestamp = 0;
    return SaveLockFile();
}

bool SceneLockManager::IsSceneLocked() const {
    return !m_LockInfo.sceneLockedBy.empty();
}

bool SceneLockManager::IsSceneLockedByOther() const {
    if (m_LockInfo.sceneLockedBy.empty()) return false;
    return m_LockInfo.sceneLockedBy != m_CurrentUser ||
           m_LockInfo.sceneLockedMachine != m_CurrentMachine;
}

std::unordered_set<u64> SceneLockManager::GetLockedEntityIds() const {
    std::unordered_set<u64> ids;
    for (const auto& [id, lock] : m_LockInfo.entityLocks) {
        ids.insert(id);
    }
    return ids;
}

bool SceneLockManager::BreakEntityLock(ECS::Entity entity) {
    u64 id = static_cast<u64>(entity);
    auto it = m_LockInfo.entityLocks.find(id);
    if (it == m_LockInfo.entityLocks.end()) return true;

    ENJIN_LOG_WARN(Editor, "Breaking lock on entity %llu (held by %s@%s)",
        id, it->second.user.c_str(), it->second.machine.c_str());
    m_LockInfo.entityLocks.erase(it);
    return SaveLockFile();
}

bool SceneLockManager::BreakSceneLock() {
    if (m_LockInfo.sceneLockedBy.empty()) return true;

    ENJIN_LOG_WARN(Editor, "Breaking scene lock (held by %s@%s)",
        m_LockInfo.sceneLockedBy.c_str(), m_LockInfo.sceneLockedMachine.c_str());
    m_LockInfo.sceneLockedBy.clear();
    m_LockInfo.sceneLockedMachine.clear();
    m_LockInfo.sceneLockTimestamp = 0;
    return SaveLockFile();
}

bool SceneLockManager::IsEntityLockStale(ECS::Entity entity, u64 thresholdSeconds) const {
    auto it = m_LockInfo.entityLocks.find(static_cast<u64>(entity));
    if (it == m_LockInfo.entityLocks.end()) return false;
    u64 now = const_cast<SceneLockManager*>(this)->GetTimestamp();
    return (now - it->second.timestamp) > thresholdSeconds;
}

bool SceneLockManager::IsSceneLockStale(u64 thresholdSeconds) const {
    if (m_LockInfo.sceneLockedBy.empty()) return false;
    u64 now = const_cast<SceneLockManager*>(this)->GetTimestamp();
    return (now - m_LockInfo.sceneLockTimestamp) > thresholdSeconds;
}

usize SceneLockManager::GetMyEntityLockCount() const {
    usize count = 0;
    for (const auto& [id, lock] : m_LockInfo.entityLocks) {
        if (lock.user == m_CurrentUser && lock.machine == m_CurrentMachine) ++count;
    }
    return count;
}

usize SceneLockManager::GetOtherEntityLockCount() const {
    usize count = 0;
    for (const auto& [id, lock] : m_LockInfo.entityLocks) {
        if (lock.user != m_CurrentUser || lock.machine != m_CurrentMachine) ++count;
    }
    return count;
}

void SceneLockManager::ReleaseAllLocks() {
    if (m_ScenePath.empty()) return;

    bool changed = false;

    // Release scene lock if ours
    if (!m_LockInfo.sceneLockedBy.empty() &&
        m_LockInfo.sceneLockedBy == m_CurrentUser &&
        m_LockInfo.sceneLockedMachine == m_CurrentMachine) {
        m_LockInfo.sceneLockedBy.clear();
        m_LockInfo.sceneLockedMachine.clear();
        m_LockInfo.sceneLockTimestamp = 0;
        changed = true;
    }

    // Release entity locks that belong to us
    for (auto it = m_LockInfo.entityLocks.begin(); it != m_LockInfo.entityLocks.end();) {
        if (it->second.user == m_CurrentUser && it->second.machine == m_CurrentMachine) {
            it = m_LockInfo.entityLocks.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    if (changed) {
        SaveLockFile();
    }
}

std::string SceneLockManager::GetLockFilePath() const {
    if (m_ScenePath.empty()) return "";
    std::filesystem::path p(m_ScenePath);
    return (p.parent_path() / (p.stem().string() + ".enjinlock")).string();
}

u64 SceneLockManager::GetTimestamp() const {
    return static_cast<u64>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

bool SceneLockManager::LoadLockFile() {
    std::string lockPath = GetLockFilePath();
    if (lockPath.empty()) return false;

    if (!std::filesystem::exists(lockPath)) {
        m_LockInfo = SceneLockInfo{};
        m_LockInfo.scenePath = m_ScenePath;
        return true;
    }

    try {
        std::ifstream file(lockPath);
        if (!file.is_open()) return false;

        json j;
        file >> j;
        file.close();

        m_LockInfo = SceneLockInfo{};
        m_LockInfo.scenePath = m_ScenePath;

        if (j.contains("sceneLockedBy")) m_LockInfo.sceneLockedBy = j["sceneLockedBy"].get<std::string>();
        if (j.contains("sceneLockedMachine")) m_LockInfo.sceneLockedMachine = j["sceneLockedMachine"].get<std::string>();
        if (j.contains("sceneLockTimestamp")) m_LockInfo.sceneLockTimestamp = j["sceneLockTimestamp"].get<u64>();

        if (j.contains("entities") && j["entities"].is_object()) {
            for (auto& [key, val] : j["entities"].items()) {
                u64 entityId = std::stoull(key);
                EntityLockInfo lock;
                if (val.contains("user")) lock.user = val["user"].get<std::string>();
                if (val.contains("machine")) lock.machine = val["machine"].get<std::string>();
                if (val.contains("timestamp")) lock.timestamp = val["timestamp"].get<u64>();
                m_LockInfo.entityLocks[entityId] = lock;
            }
        }

        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_WARN(Editor, "Failed to load lock file: %s", e.what());
        return false;
    }
}

bool SceneLockManager::SaveLockFile() {
    std::string lockPath = GetLockFilePath();
    if (lockPath.empty()) return false;

    // If no locks remain, delete the lock file
    if (m_LockInfo.sceneLockedBy.empty() && m_LockInfo.entityLocks.empty()) {
        std::error_code ec;
        std::filesystem::remove(lockPath, ec);
        return true;
    }

    try {
        json j;
        j["scenePath"] = m_LockInfo.scenePath;

        if (!m_LockInfo.sceneLockedBy.empty()) {
            j["sceneLockedBy"] = m_LockInfo.sceneLockedBy;
            j["sceneLockedMachine"] = m_LockInfo.sceneLockedMachine;
            j["sceneLockTimestamp"] = m_LockInfo.sceneLockTimestamp;
        }

        if (!m_LockInfo.entityLocks.empty()) {
            json entities = json::object();
            for (const auto& [id, lock] : m_LockInfo.entityLocks) {
                json el;
                el["user"] = lock.user;
                el["machine"] = lock.machine;
                el["timestamp"] = lock.timestamp;
                entities[std::to_string(id)] = el;
            }
            j["entities"] = entities;
        }

        std::ofstream file(lockPath);
        if (!file.is_open()) {
            ENJIN_LOG_ERROR(Editor, "Failed to write lock file: %s", lockPath.c_str());
            return false;
        }

        file << j.dump(2);
        file.close();
        return true;
    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Editor, "Failed to save lock file: %s", e.what());
        return false;
    }
}

} // namespace Editor
} // namespace Enjin
