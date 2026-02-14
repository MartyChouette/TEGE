#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Editor/NodeGraph.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Editor/EditorSettings.h"
#include <string>

namespace Enjin {
namespace Editor {

// Visual editor for StateMachineComponent and AnimatorComponent state machines.
// Uses the generic NodeGraphEditor framework to render states as nodes and
// transitions as links. Syncs bidirectionally with the ECS component data.
class ENJIN_API AnimationGraphEditor {
public:
    AnimationGraphEditor() = default;

    // Set the entity and world to edit
    void SetTarget(ECS::World* world, ECS::Entity entity);

    // Clear target (e.g., when entity is deselected)
    void ClearTarget();

    // Main render function — call inside an ImGui window (between Begin/End)
    void Render(const EditorSettings& settings, bool isPlaying);

    // Query
    ECS::Entity GetTargetEntity() const { return m_TargetEntity; }
    bool HasTarget() const { return m_World != nullptr && m_TargetEntity != ECS::INVALID_ENTITY; }

private:
    // Sync graph data from ECS component
    void SyncFromComponent();

    // Apply graph changes back to ECS component
    void SyncToComponent();

    // Build callbacks for the node graph editor
    void SetupCallbacks();

    // Draw the sidebar inspector panel
    void DrawInspector(bool isPlaying);
    void DrawInspectorAnimatorMode(NodeId selectedNode, LinkId selectedLink, bool isPlaying);
    void DrawInspectorSMMode(NodeId selectedNode, LinkId selectedLink, bool isPlaying);

    // Draw toolbar (add state, auto layout, fit all)
    void DrawToolbar();

    // Auto-arrange nodes in a grid layout
    void AutoLayout();

    // Highlight the current active state during play mode
    void UpdatePlayModeHighlight();

    ECS::World* m_World = nullptr;
    ECS::Entity m_TargetEntity = ECS::INVALID_ENTITY;

    // Node graph framework instances
    NodeGraphData m_GraphData;
    NodeGraphEditor m_GraphEditor;
    NodeGraphCallbacks m_Callbacks;
    NodeGraphColors m_Colors;

    // Mapping between graph node IDs and state names
    // Node userData stores index into this vector
    std::vector<std::string> m_StateNames;

    // Mapping: state name -> node ID
    std::unordered_map<std::string, NodeId> m_StateToNodeId;

    // Entry node ID (pseudo-node representing the default/initial state)
    NodeId m_EntryNodeId = 0;

    // Track whether we need to re-sync
    bool m_NeedsSync = true;

    // True when editing AnimatorComponent's AnimationStateMachine (animation clips),
    // false when editing StateMachineComponent (game logic SM)
    bool m_IsAnimatorMode = false;

    // New state name buffer
    char m_NewStateName[128] = "NewState";

    // New parameter buffers
    char m_NewParamName[128] = "";
    int m_NewParamType = 0;  // 0=Bool, 1=Float, 2=Int

    // Selected transition inspector state
    struct SelectedTransition {
        std::string fromState;
        std::string toState;
        bool valid = false;
    };
    SelectedTransition m_SelectedTransition;

    // New condition state
    int m_NewCondType = 0;
    char m_NewCondParam[128] = "";
    float m_NewCondThreshold = 0.0f;
    int m_NewCondIntValue = 0;

    // Trigger send buffer (play mode)
    char m_TriggerNameBuf[128] = "";
};

} // namespace Editor
} // namespace Enjin
