#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Core/Assert.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Component.h"
#include "Enjin/ECS/System.h"
#include <unordered_map>
#include <typeinfo>
#include <utility>
#include <unordered_set>
#include <memory>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
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
 * Thread safety (adr-0004 — single-writer / fork-join reads):
 *   - Structural mutation (Create/Destroy/Add/Remove/Clear and the deferred
 *     destruction flush) runs ONLY on the owner thread — the thread that
 *     constructed the World, or one re-designated via AdoptOwnerThread(). Those
 *     paths are serialized by a recursive mutex and, in debug builds, assert the
 *     caller is the owner thread.
 *   - GetComponent()/HasComponent() are LOCK-FREE reads. They are safe without a
 *     lock because the only writer (the owner thread) never mutates concurrently
 *     with a parallel read region: every parallel region in the engine is
 *     fork-join (RenderSystem's animation-sample and shadow-record passes), so
 *     the owner is parked at the join while worker threads read. Removing the
 *     per-call lock eliminated thousands of uncontended lock/unlock pairs per
 *     frame in the component hot path. If a future job system needs to mutate
 *     components off the owner thread, this invariant no longer holds —
 *     AssertOwnerThread() catches it in every build (debug aborts at the call;
 *     release logs a loud error once). Structural changes must stay on the owner
 *     thread; worker threads may only read.
 *   - DestroyEntity is deferred — entities are queued and destroyed at the start
 *     of Update() to prevent iterator invalidation during system iteration.
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

    /**
     * @brief True if any entity is queued for deferred destruction this frame.
     *
     * Lock-free read, safe under adr-0004: the pending set is mutated only on
     * the owner thread, and every parallel region is fork-join. Hot per-frame
     * loops use this to skip per-entity IsValid() (which takes the world
     * mutex) in the common no-destructions frame.
     */
    bool HasPendingDestructions() const { return !m_PendingDestructionSet.empty(); }

    // Component management
    template<typename T>
    T& AddComponent(Entity entity, const T& component = T{}) {
        AssertOwnerThread();
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
        AssertOwnerThread();
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        auto storage = GetOrCreateStorage<T>();
        if (storage->Has(entity)) {
            storage->Remove(entity);
            m_SystemManager->OnEntityRemoved(entity);
        }
    }

    template<typename T>
    T* GetComponent(Entity entity) {
        // Lock-free read (adr-0004): structural mutation is owner-thread-only and
        // parallel read regions are fork-join, so no writer ever overlaps this read.
        auto* storage = GetStorageMut<T>();
        if (!storage) return nullptr;
        return storage->Get(entity);
    }

    template<typename T>
    const T* GetComponent(Entity entity) const {
        // Lock-free read (adr-0004): structural mutation is owner-thread-only and
        // parallel read regions are fork-join, so no writer ever overlaps this read.
        auto storage = GetStorage<T>();
        if (!storage) {
            return nullptr;
        }
        return storage->Get(entity);
    }

    template<typename T>
    bool HasComponent(Entity entity) const {
        // Lock-free read (adr-0004): structural mutation is owner-thread-only and
        // parallel read regions are fork-join, so no writer ever overlaps this read.
        auto storage = GetStorage<T>();
        if (!storage) {
            return false;
        }
        return storage->Has(entity);
    }

    /**
     * @brief Get direct access to a component storage for batch operations
     *
     * Returns the underlying ComponentStorage<T> pointer, bypassing the per-entity
     * hash map lookup in GetComponent<T>(). Callers can then use storage->Get(entity)
     * for O(1) lookups without the type-ID hash map indirection on each call.
     *
     * @tparam T The component type
     * @return Pointer to the ComponentStorage<T>, or nullptr if no entities have this component
     */
    template<typename T>
    ComponentStorage<T>* GetComponentStorage() {
        return GetStorageMut<T>();
    }

    template<typename T>
    const ComponentStorage<T>* GetComponentStorage() const {
        return GetStorage<T>();
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
    /**
     * @brief Fill a caller-provided vector with entities that have both component types.
     *
     * Same intersection as the returning overload, but writes into @p out (cleared first) so a
     * per-frame caller can reuse one buffer instead of heap-allocating a fresh vector each call.
     */
    template<typename T1, typename T2>
    void GetEntitiesWithComponents(std::vector<Entity>& out) const {
        out.clear();
        auto* s1 = GetStorage<T1>();
        auto* s2 = GetStorage<T2>();
        if (!s1 || !s2) return;
        if (s1->Size() <= s2->Size()) {
            for (Entity e : s1->GetEntities()) { if (s2->Has(e)) out.push_back(e); }
        } else {
            for (Entity e : s2->GetEntities()) { if (s1->Has(e)) out.push_back(e); }
        }
    }

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
     * @brief Rename an entity. THE way to rename -- see FindEntityByName.
     *
     * Adds a NameComponent if the entity has none. Writing NameComponent::name
     * directly still works, but the name cache cannot see it, so a lookup by
     * the new name will miss until something structural forces a rebuild.
     */
    void SetEntityName(Entity e, const std::string& name);

    /**
     * @brief Invalidate the name cache.
     *
     * Almost never needed: FindEntityByName derives its own validity from the
     * NameComponent storage, so adds, removes and Clear are all handled without
     * anyone calling this. It remains for a caller that has written
     * NameComponent::name directly instead of using SetEntityName.
     */
    void InvalidateNameCache() { m_NameCacheDirty = true; }

    /**
     * @brief Observe every entity destruction just BEFORE its components are removed.
     *
     * The callback fires inside DestroyEntityInternal, the single choke point for ALL
     * destruction (deferred flush and DestroyEntityImmediate alike), while the entity's
     * data is still fully intact. It runs on the owner thread.
     *
     * Two systems need this and they must not have to know about each other, so
     * it is a list: PlayMode serializes each pre-play entity the moment it dies
     * so Stop can recreate exactly what was destroyed, and ScriptSystem runs the
     * script teardown (OnDestroy, instance release, coroutines, listeners) that
     * otherwise only ever ran at Stop. It was a single slot, PlayMode held it,
     * and every despawn during play leaked its AngelScript object.
     *
     * Keep observers cheap and read-only; do not mutate the World from inside one.
     *
     * @return A token to pass to RemoveEntityDestroyObserver. Registering is the
     *         caller's to undo -- an observer that outlives its owner is a
     *         dangling capture.
     */
    using EntityDestroyObserver = std::function<void(Entity)>;
    using DestroyObserverToken = u32;

    DestroyObserverToken AddEntityDestroyObserver(EntityDestroyObserver observer) {
        const DestroyObserverToken token = ++m_NextDestroyObserverToken;
        m_DestroyObservers.push_back({token, std::move(observer)});
        return token;
    }
    void RemoveEntityDestroyObserver(DestroyObserverToken token) {
        for (usize i = 0; i < m_DestroyObservers.size(); ++i) {
            if (m_DestroyObservers[i].token == token) {
                m_DestroyObservers.erase(m_DestroyObservers.begin() + static_cast<ssize>(i));
                return;
            }
        }
    }

    /**
     * @brief Storage epoch — incremented every time Clear() destroys the
     * component storages. Systems caching raw ComponentStorage pointers MUST
     * compare this against the epoch captured at refetch time before using a
     * cached pointer; a mismatch means every cached pointer is dangling
     * (scene reload, play-stop full restore, template apply).
     */
    u32 GetStorageEpoch() const { return m_StorageEpoch; }

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

    // --- ECS introspection (for the debug workstation) ---------------------------
    // Number of distinct component TYPES that currently have storage allocated.
    usize GetComponentStorageCount() const { return m_ComponentStorages.size(); }

    // Total live component INSTANCES across every storage (sum of per-type counts).
    usize GetTotalComponentCount() const {
        usize total = 0;
        for (const auto& [id, storage] : m_ComponentStorages) {
            if (storage) total += storage->Size();
        }
        return total;
    }

    // Fill out with (component name, live count) for every non-empty storage. Unsorted.
    void GetComponentStats(std::vector<std::pair<const char*, usize>>& out) const {
        out.clear();
        out.reserve(m_ComponentStorages.size());
        for (const auto& [id, storage] : m_ComponentStorages) {
            if (storage && storage->Size() > 0)
                out.emplace_back(storage->TypeName(), storage->Size());
        }
    }

    /**
     * @brief Lock the world for external batch operations (prevents structural modifications)
     * Use sparingly — prefer DestroyEntity (deferred) over DestroyEntityImmediate
     */
    void Lock() { m_Mutex.lock(); }
    void Unlock() { m_Mutex.unlock(); }

    /**
     * @brief Re-designate the calling thread as the structural-mutation owner (adr-0004).
     *
     * The owner thread is captured at construction. Call this only if the World is
     * created on one thread and then driven on another (e.g. a loader hands the World
     * off to the main thread). Compiles to a trivial store; only the debug asserts in
     * AssertOwnerThread() consult it.
     */
    void AdoptOwnerThread() { m_OwnerThreadId = std::this_thread::get_id(); }

private:
    void RebuildNameCache();

    // Invariant guard (adr-0004): every structural mutation
    // (Create/Destroy/Add/Remove/Clear + the deferred flush) must run on the owner
    // thread. This is what legalizes the lock-free GetComponent/HasComponent reads —
    // writes never happen off the owner thread, and parallel read regions are
    // fork-join (the owner is parked at the join), so no write overlaps a worker read.
    //
    // The thread check runs in ALL builds (mutation is the rare, non-hot path, so a
    // thread-id compare here is free relative to a frame). Debug builds hard-stop at
    // the offending call via ENJIN_ASSERT; release builds log a loud error once and
    // keep going, so even a shipped build surfaces a broken invariant instead of
    // silently racing the lock-free readers. The reads themselves stay unchecked and
    // lock-free.
    void AssertOwnerThread() const {
        if (std::this_thread::get_id() == m_OwnerThreadId) return;
        ENJIN_ASSERT(false,
            "World structural mutation off the owner thread — breaks the lock-free "
            "GetComponent/HasComponent invariant (adr-0004)");
#ifndef ENJIN_BUILD_DEBUG
        if (!m_OwnerThreadWarned.exchange(true)) {
            ENJIN_LOG_ERROR(Core,
                "World structural mutation off the owner thread — this breaks the "
                "lock-free ECS read invariant (adr-0004). Structural changes "
                "(Add/Remove/Create/Destroy/Clear) must run on the main thread; "
                "worker threads may only read components.");
        }
#endif
    }

    void DestroyEntityInternal(Entity entity);

    // Type-erased component storage wrapper
    struct StorageBase {
        virtual ~StorageBase() = default;
        virtual void Remove(Entity entity) = 0;
        virtual bool Has(Entity entity) const = 0;
        virtual usize Size() const = 0;
        virtual const std::vector<Entity>& GetEntities() const = 0;
        // Short component name (e.g. "MeshComponent") for the ECS debug panel.
        virtual const char* TypeName() const = 0;
    };

    // Strip the compiler's decorated type name down to the bare class name:
    // "struct Enjin::ECS::MeshComponent" -> "MeshComponent".
    static std::string CleanTypeName(const char* raw) {
        std::string s = raw ? raw : "";
        usize colon = s.rfind(':');
        if (colon != std::string::npos) s = s.substr(colon + 1);
        else if (s.rfind("struct ", 0) == 0) s = s.substr(7);
        else if (s.rfind("class ", 0) == 0) s = s.substr(6);
        return s;
    }

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
        const char* TypeName() const override {
            static const std::string s = CleanTypeName(typeid(T).name());
            return s.c_str();
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
    ComponentStorage<T>* GetStorageMut() {
        ComponentTypeId typeId = ComponentRegistry::GetTypeId<T>();
        auto it = m_ComponentStorages.find(typeId);
        if (it == m_ComponentStorages.end()) {
            return nullptr;
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

    // Name cache for O(1) entity lookup by name.
    //
    // This used to be invalidated by hand, and it was maintained at 3 of the
    // 210 sites that add a NameComponent -- so once the cache had been built,
    // anything spawned afterwards (a prefab instance, for one) was invisible to
    // FindEntityByName, and a rename left a stale entry. It read as
    // intermittent because any entity destroyed that frame masked it.
    //
    // Validity is now derived from state the engine already owns rather than
    // from anyone remembering: the storage epoch catches Clear(), and the
    // NameComponent entity count catches every add and remove. Renames go
    // through SetEntityName, which patches the cache in place.
    std::unordered_map<std::string, Entity> m_NameCache;
    bool m_NameCacheDirty = true;
    u32 m_NameCacheEpoch = 0;
    usize m_NameCacheCount = 0;

    // Bumped by Clear() -- see GetStorageEpoch()
    u32 m_StorageEpoch = 1;

    // Thread safety: guards structural modifications (Create/Destroy/Add/Remove/Clear).
    // Reads (GetComponent/HasComponent) are lock-free — see the class-level thread
    // safety note and adr-0004.
    mutable std::recursive_mutex m_Mutex;

    // The one thread allowed to perform structural mutation (adr-0004). Captured at
    // construction, overridable via AdoptOwnerThread(). Consulted by AssertOwnerThread()
    // on every mutation in all build configs (the write path is not the hot path).
    std::thread::id m_OwnerThreadId;

    // Latches the first off-owner-thread mutation in release builds so the error is
    // logged once, not every frame. Debug builds abort instead, so this is unused there.
    mutable std::atomic<bool> m_OwnerThreadWarned{false};

    // Deferred entity destruction queue (flushed at start of Update)
    std::vector<Entity> m_PendingDestructions;
    std::unordered_set<Entity> m_PendingDestructionSet;  // O(1) lookup companion

    // Observers fired just before an entity's components are removed. See
    // AddEntityDestroyObserver(). Empty when nothing is watching.
    struct DestroyObserverEntry {
        DestroyObserverToken token;
        EntityDestroyObserver fn;
    };
    std::vector<DestroyObserverEntry> m_DestroyObservers;
    DestroyObserverToken m_NextDestroyObserverToken = 0;
};

} // namespace ECS
} // namespace Enjin
