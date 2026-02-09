#include "Enjin/ECS/Systems/BehaviorTreeSystem.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace ECS {

void BehaviorTreeSystem::Initialize() {
    if (!m_World) return;

    auto entities = m_World->GetEntitiesWithComponent<BehaviorTreeComponent>();
    for (auto entity : entities) {
        auto* bt = m_World->GetComponent<BehaviorTreeComponent>(entity);
        if (bt) {
            bt->ResetRuntimeState();
        }
    }

    ENJIN_LOG_INFO(Editor, "BehaviorTreeSystem initialized (%zu trees)",
                   entities.size());
}

void BehaviorTreeSystem::Shutdown() {
    if (!m_World) return;

    auto entities = m_World->GetEntitiesWithComponent<BehaviorTreeComponent>();
    for (auto entity : entities) {
        auto* bt = m_World->GetComponent<BehaviorTreeComponent>(entity);
        if (bt) {
            bt->ResetRuntimeState();
        }
    }
}

void BehaviorTreeSystem::Update(f32 deltaTime) {
    if (!m_World) return;

    auto entities = m_World->GetEntitiesWithComponent<BehaviorTreeComponent>();
    for (auto entity : entities) {
        auto* bt = m_World->GetComponent<BehaviorTreeComponent>(entity);
        if (!bt || !bt->enabled) continue;

        // Respect tick interval
        if (bt->tickInterval > 0.0f) {
            bt->tickTimer += deltaTime;
            if (bt->tickTimer < bt->tickInterval) continue;
            bt->tickTimer = 0.0f;
        }

        m_Executor.Tick(m_World, entity, *bt, deltaTime);
    }
}

} // namespace ECS
} // namespace Enjin
