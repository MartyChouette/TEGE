#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vector>

namespace Enjin {
namespace ECS {

// Entity is just an ID
using Entity = u64;

// Invalid entity ID
constexpr Entity INVALID_ENTITY = 0;

// Entity manager - creates and manages entity IDs
class ENJIN_API EntityManager {
public:
    EntityManager();
    ~EntityManager();

    Entity CreateEntity();
    void DestroyEntity(Entity entity);
    bool IsValid(Entity entity) const;

    void Reset(); // Destroy all entities

    const std::vector<Entity>& GetAllEntities() const { return m_ActiveEntities; }
    usize GetEntityCount() const { return m_ActiveEntities.size(); }

private:
    Entity m_NextEntity = 1;
    std::vector<Entity> m_ActiveEntities;
    std::vector<Entity> m_FreeEntities; // Recycled entity IDs
};

} // namespace ECS
} // namespace Enjin
