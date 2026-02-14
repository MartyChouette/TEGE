#include "Enjin/ECS/Systems/StateMachineSystem.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include <algorithm>

namespace Enjin {
namespace ECS {

void StateMachineSystem::InvokeCallback(World* world, Entity entity, const std::string& methodName) {
    if (!m_ScriptEngine || methodName.empty()) return;

    auto* scriptComp = world->GetComponent<ScriptComponent>(entity);
    if (!scriptComp) return;

    for (auto& attachment : scriptComp->scripts) {
        if (!attachment.enabled || !attachment.instance) continue;

        auto* obj = static_cast<asIScriptObject*>(attachment.instance);
        auto* func = m_ScriptEngine->FindMethod(obj, "void " + methodName + "()");
        if (!func) continue;

        auto* ctx = m_ScriptEngine->AcquireContext();
        if (ctx) {
            m_ScriptEngine->ExecuteMethod(ctx, obj, func);
            m_ScriptEngine->ReturnContext(ctx);
        }
    }
}

void StateMachineSystem::Update(World* world, f32 deltaTime) {
    if (!m_Enabled || !world) return;

    auto entities = world->GetEntitiesWithComponent<StateMachineComponent>();
    for (auto entity : entities) {
        auto* sm = world->GetComponent<StateMachineComponent>(entity);
        if (!sm) continue;

        // Auto-initialize: if currentState is empty and states exist, use first state
        if (sm->currentState.empty() && !sm->states.empty()) {
            sm->currentState = sm->states[0].name;
            sm->stateTime = 0.0f;

            // Fire onEnter for the initial state
            if (!sm->states[0].onEnter.empty()) {
                InvokeCallback(world, entity, sm->states[0].onEnter);
            }
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

                // Fire onExit for the old state
                if (!current->onExit.empty()) {
                    InvokeCallback(world, entity, current->onExit);
                }

                // Transition
                sm->previousState = sm->currentState;
                sm->currentState = transition.toState;
                sm->stateTime = 0.0f;
                transitioned = true;

                // Fire onEnter for the new state
                for (const auto& s : sm->states) {
                    if (s.name == sm->currentState) {
                        if (!s.onEnter.empty()) {
                            InvokeCallback(world, entity, s.onEnter);
                        }
                        break;
                    }
                }

                break;
            }
        }

        if (!transitioned) {
            sm->stateTime += deltaTime;

            // Fire onUpdate for the current state each frame
            if (!current->onUpdate.empty()) {
                InvokeCallback(world, entity, current->onUpdate);
            }
        }
    }
}

} // namespace ECS
} // namespace Enjin
