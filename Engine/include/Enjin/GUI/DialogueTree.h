#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

namespace Enjin::GUI {

    enum class DialogueNodeType : u8 {
        Text,
        Choice,
        Condition,
        SetVariable,
        Event,
        Root,
        End
    };

    struct DialogueCondition {
        std::string variable;

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

    private:
        DialogueTreeData m_Tree;
        u32 m_CurrentNodeId = 0;
        bool m_Active = false;
        bool m_WaitingForInput = false;
        std::unordered_map<std::string, std::string> m_Variables;
        EventCallback m_EventCallback;

        void ProcessNode();
        bool EvaluateCondition(const DialogueCondition& cond);
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
    };

} // namespace Enjin::GUI
