#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include <vector>
#include <algorithm>

namespace Enjin {
namespace ECS {

struct ParentComponent {
    Entity parent = INVALID_ENTITY;
};

struct ChildrenComponent {
    std::vector<Entity> children;
};

// Helper functions for parent-child relationships
inline void SetParent(World* world, Entity child, Entity parent) {
    if (!world || child == INVALID_ENTITY || child == parent) return;

    // Remove from old parent
    if (world->HasComponent<ParentComponent>(child)) {
        Entity oldParent = world->GetComponent<ParentComponent>(child)->parent;
        if (oldParent != INVALID_ENTITY && world->HasComponent<ChildrenComponent>(oldParent)) {
            auto& children = world->GetComponent<ChildrenComponent>(oldParent)->children;
            children.erase(std::remove(children.begin(), children.end(), child), children.end());
        }
    }

    if (parent == INVALID_ENTITY) {
        // Remove parent component
        if (world->HasComponent<ParentComponent>(child)) {
            world->RemoveComponent<ParentComponent>(child);
        }
        return;
    }

    // Set new parent
    if (!world->HasComponent<ParentComponent>(child)) {
        world->AddComponent<ParentComponent>(child);
    }
    world->GetComponent<ParentComponent>(child)->parent = parent;

    // Add to new parent's children
    if (!world->HasComponent<ChildrenComponent>(parent)) {
        world->AddComponent<ChildrenComponent>(parent);
    }
    auto& children = world->GetComponent<ChildrenComponent>(parent)->children;
    if (std::find(children.begin(), children.end(), child) == children.end()) {
        children.push_back(child);
    }
}

inline void RemoveParent(World* world, Entity child) {
    SetParent(world, child, INVALID_ENTITY);
}

inline std::vector<Entity> GetChildren(World* world, Entity parent) {
    if (!world || parent == INVALID_ENTITY) return {};
    if (!world->HasComponent<ChildrenComponent>(parent)) return {};
    return world->GetComponent<ChildrenComponent>(parent)->children;
}

inline Entity GetParent(World* world, Entity child) {
    if (!world || child == INVALID_ENTITY) return INVALID_ENTITY;
    if (!world->HasComponent<ParentComponent>(child)) return INVALID_ENTITY;
    return world->GetComponent<ParentComponent>(child)->parent;
}

inline bool HasParent(World* world, Entity child) {
    return GetParent(world, child) != INVALID_ENTITY;
}

inline bool IsRoot(World* world, Entity entity) {
    return !HasParent(world, entity);
}

} // namespace ECS
} // namespace Enjin
