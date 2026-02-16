#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Component.h"
#include "Enjin/ECS/System.h"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <mutex>
#include <vector>

/**
 * @file World.h
 * @brief Main ECS container managing entities, components, and systems
 * @author Enjin Engine Team
 * @date 2025
 */

namespace Enjin {
namespace ECS {

/**
 * @brief The World class manages the entire ECS state
 *
 * It acts as the container for all entities, components, and systems.
 * It provides methods to create/destroy entities and access components.
 *
 * Thread safety: Structural modifications (Create/Destroy/Add/Remove/Clear)
 * are guarded by a recursive mutex. Read operations (Get/Has/GetEntities)
 * are lock-free for performance. DestroyEntity is deferred — entities are
 * queued and actually destroyed at the start of Update() to prevent
 * iterator invalidation during system iteration.
 */
class ENJIN_API World {
public:
    World();
    ~World();

    /**
     * @brief Create a new entity
     * @return The created Entity handle
     */
    Entity CreateEntity();

    /**
     * @brief Queue an entity for deferred destruction
     * @param entity The entity to destroy (actual destruction happens in Update)
     */
    void DestroyEntity(Entity entity);

    /**
     * @brief Immediately destroy an entity (use with caution — not safe during iteration)
     * @param entity The entity to destroy
     */
    void DestroyEntityImmediate(Entity entity);

    /**
     * @brief Flush all pending entity destructions
     * Called automatically at the start of Update(). Can also be called manually
     * between system updates when it's safe to modify entity lists.
     */
    void FlushPendingDestructions();

    /**
     * @brief Check if an entity is valid (not destroyed or pending destruction)
     * @param entity The entity to check
     * @return true if valid, false otherwise
     */
    bool IsValid(Entity entity) const;

    /**
     * @brief Check if an entity is pending deferred destruction
     * @param entity The entity to check
     * @return true if queued for destruction
     */
    bool IsPendingDestruction(Entity entity) const;

    // Component management
    template<typename T>
    T& AddComponent(Entity entity, const T& component = T{}) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        auto storage = GetOrCreateStorage<T>();
        if (storage->Has(entity)) {
            *storage->Get(entity) = component;
            return *storage->Get(entity);
        }
        T& comp = storage->Add(entity);
        comp = component;
        m_SystemManager->OnEntityAdded(entity);
        return comp;
    }

    template<typename T>
    void RemoveComponent(Entity entity) {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        auto storage = GetOrCreateStorage<T>();
        if (storage->Has(entity)) {
            storage->Remove(entity);
            m_SystemManager->OnEntityRemoved(entity);
        }
    }

    template<typename T>
    T* GetComponent(Entity entity) {
        auto storage = GetOrCreateStorage<T>();
        return storage->Get(entity);
    }

    template<typename T>
    const T* GetComponent(Entity entity) const {
        auto storage = GetStorage<T>();
        if (!storage) {
            return nullptr;
        }
        return storage->Get(entity);
    }

    template<typename T>
    bool HasComponent(Entity entity) const {
        auto storage = GetStorage<T>();
        if (!storage) {
            return false;
        }
        return storage->Has(entity);
    }

    // System management
    template<typename T, typename... Args>
    T* RegisterSystem(Args&&... args) {
        return m_SystemManager->RegisterSystem<T>(std::forward<Args>(args)...);
    }

    /**
     * @brief Update the world state
     * @param deltaTime Time elapsed since last frame in seconds
     */
    void Update(f32 deltaTime);

    /**
     * @brief Clear all entities and components
     */
    void Clear();

    /**
     * @brief Get all entities that have a specific component type
     * @tparam T The component type to filter by
     * @return Vector of entities with the given component
     */
    template<typename T>
    const std::vector<Entity>& GetEntitiesWithComponent() const {
        auto storage = GetStorage<T>();
        if (!storage) {
            static const std::vector<Entity> empty;
            return empty;
        }
        return storage->GetEntities();
    }

    /**
     * @brief Get entities that have both component types (intersection)
     * @tparam T1 First component type
     * @tparam T2 Second component type
     * @return Vector of entities with both components
     */
    template<typename T1, typename T2>
    std::vector<Entity> GetEntitiesWithComponents() const {
        auto* s1 = GetStorage<T1>();
        auto* s2 = GetStorage<T2>();
        if (!s1 || !s2) return {};
        // Iterate smaller set, check membership in larger
        if (s1->Size() <= s2->Size()) {
            const auto& entities = s1->GetEntities();
            std::vector<Entity> result;
            result.reserve(entities.size());
            for (Entity e : entities) {
                if (s2->Has(e)) result.push_back(e);
            }
            return result;
        } else {
            const auto& entities = s2->GetEntities();
            std::vector<Entity> result;
            result.reserve(entities.size());
            for (Entity e : entities) {
                if (s1->Has(e)) result.push_back(e);
            }
            return result;
        }
    }

    /**
     * @brief Find an entity by name using O(1) cached lookup
     * @param name The entity name to search for
     * @return The entity handle, or INVALID_ENTITY if not found
     */
    Entity FindEntityByName(const std::string& name);

    /**
     * @brief Invalidate the name cache (call when NameComponents change)
     */
    void InvalidateNameCache() { m_NameCacheDirty = true; }

    /**
     * @brief Get all active entities
     * @return Vector of all active entity handles
     */
    const std::vector<Entity>& GetAllEntities() const { return m_EntityManager.GetAllEntities(); }

    /**
     * @brief Get the number of active entities
     * @return Count of active entities
     */
    usize GetEntityCount() const { return m_EntityManager.GetEntityCount(); }

    /**
     * @brief Lock the world for external batch operations (prevents structural modifications)
     * Use sparingly — prefer DestroyEntity (deferred) over DestroyEntityImmediate
     */
    void Lock() { m_Mutex.lock(); }
    void Unlock() { m_Mutex.unlock(); }

private:
    void RebuildNameCache();
    void DestroyEntityInternal(Entity entity);

    // Type-erased component storage wrapper
    struct StorageBase {
        virtual ~StorageBase() = default;
        virtual void Remove(Entity entity) = 0;
        virtual bool Has(Entity entity) const = 0;
        virtual usize Size() const = 0;
        virtual const std::vector<Entity>& GetEntities() const = 0;
    };

    template<typename T>
    struct StorageWrapper : public StorageBase {
        ComponentStorage<T> storage;

        void Remove(Entity entity) override {
            storage.Remove(entity);
        }
        bool Has(Entity entity) const override {
            return storage.Has(entity);
        }
        usize Size() const override {
            return storage.Size();
        }
        const std::vector<Entity>& GetEntities() const override {
            return storage.GetEntities();
        }
    };

    template<typename T>
    ComponentStorage<T>* GetOrCreateStorage() {
        ComponentTypeId typeId = ComponentRegistry::GetTypeId<T>();
        auto it = m_ComponentStorages.find(typeId);
        if (it == m_ComponentStorages.end()) {
            auto wrapper = std::make_unique<StorageWrapper<T>>();
            ComponentStorage<T>* ptr = &wrapper->storage;
            m_ComponentStorages[typeId] = std::move(wrapper);
            return ptr;
        }
        return &static_cast<StorageWrapper<T>*>(it->second.get())->storage;
    }

    template<typename T>
    const ComponentStorage<T>* GetStorage() const {
        ComponentTypeId typeId = ComponentRegistry::GetTypeId<T>();
        auto it = m_ComponentStorages.find(typeId);
        if (it == m_ComponentStorages.end()) {
            return nullptr;
        }
        return &static_cast<const StorageWrapper<T>*>(it->second.get())->storage;
    }

    EntityManager m_EntityManager;
    std::unique_ptr<SystemManager> m_SystemManager;
    std::unordered_map<ComponentTypeId, std::unique_ptr<StorageBase>> m_ComponentStorages;

    // Name cache for O(1) entity lookup by name
    std::unordered_map<std::string, Entity> m_NameCache;
    bool m_NameCacheDirty = true;

    // Thread safety: guards structural modifications (Create/Destroy/Add/Remove/Clear)
    mutable std::recursive_mutex m_Mutex;

    // Deferred entity destruction queue (flushed at start of Update)
    std::vector<Entity> m_PendingDestructions;
    std::unordered_set<Entity> m_PendingDestructionSet;  // O(1) lookup companion
};

} // namespace ECS
} // namespace Enjin
