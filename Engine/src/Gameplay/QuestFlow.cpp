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

// Apply an operator to two values.
//
// Compares numerically when BOTH sides parse as numbers, and as text otherwise.
// That order matters: "10" > "9" is false as text and true as a number, and a
// quest counter compared as text would take the wrong branch exactly once the
// count reaches double figures -- late enough that the graph looks correct while
// it is being built.
static bool ApplyOperator(const std::string& lhs, const std::string& op,
                          const std::string& rhs) {
    auto asNumber = [](const std::string& v, f64& out) {
        if (v.empty()) return false;
        char* end = nullptr;
        out = std::strtod(v.c_str(), &end);
        return end != nullptr && *end == '\0';
    };

    f64 a = 0.0, b = 0.0;
    if (asNumber(lhs, a) && asNumber(rhs, b)) {
        if (op == "==") return a == b;
        if (op == "!=") return a != b;
        if (op == ">")  return a > b;
        if (op == ">=") return a >= b;
        if (op == "<")  return a < b;
        if (op == "<=") return a <= b;
    } else {
        if (op == "==") return lhs == rhs;
        if (op == "!=") return lhs != rhs;
        if (op == ">")  return lhs > rhs;
        if (op == ">=") return lhs >= rhs;
        if (op == "<")  return lhs < rhs;
        if (op == "<=") return lhs <= rhs;
    }

    ENJIN_LOG_WARN(Editor, "Quest branch: unknown operator '%s'; treating as false",
                   op.c_str());
    return false;
}

// Evaluate a branch node's condition.
//
// Every path in this function used to end in `return true`. The key, the operator
// and the value were read off the node and compared against nothing, so a quest
// graph with two outcomes always took the first one and the second was
// unreachable -- an authored branch that could not branch.
static bool EvaluateCondition(const QuestNodeMeta& meta,
                               const ECS::QuestFlowComponent& flow) {
    auto condIt = meta.properties.find("conditionType");
    // No condition set at all is not a failure: an unconditional node passes.
    if (condIt == meta.properties.end()) return true;

    auto keyIt = meta.properties.find("key");
    auto opIt = meta.properties.find("operator");
    auto valIt = meta.properties.find("value");

    const std::string key = (keyIt != meta.properties.end()) ? keyIt->second : "";
    const std::string op = (opIt != meta.properties.end()) ? opIt->second : "==";
    const std::string val = (valIt != meta.properties.end()) ? valIt->second : "";

    const auto& cond = condIt->second;

    if (cond == "questComplete") {
        // With no key, this asks about the flow it lives on. With a key, it names
        // another quest -- and this component cannot see the QuestSystem, so
        // rather than answer for the wrong quest it says it cannot answer.
        if (key.empty() || key == flow.questId) {
            return flow.status == QuestFlowStatus::Completed;
        }
        ENJIN_LOG_WARN(Editor,
            "Quest branch on '%s': questComplete for another quest ('%s') is not "
            "evaluated here; use a variable set by that quest instead. Taking the "
            "false branch.",
            flow.questId.c_str(), key.c_str());
        return false;
    }

    if (cond == "variable") {
        auto it = flow.variables.find(key);
        if (it == flow.variables.end()) {
            // An unset variable is not equal to anything, and is not greater or
            // less than anything either. "!=" is the one operator it can answer.
            return op == "!=";
        }
        return ApplyOperator(it->second, op, val);
    }

    if (cond == "counter") {
        // nodeCounters is keyed by node id, so the key is a node number.
        char* end = nullptr;
        const unsigned long nodeId = std::strtoul(key.c_str(), &end, 10);
        if (key.empty() || (end && *end != '\0')) {
            ENJIN_LOG_WARN(Editor,
                "Quest branch on '%s': counter condition needs a node id as its "
                "key, got '%s'. Taking the false branch.",
                flow.questId.c_str(), key.c_str());
            return false;
        }
        auto it = flow.nodeCounters.find(static_cast<Editor::NodeId>(nodeId));
        const i32 count = (it != flow.nodeCounters.end()) ? it->second : 0;
        return ApplyOperator(std::to_string(count), op, val);
    }

    if (cond == "custom") {
        // The engine cannot know what a game-defined condition means. Answering
        // "true" is how an unimplementable check became an always-open path; the
        // honest answer is that it did not pass, said loudly enough to find.
        ENJIN_LOG_WARN(Editor,
            "Quest branch on '%s': custom condition '%s' has no evaluator; set a "
            "variable from script instead. Taking the false branch.",
            flow.questId.c_str(), key.c_str());
        return false;
    }

    ENJIN_LOG_WARN(Editor, "Quest branch on '%s': unknown condition type '%s'. "
                           "Taking the false branch.",
                   flow.questId.c_str(), cond.c_str());
    return false;
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
