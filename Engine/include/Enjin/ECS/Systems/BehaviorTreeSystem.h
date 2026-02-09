#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/System.h"
#include "Enjin/ECS/World.h"
#include "Enjin/AI/BehaviorTreeExecutor.h"

namespace Enjin {
namespace ECS {

class ENJIN_API BehaviorTreeSystem : public ISystem {
public:
    BehaviorTreeSystem() = default;
    ~BehaviorTreeSystem() override = default;

    void SetWorld(World* world) { m_World = world; }

    void Initialize();
    void Shutdown();

    // ISystem interface
    void Update(f32 deltaTime) override;

private:
    World* m_World = nullptr;
    AI::BehaviorTreeExecutor m_Executor;
};

} // namespace ECS
} // namespace Enjin
