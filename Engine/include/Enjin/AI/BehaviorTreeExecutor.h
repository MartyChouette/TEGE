#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <vector>

namespace Enjin {

// Forward declarations
namespace ECS {
    struct AIControllerComponent;
    struct TransformComponent;
}

namespace AI {

struct BTExecutionContext {
    ECS::World* world = nullptr;
    ECS::Entity entity = 0;
    ECS::BehaviorTreeComponent* tree = nullptr;
    ECS::AIControllerComponent* aiController = nullptr;
    ECS::TransformComponent* transform = nullptr;
    f32 deltaTime = 0.0f;
};

class ENJIN_API BehaviorTreeExecutor {
public:
    BehaviorTreeExecutor() = default;

    BTStatus Tick(ECS::World* world, ECS::Entity entity,
                  ECS::BehaviorTreeComponent& tree, f32 deltaTime);

private:
    BTStatus TickNode(BTExecutionContext& ctx, Editor::NodeId nodeId, i32 depth);

    // Get sorted child node IDs (by Y position, top-first)
    std::vector<Editor::NodeId> GetChildren(BTExecutionContext& ctx, Editor::NodeId nodeId);

    // Category dispatchers
    BTStatus TickComposite(BTExecutionContext& ctx, Editor::NodeId nodeId,
                           const BTNodeMeta& meta, i32 depth);
    BTStatus TickDecorator(BTExecutionContext& ctx, Editor::NodeId nodeId,
                           const BTNodeMeta& meta, i32 depth);
    BTStatus TickAction(BTExecutionContext& ctx, Editor::NodeId nodeId,
                        const BTNodeMeta& meta);
    BTStatus TickCondition(BTExecutionContext& ctx, Editor::NodeId nodeId,
                           const BTNodeMeta& meta);

    static constexpr i32 MAX_DEPTH = 64;
};

} // namespace AI
} // namespace Enjin
