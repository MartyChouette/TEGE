#include "Enjin/AI/BehaviorTreeExecutor.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Enjin {
namespace AI {

// ============================================================================
// Main Tick
// ============================================================================

BTStatus BehaviorTreeExecutor::Tick(ECS::World* world, ECS::Entity entity,
                                     ECS::BehaviorTreeComponent& tree, f32 deltaTime) {
    if (!tree.enabled || tree.rootNodeId == 0) {
        return BTStatus::Failure;
    }

    // Initialize blackboard from defaults on first tick
    if (!tree.initialized) {
        tree.blackboard.Clear();
        for (const auto& entry : tree.blackboardDefaults) {
            tree.blackboard.Set(entry.key, entry.value);
        }
        tree.initialized = true;
    }

    BTExecutionContext ctx;
    ctx.world = world;
    ctx.entity = entity;
    ctx.tree = &tree;
    ctx.aiController = world->GetComponent<ECS::AIControllerComponent>(entity);
    ctx.transform = world->GetComponent<ECS::TransformComponent>(entity);
    ctx.deltaTime = deltaTime;

    BTStatus result = TickNode(ctx, tree.rootNodeId, 0);
    return result;
}

// ============================================================================
// TickNode — dispatch by category
// ============================================================================

BTStatus BehaviorTreeExecutor::TickNode(BTExecutionContext& ctx, Editor::NodeId nodeId, i32 depth) {
    if (depth > MAX_DEPTH) {
        return BTStatus::Failure;
    }

    auto metaIt = ctx.tree->nodeMeta.find(nodeId);
    if (metaIt == ctx.tree->nodeMeta.end()) {
        return BTStatus::Failure;
    }

    const BTNodeMeta& meta = metaIt->second;
    BTNodeCategory category = GetNodeCategory(meta.nodeType);

    BTStatus result = BTStatus::Failure;

    switch (category) {
        case BTNodeCategory::Root: {
            auto children = GetChildren(ctx, nodeId);
            if (!children.empty()) {
                result = TickNode(ctx, children[0], depth + 1);
            }
            break;
        }
        case BTNodeCategory::Composite:
            result = TickComposite(ctx, nodeId, meta, depth);
            break;
        case BTNodeCategory::Decorator:
            result = TickDecorator(ctx, nodeId, meta, depth);
            break;
        case BTNodeCategory::Action:
            result = TickAction(ctx, nodeId, meta);
            break;
        case BTNodeCategory::Condition:
            result = TickCondition(ctx, nodeId, meta);
            break;
    }

    // Store result
    ctx.tree->nodeStatuses[nodeId] = result;
    ctx.tree->nodeMeta[nodeId].lastStatus = result;
    if (result == BTStatus::Running) {
        ctx.tree->activeNode = nodeId;
    }

    return result;
}

// ============================================================================
// Get Children (sorted by Y position, top-first)
// ============================================================================

std::vector<Editor::NodeId> BehaviorTreeExecutor::GetChildren(BTExecutionContext& ctx, Editor::NodeId nodeId) {
    std::vector<Editor::NodeId> children;

    auto* node = ctx.tree->graph.FindNode(nodeId);
    if (!node || node->outputs.empty()) return children;

    for (const auto& outPin : node->outputs) {
        auto links = ctx.tree->graph.GetLinksForPin(outPin.id);
        for (auto linkId : links) {
            auto* link = ctx.tree->graph.FindLink(linkId);
            if (!link) continue;

            Editor::PinId childPinId = (link->startPinId == outPin.id) ? link->endPinId : link->startPinId;
            Editor::NodeId childNodeId = ctx.tree->graph.GetPinOwner(childPinId);
            if (childNodeId != 0 && childNodeId != nodeId) {
                children.push_back(childNodeId);
            }
        }
    }

    std::sort(children.begin(), children.end(), [&](Editor::NodeId a, Editor::NodeId b) {
        auto* na = ctx.tree->graph.FindNode(a);
        auto* nb = ctx.tree->graph.FindNode(b);
        if (!na || !nb) return false;
        return na->position.y < nb->position.y;
    });

    return children;
}

// ============================================================================
// Composite Tick
// ============================================================================

BTStatus BehaviorTreeExecutor::TickComposite(BTExecutionContext& ctx, Editor::NodeId nodeId,
                                              const BTNodeMeta& meta, i32 depth) {
    auto children = GetChildren(ctx, nodeId);
    if (children.empty()) return BTStatus::Failure;

    switch (meta.nodeType) {
        case BTNodeType::Selector: {
            for (auto childId : children) {
                BTStatus childResult = TickNode(ctx, childId, depth + 1);
                if (childResult == BTStatus::Running || childResult == BTStatus::Success) {
                    return childResult;
                }
            }
            return BTStatus::Failure;
        }

        case BTNodeType::Sequence: {
            for (auto childId : children) {
                BTStatus childResult = TickNode(ctx, childId, depth + 1);
                if (childResult == BTStatus::Running || childResult == BTStatus::Failure) {
                    return childResult;
                }
            }
            return BTStatus::Success;
        }

        case BTNodeType::Parallel: {
            auto policyIt = meta.properties.find("policy");
            bool requireAll = (policyIt != meta.properties.end() && policyIt->second == "RequireAll");

            i32 successCount = 0;
            i32 failCount = 0;
            bool anyRunning = false;

            for (auto childId : children) {
                BTStatus childResult = TickNode(ctx, childId, depth + 1);
                if (childResult == BTStatus::Success) successCount++;
                else if (childResult == BTStatus::Failure) failCount++;
                else if (childResult == BTStatus::Running) anyRunning = true;
            }

            if (requireAll) {
                if (successCount == static_cast<i32>(children.size())) return BTStatus::Success;
                if (failCount > 0) return BTStatus::Failure;
            } else {
                if (successCount > 0) return BTStatus::Success;
                if (failCount == static_cast<i32>(children.size())) return BTStatus::Failure;
            }

            return anyRunning ? BTStatus::Running : BTStatus::Failure;
        }

        default:
            return BTStatus::Failure;
    }
}

// ============================================================================
// Decorator Tick
// ============================================================================

BTStatus BehaviorTreeExecutor::TickDecorator(BTExecutionContext& ctx, Editor::NodeId nodeId,
                                              const BTNodeMeta& meta, i32 depth) {
    auto children = GetChildren(ctx, nodeId);
    if (children.empty()) return BTStatus::Failure;

    Editor::NodeId childId = children[0];

    switch (meta.nodeType) {
        case BTNodeType::Inverter: {
            BTStatus childResult = TickNode(ctx, childId, depth + 1);
            if (childResult == BTStatus::Success) return BTStatus::Failure;
            if (childResult == BTStatus::Failure) return BTStatus::Success;
            return BTStatus::Running;
        }

        case BTNodeType::Repeater: {
            auto countIt = meta.properties.find("count");
            i32 maxCount = 0;
            if (countIt != meta.properties.end()) {
                maxCount = std::atoi(countIt->second.c_str());
            }

            auto& counter = ctx.tree->nodeCounters[nodeId];
            BTStatus childResult = TickNode(ctx, childId, depth + 1);

            if (childResult == BTStatus::Running) return BTStatus::Running;

            counter++;
            if (maxCount > 0 && counter >= maxCount) {
                counter = 0;
                return childResult;
            }
            return BTStatus::Running;
        }

        case BTNodeType::Timeout: {
            auto durationIt = meta.properties.find("duration");
            f32 duration = 5.0f;
            if (durationIt != meta.properties.end()) {
                duration = static_cast<f32>(std::atof(durationIt->second.c_str()));
            }

            auto& timer = ctx.tree->nodeTimers[nodeId];
            timer += ctx.deltaTime;

            if (timer >= duration) {
                timer = 0.0f;
                return BTStatus::Failure;
            }

            BTStatus childResult = TickNode(ctx, childId, depth + 1);
            if (childResult != BTStatus::Running) {
                timer = 0.0f;
            }
            return childResult;
        }

        case BTNodeType::ForceSuccess: {
            BTStatus childResult = TickNode(ctx, childId, depth + 1);
            if (childResult == BTStatus::Running) return BTStatus::Running;
            return BTStatus::Success;
        }

        case BTNodeType::ForceFailure: {
            BTStatus childResult = TickNode(ctx, childId, depth + 1);
            if (childResult == BTStatus::Running) return BTStatus::Running;
            return BTStatus::Failure;
        }

        default:
            return BTStatus::Failure;
    }
}

// ============================================================================
// Action Tick
// ============================================================================

static f32 DistanceBetween(const Math::Vector3& a, const Math::Vector3& b) {
    f32 dx = a.x - b.x;
    f32 dy = a.y - b.y;
    f32 dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

BTStatus BehaviorTreeExecutor::TickAction(BTExecutionContext& ctx, Editor::NodeId nodeId,
                                           const BTNodeMeta& meta) {
    switch (meta.nodeType) {
        case BTNodeType::MoveTo: {
            if (!ctx.aiController || !ctx.transform) return BTStatus::Failure;
            // Move towards target position using AI controller settings
            Math::Vector3 target = ctx.aiController->targetPosition;
            f32 dist = DistanceBetween(ctx.transform->position, target);
            if (dist <= ctx.aiController->stoppingDistance) {
                return BTStatus::Success;
            }
            // Skip movement if distance is near-zero to avoid divide-by-zero
            if (dist < 0.0001f) {
                return BTStatus::Running;
            }
            // Move toward target
            Math::Vector3 dir = Math::Vector3(
                target.x - ctx.transform->position.x,
                target.y - ctx.transform->position.y,
                target.z - ctx.transform->position.z
            );
            f32 invDist = 1.0f / dist;
            dir.x *= invDist;
            dir.y *= invDist;
            dir.z *= invDist;
            f32 speed = ctx.aiController->moveSpeed * ctx.deltaTime;
            ctx.transform->position.x += dir.x * speed;
            ctx.transform->position.y += dir.y * speed;
            ctx.transform->position.z += dir.z * speed;
            return BTStatus::Running;
        }

        case BTNodeType::Attack: {
            if (!ctx.aiController) return BTStatus::Failure;
            // Reset attack timer
            ctx.aiController->attackTimer = ctx.aiController->attackCooldown;
            return BTStatus::Success;
        }

        case BTNodeType::Patrol: {
            if (!ctx.aiController || !ctx.transform) return BTStatus::Failure;
            if (ctx.aiController->patrolPoints.empty()) return BTStatus::Failure;

            auto& ai = *ctx.aiController;
            Math::Vector3 target = ai.patrolPoints[ai.currentPatrolIndex];
            f32 dist = DistanceBetween(ctx.transform->position, target);

            if (dist <= ai.stoppingDistance) {
                // Advance to next patrol point
                ai.currentPatrolIndex = (ai.currentPatrolIndex + 1) % ai.patrolPoints.size();
            } else if (dist >= 0.0001f) {
                Math::Vector3 dir = Math::Vector3(
                    target.x - ctx.transform->position.x,
                    target.y - ctx.transform->position.y,
                    target.z - ctx.transform->position.z
                );
                f32 invDist = 1.0f / dist;
                dir.x *= invDist; dir.y *= invDist; dir.z *= invDist;
                f32 speed = ai.moveSpeed * ctx.deltaTime;
                ctx.transform->position.x += dir.x * speed;
                ctx.transform->position.y += dir.y * speed;
                ctx.transform->position.z += dir.z * speed;
            }
            return BTStatus::Running;
        }

        case BTNodeType::Wait: {
            auto durationIt = meta.properties.find("duration");
            f32 duration = 1.0f;
            if (durationIt != meta.properties.end()) {
                duration = static_cast<f32>(std::atof(durationIt->second.c_str()));
            }

            auto& timer = ctx.tree->nodeTimers[nodeId];
            timer += ctx.deltaTime;

            if (timer >= duration) {
                timer = 0.0f;
                return BTStatus::Success;
            }
            return BTStatus::Running;
        }

        case BTNodeType::PlayAnimation: {
            auto nameIt = meta.properties.find("animation");
            if (nameIt == meta.properties.end()) return BTStatus::Failure;

            auto* animator = ctx.world->GetComponent<ECS::AnimatorComponent>(ctx.entity);
            if (!animator) return BTStatus::Failure;

            animator->animator.Play(nameIt->second);
            return BTStatus::Success;
        }

        case BTNodeType::SetVariable: {
            auto keyIt = meta.properties.find("key");
            auto valueIt = meta.properties.find("value");
            auto typeIt = meta.properties.find("type");
            if (keyIt == meta.properties.end() || valueIt == meta.properties.end()) {
                return BTStatus::Failure;
            }

            std::string type = (typeIt != meta.properties.end()) ? typeIt->second : "string";
            if (type == "bool") {
                ctx.tree->blackboard.Set(keyIt->second, valueIt->second == "true");
            } else if (type == "int") {
                ctx.tree->blackboard.Set(keyIt->second, static_cast<i32>(std::atoi(valueIt->second.c_str())));
            } else if (type == "float") {
                ctx.tree->blackboard.Set(keyIt->second, static_cast<f32>(std::atof(valueIt->second.c_str())));
            } else {
                ctx.tree->blackboard.Set(keyIt->second, valueIt->second);
            }
            return BTStatus::Success;
        }

        default:
            return BTStatus::Failure;
    }
}

// ============================================================================
// Condition Tick
// ============================================================================

BTStatus BehaviorTreeExecutor::TickCondition(BTExecutionContext& ctx, Editor::NodeId nodeId,
                                              const BTNodeMeta& meta) {
    switch (meta.nodeType) {
        case BTNodeType::CanSeeTarget: {
            if (!ctx.aiController || !ctx.transform) return BTStatus::Failure;
            // Simple range + FOV check
            if (ctx.aiController->targetEntity == 0) return BTStatus::Failure;
            auto* targetTransform = ctx.world->GetComponent<ECS::TransformComponent>(ctx.aiController->targetEntity);
            if (!targetTransform) return BTStatus::Failure;

            f32 dist = DistanceBetween(ctx.transform->position, targetTransform->position);
            if (dist > ctx.aiController->detectionRange) return BTStatus::Failure;
            return BTStatus::Success;
        }

        case BTNodeType::IsInRange: {
            if (!ctx.aiController || !ctx.transform) return BTStatus::Failure;
            auto rangeIt = meta.properties.find("range");
            f32 range = ctx.aiController->attackRange;
            if (rangeIt != meta.properties.end()) {
                range = static_cast<f32>(std::atof(rangeIt->second.c_str()));
            }

            if (ctx.aiController->targetEntity == 0) return BTStatus::Failure;
            auto* targetTransform = ctx.world->GetComponent<ECS::TransformComponent>(ctx.aiController->targetEntity);
            if (!targetTransform) return BTStatus::Failure;

            f32 dist = DistanceBetween(ctx.transform->position, targetTransform->position);
            return (dist <= range) ? BTStatus::Success : BTStatus::Failure;
        }

        case BTNodeType::HasTarget: {
            if (!ctx.aiController) return BTStatus::Failure;
            return (ctx.aiController->targetEntity != 0) ? BTStatus::Success : BTStatus::Failure;
        }

        case BTNodeType::CheckVariable: {
            auto keyIt = meta.properties.find("key");
            auto opIt = meta.properties.find("operator");
            auto valueIt = meta.properties.find("value");
            if (keyIt == meta.properties.end()) return BTStatus::Failure;

            auto* bbVal = ctx.tree->blackboard.Get(keyIt->second);
            if (!bbVal) return BTStatus::Failure;

            std::string op = (opIt != meta.properties.end()) ? opIt->second : "==";
            std::string compareVal = (valueIt != meta.properties.end()) ? valueIt->second : "";

            if (auto* boolPtr = std::get_if<bool>(bbVal)) {
                bool expected = (compareVal == "true");
                if (op == "==") return (*boolPtr == expected) ? BTStatus::Success : BTStatus::Failure;
                if (op == "!=") return (*boolPtr != expected) ? BTStatus::Success : BTStatus::Failure;
            }

            if (auto* floatPtr = std::get_if<f32>(bbVal)) {
                f32 expected = static_cast<f32>(std::atof(compareVal.c_str()));
                if (op == "==") return (*floatPtr == expected) ? BTStatus::Success : BTStatus::Failure;
                if (op == "!=") return (*floatPtr != expected) ? BTStatus::Success : BTStatus::Failure;
                if (op == "<")  return (*floatPtr < expected)  ? BTStatus::Success : BTStatus::Failure;
                if (op == ">")  return (*floatPtr > expected)  ? BTStatus::Success : BTStatus::Failure;
                if (op == "<=") return (*floatPtr <= expected)  ? BTStatus::Success : BTStatus::Failure;
                if (op == ">=") return (*floatPtr >= expected)  ? BTStatus::Success : BTStatus::Failure;
            }

            if (auto* intPtr = std::get_if<i32>(bbVal)) {
                i32 expected = std::atoi(compareVal.c_str());
                if (op == "==") return (*intPtr == expected) ? BTStatus::Success : BTStatus::Failure;
                if (op == "!=") return (*intPtr != expected) ? BTStatus::Success : BTStatus::Failure;
                if (op == "<")  return (*intPtr < expected)  ? BTStatus::Success : BTStatus::Failure;
                if (op == ">")  return (*intPtr > expected)  ? BTStatus::Success : BTStatus::Failure;
                if (op == "<=") return (*intPtr <= expected)  ? BTStatus::Success : BTStatus::Failure;
                if (op == ">=") return (*intPtr >= expected)  ? BTStatus::Success : BTStatus::Failure;
            }

            if (auto* strPtr = std::get_if<std::string>(bbVal)) {
                if (op == "==") return (*strPtr == compareVal) ? BTStatus::Success : BTStatus::Failure;
                if (op == "!=") return (*strPtr != compareVal) ? BTStatus::Success : BTStatus::Failure;
            }

            return BTStatus::Failure;
        }

        case BTNodeType::IsAlive: {
            auto* health = ctx.world->GetComponent<ECS::HealthComponent>(ctx.entity);
            if (!health) return BTStatus::Success;
            return (health->currentHealth > 0.0f && !health->isDead) ? BTStatus::Success : BTStatus::Failure;
        }

        default:
            return BTStatus::Failure;
    }
}

} // namespace AI
} // namespace Enjin
