#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Enjin {
namespace Assets {

// ============================================================================
// Prefab Component Data
// ============================================================================

/**
 * @brief Serializable component data
 * Uses a variant-like approach for different component types
 */
struct PrefabComponentData {
    std::string typeName;
    std::unordered_map<std::string, std::string> stringProperties;
    std::unordered_map<std::string, f32> floatProperties;
    std::unordered_map<std::string, i32> intProperties;
    std::unordered_map<std::string, bool> boolProperties;
    std::unordered_map<std::string, Math::Vector3> vec3Properties;
    std::unordered_map<std::string, Math::Vector4> vec4Properties;
};

// ============================================================================
// Prefab Entity Data
// ============================================================================

/**
 * @brief Data for a single entity in a prefab
 */
struct PrefabEntityData {
    std::string name;
    i32 parentIndex = -1;  // -1 = root entity
    std::vector<PrefabComponentData> components;
};

// ============================================================================
// Prefab
// ============================================================================

/**
 * @brief A reusable entity template
 *
 * Prefabs store entity hierarchies that can be instantiated multiple times.
 * Changes to a prefab can optionally propagate to all instances.
 */
class ENJIN_API Prefab {
public:
    Prefab() = default;
    Prefab(const std::string& name);
    ~Prefab() = default;

    /// Get prefab name
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& name) { m_Name = name; }

    /// Get prefab path (if loaded from file)
    const std::string& GetPath() const { return m_Path; }
    void SetPath(const std::string& path) { m_Path = path; }

    /// Get unique ID
    u64 GetId() const { return m_Id; }

    /// Get entity data
    const std::vector<PrefabEntityData>& GetEntities() const { return m_Entities; }
    std::vector<PrefabEntityData>& GetEntities() { return m_Entities; }

    /// Add entity data
    void AddEntity(const PrefabEntityData& entity);

    /// Clear all entities
    void Clear();

    /// Check if empty
    bool IsEmpty() const { return m_Entities.empty(); }

    /// Get root entity count
    u32 GetRootCount() const;

private:
    std::string m_Name = "Prefab";
    std::string m_Path;
    u64 m_Id = 0;
    std::vector<PrefabEntityData> m_Entities;

    static u64 s_NextId;
};

// ============================================================================
// Prefab Instance Component
// ============================================================================

/**
 * @brief Component marking an entity as a prefab instance
 */
struct PrefabInstanceComponent {
    u64 prefabId = 0;
    std::string prefabPath;
    bool overridesApplied = false;

    // Property overrides (entity index -> component type -> property -> value)
    std::unordered_map<u32, std::unordered_map<std::string, PrefabComponentData>> overrides;
};

// ============================================================================
// Prefab Manager
// ============================================================================

/**
 * @brief Manages prefab loading, saving, and instantiation
 */
class ENJIN_API PrefabManager {
public:
    static PrefabManager& Get();

    /// Create prefab from entity hierarchy
    std::shared_ptr<Prefab> CreateFromEntity(ECS::World* world, ECS::Entity rootEntity,
                                             const std::string& name = "Prefab");

    /// Create prefab from multiple entities
    std::shared_ptr<Prefab> CreateFromEntities(ECS::World* world,
                                               const std::vector<ECS::Entity>& entities,
                                               const std::string& name = "Prefab");

    /// Root that relative prefab paths resolve against. The process working
    /// directory is never the project directory (it is the exe folder for both
    /// the editor and the player, and does not exist at all on web), so a
    /// component holding "prefabs/Rock.prefab" cannot be opened without one.
    /// Set at play start / boot alongside the script and audio roots.
    void SetAssetRoot(const std::string& root);
    const std::string& GetAssetRoot() const { return m_AssetRoot; }

    /// Instantiate prefab into world
    ECS::Entity Instantiate(ECS::World* world, const Prefab& prefab,
                           const Math::Vector3& position = Math::Vector3(0, 0, 0),
                           const Math::Vector3& rotation = Math::Vector3(0, 0, 0),
                           const Math::Vector3& scale = Math::Vector3(1, 1, 1));

    /// Instantiate prefab by ID
    ECS::Entity Instantiate(ECS::World* world, u64 prefabId,
                           const Math::Vector3& position = Math::Vector3(0, 0, 0));

    /// Save prefab to file
    bool SavePrefab(const Prefab& prefab, const std::string& filepath);

    /// Load prefab from file
    std::shared_ptr<Prefab> LoadPrefab(const std::string& filepath);

    /// Get cached prefab by path
    std::shared_ptr<Prefab> GetPrefab(const std::string& filepath);

    /// Get prefab by ID
    std::shared_ptr<Prefab> GetPrefabById(u64 id);

    /// Register prefab
    void RegisterPrefab(std::shared_ptr<Prefab> prefab);

    /// Unregister prefab
    void UnregisterPrefab(u64 id);

    /// Clear all cached prefabs
    void ClearCache();

    /// Get all registered prefabs
    const std::unordered_map<u64, std::shared_ptr<Prefab>>& GetAllPrefabs() const {
        return m_Prefabs;
    }

    /// Apply prefab updates to all instances
    void ApplyPrefabToInstances(ECS::World* world, u64 prefabId);

    /// Unpack prefab instance (remove link to prefab)
    void UnpackInstance(ECS::World* world, ECS::Entity entity);

    /// Component serialization callbacks
    using ComponentSerializer = std::function<PrefabComponentData(ECS::World*, ECS::Entity)>;
    using ComponentDeserializer = std::function<void(ECS::World*, ECS::Entity, const PrefabComponentData&)>;

    void RegisterComponentType(const std::string& typeName,
                              ComponentSerializer serializer,
                              ComponentDeserializer deserializer);

private:
    PrefabManager();
    ~PrefabManager();

    PrefabManager(const PrefabManager&) = delete;
    PrefabManager& operator=(const PrefabManager&) = delete;

    void RegisterBuiltInComponents();
    void SerializeEntityRecursive(ECS::World* world, ECS::Entity entity,
                                  Prefab& prefab, i32 parentIndex);

    std::unordered_map<u64, std::shared_ptr<Prefab>> m_Prefabs;
    std::unordered_map<std::string, std::shared_ptr<Prefab>> m_PathCache;
    std::string m_AssetRoot;

    struct ComponentCallbacks {
        ComponentSerializer serializer;
        ComponentDeserializer deserializer;
    };
    std::unordered_map<std::string, ComponentCallbacks> m_ComponentCallbacks;
};

// ============================================================================
// Prefab Utilities
// ============================================================================

namespace PrefabUtils {
    /// Check if entity is a prefab instance root
    ENJIN_API bool IsPrefabInstance(ECS::World* world, ECS::Entity entity);

    /// Check if entity belongs to a prefab instance
    ENJIN_API bool IsPartOfPrefabInstance(ECS::World* world, ECS::Entity entity);

    /// Get root of prefab instance
    ENJIN_API ECS::Entity GetPrefabInstanceRoot(ECS::World* world, ECS::Entity entity);

    /// Get prefab ID from instance
    ENJIN_API u64 GetPrefabId(ECS::World* world, ECS::Entity entity);
}

} // namespace Assets
} // namespace Enjin
