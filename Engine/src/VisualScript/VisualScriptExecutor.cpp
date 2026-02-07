#include "Enjin/VisualScript/VisualScriptExecutor.h"
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
                                          f32 deltaTime) {
    if (!script || !script->enabled) return;

    Editor::NodeId entryNode = script->GetEventNode(event);
    if (entryNode == 0) return;  // No handler for this event

    ExecuteFromNode(world, entity, script, entryNode, deltaTime);
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
                                            f32 deltaTime) {
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

        // Don't execute pure nodes via flow (they're evaluated on demand)
        if (def->IsPure()) {
            break;
        }

        // Gather input values for the node
        std::vector<ECS::VariableValue> inputs = GatherInputValues(ctx, ctx.script, graphNode, def);
        std::vector<ECS::VariableValue> outputs;

        // Reset flow index
        ctx.nextFlowIndex = -1;

        // Execute the node
        if (def->execute) {
            def->execute(ctx, inputs, outputs);
        }

        m_LastStats.nodesExecuted++;

        // Handle special nodes
        if (nodeType == NodeTypes::Sequence) {
            // Sequence node: execute all flow outputs in order
            for (usize i = 0; i < graphNode->outputs.size(); i++) {
                if (graphNode->outputs[i].type == Editor::PinType::Flow) {
                    Editor::NodeId nextNode = FollowFlowLink(ctx.script, graphNode->outputs[i].id);
                    if (nextNode != 0) {
                        ExecuteFlow(ctx, nextNode);
                    }
                }
            }
            // All outputs executed, done with this branch
            break;
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
}

// ============================================================================
// PURE NODE EVALUATION
// ============================================================================

ECS::VariableValue VisualScriptExecutor::EvaluatePureNode(const ExecutionContext& ctx,
                                                           const ECS::VisualScriptComponent* script,
                                                           Editor::NodeId nodeId) {
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
