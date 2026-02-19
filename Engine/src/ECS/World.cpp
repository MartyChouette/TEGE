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
}

World::~World() {
    Clear();
}

Entity World::CreateEntity() {
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    return m_EntityManager.CreateEntity();
}

void World::DestroyEntity(Entity entity) {
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
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    DestroyEntityInternal(entity);
}

void World::DestroyEntityInternal(Entity entity) {
    if (!m_EntityManager.IsValid(entity)) {
        return;
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
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_PendingDestructions.clear();
    m_PendingDestructionSet.clear();
    m_ComponentStorages.clear();
    m_EntityManager.Reset();
    m_NameCache.clear();
    m_NameCacheDirty = true;
}

void World::RebuildNameCache() {
    m_NameCache.clear();
    for (Entity e : GetEntitiesWithComponent<NameComponent>()) {
        auto* nc = GetComponent<NameComponent>(e);
        if (nc && !nc->name.empty()) {
            m_NameCache[nc->name] = e;
        }
    }
    m_NameCacheDirty = false;
}

Entity World::FindEntityByName(const std::string& name) {
    if (m_NameCacheDirty) {
        RebuildNameCache();
    }
    auto it = m_NameCache.find(name);
    if (it != m_NameCache.end()) {
        return it->second;
    }
    return INVALID_ENTITY;
}

} // namespace ECS
} // namespace Enjin
