#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <nlohmann/json_fwd.hpp>

namespace Enjin::GUI {

    enum class DialogueNodeType : u8 {
        Text,
        Choice,
        Condition,
        SetVariable,
        Event,
        Root,
        End,
        QuestAction,      // Start/complete/fail a quest directly from dialogue
        PlayCinematic,     // Trigger a cinematic camera entity
        SetGameFlag        // Set a persistent game-wide flag
    };

    struct DialogueCondition {
        // Source determines what the condition checks against
        enum class Source : u8 {
            Variable,      // Dialogue variable (existing behavior)
            QuestStatus,   // Quest state: variable=questId, value="active"/"complete"/"failed"/"notstarted"
            GameFlag       // Persistent game flag from save system
        };
        Source source = Source::Variable;

        std::string variable;  // Variable name, quest ID, or flag key (depending on source)

        enum class Op : u8 {
            Equals,
            NotEquals,
            GreaterThan,
            LessThan
        };

        Op op = Op::Equals;
        std::string value;
    };

    struct DialogueChoice {
        std::string text;
        u32 targetNodeId = 0;
        DialogueCondition showCondition;
        bool hasCondition = false;
    };

    struct DialogueNode {
        u32 id = 0;
        DialogueNodeType type = DialogueNodeType::Text;

        std::string speakerName;
        std::string text;
        Math::Vector3 speakerColor = Math::Vector3(1, 1, 1);

        std::vector<DialogueChoice> choices;

        u32 nextNodeId = 0;

        DialogueCondition condition;
        u32 trueNodeId = 0;
        u32 falseNodeId = 0;

        std::string variableName;
        std::string variableValue;

        std::string eventName;

        // QuestAction node data
        enum class QuestActionType : u8 { StartQuest, CompleteObjective, FailQuest };
        QuestActionType questAction = QuestActionType::StartQuest;
        std::string questId;
        i32 questObjectiveIndex = 0;

        // PlayCinematic node data
        std::string cinematicEntityName;

        // SetGameFlag node data
        std::string gameFlagKey;
        std::string gameFlagValue;

        Math::Vector2 editorPosition = Math::Vector2(0, 0);
    };

    struct DialogueTreeData {
        std::string treeName;
        u32 rootNodeId = 0;
        std::vector<DialogueNode> nodes;
        u32 nextId = 1;

        u32 AddNode(DialogueNodeType type);
        void RemoveNode(u32 id);
        DialogueNode* GetNode(u32 id);

        nlohmann::json ToJson() const;
        static DialogueTreeData FromJson(const nlohmann::json& j);
    };

    class ENJIN_API DialoguePlayer {
    public:
        void Start(const DialogueTreeData& tree);
        void SelectChoice(u32 choiceIndex);
        void Advance();

        bool IsActive() const;
        bool IsWaitingForInput() const;
        const DialogueNode* GetCurrentNode() const;
        std::vector<DialogueChoice> GetAvailableChoices() const;

        void SetVariable(const std::string& name, const std::string& value);
        std::string GetVariable(const std::string& name) const;

        using EventCallback = std::function<void(const std::string&)>;
        void SetEventCallback(EventCallback cb);

        // Action callback: fired for QuestAction, PlayCinematic, SetGameFlag nodes
        using ActionCallback = std::function<void(const DialogueNode&)>;
        void SetActionCallback(ActionCallback cb);

        // Condition resolver: called for non-Variable condition sources (QuestStatus, GameFlag)
        using ConditionResolver = std::function<bool(const DialogueCondition&)>;
        void SetConditionResolver(ConditionResolver resolver);

    private:
        DialogueTreeData m_Tree;
        u32 m_CurrentNodeId = 0;
        bool m_Active = false;
        bool m_WaitingForInput = false;
        std::unordered_map<std::string, std::string> m_Variables;
        EventCallback m_EventCallback;
        ActionCallback m_ActionCallback;
        ConditionResolver m_ConditionResolver;

        void ProcessNode(u32 depth = 0);
        bool EvaluateCondition(const DialogueCondition& cond) const;
    };

    class ENJIN_API DialogueTreeEditor {
    public:
        void Render();
        void SetTree(DialogueTreeData* tree);

        bool IsOpen() const;
        void SetOpen(bool open);

    private:
        DialogueTreeData* m_Tree = nullptr;
        bool m_Open = false;
        u32 m_SelectedNodeId = 0;
        Math::Vector2 m_ScrollOffset;
        f32 m_Zoom = 1.0f;

        void DrawNode(DialogueNode& node);
        void DrawConnections();
        void DrawInspector();

        // In-editor dialogue preview (walk through tree without play mode)
        DialoguePlayer m_PreviewPlayer;
        bool m_PreviewActive = false;
        void DrawPreviewPanel();
    };

} // namespace Enjin::GUI
