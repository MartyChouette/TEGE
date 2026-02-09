#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Editor/NodeGraph.h"
#include "Enjin/AI/BehaviorTree.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Editor/EditorSettings.h"
#include <string>
#include <unordered_map>

namespace Enjin {
namespace Editor {

class ENJIN_API BehaviorTreeEditor {
public:
    BehaviorTreeEditor() = default;

    void SetTarget(ECS::World* world, ECS::Entity entity);
    void ClearTarget();
    void Render(const EditorSettings& settings, bool isPlaying);

    ECS::Entity GetTargetEntity() const { return m_TargetEntity; }
    bool HasTarget() const { return m_World != nullptr && m_TargetEntity != ECS::INVALID_ENTITY; }

private:
    void SyncFromComponent();
    void SyncToComponent();
    void SetupCallbacks();
    void DrawInspector(bool isPlaying);
    void DrawToolbar();
    void AutoLayout();
    void UpdatePlayModeHighlight();

    // Node creation helpers
    NodeId CreateBTNode(AI::BTNodeType type, Math::Vector2 position);

    ECS::World* m_World = nullptr;
    ECS::Entity m_TargetEntity = ECS::INVALID_ENTITY;

    NodeGraphData m_GraphData;
    NodeGraphEditor m_GraphEditor;
    NodeGraphCallbacks m_Callbacks;
    NodeGraphColors m_Colors;

    bool m_NeedsSync = true;

    // Property editing buffers
    char m_PropKeyBuf[128] = "";
    char m_PropValueBuf[256] = "";

    // Blackboard editing
    char m_NewBBKey[128] = "";
    int m_NewBBType = 0;  // 0=bool, 1=int, 2=float, 3=string
};

} // namespace Editor
} // namespace Enjin
