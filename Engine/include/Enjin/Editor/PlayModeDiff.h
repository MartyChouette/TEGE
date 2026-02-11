#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS { class World; }
namespace Editor {

enum class DiffAction : u8 { Created, Deleted, Modified };

struct PropertyDiff {
    std::string name;       // "position.x", "baseColor", etc.
    std::string oldValue;   // JSON string
    std::string newValue;   // JSON string
    bool selected = false;  // User checkbox
};

struct ComponentDiff {
    std::string componentType;   // "TransformComponent", "MaterialComponent", etc.
    DiffAction action;           // Added, Removed, Modified
    std::vector<PropertyDiff> properties;
    bool selected = false;       // Component-level checkbox
    bool expanded = false;       // UI tree state
};

struct EntityDiff {
    ECS::Entity entity;
    std::string entityName;
    DiffAction action;           // Created, Deleted, Modified
    std::vector<ComponentDiff> components;
    bool selected = false;
    bool expanded = false;
    bool isPrefabInstance = false;
    std::string prefabPath;      // If prefab instance
};

struct PlayModeDiff {
    std::vector<EntityDiff> entities;

    bool HasChanges() const { return !entities.empty(); }

    u32 CountCreated() const {
        u32 n = 0;
        for (const auto& e : entities) if (e.action == DiffAction::Created) ++n;
        return n;
    }
    u32 CountDeleted() const {
        u32 n = 0;
        for (const auto& e : entities) if (e.action == DiffAction::Deleted) ++n;
        return n;
    }
    u32 CountModified() const {
        u32 n = 0;
        for (const auto& e : entities) if (e.action == DiffAction::Modified) ++n;
        return n;
    }
};

// Compute diff between pre-play JSON and current world state JSON
ENJIN_API PlayModeDiff ComputePlayModeDiff(const std::string& savedJson,
                                            const std::string& currentJson);

// Apply selected diffs to the restored world
ENJIN_API void ApplySelectedDiffs(ECS::World* world, const PlayModeDiff& diff,
                                   const std::string& currentJson);

} // namespace Editor
} // namespace Enjin
