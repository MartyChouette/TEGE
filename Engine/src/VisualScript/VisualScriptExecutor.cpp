#include "Enjin/VisualScript/VisualScriptExecutor.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Logging/Log.h"
#include <chrono>

namespace Enjin {
namespace VisualScript {

// ============================================================================
// EVENT EXECUTION
// ============================================================================

void VisualScriptExecutor::ExecuteEvent(ECS::World* world, ECS::Entity entity,
                                          ECS::VisualScriptComponent* script,
                                          ECS::VisualScriptEvent event,
                                          f32 deltaTime,
                                          ECS::Entity otherEntity) {
    if (!script || !script->enabled) return;

    Editor::NodeId entryNode = script->GetEventNode(event);
    if (entryNode == 0) return;  // No handler for this event

    ExecuteFromNode(world, entity, script, entryNode, deltaTime, otherEntity);
}

void VisualScriptExecutor::ExecuteCustomEvent(ECS::World* world, ECS::Entity entity,
                                                ECS::VisualScriptComponent* script,
                                                const std::string& eventName,
                                                f32 deltaTime) {
    if (!script || !script->enabled) return;

    Editor::NodeId entryNode = script->GetCustomEventNode(eventName);
    if (entryNode == 0) return;

    ExecuteFromNode(world, entity, script, entryNode, deltaTime);
}

void VisualScriptExecutor::ExecuteFromNode(ECS::World* world, ECS::Entity entity,
                                            ECS::VisualScriptComponent* script,
                                            Editor::NodeId startNode,
                                            f32 deltaTime,
                                            ECS::Entity otherEntity) {
    if (!script || !world) return;

    auto startTime = std::chrono::high_resolution_clock::now();
    m_LastStats = {};

    // Clear per-frame pure node cache
    script->ClearRuntimeCache();

    // Build execution context
    ExecutionContext ctx;
    ctx.world = world;
    ctx.entity = entity;
    ctx.script = script;
    ctx.deltaTime = deltaTime;
    ctx.otherEntity = otherEntity;
    ctx.physics = m_Physics;
    ctx.physics2d = m_Physics2D;
    ctx.scriptEngine = m_ScriptEngine;
    ctx.networking = m_Networking;
    ctx.nextFlowIndex = 0;

    // Execute flow starting from the entry node
    ExecuteFlow(ctx, startNode);

    auto endTime = std::chrono::high_resolution_clock::now();
    m_LastStats.executionTimeMs = std::chrono::duration<f32, std::milli>(endTime - startTime).count();
}

// ============================================================================
// FLOW EXECUTION
// ============================================================================

void VisualScriptExecutor::ExecuteFlow(ExecutionContext& ctx, Editor::NodeId nodeId) {
    u32 iterations = 0;
    const auto& registry = NodeRegistry::Instance();

    while (nodeId != 0 && iterations < m_MaxIterations) {
        iterations++;

        const auto* graphNode = ctx.script->graph.FindNode(nodeId);
        if (!graphNode) {
            ENJIN_LOG_WARN(Script, "VisualScript: Node %u not found", nodeId);
            break;
        }

        // Get node metadata
        auto metaIt = ctx.script->nodeMeta.find(nodeId);
        if (metaIt == ctx.script->nodeMeta.end()) {
            ENJIN_LOG_WARN(Script, "VisualScript: No metadata for node %u", nodeId);
            break;
        }

        const std::string& nodeType = metaIt->second.nodeType;
        const NodeDefinition* def = registry.FindNode(nodeType);
        if (!def) {
            ENJIN_LOG_WARN(Script, "VisualScript: Unknown node type '%s'", nodeType.c_str());
            break;
        }

        // ===== BREAKPOINT HANDLING =====
        // Check if paused and not stepping
        if (ctx.script->isPaused && !ctx.script->stepRequested) {
            return;  // Stay paused, don't execute
        }

        // Check for breakpoint at this node
        {
            auto bpIt = ctx.script->breakpoints.find(nodeId);
            if (bpIt != ctx.script->breakpoints.end() && bpIt->second.enabled) {
                if (!ctx.script->stepRequested) {
                    auto& bp = bpIt->second;
                    bool shouldBreak = true;

                    // Evaluate simple condition: "varName op literal"
                    if (!bp.condition.empty()) {
                        shouldBreak = false;
                        // Parse: varName op literal (e.g. "health < 50")
                        std::string cond = bp.condition;
                        // Find operator
                        std::string ops[] = {"<=", ">=", "!=", "==", "<", ">"};
                        for (const auto& op : ops) {
                            auto pos = cond.find(op);
                            if (pos != std::string::npos) {
                                std::string varName = cond.substr(0, pos);
                                std::string literal = cond.substr(pos + op.size());
                                // Trim whitespace
                                while (!varName.empty() && varName.back() == ' ') varName.pop_back();
                                while (!literal.empty() && literal.front() == ' ') literal.erase(literal.begin());

                                const auto* var = ctx.script->FindVariable(varName);
                                if (var && std::holds_alternative<f32>(var->value)) {
                                    f32 val = std::get<f32>(var->value);
                                    f32 lit = 0.0f;
                                    try { lit = std::stof(literal); } catch (...) {}

                                    if (op == "<") shouldBreak = val < lit;
                                    else if (op == ">") shouldBreak = val > lit;
                                    else if (op == "<=") shouldBreak = val <= lit;
                                    else if (op == ">=") shouldBreak = val >= lit;
                                    else if (op == "==") shouldBreak = std::abs(val - lit) < 0.0001f;
                                    else if (op == "!=") shouldBreak = std::abs(val - lit) >= 0.0001f;
                                } else if (var && std::holds_alternative<i32>(var->value)) {
                                    i32 val = std::get<i32>(var->value);
                                    i32 lit = 0;
                                    try { lit = std::stoi(literal); } catch (...) {}

                                    if (op == "<") shouldBreak = val < lit;
                                    else if (op == ">") shouldBreak = val > lit;
                                    else if (op == "<=") shouldBreak = val <= lit;
                                    else if (op == ">=") shouldBreak = val >= lit;
                                    else if (op == "==") shouldBreak = val == lit;
                                    else if (op == "!=") shouldBreak = val != lit;
                                }
                                break;
                            }
                        }
                    }

                    // Hit count check
                    if (shouldBreak) {
                        bp.hitCount++;
                        if (bp.hitCountTarget > 0 && bp.hitCount < bp.hitCountTarget) {
                            shouldBreak = false;
                        }
                    }

                    if (shouldBreak) {
                        ctx.script->isPaused = true;
                        ctx.script->pausedAtNode = nodeId;
                        ctx.script->currentlyExecutingNode = nodeId;
                        return;
                    }
                }
            }
        }

        // Clear step flag after handling (we're about to execute one node)
        if (ctx.script->stepRequested) {
            ctx.script->stepRequested = false;
            ctx.script->pausedAtNode = nodeId;
            // Will pause again after this node completes
        }

        // Track currently executing node for visualization
        ctx.script->currentlyExecutingNode = nodeId;

        // ===== CALL STACK TRACKING =====
        {
            ECS::VisualScriptComponent::CallStackEntry entry;
            entry.nodeId = nodeId;
            entry.nodeType = nodeType;
            entry.displayName = graphNode->title;
            ctx.script->callStack.push_back(entry);
        }

        // ===== EXECUTION RECORDING =====
        auto nodeStartTime = std::chrono::high_resolution_clock::now();

        // Don't execute pure nodes via flow (they're evaluated on demand)
        if (def->IsPure()) {
            break;
        }

        // Gather input values for the node
        std::vector<ECS::VariableValue> inputs = GatherInputValues(ctx, ctx.script, graphNode, def);
        std::vector<ECS::VariableValue> outputs;

        // Reset flow index
        ctx.nextFlowIndex = -1;

        // Handle Delay node specially (latent action)
        if (nodeType == NodeTypes::Delay) {
            f32 duration = 1.0f;
            if (inputs.size() > 0) {
                if (std::holds_alternative<f32>(inputs[0])) {
                    duration = std::get<f32>(inputs[0]);
                } else if (std::holds_alternative<i32>(inputs[0])) {
                    duration = static_cast<f32>(std::get<i32>(inputs[0]));
                }
            }

            // Get the flow output pin for resumption
            Editor::PinId flowOutPin = GetFlowOutputPin(graphNode, 0);

            // Start latent action
            ECS::LatentNodeState state;
            state.timeRemaining = duration;
            state.isActive = true;
            state.resumeFlowPin = flowOutPin;
            ctx.script->latentStates[nodeId] = state;

            // Stop synchronous execution (will resume when delay completes)
            ctx.nextFlowIndex = -1;
            m_LastStats.nodesExecuted++;
            break;
        }

        // Handle WaitForAudioComplete (latent audio)
        if (nodeType == NodeTypes::WaitForAudioComplete) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            Editor::PinId flowOutPin = GetFlowOutputPin(graphNode, 0);

            ECS::LatentNodeState state;
            state.isActive = true;
            state.resumeFlowPin = flowOutPin;
            state.targetEntity = target;
            ctx.script->latentStates[nodeId] = state;

            ctx.nextFlowIndex = -1;
            m_LastStats.nodesExecuted++;
            break;
        }

        // Handle WaitForAnimationComplete (latent animation)
        if (nodeType == NodeTypes::WaitForAnimationComplete) {
            ECS::Entity target = ctx.entity;
            if (inputs.size() > 0 && std::holds_alternative<ECS::Entity>(inputs[0])) {
                ECS::Entity e = std::get<ECS::Entity>(inputs[0]);
                if (e != ECS::INVALID_ENTITY) target = e;
            }

            std::string animName;
            if (inputs.size() > 1 && std::holds_alternative<std::string>(inputs[1])) {
                animName = std::get<std::string>(inputs[1]);
            }

            Editor::PinId flowOutPin = GetFlowOutputPin(graphNode, 0);

            ECS::LatentNodeState state;
            state.isActive = true;
            state.resumeFlowPin = flowOutPin;
            state.targetEntity = target;
            state.targetName = animName;
            ctx.script->latentStates[nodeId] = state;

            ctx.nextFlowIndex = -1;
            m_LastStats.nodesExecuted++;
            break;
        }

        // Execute the node
        if (def->execute) {
            def->execute(ctx, inputs, outputs);
        }

        // Record execution for profiler
        if (ctx.script->recordingEnabled) {
            auto nodeEndTime = std::chrono::high_resolution_clock::now();
            f32 durationMs = std::chrono::duration<f32, std::milli>(nodeEndTime - nodeStartTime).count();

            if (ctx.script->executionHistory.size() < ECS::VisualScriptComponent::MAX_HISTORY) {
                ECS::ExecutionRecord record;
                record.nodeId = nodeId;
                record.timestamp = ctx.deltaTime;  // Simplified: use delta time as relative timestamp
                record.duration = durationMs;
                record.nodeType = nodeType;
                ctx.script->executionHistory.push_back(record);
            }
        }

        m_LastStats.nodesExecuted++;

        // Pop call stack entry after execution
        if (!ctx.script->callStack.empty()) {
            ctx.script->callStack.pop_back();
        }

        // If we're stepping, pause after this node
        if (ctx.script->isPaused) {
            return;
        }

        // Handle special nodes
        if (nodeType == NodeTypes::Sequence) {
            // S8: Apply same depth limit to Sequence recursion
            if (m_FunctionCallDepth >= MAX_CALL_DEPTH) {
                ENJIN_LOG_WARN(Script, "VisualScript: Sequence recursion depth limit reached (%u)", MAX_CALL_DEPTH);
                break;
            }
            // Sequence node: execute all flow outputs in order
            m_FunctionCallDepth++;
            for (usize i = 0; i < graphNode->outputs.size(); i++) {
                if (graphNode->outputs[i].type == Editor::PinType::Flow) {
                    Editor::NodeId nextNode = FollowFlowLink(ctx.script, graphNode->outputs[i].id);
                    if (nextNode != 0) {
                        ExecuteFlow(ctx, nextNode);
                    }
                }
            }
            m_FunctionCallDepth--;
            // All outputs executed, done with this branch
            break;
        }

        // Handle Function Call specially (execute subgraph)
        if (nodeType == NodeTypes::FunctionCall) {
            const auto& props = metaIt->second.properties;
            auto funcNameIt = props.find("functionName");
            if (funcNameIt != props.end() && ctx.script) {
                const std::string& funcName = funcNameIt->second;
                // Find function in script
                for (const auto& func : ctx.script->functions) {
                    if (func.name == funcName) {
                        if (m_FunctionCallDepth >= MAX_CALL_DEPTH) {
                            ENJIN_LOG_WARN(Script, "VisualScript: Function call depth limit reached (%u)", MAX_CALL_DEPTH);
                            break;
                        }

                        m_FunctionCallDepth++;

                        // Find the Function_Entry node in the subgraph
                        Editor::NodeId entryNodeId = 0;
                        for (const auto& [nid, meta] : func.nodeMeta) {
                            if (meta.nodeType == NodeTypes::FunctionEntry) {
                                entryNodeId = nid;
                                break;
                            }
                        }

                        if (entryNodeId != 0) {
                            // Save current script state and swap to function subgraph
                            auto savedGraph = ctx.script->graph;
                            auto savedMeta = ctx.script->nodeMeta;

                            ctx.script->graph = func.graph;
                            ctx.script->nodeMeta = func.nodeMeta;

                            // Bind input parameters to function entry outputs
                            // (inputs to CallFunction become outputs of FunctionEntry)

                            // Execute the function subgraph
                            Editor::NodeId funcFlowStart = 0;
                            const auto* entryGraphNode = func.graph.FindNode(entryNodeId);
                            if (entryGraphNode) {
                                for (const auto& pin : entryGraphNode->outputs) {
                                    if (pin.type == Editor::PinType::Flow) {
                                        funcFlowStart = FollowFlowLink(ctx.script, pin.id);
                                        break;
                                    }
                                }
                            }

                            if (funcFlowStart != 0) {
                                ExecuteFlow(ctx, funcFlowStart);
                            }

                            // Restore main graph
                            ctx.script->graph = savedGraph;
                            ctx.script->nodeMeta = savedMeta;
                        }

                        m_FunctionCallDepth--;
                        break;
                    }
                }
            }

            // Follow normal flow output after function completes
            ctx.nextFlowIndex = 0;
        }

        // Handle Set Variable specially
        if (nodeType == NodeTypes::SetVariable) {
            const auto& props = metaIt->second.properties;
            auto varNameIt = props.find("variableName");
            if (varNameIt != props.end() && !inputs.empty()) {
                ECS::VisualScriptVariable* var = ctx.script->FindVariable(varNameIt->second);
                if (var && inputs.size() > 0) {
                    var->value = inputs[0];
                }
            }
        }

        // Follow flow link to next node
        if (ctx.nextFlowIndex < 0) {
            // Execution stopped (no more flow)
            break;
        }

        Editor::PinId flowOutPin = GetFlowOutputPin(graphNode, ctx.nextFlowIndex);
        if (flowOutPin == 0) {
            break;  // No flow output pin at this index
        }

        nodeId = FollowFlowLink(ctx.script, flowOutPin);
    }

    if (iterations >= m_MaxIterations) {
        ENJIN_LOG_WARN(Script, "VisualScript: Execution exceeded max iterations (%u), possible infinite loop", m_MaxIterations);
    }

    // Clear currently executing node when done
    ctx.script->currentlyExecutingNode = 0;
}

// ============================================================================
// LATENT NODE UPDATES
// ============================================================================

void VisualScriptExecutor::UpdateLatentNodes(ECS::World* world, ECS::Entity entity,
                                              ECS::VisualScriptComponent* script,
                                              f32 deltaTime) {
    if (!script || !world) return;

    // Collect nodes that completed this frame
    std::vector<Editor::NodeId> completedNodes;

    for (auto& [nodeId, state] : script->latentStates) {
        if (!state.isActive) continue;

        // Get node type to determine how to check for completion
        auto metaIt = script->nodeMeta.find(nodeId);
        if (metaIt == script->nodeMeta.end()) continue;

        const std::string& nodeType = metaIt->second.nodeType;

        if (nodeType == NodeTypes::Delay) {
            // Timer-based delay
            state.timeRemaining -= deltaTime;
            if (state.timeRemaining <= 0.0f) {
                state.isActive = false;
                completedNodes.push_back(nodeId);
            }
        }
        else if (nodeType == NodeTypes::WaitForAudioComplete) {
            // Check if audio is still playing
            ECS::Entity target = state.targetEntity;
            if (target == ECS::INVALID_ENTITY) target = entity;

            auto* audio = world->GetComponent<ECS::AudioSourceComponent>(target);
            bool stillPlaying = false;

            if (audio) {
                // Use the isPlaying flag from AudioSourceComponent
                stillPlaying = audio->isPlaying;
            }

            if (!stillPlaying) {
                state.isActive = false;
                completedNodes.push_back(nodeId);
            }
        }
        else if (nodeType == NodeTypes::WaitForAnimationComplete) {
            // Check if animation is still playing
            ECS::Entity target = state.targetEntity;
            if (target == ECS::INVALID_ENTITY) target = entity;

            auto* animator = world->GetComponent<ECS::AnimatorComponent>(target);
            bool stillPlaying = false;

            if (animator) {
                // Check if the specific animation is playing, or any animation if name is empty
                if (state.targetName.empty()) {
                    stillPlaying = animator->animator.IsPlaying();
                } else {
                    // Check if the current animation matches and is still playing
                    stillPlaying = animator->animator.IsPlaying();
                }
            }

            if (!stillPlaying) {
                state.isActive = false;
                completedNodes.push_back(nodeId);
            }
        }
    }

    // Resume execution for completed latent nodes
    for (Editor::NodeId nodeId : completedNodes) {
        auto it = script->latentStates.find(nodeId);
        if (it == script->latentStates.end()) continue;

        Editor::PinId resumePin = it->second.resumeFlowPin;

        // Find next node from the resume pin
        Editor::NodeId nextNode = FollowFlowLink(script, resumePin);
        if (nextNode != 0) {
            // Build execution context
            ExecutionContext ctx;
            ctx.world = world;
            ctx.entity = entity;
            ctx.script = script;
            ctx.deltaTime = deltaTime;
            ctx.nextFlowIndex = 0;

            // Continue execution from next node
            ExecuteFlow(ctx, nextNode);
        }

        // Clean up completed latent state
        script->latentStates.erase(nodeId);
    }
}

// ============================================================================
// PURE NODE EVALUATION
// ============================================================================

ECS::VariableValue VisualScriptExecutor::EvaluatePureNode(const ExecutionContext& ctx,
                                                           const ECS::VisualScriptComponent* script,
                                                           Editor::NodeId nodeId) {
    // S12: Recursion depth limit to catch cyclic graphs
    static thread_local int s_PureEvalDepth = 0;
    if (++s_PureEvalDepth > 256) {
        --s_PureEvalDepth;
        ENJIN_LOG_WARN(Script, "VisualScript: Pure node evaluation depth exceeded (cyclic graph?)");
        return false;
    }
    struct DepthGuard { ~DepthGuard() { s_PureEvalDepth--; } } guard;

    // Check cache first
    auto cacheIt = script->pureNodeCache.find(nodeId);
    if (cacheIt != script->pureNodeCache.end()) {
        return cacheIt->second;
    }

    const auto* graphNode = script->graph.FindNode(nodeId);
    if (!graphNode) return false;

    auto metaIt = script->nodeMeta.find(nodeId);
    if (metaIt == script->nodeMeta.end()) return false;

    const std::string& nodeType = metaIt->second.nodeType;
    const NodeDefinition* def = NodeRegistry::Instance().FindNode(nodeType);
    if (!def || !def->IsPure()) return false;

    // Handle Get Variable specially
    if (nodeType == NodeTypes::GetVariable) {
        const auto& props = metaIt->second.properties;
        auto varNameIt = props.find("variableName");
        if (varNameIt != props.end()) {
            const ECS::VisualScriptVariable* var = script->FindVariable(varNameIt->second);
            if (var) {
                const_cast<ECS::VisualScriptComponent*>(script)->pureNodeCache[nodeId] = var->value;
                return var->value;
            }
        }
        return false;
    }

    // Gather inputs recursively
    std::vector<ECS::VariableValue> inputs;
    usize dataInputIdx = 0;
    for (const auto& pin : graphNode->inputs) {
        if (pin.type != Editor::PinType::Flow) {
            inputs.push_back(GetInputPinValue(ctx, script, pin.id));
            dataInputIdx++;
        }
    }

    // Evaluate the pure node
    ECS::VariableValue result = false;
    if (def->evaluate) {
        result = def->evaluate(ctx, inputs);
        const_cast<std::unordered_map<Editor::NodeId, ECS::VariableValue>&>(script->pureNodeCache)[nodeId] = result;
        const_cast<ExecutionStats&>(m_LastStats).pureNodesEvaluated++;
    }

    return result;
}

// ============================================================================
// INPUT VALUE RESOLUTION
// ============================================================================

ECS::VariableValue VisualScriptExecutor::GetInputPinValue(const ExecutionContext& ctx,
                                                            const ECS::VisualScriptComponent* script,
                                                            Editor::PinId pinId) {
    // Find any link connected to this input pin
    auto links = script->graph.GetLinksForPin(pinId);
    for (auto linkId : links) {
        const auto* link = script->graph.FindLink(linkId);
        if (!link) continue;

        // Link goes from output (startPin) to input (endPin)
        if (link->endPinId == pinId) {
            // Find the source pin and its node
            const auto* sourcePin = script->graph.FindPin(link->startPinId);
            if (!sourcePin) continue;

            Editor::NodeId sourceNodeId = sourcePin->nodeId;
            const auto* sourceNode = script->graph.FindNode(sourceNodeId);
            if (!sourceNode) continue;

            // Get node metadata
            auto metaIt = script->nodeMeta.find(sourceNodeId);
            if (metaIt == script->nodeMeta.end()) continue;

            const std::string& nodeType = metaIt->second.nodeType;
            const NodeDefinition* def = NodeRegistry::Instance().FindNode(nodeType);
            if (!def) continue;

            if (def->IsPure()) {
                // Evaluate the pure node
                return EvaluatePureNode(ctx, script, sourceNodeId);
            } else {
                // For impure nodes, check if we have a cached output value
                auto pinValueIt = metaIt->second.pinValues.find(link->startPinId);
                if (pinValueIt != metaIt->second.pinValues.end()) {
                    return pinValueIt->second;
                }
            }
        }
    }

    // No connection found - use default value from pin definition
    const auto* pin = script->graph.FindPin(pinId);
    if (pin) {
        const auto* node = script->graph.FindNode(pin->nodeId);
        if (node) {
            auto metaIt = script->nodeMeta.find(pin->nodeId);
            if (metaIt != script->nodeMeta.end()) {
                const NodeDefinition* def = NodeRegistry::Instance().FindNode(metaIt->second.nodeType);
                if (def) {
                    // Find the corresponding input definition
                    usize inputIdx = 0;
                    for (const auto& nodePin : node->inputs) {
                        if (nodePin.type == Editor::PinType::Flow) continue;
                        if (nodePin.id == pinId) {
                            // Found it - get default from definition
                            usize defInputIdx = 0;
                            for (const auto& defPin : def->inputs) {
                                if (defPin.type == Editor::PinType::Flow) continue;
                                if (defInputIdx == inputIdx) {
                                    return defPin.defaultValue;
                                }
                                defInputIdx++;
                            }
                        }
                        inputIdx++;
                    }
                }
            }
        }
    }

    return false;  // Fallback default
}

std::vector<ECS::VariableValue> VisualScriptExecutor::GatherInputValues(const ExecutionContext& ctx,
                                                                          const ECS::VisualScriptComponent* script,
                                                                          const Editor::GraphNode* node,
                                                                          const NodeDefinition* def) {
    std::vector<ECS::VariableValue> inputs;

    for (const auto& pin : node->inputs) {
        // Skip flow pins
        if (pin.type == Editor::PinType::Flow) continue;
        inputs.push_back(GetInputPinValue(ctx, script, pin.id));
    }

    return inputs;
}

// ============================================================================
// FLOW LINK RESOLUTION
// ============================================================================

Editor::NodeId VisualScriptExecutor::FollowFlowLink(const ECS::VisualScriptComponent* script,
                                                     Editor::PinId outputPinId) {
    auto links = script->graph.GetLinksForPin(outputPinId);
    for (auto linkId : links) {
        const auto* link = script->graph.FindLink(linkId);
        if (link && link->startPinId == outputPinId) {
            const auto* targetPin = script->graph.FindPin(link->endPinId);
            if (targetPin) {
                return targetPin->nodeId;
            }
        }
    }
    return 0;  // No connection
}

Editor::PinId VisualScriptExecutor::GetFlowOutputPin(const Editor::GraphNode* node, i32 index) {
    if (!node || index < 0) return 0;

    i32 flowIdx = 0;
    for (const auto& pin : node->outputs) {
        if (pin.type == Editor::PinType::Flow) {
            if (flowIdx == index) {
                return pin.id;
            }
            flowIdx++;
        }
    }
    return 0;
}

} // namespace VisualScript
} // namespace Enjin
