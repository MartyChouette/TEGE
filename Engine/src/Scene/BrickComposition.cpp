#include "Enjin/Scene/BrickComposition.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>

namespace Enjin {
namespace Scene {

void BrickComposition::AddBrick(const Brick& brick) {
    // Check for duplicate
    for (const auto& existing : m_Bricks) {
        if (existing.brickId == brick.brickId) {
            ENJIN_LOG_WARN(Game, "Brick '%s' already registered", brick.brickId.c_str());
            return;
        }
    }
    m_Bricks.push_back(brick);

    // Keep sorted by layer index for deterministic composition order
    std::sort(m_Bricks.begin(), m_Bricks.end(),
              [](const Brick& a, const Brick& b) { return a.layerIndex < b.layerIndex; });

    ENJIN_LOG_INFO(Game, "Registered brick '%s' at layer %d (%s)",
                   brick.brickId.c_str(), brick.layerIndex,
                   brick.operation == BrickOperation::Additive ? "additive" :
                   brick.operation == BrickOperation::Override ? "override" : "remove");
}

void BrickComposition::RemoveBrick(const std::string& brickId) {
    Decompose(brickId);
    m_Bricks.erase(
        std::remove_if(m_Bricks.begin(), m_Bricks.end(),
                        [&](const Brick& b) { return b.brickId == brickId; }),
        m_Bricks.end());
}

void BrickComposition::ClearBricks() {
    for (auto& brick : m_Bricks) {
        if (brick.loaded) Decompose(brick.brickId);
    }
    m_Bricks.clear();
    m_BrickEntities.clear();
}

void BrickComposition::Compose() {
    if (!m_World) return;

    // Apply bricks in layer order (already sorted by AddBrick)
    for (auto& brick : m_Bricks) {
        if (!brick.loaded) {
            ApplyBrick(brick);
        }
    }
}

void BrickComposition::Decompose(const std::string& brickId) {
    if (!m_World) return;

    for (auto& brick : m_Bricks) {
        if (brick.brickId == brickId && brick.loaded) {
            // Destroy entities that this brick added
            auto it = m_BrickEntities.find(brickId);
            if (it != m_BrickEntities.end()) {
                for (ECS::Entity e : it->second) {
                    m_World->DestroyEntity(e);
                }
                m_BrickEntities.erase(it);
            }

            brick.entities.clear();
            brick.loaded = false;

            ENJIN_LOG_INFO(Game, "Decomposed brick '%s'", brickId.c_str());
            return;
        }
    }
}

const Brick* BrickComposition::FindBrick(const std::string& brickId) const {
    for (const auto& brick : m_Bricks) {
        if (brick.brickId == brickId) return &brick;
    }
    return nullptr;
}

u32 BrickComposition::GetLoadedBrickCount() const {
    u32 count = 0;
    for (const auto& b : m_Bricks) { if (b.loaded) count++; }
    return count;
}

u32 BrickComposition::GetTotalEntityCount() const {
    u32 count = 0;
    for (const auto& [id, entities] : m_BrickEntities) {
        count += static_cast<u32>(entities.size());
    }
    return count;
}

std::vector<ECS::Entity> BrickComposition::GetComposedEntities() const {
    std::vector<ECS::Entity> result;
    for (const auto& [id, entities] : m_BrickEntities) {
        result.insert(result.end(), entities.begin(), entities.end());
    }
    return result;
}

void BrickComposition::ApplyBrick(Brick& brick) {
    switch (brick.operation) {
    case BrickOperation::Additive: {
        // Load scene file additively
        if (!brick.scenePath.empty()) {
            SceneSerializer serializer(m_World);

            auto entitiesBefore = m_World->GetAllEntities();
            std::unordered_set<ECS::Entity> beforeSet(entitiesBefore.begin(), entitiesBefore.end());

            auto result = serializer.LoadAdditive(brick.scenePath);
            if (result.success) {
                auto entitiesAfter = m_World->GetAllEntities();
                std::vector<ECS::Entity> newEntities;
                for (auto e : entitiesAfter) {
                    if (beforeSet.find(e) == beforeSet.end()) {
                        newEntities.push_back(e);
                    }
                }
                brick.entities = newEntities;
                m_BrickEntities[brick.brickId] = std::move(newEntities);

                ENJIN_LOG_INFO(Game, "Brick '%s' added %u entities",
                               brick.brickId.c_str(), static_cast<u32>(brick.entities.size()));
            } else {
                ENJIN_LOG_ERROR(Game, "Failed to load brick '%s' scene: %s",
                                brick.brickId.c_str(), brick.scenePath.c_str());
            }
        }
        break;
    }

    case BrickOperation::Override:
        ApplyOverrides(brick);
        break;

    case BrickOperation::Remove:
        ApplyRemovals(brick);
        break;
    }

    brick.loaded = true;
}

void BrickComposition::ApplyOverrides(Brick& brick) {
    // Find named entities and modify their components
    for (const auto& override : brick.overrides) {
        ECS::Entity target = m_World->FindEntityByName(override.entityName);
        if (target == ECS::INVALID_ENTITY) {
            ENJIN_LOG_WARN(Game, "Brick '%s' override: entity '%s' not found",
                           brick.brickId.c_str(), override.entityName.c_str());
            continue;
        }

        // TODO: Apply component-level overrides via SceneSerializer's component
        // deserialization. For now, log the override intent.
        ENJIN_LOG_INFO(Game, "Brick '%s' override: entity '%s' component '%s'",
                       brick.brickId.c_str(), override.entityName.c_str(),
                       override.componentType.c_str());
    }
}

void BrickComposition::ApplyRemovals(Brick& brick) {
    // Find and destroy named entities from lower layers
    for (const auto& override : brick.overrides) {
        ECS::Entity target = m_World->FindEntityByName(override.entityName);
        if (target != ECS::INVALID_ENTITY) {
            m_World->DestroyEntity(target);
            brick.removedEntities.insert(override.entityName);
            ENJIN_LOG_INFO(Game, "Brick '%s' removed entity '%s'",
                           brick.brickId.c_str(), override.entityName.c_str());
        }
    }
}

} // namespace Scene
} // namespace Enjin
