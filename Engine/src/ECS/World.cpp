#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Logging/Log.h"

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
    return m_EntityManager.CreateEntity();
}

void World::DestroyEntity(Entity entity) {
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

bool World::IsValid(Entity entity) const {
    return m_EntityManager.IsValid(entity);
}

void World::Update(f32 deltaTime) {
    m_SystemManager->Update(deltaTime);
}

void World::Clear() {
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
