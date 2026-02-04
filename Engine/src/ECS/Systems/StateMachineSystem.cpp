#include "Enjin/ECS/Systems/StateMachineSystem.h"
#include <algorithm>

namespace Enjin {
namespace ECS {

void StateMachineSystem::Update(World* world, f32 deltaTime) {
    if (!world) return;

    auto entities = world->GetEntitiesWithComponent<StateMachineComponent>();
    for (auto entity : entities) {
        auto* sm = world->GetComponent<StateMachineComponent>(entity);
        if (!sm) continue;

        // Auto-initialize: if currentState is empty and states exist, use first state
        if (sm->currentState.empty() && !sm->states.empty()) {
            sm->currentState = sm->states[0].name;
            sm->stateTime = 0.0f;
        }

        // Find current state definition
        const SMState* current = nullptr;
        for (const auto& s : sm->states) {
            if (s.name == sm->currentState) {
                current = &s;
                break;
            }
        }

        if (!current) {
            sm->stateTime += deltaTime;
            continue;
        }

        // Evaluate transitions (first match wins, order = priority)
        bool transitioned = false;
        for (const auto& transition : current->transitions) {
            bool allPass = true;
            for (const auto& cond : transition.conditions) {
                switch (cond.type) {
                case SMConditionType::BoolTrue: {
                    auto it = sm->boolParams.find(cond.paramName);
                    if (it == sm->boolParams.end() || !it->second) allPass = false;
                    break;
                }
                case SMConditionType::BoolFalse: {
                    auto it = sm->boolParams.find(cond.paramName);
                    if (it == sm->boolParams.end() || it->second) allPass = false;
                    break;
                }
                case SMConditionType::FloatGreater: {
                    auto it = sm->floatParams.find(cond.paramName);
                    if (it == sm->floatParams.end() || !(it->second > cond.threshold)) allPass = false;
                    break;
                }
                case SMConditionType::FloatLess: {
                    auto it = sm->floatParams.find(cond.paramName);
                    if (it == sm->floatParams.end() || !(it->second < cond.threshold)) allPass = false;
                    break;
                }
                case SMConditionType::IntEquals: {
                    auto it = sm->intParams.find(cond.paramName);
                    if (it == sm->intParams.end() || it->second != cond.intValue) allPass = false;
                    break;
                }
                case SMConditionType::IntNotEquals: {
                    auto it = sm->intParams.find(cond.paramName);
                    if (it == sm->intParams.end() || it->second == cond.intValue) allPass = false;
                    break;
                }
                case SMConditionType::Trigger: {
                    auto found = std::find(sm->activeTriggers.begin(), sm->activeTriggers.end(), cond.paramName);
                    if (found == sm->activeTriggers.end()) allPass = false;
                    break;
                }
                default:
                    allPass = false;
                    break;
                }

                if (!allPass) break;
            }

            if (allPass && !transition.conditions.empty()) {
                // Consume triggers used by this transition
                for (const auto& cond : transition.conditions) {
                    if (cond.type == SMConditionType::Trigger) {
                        auto found = std::find(sm->activeTriggers.begin(), sm->activeTriggers.end(), cond.paramName);
                        if (found != sm->activeTriggers.end()) {
                            sm->activeTriggers.erase(found);
                        }
                    }
                }

                // Transition
                sm->previousState = sm->currentState;
                sm->currentState = transition.toState;
                sm->stateTime = 0.0f;
                transitioned = true;
                break;
            }
        }

        if (!transitioned) {
            sm->stateTime += deltaTime;
        }
    }
}

} // namespace ECS
} // namespace Enjin
