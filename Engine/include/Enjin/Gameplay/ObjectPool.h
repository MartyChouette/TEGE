#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <string>

namespace Enjin {
namespace Gameplay {

class ENJIN_API ObjectPool {
public:
    // Create pool with N pre-allocated entities
    void CreatePool(const std::string& poolId, ECS::World* world,
                    u32 initialSize, std::function<void(ECS::Entity)> setupFunc);

    // Get entity from pool (returns INVALID_ENTITY if exhausted)
    ECS::Entity Acquire(const std::string& poolId);

    // Return entity to pool
    void Release(const std::string& poolId, ECS::Entity entity);

    // Update: auto-release expired entities
    void Update(ECS::World* world, f32 deltaTime);

    // Cleanup
    void DestroyPool(const std::string& poolId, ECS::World* world);
    void DestroyAll(ECS::World* world);

private:
    struct Pool {
        std::vector<ECS::Entity> available;
        std::vector<ECS::Entity> inUse;
        std::function<void(ECS::Entity)> setupFunc;
    };
    std::unordered_map<std::string, Pool> m_Pools;
};

} // namespace Gameplay
} // namespace Enjin
