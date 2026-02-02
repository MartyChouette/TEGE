#include "Enjin/Gameplay/ObjectPool.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Gameplay {

void ObjectPool::CreatePool(const std::string& poolId, ECS::World* world,
                            u32 initialSize, std::function<void(ECS::Entity)> setupFunc) {
    if (!world) return;

    Pool pool;
    pool.setupFunc = setupFunc;

    for (u32 i = 0; i < initialSize; ++i) {
        ECS::Entity entity = world->CreateEntity();
        if (setupFunc) setupFunc(entity);

        // Mark as pooled but inactive
        auto* poolable = world->GetComponent<ECS::PoolableComponent>(entity);
        if (!poolable) {
            ECS::PoolableComponent pc;
            pc.poolId = poolId;
            pc.isActive = false;
            world->AddComponent<ECS::PoolableComponent>(entity, pc);
        } else {
            poolable->poolId = poolId;
            poolable->isActive = false;
        }

        pool.available.push_back(entity);
    }

    m_Pools[poolId] = std::move(pool);
    ENJIN_LOG_INFO(Editor, "Created object pool '%s' with %u entities", poolId.c_str(), initialSize);
}

ECS::Entity ObjectPool::Acquire(const std::string& poolId) {
    auto it = m_Pools.find(poolId);
    if (it == m_Pools.end() || it->second.available.empty()) {
        return ECS::INVALID_ENTITY;
    }

    ECS::Entity entity = it->second.available.back();
    it->second.available.pop_back();
    it->second.inUse.push_back(entity);
    return entity;
}

void ObjectPool::Release(const std::string& poolId, ECS::Entity entity) {
    auto it = m_Pools.find(poolId);
    if (it == m_Pools.end()) return;

    auto& inUse = it->second.inUse;
    for (auto iu = inUse.begin(); iu != inUse.end(); ++iu) {
        if (*iu == entity) {
            inUse.erase(iu);
            it->second.available.push_back(entity);
            return;
        }
    }
}

void ObjectPool::Update(ECS::World* world, f32 deltaTime) {
    if (!world) return;

    for (auto& [poolId, pool] : m_Pools) {
        // Check in-use entities for lifetime expiry
        for (auto it = pool.inUse.begin(); it != pool.inUse.end(); ) {
            auto* poolable = world->GetComponent<ECS::PoolableComponent>(*it);
            if (poolable && poolable->isActive && poolable->lifetime > 0.0f) {
                poolable->activeTime += deltaTime;
                if (poolable->activeTime >= poolable->lifetime) {
                    poolable->isActive = false;
                    poolable->activeTime = 0.0f;
                    pool.available.push_back(*it);
                    it = pool.inUse.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }
}

void ObjectPool::DestroyPool(const std::string& poolId, ECS::World* world) {
    auto it = m_Pools.find(poolId);
    if (it == m_Pools.end()) return;

    if (world) {
        for (auto entity : it->second.available) world->DestroyEntity(entity);
        for (auto entity : it->second.inUse) world->DestroyEntity(entity);
    }

    m_Pools.erase(it);
}

void ObjectPool::DestroyAll(ECS::World* world) {
    if (world) {
        for (auto& [id, pool] : m_Pools) {
            for (auto entity : pool.available) world->DestroyEntity(entity);
            for (auto entity : pool.inUse) world->DestroyEntity(entity);
        }
    }
    m_Pools.clear();
}

} // namespace Gameplay
} // namespace Enjin
