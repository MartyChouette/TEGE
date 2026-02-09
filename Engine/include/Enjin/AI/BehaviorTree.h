#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Editor/NodeGraph.h"
#include <string>
#include <vector>
#include <variant>
#include <unordered_map>

namespace Enjin {
namespace AI {

// ============================================================================
// Behavior Tree Node Types (20 total)
// ============================================================================

enum class BTNodeType : u8 {
    Root = 0,

    // Composites
    Selector,
    Sequence,
    Parallel,

    // Decorators
    Inverter,
    Repeater,
    Timeout,
    ForceSuccess,
    ForceFailure,

    // Actions
    MoveTo,
    Attack,
    Patrol,
    Wait,
    PlayAnimation,
    SetVariable,

    // Conditions
    CanSeeTarget,
    IsInRange,
    HasTarget,
    CheckVariable,
    IsAlive,

    COUNT
};

enum class BTNodeCategory : u8 {
    Root,
    Composite,
    Decorator,
    Action,
    Condition
};

enum class BTStatus : u8 {
    Running,
    Success,
    Failure,
    Invalid
};

const char* BTNodeTypeToString(BTNodeType type);
const char* BTStatusToString(BTStatus status);
BTNodeCategory GetNodeCategory(BTNodeType type);
const char* BTNodeCategoryToString(BTNodeCategory cat);

// ============================================================================
// Node Metadata
// ============================================================================

struct BTNodeMeta {
    BTNodeType nodeType = BTNodeType::Root;
    std::unordered_map<std::string, std::string> properties;
    BTStatus lastStatus = BTStatus::Invalid;
};

// ============================================================================
// Blackboard
// ============================================================================

using BlackboardValue = std::variant<bool, i32, f32, std::string, Math::Vector3, ECS::Entity>;

struct BlackboardEntry {
    std::string key;
    BlackboardValue value;
};

class ENJIN_API Blackboard {
public:
    void Set(const std::string& key, const BlackboardValue& value);
    BlackboardValue* Get(const std::string& key);
    const BlackboardValue* Get(const std::string& key) const;
    bool Has(const std::string& key) const;
    void Remove(const std::string& key);
    void Clear();

    const std::unordered_map<std::string, BlackboardValue>& GetAll() const { return m_Data; }

private:
    std::unordered_map<std::string, BlackboardValue> m_Data;
};

// ============================================================================
// Behavior Tree Component
// ============================================================================

} // namespace AI

namespace ECS {

struct BehaviorTreeComponent {
    // Graph data (nodes, pins, links) — serialized
    Editor::NodeGraphData graph;

    // Per-node BT metadata — serialized
    std::unordered_map<Editor::NodeId, AI::BTNodeMeta> nodeMeta;

    // Entry point node — serialized
    Editor::NodeId rootNodeId = 0;

    // Initial blackboard values — serialized
    std::vector<AI::BlackboardEntry> blackboardDefaults;

    // Settings — serialized
    bool enabled = true;
    f32 tickInterval = 0.0f;  // 0 = every frame
    bool debugEnabled = false;

    // ========== Runtime state (not serialized) ==========

    AI::Blackboard blackboard;
    std::unordered_map<Editor::NodeId, AI::BTStatus> nodeStatuses;
    Editor::NodeId activeNode = 0;
    f32 tickTimer = 0.0f;
    bool initialized = false;

    // Per-node runtime accumulators (for Wait, Timeout, Repeater, etc.)
    std::unordered_map<Editor::NodeId, f32> nodeTimers;
    std::unordered_map<Editor::NodeId, i32> nodeCounters;

    void ResetRuntimeState() {
        blackboard.Clear();
        nodeStatuses.clear();
        activeNode = 0;
        tickTimer = 0.0f;
        initialized = false;
        nodeTimers.clear();
        nodeCounters.clear();
    }
};

} // namespace ECS
} // namespace Enjin
