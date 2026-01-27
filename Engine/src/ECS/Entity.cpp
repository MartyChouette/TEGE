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
    return entity;
}

void EntityManager::DestroyEntity(Entity entity) {
    auto it = std::find(m_ActiveEntities.begin(), m_ActiveEntities.end(), entity);
    if (it != m_ActiveEntities.end()) {
        m_ActiveEntities.erase(it);
        m_FreeEntities.push_back(entity);
    }
}

bool EntityManager::IsValid(Entity entity) const {
    if (entity == INVALID_ENTITY) return false;
    return std::find(m_ActiveEntities.begin(), m_ActiveEntities.end(), entity) != m_ActiveEntities.end();
}

void EntityManager::Reset() {
    m_NextEntity = 1;
    m_ActiveEntities.clear();
    m_FreeEntities.clear();
}

} // namespace ECS
} // namespace Enjin
