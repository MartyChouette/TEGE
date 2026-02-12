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

    // Validate parent exists (prevent stale entity references)
    if (parent != INVALID_ENTITY && !world->IsValid(parent)) return;

    // Prevent circular parent chains: walk up from parent to see if child is an ancestor
    if (parent != INVALID_ENTITY) {
        Entity check = parent;
        u32 depth = 0;
        while (check != INVALID_ENTITY && depth < 1000) {
            if (check == child) return;  // Would create cycle
            if (!world->HasComponent<ParentComponent>(check)) break;
            check = world->GetComponent<ParentComponent>(check)->parent;
            ++depth;
        }
    }

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

inline const std::vector<Entity>& GetChildren(World* world, Entity parent) {
    static const std::vector<Entity> empty;
    if (!world || parent == INVALID_ENTITY) return empty;
    auto* cc = world->GetComponent<ChildrenComponent>(parent);
    if (!cc) return empty;
    return cc->children;
}

inline Entity GetParent(World* world, Entity child) {
    if (!world || child == INVALID_ENTITY) return INVALID_ENTITY;
    auto* pc = world->GetComponent<ParentComponent>(child);
    if (!pc) return INVALID_ENTITY;
    return pc->parent;
}

inline bool HasParent(World* world, Entity child) {
    return GetParent(world, child) != INVALID_ENTITY;
}

inline bool IsRoot(World* world, Entity entity) {
    return !HasParent(world, entity);
}

} // namespace ECS
} // namespace Enjin
