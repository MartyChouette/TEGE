#include "Enjin/ECS/Entity.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Core/Assert.h"
#include <algorithm>

namespace Enjin {
namespace ECS {

EntityManager::EntityManager() {
}

EntityManager::~EntityManager() {
}

Entity EntityManager::CreateEntity() {
    Entity entity;
    if (!m_FreeEntities.empty()) {
        entity = m_FreeEntities.back();
        m_FreeEntities.pop_back();
    } else {
        entity = m_NextEntity++;
        if (m_NextEntity == INVALID_ENTITY) {
            m_NextEntity = 1; // Wrap around (skip 0)
        }
    }
    m_ActiveEntities.push_back(entity);
    m_ActiveEntitySet.insert(entity);
    return entity;
}

void EntityManager::DestroyEntity(Entity entity) {
    if (m_ActiveEntitySet.erase(entity) == 0) return;
    // Swap-and-pop for O(1) removal from vector
    auto it = std::find(m_ActiveEntities.begin(), m_ActiveEntities.end(), entity);
    if (it != m_ActiveEntities.end()) {
        *it = m_ActiveEntities.back();
        m_ActiveEntities.pop_back();
    }
    m_FreeEntities.push_back(entity);
}

bool EntityManager::IsValid(Entity entity) const {
    if (entity == INVALID_ENTITY) return false;
    return m_ActiveEntitySet.count(entity) > 0;
}

void EntityManager::Reset() {
    m_NextEntity = 1;
    m_ActiveEntities.clear();
    m_ActiveEntitySet.clear();
    m_FreeEntities.clear();
}

} // namespace ECS
} // namespace Enjin
