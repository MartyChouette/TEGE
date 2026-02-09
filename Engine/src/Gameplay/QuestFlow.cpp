#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/Logging/Log.h"
#include <cstdlib>

namespace Enjin {
namespace Gameplay {

const char* QuestNodeTypeToString(QuestNodeType type) {
    switch (type) {
        case QuestNodeType::Start:     return "Start";
        case QuestNodeType::Objective: return "Objective";
        case QuestNodeType::Delay:     return "Delay";
        case QuestNodeType::Event:     return "Event";
        case QuestNodeType::Condition: return "Condition";
        case QuestNodeType::Branch:    return "Branch";
        case QuestNodeType::Reward:    return "Reward";
        case QuestNodeType::End:       return "End";
        default:                       return "Unknown";
    }
}

const char* QuestNodeCategoryToString(QuestNodeCategory cat) {
    switch (cat) {
        case QuestNodeCategory::Entry:    return "Entry";
        case QuestNodeCategory::Flow:     return "Flow";
        case QuestNodeCategory::Check:    return "Check";
        case QuestNodeCategory::Grant:    return "Grant";
        case QuestNodeCategory::Terminal: return "Terminal";
        default:                          return "Unknown";
    }
}

const char* QuestFlowStatusToString(QuestFlowStatus status) {
    switch (status) {
        case QuestFlowStatus::Inactive:  return "Inactive";
        case QuestFlowStatus::Active:    return "Active";
        case QuestFlowStatus::Completed: return "Completed";
        case QuestFlowStatus::Failed:    return "Failed";
        default:                         return "Unknown";
    }
}

QuestNodeCategory GetQuestNodeCategory(QuestNodeType type) {
    switch (type) {
        case QuestNodeType::Start:
            return QuestNodeCategory::Entry;

        case QuestNodeType::Objective:
        case QuestNodeType::Delay:
        case QuestNodeType::Event:
            return QuestNodeCategory::Flow;

        case QuestNodeType::Condition:
        case QuestNodeType::Branch:
            return QuestNodeCategory::Check;

        case QuestNodeType::Reward:
            return QuestNodeCategory::Grant;

        case QuestNodeType::End:
            return QuestNodeCategory::Terminal;

        default:
            return QuestNodeCategory::Entry;
    }
}

// ============================================================================
// Helper: get output pin IDs for a node
// ============================================================================

static std::vector<Editor::PinId> GetOutputPins(const Editor::NodeGraphData& graph,
                                                  Editor::NodeId nodeId) {
    std::vector<Editor::PinId> result;
    auto* node = const_cast<Editor::NodeGraphData&>(graph).FindNode(nodeId);
    if (!node) return result;
    for (const auto& pin : node->outputs) {
        result.push_back(pin.id);
    }
    return result;
}

// Activate all nodes connected to a specific output pin
static void ActivateOutputPin(ECS::QuestFlowComponent& flow,
                               Editor::PinId pinId) {
    auto links = flow.graph.GetLinksForPin(pinId);
    for (auto linkId : links) {
        auto* link = flow.graph.FindLink(linkId);
        if (!link) continue;

        Editor::PinId targetPinId = (link->startPinId == pinId) ?
            link->endPinId : link->startPinId;
        Editor::NodeId targetNodeId = flow.graph.GetPinOwner(targetPinId);
        if (targetNodeId != 0 && flow.completedNodes.find(targetNodeId) == flow.completedNodes.end()) {
            flow.activeNodes.insert(targetNodeId);
            auto metaIt = flow.nodeMeta.find(targetNodeId);
            if (metaIt != flow.nodeMeta.end()) {
                metaIt->second.reached = true;
            }
        }
    }
}

// Activate all nodes connected to all output pins
static void ActivateAllOutputs(ECS::QuestFlowComponent& flow,
                                Editor::NodeId nodeId) {
    auto pins = GetOutputPins(flow.graph, nodeId);
    for (auto pinId : pins) {
        ActivateOutputPin(flow, pinId);
    }
}

// Mark node complete and activate outputs
static void CompleteNode(ECS::QuestFlowComponent& flow,
                          Editor::NodeId nodeId) {
    flow.activeNodes.erase(nodeId);
    flow.completedNodes.insert(nodeId);
    ActivateAllOutputs(flow, nodeId);
}

// Activate a specific output by index (0 = first, 1 = second)
static void ActivateOutputByIndex(ECS::QuestFlowComponent& flow,
                                    Editor::NodeId nodeId,
                                    usize index) {
    auto pins = GetOutputPins(flow.graph, nodeId);
    if (index < pins.size()) {
        ActivateOutputPin(flow, pins[index]);
    }
}

// Evaluate a simple condition from node properties
static bool EvaluateCondition(const QuestNodeMeta& meta,
                               const ECS::QuestFlowComponent& flow) {
    auto condIt = meta.properties.find("conditionType");
    if (condIt == meta.properties.end()) return true;

    auto keyIt = meta.properties.find("key");
    auto opIt = meta.properties.find("operator");
    auto valIt = meta.properties.find("value");

    std::string key = (keyIt != meta.properties.end()) ? keyIt->second : "";
    std::string op = (opIt != meta.properties.end()) ? opIt->second : "==";
    std::string val = (valIt != meta.properties.end()) ? valIt->second : "";

    const auto& cond = condIt->second;
    if (cond == "questComplete") {
        // Check if another quest flow on same entity completed
        // Simple check: compare questId in key with flow.questId
        return flow.status == QuestFlowStatus::Completed;
    }

    if (cond == "variable" || cond == "custom") {
        // Check node counters as simple variable store
        auto counterIt = flow.nodeCounters.find(0);  // placeholder
        // For now, custom conditions evaluate to true by default
        return true;
    }

    // Default: true
    return true;
}

// ============================================================================
// Quest Flow Runtime Advance
// ============================================================================

void AdvanceQuestFlow(ECS::World* world, ECS::Entity entity, f32 deltaTime) {
    auto* flow = world->GetComponent<ECS::QuestFlowComponent>(entity);
    if (!flow || !flow->enabled) return;

    // Initialize on first tick
    if (flow->status == QuestFlowStatus::Inactive) {
        flow->status = QuestFlowStatus::Active;
        if (flow->startNodeId != 0) {
            flow->activeNodes.insert(flow->startNodeId);
            auto metaIt = flow->nodeMeta.find(flow->startNodeId);
            if (metaIt != flow->nodeMeta.end()) {
                metaIt->second.reached = true;
            }
        }
    }

    if (flow->status != QuestFlowStatus::Active) return;

    // Copy active nodes (processing may modify the set)
    auto activeSnapshot = flow->activeNodes;

    for (auto nodeId : activeSnapshot) {
        // Skip if already completed during this frame
        if (flow->completedNodes.find(nodeId) != flow->completedNodes.end()) continue;

        auto metaIt = flow->nodeMeta.find(nodeId);
        if (metaIt == flow->nodeMeta.end()) continue;

        const auto& meta = metaIt->second;

        switch (meta.nodeType) {
            case QuestNodeType::Start:
                CompleteNode(*flow, nodeId);
                break;

            case QuestNodeType::Objective: {
                // Check completion: nodeCounters[nodeId] >= targetCount
                i32 targetCount = 1;
                auto tcIt = meta.properties.find("targetCount");
                if (tcIt != meta.properties.end()) {
                    targetCount = std::atoi(tcIt->second.c_str());
                    if (targetCount <= 0) targetCount = 1;
                }
                i32 current = flow->nodeCounters[nodeId];
                if (current >= targetCount) {
                    CompleteNode(*flow, nodeId);
                }
                break;
            }

            case QuestNodeType::Condition: {
                bool result = EvaluateCondition(meta, *flow);
                flow->activeNodes.erase(nodeId);
                flow->completedNodes.insert(nodeId);
                // Index 0 = True, 1 = False
                ActivateOutputByIndex(*flow, nodeId, result ? 0 : 1);
                break;
            }

            case QuestNodeType::Reward: {
                // Log the reward grant (actual reward logic is game-specific)
                auto typeIt = meta.properties.find("rewardType");
                auto amtIt = meta.properties.find("amount");
                std::string rType = (typeIt != meta.properties.end()) ? typeIt->second : "custom";
                std::string rAmt = (amtIt != meta.properties.end()) ? amtIt->second : "1";
                ENJIN_LOG_INFO(Editor, "Quest '%s': Reward granted - %s x%s",
                    flow->questId.c_str(), rType.c_str(), rAmt.c_str());
                CompleteNode(*flow, nodeId);
                break;
            }

            case QuestNodeType::Branch: {
                bool result = EvaluateCondition(meta, *flow);
                flow->activeNodes.erase(nodeId);
                flow->completedNodes.insert(nodeId);
                ActivateOutputByIndex(*flow, nodeId, result ? 0 : 1);
                break;
            }

            case QuestNodeType::Delay: {
                auto durIt = meta.properties.find("duration");
                f32 duration = 1.0f;
                if (durIt != meta.properties.end()) {
                    duration = static_cast<f32>(std::atof(durIt->second.c_str()));
                    if (duration <= 0.0f) duration = 1.0f;
                }
                flow->nodeTimers[nodeId] += deltaTime;
                if (flow->nodeTimers[nodeId] >= duration) {
                    CompleteNode(*flow, nodeId);
                }
                break;
            }

            case QuestNodeType::Event: {
                auto nameIt = meta.properties.find("eventName");
                std::string evName = (nameIt != meta.properties.end()) ? nameIt->second : "";
                if (!evName.empty()) {
                    ENJIN_LOG_INFO(Editor, "Quest '%s': Event fired - '%s'",
                        flow->questId.c_str(), evName.c_str());
                }
                CompleteNode(*flow, nodeId);
                break;
            }

            case QuestNodeType::End: {
                auto endIt = meta.properties.find("endStatus");
                std::string endStatus = (endIt != meta.properties.end()) ? endIt->second : "completed";
                if (endStatus == "failed") {
                    flow->status = QuestFlowStatus::Failed;
                } else {
                    flow->status = QuestFlowStatus::Completed;
                }
                flow->activeNodes.clear();
                ENJIN_LOG_INFO(Editor, "Quest '%s': %s",
                    flow->questId.c_str(), QuestFlowStatusToString(flow->status));
                return;  // Quest is done
            }

            default:
                break;
        }
    }
}

} // namespace Gameplay
} // namespace Enjin
