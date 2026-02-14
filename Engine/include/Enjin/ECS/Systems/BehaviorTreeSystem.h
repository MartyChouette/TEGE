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

    // Enable/disable behavior tree updates
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    void SetWorld(World* world) { m_World = world; }

    void Initialize();
    void Shutdown();

    // ISystem interface
    void Update(f32 deltaTime) override;

private:
    World* m_World = nullptr;
    AI::BehaviorTreeExecutor m_Executor;
    bool m_Enabled = true;
};

} // namespace ECS
} // namespace Enjin
