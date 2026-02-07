#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/VisualScript/NodeDefinition.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace VisualScript {

// ============================================================================
// NODE REGISTRY
// ============================================================================

// Singleton registry of all available node types
class ENJIN_API NodeRegistry {
public:
    // Get singleton instance
    static NodeRegistry& Instance();

    // Registration
    void RegisterNode(const NodeDefinition& def);
    void RegisterBuiltinNodes();

    // Lookup
    const NodeDefinition* FindNode(const std::string& typeId) const;
    std::vector<const NodeDefinition*> GetNodesByCategory(NodeCategory category) const;
    std::vector<const NodeDefinition*> GetAllNodes() const;

    // Search nodes by name/keywords (for add node menu)
    std::vector<const NodeDefinition*> SearchNodes(const std::string& query) const;

    // Get categories with at least one node
    std::vector<NodeCategory> GetActiveCategories() const;

private:
    NodeRegistry();
    ~NodeRegistry() = default;
    NodeRegistry(const NodeRegistry&) = delete;
    NodeRegistry& operator=(const NodeRegistry&) = delete;

    std::unordered_map<std::string, NodeDefinition> m_Nodes;
    bool m_Initialized = false;
};

// ============================================================================
// BUILT-IN NODE TYPE IDs
// ============================================================================

namespace NodeTypes {

// Events
constexpr const char* OnStart       = "Event_OnStart";
constexpr const char* OnUpdate      = "Event_OnUpdate";
constexpr const char* OnCollision   = "Event_OnCollision";
constexpr const char* CustomEvent   = "Event_Custom";

// Flow Control
constexpr const char* Branch        = "Flow_Branch";
constexpr const char* Sequence      = "Flow_Sequence";
constexpr const char* ForLoop       = "Flow_ForLoop";

// Variables
constexpr const char* GetVariable   = "Var_Get";
constexpr const char* SetVariable   = "Var_Set";
constexpr const char* GetSelf       = "Var_GetSelf";

// Math (Pure)
constexpr const char* Add           = "Math_Add";
constexpr const char* Subtract      = "Math_Subtract";
constexpr const char* Multiply      = "Math_Multiply";
constexpr const char* Divide        = "Math_Divide";
constexpr const char* Negate        = "Math_Negate";
constexpr const char* Abs           = "Math_Abs";

// Logic (Pure)
constexpr const char* And           = "Logic_And";
constexpr const char* Or            = "Logic_Or";
constexpr const char* Not           = "Logic_Not";
constexpr const char* Equal         = "Logic_Equal";
constexpr const char* NotEqual      = "Logic_NotEqual";
constexpr const char* Greater       = "Logic_Greater";
constexpr const char* Less          = "Logic_Less";

// Transform
constexpr const char* GetPosition   = "Transform_GetPosition";
constexpr const char* SetPosition   = "Transform_SetPosition";
constexpr const char* GetRotation   = "Transform_GetRotation";
constexpr const char* SetRotation   = "Transform_SetRotation";

// Debug
constexpr const char* Print         = "Debug_Print";
constexpr const char* PrintWarning  = "Debug_PrintWarning";
constexpr const char* PrintError    = "Debug_PrintError";

} // namespace NodeTypes

} // namespace VisualScript
} // namespace Enjin
