#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>

/**
 * @file World.cpp
 * @brief Implementation of World class
 * @author Enjin Engine Team
 * @date 2025
 */

namespace Enjin {
namespace ECS {

World::World() {
    m_SystemManager = std::make_unique<SystemManager>();
    // Capture the constructing thread as the structural-mutation owner (adr-0004).
    // Re-designate with AdoptOwnerThread() if the World is later driven elsewhere.
    m_OwnerThreadId = std::this_thread::get_id();
}

World::~World() {
    Clear();
}

Entity World::CreateEntity() {
    AssertOwnerThread();
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_EntityManager.CreateEntity();
}

void World::DestroyEntity(Entity entity) {
    AssertOwnerThread();
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (!m_EntityManager.IsValid(entity)) {
        return;
    }
    // Avoid duplicate queue entries
    if (m_PendingDestructionSet.count(entity)) return;
    m_PendingDestructions.push_back(entity);
    m_PendingDestructionSet.insert(entity);
}

void World::DestroyEntityImmediate(Entity entity) {
    AssertOwnerThread();
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    DestroyEntityInternal(entity);
}

void World::DestroyEntityInternal(Entity entity) {
    if (!m_EntityManager.IsValid(entity)) {
        return;
    }

    // Notify any observer while the entity's data is still fully intact (before the
    // hierarchy fix-up and component removal below). Single choke point for both the
    // deferred flush and DestroyEntityImmediate. See SetEntityDestroyObserver().
    if (m_DestroyObserver) {
        m_DestroyObserver(entity);
    }

    // Clean up hierarchy: reparent children to root
    if (HasComponent<ChildrenComponent>(entity)) {
        auto children = GetComponent<ChildrenComponent>(entity)->children; // copy
        for (Entity child : children) {
            if (m_EntityManager.IsValid(child) && HasComponent<ParentComponent>(child)) {
                GetComponent<ParentComponent>(child)->parent = INVALID_ENTITY;
                RemoveComponent<ParentComponent>(child);
            }
        }
    }

    // Remove from parent's children list
    if (HasComponent<ParentComponent>(entity)) {
        Entity parent = GetComponent<ParentComponent>(entity)->parent;
        if (parent != INVALID_ENTITY && m_EntityManager.IsValid(parent) &&
            HasComponent<ChildrenComponent>(parent)) {
            auto& siblings = GetComponent<ChildrenComponent>(parent)->children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
        }
    }

    m_SystemManager->OnEntityRemoved(entity);

    // Remove components from all storages
    for (auto& [typeId, storage] : m_ComponentStorages) {
        storage->Remove(entity);
    }

    m_EntityManager.DestroyEntity(entity);
    m_NameCacheDirty = true;
}

void World::FlushPendingDestructions() {
    AssertOwnerThread();
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (m_PendingDestructions.empty()) return;

    // Move to local to allow new destructions during flush
    std::vector<Entity> pending = std::move(m_PendingDestructions);
    m_PendingDestructions.clear();
    m_PendingDestructionSet.clear();

    for (Entity entity : pending) {
        DestroyEntityInternal(entity);
    }
}

bool World::IsValid(Entity entity) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (!m_EntityManager.IsValid(entity)) return false;
    // Entities queued for deferred destruction are not considered valid
    if (m_PendingDestructionSet.count(entity)) return false;
    return true;
}

bool World::IsPendingDestruction(Entity entity) const {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_PendingDestructionSet.count(entity) > 0;
}

void World::Update(f32 deltaTime) {
    // Flush deferred destructions before system updates
    FlushPendingDestructions();
    m_SystemManager->Update(deltaTime);
}

void World::Clear() {
    AssertOwnerThread();
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_PendingDestructions.clear();
    m_PendingDestructionSet.clear();
    m_ComponentStorages.clear();
    m_EntityManager.Reset();
    m_NameCache.clear();
    m_NameCacheDirty = true;
    // Every raw ComponentStorage* cached anywhere is now dangling.
    m_StorageEpoch++;
}

void World::RebuildNameCache() {
    m_NameCache.clear();
    const auto& named = GetEntitiesWithComponent<NameComponent>();
    for (Entity e : named) {
        auto* nc = GetComponent<NameComponent>(e);
        if (nc && !nc->name.empty()) {
            m_NameCache[nc->name] = e;
        }
    }
    m_NameCacheDirty = false;
    // Remember what the cache was built against, so the next lookup can tell
    // whether it is still describing the same world.
    m_NameCacheEpoch = m_StorageEpoch;
    m_NameCacheCount = named.size();
}

void World::SetEntityName(Entity e, const std::string& name) {
    AssertOwnerThread();
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    if (!IsValid(e)) return;

    auto* nc = GetComponent<NameComponent>(e);
    if (!nc) {
        // AddComponent changes the NameComponent count, which is what the next
        // lookup checks, so the cache repairs itself without a flag.
        NameComponent added;
        added.name = name;
        AddComponent<NameComponent>(e, added);
        return;
    }
    if (nc->name == name) return;

    // Patch the cache in place rather than dropping it. A rename is invisible
    // to the storage count, so this is the one mutation the cache cannot
    // detect on its own.
    if (!m_NameCacheDirty) {
        auto old = m_NameCache.find(nc->name);
        if (old != m_NameCache.end() && old->second == e) {
            m_NameCache.erase(old);
        }
        if (!name.empty()) {
            m_NameCache[name] = e;
        }
    }
    nc->name = name;
}

Entity World::FindEntityByName(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);

    // Derive validity instead of trusting anyone to have invalidated: the epoch
    // catches Clear(), the count catches every add and remove of a
    // NameComponent. Both are O(1).
    const usize namedCount = GetEntitiesWithComponent<NameComponent>().size();
    if (m_NameCacheDirty || m_NameCacheEpoch != m_StorageEpoch ||
        m_NameCacheCount != namedCount) {
        RebuildNameCache();
    }

    auto it = m_NameCache.find(name);
    if (it == m_NameCache.end()) return INVALID_ENTITY;

    // A cached entry can still be stale if someone wrote NameComponent::name
    // directly instead of calling SetEntityName. Confirm before handing it back;
    // this is O(1) and stops a lookup returning an entity that no longer holds
    // the name that was asked for.
    const Entity cached = it->second;
    if (!IsValid(cached)) {
        RebuildNameCache();
        auto again = m_NameCache.find(name);
        return again != m_NameCache.end() ? again->second : INVALID_ENTITY;
    }
    auto* nc = GetComponent<NameComponent>(cached);
    if (!nc || nc->name != name) {
        RebuildNameCache();
        auto again = m_NameCache.find(name);
        return again != m_NameCache.end() ? again->second : INVALID_ENTITY;
    }
    return cached;
}

} // namespace ECS
} // namespace Enjin
