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
            auto* pc = world->GetComponent<ParentComponent>(check);
            if (!pc) break;
            check = pc->parent;
            ++depth;
        }
    }

    // Remove from old parent
    {
        auto* oldPc = world->GetComponent<ParentComponent>(child);
        if (oldPc) {
            Entity oldParent = oldPc->parent;
            if (oldParent != INVALID_ENTITY) {
                auto* oldCc = world->GetComponent<ChildrenComponent>(oldParent);
                if (oldCc) {
                    oldCc->children.erase(std::remove(oldCc->children.begin(), oldCc->children.end(), child), oldCc->children.end());
                }
            }
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
    auto* childPc = world->GetComponent<ParentComponent>(child);
    if (!childPc) {
        world->AddComponent<ParentComponent>(child);
        childPc = world->GetComponent<ParentComponent>(child);
    }
    childPc->parent = parent;

    // Add to new parent's children
    auto* parentCc = world->GetComponent<ChildrenComponent>(parent);
    if (!parentCc) {
        world->AddComponent<ChildrenComponent>(parent);
        parentCc = world->GetComponent<ChildrenComponent>(parent);
    }
    auto& children = parentCc->children;
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
