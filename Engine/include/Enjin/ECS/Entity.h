#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vector>

namespace Enjin {
namespace ECS {

// Entity is a generation-packed 64-bit ID:
//   Upper 32 bits = generation counter (incremented on recycle)
//   Lower 32 bits = index into entity arrays
// This allows O(1) validity checking without a hash set.
using Entity = u64;

// Invalid entity ID
constexpr Entity INVALID_ENTITY = 0;

// Pack/unpack entity ID components
inline constexpr u32 EntityIndex(Entity e) { return static_cast<u32>(e & 0xFFFFFFFF); }
inline constexpr u32 EntityGeneration(Entity e) { return static_cast<u32>(e >> 32); }
inline constexpr Entity MakeEntity(u32 index, u32 generation) {
    return (static_cast<u64>(generation) << 32) | static_cast<u64>(index);
}

// Entity manager - creates and manages entity IDs
// Phase 4 optimization: generation-packed IDs replace unordered_set for O(1) validity.
// Target: ~8 bytes overhead per entity (down from ~56 bytes with hash set).
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
    // Generation array indexed by entity index — O(1) validity check.
    // m_Generations[index] stores the current valid generation for that slot.
    // An entity is valid iff EntityGeneration(entity) == m_Generations[EntityIndex(entity)].
    std::vector<u32> m_Generations;
    // Alive flag per index (avoids scanning generations for "active" list)
    std::vector<u8> m_Alive;

    std::vector<Entity> m_ActiveEntities;
    std::vector<u32> m_FreeIndices; // Recycled entity indices
    u32 m_NextIndex = 1;            // Next fresh index (0 = reserved for INVALID_ENTITY)
};

} // namespace ECS
} // namespace Enjin
