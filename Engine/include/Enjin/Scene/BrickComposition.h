#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace Enjin {

namespace ECS { class World; }

namespace Scene {

// Brick layer operation — what a brick does to the layers below it
enum class BrickOperation : u8 {
    Additive,    // Add new entities (default)
    Override,    // Replace specific entities/components in lower layers
    Remove       // Remove specific entities from lower layers
};

// An override entry — identifies an entity by name and specifies changes
struct BrickOverride {
    std::string entityName;           // Target entity in lower layers
    std::string componentType;        // Which component to override (empty = remove entity)
    std::string propertyKey;          // Which property (empty = replace entire component)
    std::string valueJson;            // New value as JSON string
};

// A brick — a composable layer of scene content.
// Inspired by IO Interactive's Glacier engine brick system.
//
// Bricks stack: base location → event layer → mission layer → variation layer.
// Higher layers can add, override, or remove content from lower layers.
// This enables:
//   - Mission-specific modifications without duplicating scenes
//   - Elusive target variations on the same location
//   - Time-of-day / weather variants
//   - Multiplayer mode overrides
struct Brick {
    std::string brickId;
    std::string scenePath;            // Path to scene file for additive entities
    i32 layerIndex = 0;              // Stacking order (0 = base, higher = on top)
    BrickOperation operation = BrickOperation::Additive;
    std::vector<BrickOverride> overrides;  // For Override/Remove operations

    // Runtime state
    bool loaded = false;
    std::vector<ECS::Entity> entities;     // Entities owned by this brick
    std::unordered_set<std::string> removedEntities;  // Names removed from lower layers
};

// Brick composition manager — stacks and resolves layered scene content.
//
// Usage:
//   1. Register bricks with AddBrick() — order doesn't matter
//   2. Call Compose() to apply all bricks in layer order
//   3. Call Decompose() to remove a specific brick's contributions
//
// The composition is live: adding/removing bricks at runtime triggers
// incremental updates (no full scene reload).
class ENJIN_API BrickComposition {
public:
    BrickComposition() = default;

    void SetWorld(ECS::World* world) { m_World = world; }

    // Register a brick (does not load it yet)
    void AddBrick(const Brick& brick);
    void RemoveBrick(const std::string& brickId);
    void ClearBricks();

    // Apply all registered bricks in layer order.
    // Additive bricks: load scene file additively.
    // Override bricks: modify existing entities/components.
    // Remove bricks: destroy named entities.
    void Compose();

    // Remove a specific brick's contributions (entities it added, overrides it applied)
    void Decompose(const std::string& brickId);

    // Query
    const std::vector<Brick>& GetBricks() const { return m_Bricks; }
    const Brick* FindBrick(const std::string& brickId) const;
    u32 GetLoadedBrickCount() const;
    u32 GetTotalEntityCount() const;

    // Get the effective entity list (all bricks resolved, removals applied)
    std::vector<ECS::Entity> GetComposedEntities() const;

private:
    void ApplyBrick(Brick& brick);
    void ApplyOverrides(Brick& brick);
    void ApplyRemovals(Brick& brick);

    ECS::World* m_World = nullptr;
    std::vector<Brick> m_Bricks;

    // Track which entities were created by which brick (for Decompose)
    std::unordered_map<std::string, std::vector<ECS::Entity>> m_BrickEntities;
};

} // namespace Scene
} // namespace Enjin
