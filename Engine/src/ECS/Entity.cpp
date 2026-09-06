#include "Enjin/ECS/Entity.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <algorithm>

namespace Enjin {
namespace ECS {

EntityManager::EntityManager() {
    // Reserve slot 0 for INVALID_ENTITY
    m_Generations.push_back(0);
    m_Alive.push_back(0);
}

EntityManager::~EntityManager() {
}

Entity EntityManager::CreateEntity() {
    u32 index;
    u32 generation;

    if (!m_FreeIndices.empty()) {
        // Recycle a freed index with incremented generation
        index = m_FreeIndices.back();
        m_FreeIndices.pop_back();
        generation = m_Generations[index]; // Already incremented on destroy
    } else {
        // Allocate a new index
        index = m_NextIndex++;
        if (index >= static_cast<u32>(m_Generations.size())) {
            m_Generations.resize(index + 1, 0);
            m_Alive.resize(index + 1, 0);
        }
        generation = m_Generations[index]; // 0 for fresh slots
    }

    m_Alive[index] = 1;
    Entity entity = MakeEntity(index, generation);
    m_ActiveEntities.push_back(entity);
    return entity;
}

void EntityManager::DestroyEntity(Entity entity) {
    if (entity == INVALID_ENTITY) return;

    u32 index = EntityIndex(entity);
    u32 generation = EntityGeneration(entity);

    // Validate
    if (index >= static_cast<u32>(m_Generations.size())) return;
    if (m_Generations[index] != generation) return; // Stale entity
    if (!m_Alive[index]) return; // Already dead

    // Mark dead and increment generation (so stale handles become invalid)
    m_Alive[index] = 0;
    m_Generations[index]++;

    // Swap-and-pop from active list
    auto it = std::find(m_ActiveEntities.begin(), m_ActiveEntities.end(), entity);
    if (it != m_ActiveEntities.end()) {
        *it = m_ActiveEntities.back();
        m_ActiveEntities.pop_back();
    }

    m_FreeIndices.push_back(index);
}

bool EntityManager::IsValid(Entity entity) const {
    if (entity == INVALID_ENTITY) return false;

    u32 index = EntityIndex(entity);
    if (index >= static_cast<u32>(m_Generations.size())) return false;

    return m_Alive[index] && m_Generations[index] == EntityGeneration(entity);
}

void EntityManager::Reset() {
    // Bump every live slot's generation instead of clearing the table.
    //
    // Clearing it restarted at index 1 / generation 0, which is bit-identical
    // to the first entity of the PREVIOUS scene — so any Entity held across a
    // scene load (an editor selection, a script's cached handle, a physics
    // body map key, a quest or save reference) silently resolved to an
    // unrelated entity in the new scene, and IsValid() said yes. The
    // generation counter exists to make that impossible; Reset() was the one
    // place that threw it away. World::Clear() already protects cached storage
    // POINTERS with the storage epoch and protected nothing about handles.
    //
    // The table only grows (it is sized by the highest entity index ever
    // used, at 5 bytes per slot). That is the cost of keeping handles
    // distinguishable across loads, and it is worth it.
    for (usize i = 0; i < m_Generations.size(); ++i) {
        if (m_Alive[i]) {
            m_Generations[i]++;
            m_Alive[i] = 0;
        }
    }

    m_ActiveEntities.clear();
    m_FreeIndices.clear();
    m_NextIndex = 1;

    // Slot 0 stays reserved for INVALID_ENTITY. It is never alive, so its
    // generation is never bumped and MakeEntity(0, 0) == INVALID_ENTITY holds.
    if (m_Generations.empty()) {
        m_Generations.push_back(0);
        m_Alive.push_back(0);
    }
}

} // namespace ECS
} // namespace Enjin
