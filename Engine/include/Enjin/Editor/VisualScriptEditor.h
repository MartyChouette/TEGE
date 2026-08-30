#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Editor/NodeGraph.h"
#include "Enjin/Editor/EditorSettings.h"
#include "Enjin/Editor/UndoRedo.h"
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

    // Set the undo manager for graph edits
    void SetUndoManager(UndoRedoManager* manager) { m_UndoManager = manager; }

    // The editor's CURRENT hierarchy selection (may lack a VisualScript
    // component) - powers the "Add to selected entity" empty-state button.
    void SetEditorSelection(ECS::World* world, ECS::Entity e) {
        if (!m_World) m_World = world;
        m_EditorSelected = e;
    }

private:
    ECS::Entity m_EditorSelected = ECS::INVALID_ENTITY;
    bool m_OpenNodeSearch = false;   // toolbar "+ Node" requests the search popup
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

    // Draw debug controls toolbar (breakpoints, step, continue)
    void DrawDebugControls(bool isPlaying);

    // Draw execution timeline profiler
    void DrawExecutionTimeline();

    // Draw variable editor in inspector
    void DrawVariableEditor();

    // Draw node-specific properties in inspector
    void DrawNodeProperties();

    // Draw functions panel (subgraph management)
    void DrawFunctionsPanel();

    // Toggle breakpoint on a node
    void ToggleBreakpoint(NodeId nodeId);

    // Debug control actions
    void ContinueExecution();
    void StepOver();

    // Populate context menu with node categories
    void PopulateContextMenu();

    // Update node highlights for execution visualization (play mode)
    void UpdatePlayModeHighlight(bool isPlaying);

    // Clipboard operations
    void CopySelectedNodes();
    void CutSelectedNodes();
    void PasteNodes();
    void DeleteSelectedNodes();
    void HandleKeyboardShortcuts();

public:
    // Undo/redo helper methods (called by commands, need public access)
    void RemoveNodeById(NodeId nodeId);
    void RestoreNodeFromJson(NodeId nodeId, const std::string& json);
    void AddLinkById(PinId startPin, PinId endPin, LinkId linkId);
    void RemoveLinkById(LinkId linkId);
    std::string SerializeNodeToJson(NodeId nodeId);

    // Property edit helpers for undo (public for command classes)
    void SetNodePropertyValue(NodeId nodeId, const std::string& propertyName, const std::string& value);
    void SetVariableValue(const std::string& varName, const std::string& value);

    // Get the target script component
    ECS::VisualScriptComponent* GetTargetScript();

private:

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

    // Function editing state
    char m_NewFuncName[128] = "NewFunction";
    i32 m_EditingFunctionIndex = -1;  // -1 = editing main graph
    std::string m_FunctionBreadcrumb;  // For breadcrumb display

    // Selected node for inspector
    NodeId m_SelectedNode = 0;

    // Node search for context menu
    char m_NodeSearchBuf[256] = "";
    int m_NodeSearchSelectedIndex = 0;
    Math::Vector2 m_ContextMenuPos;

    // Reference to editor settings for recently-used tracking
    EditorSettings* m_EditorSettings = nullptr;

    // Reference to undo manager for graph edits
    UndoRedoManager* m_UndoManager = nullptr;

    // Mapping: node ID -> node type ID for execution
    std::unordered_map<NodeId, std::string> m_NodeTypeMap;

    // Draw the searchable node popup
    void DrawNodeSearchPopup();

    // Clipboard state
    std::string m_ClipboardJson;
    std::vector<NodeId> m_ClipboardNodeIds;
    bool m_ClipboardIsCut = false;
};

} // namespace Editor
} // namespace Enjin
