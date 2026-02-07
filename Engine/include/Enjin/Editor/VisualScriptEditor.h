#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Editor/NodeGraph.h"
#include "Enjin/Editor/EditorSettings.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/VisualScript/NodeRegistry.h"
#include <string>
#include <unordered_map>

namespace Enjin {
namespace Editor {

// ============================================================================
// VISUAL SCRIPT EDITOR
// ============================================================================

// Editor panel for visual script graphs
class ENJIN_API VisualScriptEditor {
public:
    VisualScriptEditor();
    ~VisualScriptEditor() = default;

    // Set the entity and world to edit
    void SetTarget(ECS::World* world, ECS::Entity entity);

    // Clear target (e.g., when entity is deselected)
    void ClearTarget();

    // Main render function - call inside an ImGui window
    void Render(const EditorSettings& settings, bool isPlaying);

    // Query
    ECS::Entity GetTargetEntity() const { return m_TargetEntity; }
    bool HasTarget() const { return m_World != nullptr && m_TargetEntity != ECS::INVALID_ENTITY; }

private:
    // Sync graph from ECS component
    void SyncFromComponent();

    // Sync graph changes back to ECS component
    void SyncToComponent();

    // Setup callbacks for node graph editor
    void SetupCallbacks();

    // Add a node from definition
    NodeId AddNodeFromDefinition(const VisualScript::NodeDefinition* def, Math::Vector2 position);

    // Draw sidebar inspector
    void DrawInspector(bool isPlaying);

    // Draw entity sidebar (list of entities with VisualScriptComponent)
    void DrawEntitySidebar();

    // Draw toolbar (add event, add variable, fit all)
    void DrawToolbar();

    // Draw variable editor in inspector
    void DrawVariableEditor();

    // Draw node-specific properties in inspector
    void DrawNodeProperties();

    // Populate context menu with node categories
    void PopulateContextMenu();

    ECS::World* m_World = nullptr;
    ECS::Entity m_TargetEntity = ECS::INVALID_ENTITY;

    // Node graph framework instances
    NodeGraphData m_GraphData;
    NodeGraphEditor m_GraphEditor;
    NodeGraphCallbacks m_Callbacks;
    NodeGraphColors m_Colors;

    // Track whether we need to re-sync
    bool m_NeedsSync = true;

    // New variable creation
    char m_NewVarName[128] = "NewVariable";
    int m_NewVarType = 2;  // 0=Bool, 1=Int, 2=Float, 3=String, 4=Vector3, 5=Entity

    // Selected node for inspector
    NodeId m_SelectedNode = 0;

    // Node search for context menu
    char m_NodeSearchBuf[128] = "";

    // Mapping: node ID -> node type ID for execution
    std::unordered_map<NodeId, std::string> m_NodeTypeMap;
};

} // namespace Editor
} // namespace Enjin
