#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Editor/NodeGraph.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>

namespace Enjin {
namespace Gameplay {

// ============================================================================
// Quest Flow Node Types (8 total)
// ============================================================================

enum class QuestNodeType : u8 {
    Start = 0,

    // Flow
    Objective,
    Delay,
    Event,

    // Logic
    Condition,
    Branch,
    Reward,

    // Terminal
    End,

    COUNT
};

enum class QuestNodeCategory : u8 {
    Entry,
    Flow,
    Check,
    Grant,
    Terminal
};

enum class QuestFlowStatus : u8 {
    Inactive,
    Active,
    Completed,
    Failed
};

const char* QuestNodeTypeToString(QuestNodeType type);
const char* QuestNodeCategoryToString(QuestNodeCategory cat);
const char* QuestFlowStatusToString(QuestFlowStatus status);
QuestNodeCategory GetQuestNodeCategory(QuestNodeType type);

// ============================================================================
// Node Metadata
// ============================================================================

struct QuestNodeMeta {
    QuestNodeType nodeType = QuestNodeType::Start;
    std::unordered_map<std::string, std::string> properties;
    bool reached = false;  // runtime flag
};

// ============================================================================
// Runtime Advance
// ============================================================================

void AdvanceQuestFlow(ECS::World* world, ECS::Entity entity, f32 deltaTime);

} // namespace Gameplay

// ============================================================================
// Quest Flow Component
// ============================================================================

namespace ECS {

struct QuestFlowComponent {
    // Graph data (nodes, pins, links) -- serialized
    Editor::NodeGraphData graph;

    // Per-node quest metadata -- serialized
    std::unordered_map<Editor::NodeId, Gameplay::QuestNodeMeta> nodeMeta;

    // Entry point node -- serialized
    Editor::NodeId startNodeId = 0;

    // Quest info -- serialized
    std::string questId;
    std::string questTitle;
    std::string questDescription;
    bool enabled = true;

    // ========== Runtime state (not serialized) ==========

    Gameplay::QuestFlowStatus status = Gameplay::QuestFlowStatus::Inactive;
    std::set<Editor::NodeId> activeNodes;
    std::set<Editor::NodeId> completedNodes;
    std::unordered_map<Editor::NodeId, f32> nodeTimers;
    std::unordered_map<Editor::NodeId, i32> nodeCounters;

    // Named variables a branch node can test.
    //
    // The condition evaluator has always parsed a key, an operator and a value off
    // a branch node and then compared none of them -- every branch returned true,
    // so a quest graph with two outcomes always took the first. There was nowhere
    // to look a key UP: nodeCounters is keyed by node id, not by name. This is that
    // store. Runtime state, cleared with the rest.
    std::unordered_map<std::string, std::string> variables;

    void ResetRuntimeState() {
        variables.clear();
        status = Gameplay::QuestFlowStatus::Inactive;
        activeNodes.clear();
        completedNodes.clear();
        nodeTimers.clear();
        nodeCounters.clear();
        for (auto& [id, meta] : nodeMeta) {
            meta.reached = false;
        }
    }
};

} // namespace ECS
} // namespace Enjin
