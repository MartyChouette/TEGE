#include "Enjin/AI/BehaviorTree.h"

namespace Enjin {
namespace AI {

const char* BTNodeTypeToString(BTNodeType type) {
    switch (type) {
        case BTNodeType::Root:          return "Root";
        case BTNodeType::Selector:      return "Selector";
        case BTNodeType::Sequence:      return "Sequence";
        case BTNodeType::Parallel:      return "Parallel";
        case BTNodeType::Inverter:      return "Inverter";
        case BTNodeType::Repeater:      return "Repeater";
        case BTNodeType::Timeout:       return "Timeout";
        case BTNodeType::ForceSuccess:  return "Force Success";
        case BTNodeType::ForceFailure:  return "Force Failure";
        case BTNodeType::MoveTo:        return "Move To";
        case BTNodeType::Attack:        return "Attack";
        case BTNodeType::Patrol:        return "Patrol";
        case BTNodeType::Wait:          return "Wait";
        case BTNodeType::PlayAnimation: return "Play Animation";
        case BTNodeType::SetVariable:   return "Set Variable";
        case BTNodeType::CanSeeTarget:  return "Can See Target";
        case BTNodeType::IsInRange:     return "Is In Range";
        case BTNodeType::HasTarget:     return "Has Target";
        case BTNodeType::CheckVariable: return "Check Variable";
        case BTNodeType::IsAlive:       return "Is Alive";
        default:                        return "Unknown";
    }
}

const char* BTStatusToString(BTStatus status) {
    switch (status) {
        case BTStatus::Running: return "Running";
        case BTStatus::Success: return "Success";
        case BTStatus::Failure: return "Failure";
        case BTStatus::Invalid: return "Invalid";
        default:                return "Unknown";
    }
}

BTNodeCategory GetNodeCategory(BTNodeType type) {
    switch (type) {
        case BTNodeType::Root:
            return BTNodeCategory::Root;

        case BTNodeType::Selector:
        case BTNodeType::Sequence:
        case BTNodeType::Parallel:
            return BTNodeCategory::Composite;

        case BTNodeType::Inverter:
        case BTNodeType::Repeater:
        case BTNodeType::Timeout:
        case BTNodeType::ForceSuccess:
        case BTNodeType::ForceFailure:
            return BTNodeCategory::Decorator;

        case BTNodeType::MoveTo:
        case BTNodeType::Attack:
        case BTNodeType::Patrol:
        case BTNodeType::Wait:
        case BTNodeType::PlayAnimation:
        case BTNodeType::SetVariable:
            return BTNodeCategory::Action;

        case BTNodeType::CanSeeTarget:
        case BTNodeType::IsInRange:
        case BTNodeType::HasTarget:
        case BTNodeType::CheckVariable:
        case BTNodeType::IsAlive:
            return BTNodeCategory::Condition;

        default:
            return BTNodeCategory::Root;
    }
}

const char* BTNodeCategoryToString(BTNodeCategory cat) {
    switch (cat) {
        case BTNodeCategory::Root:       return "Root";
        case BTNodeCategory::Composite:  return "Composite";
        case BTNodeCategory::Decorator:  return "Decorator";
        case BTNodeCategory::Action:     return "Action";
        case BTNodeCategory::Condition:  return "Condition";
        default:                         return "Unknown";
    }
}

// ============================================================================
// Blackboard
// ============================================================================

void Blackboard::Set(const std::string& key, const BlackboardValue& value) {
    m_Data[key] = value;
}

BlackboardValue* Blackboard::Get(const std::string& key) {
    auto it = m_Data.find(key);
    return it != m_Data.end() ? &it->second : nullptr;
}

const BlackboardValue* Blackboard::Get(const std::string& key) const {
    auto it = m_Data.find(key);
    return it != m_Data.end() ? &it->second : nullptr;
}

bool Blackboard::Has(const std::string& key) const {
    return m_Data.count(key) > 0;
}

void Blackboard::Remove(const std::string& key) {
    m_Data.erase(key);
}

void Blackboard::Clear() {
    m_Data.clear();
}

} // namespace AI
} // namespace Enjin
