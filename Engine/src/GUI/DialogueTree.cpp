#include "Enjin/GUI/DialogueTree.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>
#include <algorithm>

namespace Enjin::GUI {

    // -------------------------------------------------------------------------
    // DialogueTreeData
    // -------------------------------------------------------------------------

    u32 DialogueTreeData::AddNode(DialogueNodeType type) {
        DialogueNode node;
        node.id = nextId++;
        node.type = type;
        nodes.push_back(node);
        return node.id;
    }

    void DialogueTreeData::RemoveNode(u32 id) {
        nodes.erase(
            std::remove_if(nodes.begin(), nodes.end(),
                [id](const DialogueNode& n) { return n.id == id; }),
            nodes.end());
    }

    DialogueNode* DialogueTreeData::GetNode(u32 id) {
        for (auto& node : nodes) {
            if (node.id == id) return &node;
        }
        return nullptr;
    }

    static const char* NodeTypeToString(DialogueNodeType type) {
        switch (type) {
            case DialogueNodeType::Text:        return "Text";
            case DialogueNodeType::Choice:      return "Choice";
            case DialogueNodeType::Condition:   return "Condition";
            case DialogueNodeType::SetVariable: return "SetVariable";
            case DialogueNodeType::Event:       return "Event";
            case DialogueNodeType::Root:        return "Root";
            case DialogueNodeType::End:         return "End";
            default:                            return "Unknown";
        }
    }

    static DialogueNodeType StringToNodeType(const std::string& s) {
        if (s == "Text")        return DialogueNodeType::Text;
        if (s == "Choice")      return DialogueNodeType::Choice;
        if (s == "Condition")   return DialogueNodeType::Condition;
        if (s == "SetVariable") return DialogueNodeType::SetVariable;
        if (s == "Event")       return DialogueNodeType::Event;
        if (s == "Root")        return DialogueNodeType::Root;
        if (s == "End")         return DialogueNodeType::End;
        return DialogueNodeType::Text;
    }

    static const char* ConditionOpToString(DialogueCondition::Op op) {
        switch (op) {
            case DialogueCondition::Op::Equals:      return "Equals";
            case DialogueCondition::Op::NotEquals:    return "NotEquals";
            case DialogueCondition::Op::GreaterThan:  return "GreaterThan";
            case DialogueCondition::Op::LessThan:     return "LessThan";
            default:                                  return "Equals";
        }
    }

    static DialogueCondition::Op StringToConditionOp(const std::string& s) {
        if (s == "NotEquals")    return DialogueCondition::Op::NotEquals;
        if (s == "GreaterThan")  return DialogueCondition::Op::GreaterThan;
        if (s == "LessThan")    return DialogueCondition::Op::LessThan;
        return DialogueCondition::Op::Equals;
    }

    static nlohmann::json ConditionToJson(const DialogueCondition& cond) {
        return {
            {"variable", cond.variable},
            {"op", ConditionOpToString(cond.op)},
            {"value", cond.value}
        };
    }

    static DialogueCondition ConditionFromJson(const nlohmann::json& j) {
        DialogueCondition cond;
        if (j.contains("variable")) cond.variable = j["variable"].get<std::string>();
        if (j.contains("op"))       cond.op = StringToConditionOp(j["op"].get<std::string>());
        if (j.contains("value"))    cond.value = j["value"].get<std::string>();
        return cond;
    }

    nlohmann::json DialogueTreeData::ToJson() const {
        nlohmann::json j;
        j["treeName"] = treeName;
        j["rootNodeId"] = rootNodeId;
        j["nextId"] = nextId;

        auto& jNodes = j["nodes"];
        jNodes = nlohmann::json::array();

        for (const auto& node : nodes) {
            nlohmann::json jn;
            jn["id"] = node.id;
            jn["type"] = NodeTypeToString(node.type);
            jn["speakerName"] = node.speakerName;
            jn["text"] = node.text;
            jn["speakerColor"] = {node.speakerColor.x, node.speakerColor.y, node.speakerColor.z};
            jn["nextNodeId"] = node.nextNodeId;
            jn["trueNodeId"] = node.trueNodeId;
            jn["falseNodeId"] = node.falseNodeId;
            jn["variableName"] = node.variableName;
            jn["variableValue"] = node.variableValue;
            jn["eventName"] = node.eventName;
            jn["editorPosition"] = {node.editorPosition.x, node.editorPosition.y};
            jn["condition"] = ConditionToJson(node.condition);

            auto& jChoices = jn["choices"];
            jChoices = nlohmann::json::array();
            for (const auto& choice : node.choices) {
                nlohmann::json jc;
                jc["text"] = choice.text;
                jc["targetNodeId"] = choice.targetNodeId;
                jc["hasCondition"] = choice.hasCondition;
                jc["showCondition"] = ConditionToJson(choice.showCondition);
                jChoices.push_back(jc);
            }

            jNodes.push_back(jn);
        }

        return j;
    }

    DialogueTreeData DialogueTreeData::FromJson(const nlohmann::json& j) {
        DialogueTreeData data;

        if (j.contains("treeName"))   data.treeName = j["treeName"].get<std::string>();
        if (j.contains("rootNodeId")) data.rootNodeId = j["rootNodeId"].get<u32>();
        if (j.contains("nextId"))     data.nextId = j["nextId"].get<u32>();

        if (j.contains("nodes") && j["nodes"].is_array()) {
            for (const auto& jn : j["nodes"]) {
                DialogueNode node;
                if (jn.contains("id"))            node.id = jn["id"].get<u32>();
                if (jn.contains("type"))          node.type = StringToNodeType(jn["type"].get<std::string>());
                if (jn.contains("speakerName"))   node.speakerName = jn["speakerName"].get<std::string>();
                if (jn.contains("text"))          node.text = jn["text"].get<std::string>();
                if (jn.contains("nextNodeId"))    node.nextNodeId = jn["nextNodeId"].get<u32>();
                if (jn.contains("trueNodeId"))    node.trueNodeId = jn["trueNodeId"].get<u32>();
                if (jn.contains("falseNodeId"))   node.falseNodeId = jn["falseNodeId"].get<u32>();
                if (jn.contains("variableName"))  node.variableName = jn["variableName"].get<std::string>();
                if (jn.contains("variableValue")) node.variableValue = jn["variableValue"].get<std::string>();
                if (jn.contains("eventName"))     node.eventName = jn["eventName"].get<std::string>();

                if (jn.contains("speakerColor") && jn["speakerColor"].is_array() && jn["speakerColor"].size() >= 3) {
                    node.speakerColor.x = jn["speakerColor"][0].get<f32>();
                    node.speakerColor.y = jn["speakerColor"][1].get<f32>();
                    node.speakerColor.z = jn["speakerColor"][2].get<f32>();
                }

                if (jn.contains("editorPosition") && jn["editorPosition"].is_array() && jn["editorPosition"].size() >= 2) {
                    node.editorPosition.x = jn["editorPosition"][0].get<f32>();
                    node.editorPosition.y = jn["editorPosition"][1].get<f32>();
                }

                if (jn.contains("condition")) {
                    node.condition = ConditionFromJson(jn["condition"]);
                }

                if (jn.contains("choices") && jn["choices"].is_array()) {
                    for (const auto& jc : jn["choices"]) {
                        DialogueChoice choice;
                        if (jc.contains("text"))          choice.text = jc["text"].get<std::string>();
                        if (jc.contains("targetNodeId"))  choice.targetNodeId = jc["targetNodeId"].get<u32>();
                        if (jc.contains("hasCondition"))  choice.hasCondition = jc["hasCondition"].get<bool>();
                        if (jc.contains("showCondition")) choice.showCondition = ConditionFromJson(jc["showCondition"]);
                        node.choices.push_back(choice);
                    }
                }

                data.nodes.push_back(node);
            }
        }

        return data;
    }

    // -------------------------------------------------------------------------
    // DialoguePlayer
    // -------------------------------------------------------------------------

    void DialoguePlayer::Start(const DialogueTreeData& tree) {
        m_Tree = tree;
        m_CurrentNodeId = tree.rootNodeId;
        m_Active = true;
        m_WaitingForInput = false;
        ProcessNode();
    }

    void DialoguePlayer::SelectChoice(u32 choiceIndex) {
        if (!m_Active || !m_WaitingForInput) return;

        auto available = GetAvailableChoices();
        if (choiceIndex >= static_cast<u32>(available.size())) return;

        m_CurrentNodeId = available[choiceIndex].targetNodeId;
        m_WaitingForInput = false;
        ProcessNode();
    }

    void DialoguePlayer::Advance() {
        if (!m_Active || !m_WaitingForInput) return;

        const DialogueNode* current = GetCurrentNode();
        if (!current) {
            m_Active = false;
            return;
        }

        m_CurrentNodeId = current->nextNodeId;
        m_WaitingForInput = false;
        ProcessNode();
    }

    bool DialoguePlayer::IsActive() const {
        return m_Active;
    }

    bool DialoguePlayer::IsWaitingForInput() const {
        return m_WaitingForInput;
    }

    const DialogueNode* DialoguePlayer::GetCurrentNode() const {
        for (const auto& node : m_Tree.nodes) {
            if (node.id == m_CurrentNodeId) return &node;
        }
        return nullptr;
    }

    std::vector<DialogueChoice> DialoguePlayer::GetAvailableChoices() const {
        const DialogueNode* current = GetCurrentNode();
        if (!current) return {};

        std::vector<DialogueChoice> available;
        for (const auto& choice : current->choices) {
            if (!choice.hasCondition || const_cast<DialoguePlayer*>(this)->EvaluateCondition(choice.showCondition)) {
                available.push_back(choice);
            }
        }
        return available;
    }

    void DialoguePlayer::SetVariable(const std::string& name, const std::string& value) {
        m_Variables[name] = value;
    }

    std::string DialoguePlayer::GetVariable(const std::string& name) const {
        auto it = m_Variables.find(name);
        if (it != m_Variables.end()) return it->second;
        return "";
    }

    void DialoguePlayer::SetEventCallback(EventCallback cb) {
        m_EventCallback = std::move(cb);
    }

    void DialoguePlayer::ProcessNode() {
        if (!m_Active) return;

        const DialogueNode* node = GetCurrentNode();
        if (!node) {
            m_Active = false;
            return;
        }

        switch (node->type) {
            case DialogueNodeType::Text: {
                m_WaitingForInput = true;
                break;
            }

            case DialogueNodeType::Choice: {
                m_WaitingForInput = true;
                break;
            }

            case DialogueNodeType::Condition: {
                bool result = EvaluateCondition(node->condition);
                m_CurrentNodeId = result ? node->trueNodeId : node->falseNodeId;
                ProcessNode();
                break;
            }

            case DialogueNodeType::SetVariable: {
                m_Variables[node->variableName] = node->variableValue;
                m_CurrentNodeId = node->nextNodeId;
                ProcessNode();
                break;
            }

            case DialogueNodeType::Event: {
                if (m_EventCallback) {
                    m_EventCallback(node->eventName);
                }
                m_CurrentNodeId = node->nextNodeId;
                ProcessNode();
                break;
            }

            case DialogueNodeType::Root: {
                m_CurrentNodeId = node->nextNodeId;
                ProcessNode();
                break;
            }

            case DialogueNodeType::End: {
                m_Active = false;
                m_WaitingForInput = false;
                break;
            }
        }
    }

    bool DialoguePlayer::EvaluateCondition(const DialogueCondition& cond) {
        std::string varValue = GetVariable(cond.variable);

        switch (cond.op) {
            case DialogueCondition::Op::Equals:
                return varValue == cond.value;

            case DialogueCondition::Op::NotEquals:
                return varValue != cond.value;

            case DialogueCondition::Op::GreaterThan: {
                try {
                    f64 a = std::stod(varValue);
                    f64 b = std::stod(cond.value);
                    return a > b;
                } catch (const std::exception&) {
                    // Non-numeric values, fall back to string comparison
                    return varValue > cond.value;
                }
            }

            case DialogueCondition::Op::LessThan: {
                try {
                    f64 a = std::stod(varValue);
                    f64 b = std::stod(cond.value);
                    return a < b;
                } catch (const std::exception&) {
                    // Non-numeric values, fall back to string comparison
                    return varValue < cond.value;
                }
            }
        }

        return false;
    }

    // -------------------------------------------------------------------------
    // DialogueTreeEditor
    // -------------------------------------------------------------------------

    static const f32 NODE_WIDTH = 180.0f;
    static const f32 NODE_HEADER_HEIGHT = 24.0f;
    static const f32 NODE_LINE_HEIGHT = 18.0f;
    static const f32 NODE_PADDING = 8.0f;
    static const f32 CONNECTION_THICKNESS = 2.0f;
    static const f32 GRID_SIZE = 32.0f;
    static const f32 INSPECTOR_WIDTH = 280.0f;

    static ImU32 GetNodeColor(DialogueNodeType type) {
        switch (type) {
            case DialogueNodeType::Text:        return IM_COL32(70, 130, 180, 255);
            case DialogueNodeType::Choice:      return IM_COL32(60, 179, 113, 255);
            case DialogueNodeType::Condition:   return IM_COL32(218, 165, 32, 255);
            case DialogueNodeType::SetVariable: return IM_COL32(147, 112, 219, 255);
            case DialogueNodeType::Event:       return IM_COL32(205, 92, 92, 255);
            case DialogueNodeType::Root:        return IM_COL32(100, 200, 100, 255);
            case DialogueNodeType::End:         return IM_COL32(169, 169, 169, 255);
            default:                            return IM_COL32(128, 128, 128, 255);
        }
    }

    static f32 GetNodeHeight(const DialogueNode& node) {
        f32 height = NODE_HEADER_HEIGHT + NODE_PADDING * 2.0f;
        switch (node.type) {
            case DialogueNodeType::Text:
                height += NODE_LINE_HEIGHT * 2.0f;
                break;
            case DialogueNodeType::Choice:
                height += NODE_LINE_HEIGHT * (1.0f + static_cast<f32>(node.choices.size()));
                break;
            case DialogueNodeType::Condition:
                height += NODE_LINE_HEIGHT * 2.0f;
                break;
            case DialogueNodeType::SetVariable:
                height += NODE_LINE_HEIGHT * 2.0f;
                break;
            case DialogueNodeType::Event:
                height += NODE_LINE_HEIGHT;
                break;
            case DialogueNodeType::Root:
            case DialogueNodeType::End:
                break;
            default:
                break;
        }
        return height;
    }

    void DialogueTreeEditor::Render() {
        if (!m_Open || !m_Tree) return;

        ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Dialogue Tree Editor", &m_Open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            ImGui::End();
            return;
        }

        // Toolbar
        if (ImGui::Button("Add Text"))        { u32 id = m_Tree->AddNode(DialogueNodeType::Text); auto* n = m_Tree->GetNode(id); if (n) n->editorPosition = Math::Vector2(-m_ScrollOffset.x + 200.0f, -m_ScrollOffset.y + 200.0f); }
        ImGui::SameLine();
        if (ImGui::Button("Add Choice"))      { u32 id = m_Tree->AddNode(DialogueNodeType::Choice); auto* n = m_Tree->GetNode(id); if (n) n->editorPosition = Math::Vector2(-m_ScrollOffset.x + 200.0f, -m_ScrollOffset.y + 200.0f); }
        ImGui::SameLine();
        if (ImGui::Button("Add Condition"))   { u32 id = m_Tree->AddNode(DialogueNodeType::Condition); auto* n = m_Tree->GetNode(id); if (n) n->editorPosition = Math::Vector2(-m_ScrollOffset.x + 200.0f, -m_ScrollOffset.y + 200.0f); }
        ImGui::SameLine();
        if (ImGui::Button("Add SetVar"))      { u32 id = m_Tree->AddNode(DialogueNodeType::SetVariable); auto* n = m_Tree->GetNode(id); if (n) n->editorPosition = Math::Vector2(-m_ScrollOffset.x + 200.0f, -m_ScrollOffset.y + 200.0f); }
        ImGui::SameLine();
        if (ImGui::Button("Add Event"))       { u32 id = m_Tree->AddNode(DialogueNodeType::Event); auto* n = m_Tree->GetNode(id); if (n) n->editorPosition = Math::Vector2(-m_ScrollOffset.x + 200.0f, -m_ScrollOffset.y + 200.0f); }
        ImGui::SameLine();
        if (ImGui::Button("Add End"))         { u32 id = m_Tree->AddNode(DialogueNodeType::End); auto* n = m_Tree->GetNode(id); if (n) n->editorPosition = Math::Vector2(-m_ScrollOffset.x + 200.0f, -m_ScrollOffset.y + 200.0f); }

        ImGui::Separator();

        // Main layout: canvas on the left, inspector on the right
        ImVec2 contentRegion = ImGui::GetContentRegionAvail();
        f32 canvasWidth = contentRegion.x - INSPECTOR_WIDTH - 4.0f;
        if (canvasWidth < 200.0f) canvasWidth = contentRegion.x;

        // Canvas
        ImGui::BeginChild("DialogueCanvas", ImVec2(canvasWidth, 0), true,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove);

        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Clip to canvas
        drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

        // Draw grid
        for (f32 x = fmodf(m_ScrollOffset.x, GRID_SIZE * m_Zoom); x < canvasSize.x; x += GRID_SIZE * m_Zoom) {
            drawList->AddLine(
                ImVec2(canvasPos.x + x, canvasPos.y),
                ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y),
                IM_COL32(50, 50, 50, 80));
        }
        for (f32 y = fmodf(m_ScrollOffset.y, GRID_SIZE * m_Zoom); y < canvasSize.y; y += GRID_SIZE * m_Zoom) {
            drawList->AddLine(
                ImVec2(canvasPos.x, canvasPos.y + y),
                ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y),
                IM_COL32(50, 50, 50, 80));
        }

        // Draw connections first (behind nodes)
        DrawConnections();

        // Draw nodes
        for (auto& node : m_Tree->nodes) {
            DrawNode(node);
        }

        // Handle canvas panning (middle mouse or right mouse drag on empty space)
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            m_ScrollOffset.x += delta.x;
            m_ScrollOffset.y += delta.y;
        }

        // Handle zoom with scroll wheel
        if (ImGui::IsWindowHovered()) {
            f32 scroll = ImGui::GetIO().MouseWheel;
            if (scroll != 0.0f) {
                f32 oldZoom = m_Zoom;
                m_Zoom += scroll * 0.1f;
                if (m_Zoom < 0.25f) m_Zoom = 0.25f;
                if (m_Zoom > 3.0f) m_Zoom = 3.0f;

                // Zoom toward mouse position
                ImVec2 mousePos = ImGui::GetMousePos();
                f32 mx = mousePos.x - canvasPos.x;
                f32 my = mousePos.y - canvasPos.y;
                m_ScrollOffset.x = mx - (mx - m_ScrollOffset.x) * (m_Zoom / oldZoom);
                m_ScrollOffset.y = my - (my - m_ScrollOffset.y) * (m_Zoom / oldZoom);
            }
        }

        // Right-click context menu
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("CanvasContextMenu");
        }

        if (ImGui::BeginPopup("CanvasContextMenu")) {
            ImVec2 mousePos = ImGui::GetMousePos();
            f32 spawnX = (mousePos.x - canvasPos.x - m_ScrollOffset.x) / m_Zoom;
            f32 spawnY = (mousePos.y - canvasPos.y - m_ScrollOffset.y) / m_Zoom;

            if (ImGui::MenuItem("Add Text Node")) {
                u32 id = m_Tree->AddNode(DialogueNodeType::Text);
                auto* n = m_Tree->GetNode(id);
                if (n) n->editorPosition = Math::Vector2(spawnX, spawnY);
            }
            if (ImGui::MenuItem("Add Choice Node")) {
                u32 id = m_Tree->AddNode(DialogueNodeType::Choice);
                auto* n = m_Tree->GetNode(id);
                if (n) n->editorPosition = Math::Vector2(spawnX, spawnY);
            }
            if (ImGui::MenuItem("Add Condition Node")) {
                u32 id = m_Tree->AddNode(DialogueNodeType::Condition);
                auto* n = m_Tree->GetNode(id);
                if (n) n->editorPosition = Math::Vector2(spawnX, spawnY);
            }
            if (ImGui::MenuItem("Add SetVariable Node")) {
                u32 id = m_Tree->AddNode(DialogueNodeType::SetVariable);
                auto* n = m_Tree->GetNode(id);
                if (n) n->editorPosition = Math::Vector2(spawnX, spawnY);
            }
            if (ImGui::MenuItem("Add Event Node")) {
                u32 id = m_Tree->AddNode(DialogueNodeType::Event);
                auto* n = m_Tree->GetNode(id);
                if (n) n->editorPosition = Math::Vector2(spawnX, spawnY);
            }
            if (ImGui::MenuItem("Add Root Node")) {
                u32 id = m_Tree->AddNode(DialogueNodeType::Root);
                auto* n = m_Tree->GetNode(id);
                if (n) n->editorPosition = Math::Vector2(spawnX, spawnY);
                if (m_Tree->rootNodeId == 0) m_Tree->rootNodeId = id;
            }
            if (ImGui::MenuItem("Add End Node")) {
                u32 id = m_Tree->AddNode(DialogueNodeType::End);
                auto* n = m_Tree->GetNode(id);
                if (n) n->editorPosition = Math::Vector2(spawnX, spawnY);
            }
            ImGui::Separator();
            if (m_SelectedNodeId != 0 && ImGui::MenuItem("Delete Selected Node")) {
                m_Tree->RemoveNode(m_SelectedNodeId);
                m_SelectedNodeId = 0;
            }
            ImGui::EndPopup();
        }

        // Click on empty space to deselect
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            bool clickedOnNode = false;
            for (const auto& node : m_Tree->nodes) {
                f32 nx = canvasPos.x + node.editorPosition.x * m_Zoom + m_ScrollOffset.x;
                f32 ny = canvasPos.y + node.editorPosition.y * m_Zoom + m_ScrollOffset.y;
                f32 nw = NODE_WIDTH * m_Zoom;
                f32 nh = GetNodeHeight(node) * m_Zoom;
                ImVec2 mouseP = ImGui::GetMousePos();
                if (mouseP.x >= nx && mouseP.x <= nx + nw && mouseP.y >= ny && mouseP.y <= ny + nh) {
                    clickedOnNode = true;
                    break;
                }
            }
            if (!clickedOnNode) {
                m_SelectedNodeId = 0;
            }
        }

        drawList->PopClipRect();

        ImGui::EndChild();

        // Inspector panel on the right
        if (canvasWidth < contentRegion.x) {
            ImGui::SameLine();
            ImGui::BeginChild("DialogueInspector", ImVec2(INSPECTOR_WIDTH, 0), true);
            DrawInspector();
            ImGui::EndChild();
        }

        ImGui::End();
    }

    void DialogueTreeEditor::SetTree(DialogueTreeData* tree) {
        m_Tree = tree;
        m_SelectedNodeId = 0;
    }

    bool DialogueTreeEditor::IsOpen() const {
        return m_Open;
    }

    void DialogueTreeEditor::SetOpen(bool open) {
        m_Open = open;
    }

    void DialogueTreeEditor::DrawNode(DialogueNode& node) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();

        f32 x = canvasPos.x + node.editorPosition.x * m_Zoom + m_ScrollOffset.x;
        f32 y = canvasPos.y + node.editorPosition.y * m_Zoom + m_ScrollOffset.y;
        f32 w = NODE_WIDTH * m_Zoom;
        f32 h = GetNodeHeight(node) * m_Zoom;

        ImVec2 nodeMin(x, y);
        ImVec2 nodeMax(x + w, y + h);
        ImVec2 headerMax(x + w, y + NODE_HEADER_HEIGHT * m_Zoom);

        ImU32 nodeColor = GetNodeColor(node.type);
        bool isSelected = (node.id == m_SelectedNodeId);

        // Node body
        drawList->AddRectFilled(nodeMin, nodeMax, IM_COL32(40, 40, 40, 230), 4.0f * m_Zoom);

        // Node header
        drawList->AddRectFilled(nodeMin, headerMax, nodeColor, 4.0f * m_Zoom, ImDrawFlags_RoundCornersTop);

        // Selection border
        if (isSelected) {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(255, 255, 100, 255), 4.0f * m_Zoom, 0, 2.0f * m_Zoom);
        } else {
            drawList->AddRect(nodeMin, nodeMax, IM_COL32(80, 80, 80, 255), 4.0f * m_Zoom);
        }

        // Header text
        char headerBuf[128];
        snprintf(headerBuf, sizeof(headerBuf), "[%u] %s", node.id, NodeTypeToString(node.type));
        drawList->AddText(ImVec2(x + 4.0f * m_Zoom, y + 4.0f * m_Zoom), IM_COL32(255, 255, 255, 255), headerBuf);

        // Body text preview
        f32 textY = y + NODE_HEADER_HEIGHT * m_Zoom + NODE_PADDING * m_Zoom;
        f32 fontSize = ImGui::GetFontSize();

        switch (node.type) {
            case DialogueNodeType::Text: {
                if (!node.speakerName.empty()) {
                    drawList->AddText(ImVec2(x + 4.0f * m_Zoom, textY), IM_COL32(200, 200, 255, 255), node.speakerName.c_str());
                    textY += NODE_LINE_HEIGHT * m_Zoom;
                }
                std::string preview = node.text.substr(0, 24);
                if (node.text.size() > 24) preview += "...";
                drawList->AddText(ImVec2(x + 4.0f * m_Zoom, textY), IM_COL32(180, 180, 180, 255), preview.c_str());
                break;
            }
            case DialogueNodeType::Choice: {
                drawList->AddText(ImVec2(x + 4.0f * m_Zoom, textY), IM_COL32(200, 200, 200, 255), "Choices:");
                textY += NODE_LINE_HEIGHT * m_Zoom;
                for (usize i = 0; i < node.choices.size() && i < 5; ++i) {
                    std::string choiceText = "> " + node.choices[i].text.substr(0, 20);
                    if (node.choices[i].text.size() > 20) choiceText += "...";
                    drawList->AddText(ImVec2(x + 8.0f * m_Zoom, textY), IM_COL32(160, 220, 160, 255), choiceText.c_str());
                    textY += NODE_LINE_HEIGHT * m_Zoom;
                }
                break;
            }
            case DialogueNodeType::Condition: {
                char condBuf[128];
                snprintf(condBuf, sizeof(condBuf), "if %s %s %s", node.condition.variable.c_str(),
                    ConditionOpToString(node.condition.op), node.condition.value.c_str());
                drawList->AddText(ImVec2(x + 4.0f * m_Zoom, textY), IM_COL32(220, 200, 100, 255), condBuf);
                textY += NODE_LINE_HEIGHT * m_Zoom;
                drawList->AddText(ImVec2(x + 4.0f * m_Zoom, textY), IM_COL32(100, 200, 100, 255), "True / False");
                break;
            }
            case DialogueNodeType::SetVariable: {
                char varBuf[128];
                snprintf(varBuf, sizeof(varBuf), "%s =", node.variableName.c_str());
                drawList->AddText(ImVec2(x + 4.0f * m_Zoom, textY), IM_COL32(180, 150, 220, 255), varBuf);
                textY += NODE_LINE_HEIGHT * m_Zoom;
                drawList->AddText(ImVec2(x + 4.0f * m_Zoom, textY), IM_COL32(180, 150, 220, 255), node.variableValue.c_str());
                break;
            }
            case DialogueNodeType::Event: {
                drawList->AddText(ImVec2(x + 4.0f * m_Zoom, textY), IM_COL32(220, 140, 140, 255), node.eventName.c_str());
                break;
            }
            default:
                break;
        }

        // Handle node click (selection)
        ImVec2 mousePos = ImGui::GetMousePos();
        if (mousePos.x >= nodeMin.x && mousePos.x <= nodeMax.x &&
            mousePos.y >= nodeMin.y && mousePos.y <= nodeMax.y) {

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_SelectedNodeId = node.id;
            }

            // Handle node dragging
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && m_SelectedNodeId == node.id) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                node.editorPosition.x += delta.x / m_Zoom;
                node.editorPosition.y += delta.y / m_Zoom;
            }
        }
    }

    void DialogueTreeEditor::DrawConnections() {
        if (!m_Tree) return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();

        auto getNodeCenter = [&](u32 nodeId, bool isOutput, f32 yOffset = 0.0f) -> ImVec2 {
            auto* node = m_Tree->GetNode(nodeId);
            if (!node) return ImVec2(0, 0);
            f32 x = canvasPos.x + node->editorPosition.x * m_Zoom + m_ScrollOffset.x;
            f32 y = canvasPos.y + node->editorPosition.y * m_Zoom + m_ScrollOffset.y;
            f32 h = GetNodeHeight(*node) * m_Zoom;
            f32 w = NODE_WIDTH * m_Zoom;
            if (isOutput) {
                return ImVec2(x + w, y + h * 0.5f + yOffset);
            } else {
                return ImVec2(x, y + h * 0.5f);
            }
        };

        for (const auto& node : m_Tree->nodes) {
            ImU32 lineColor = IM_COL32(180, 180, 180, 200);

            // nextNodeId connections (Text, SetVariable, Event, Root)
            if (node.nextNodeId != 0 && node.type != DialogueNodeType::Condition && node.type != DialogueNodeType::Choice) {
                ImVec2 start = getNodeCenter(node.id, true);
                ImVec2 end = getNodeCenter(node.nextNodeId, false);
                f32 dist = fabsf(end.x - start.x) * 0.5f;
                if (dist < 40.0f) dist = 40.0f;
                drawList->AddBezierCubic(start,
                    ImVec2(start.x + dist, start.y),
                    ImVec2(end.x - dist, end.y),
                    end, lineColor, CONNECTION_THICKNESS * m_Zoom);
            }

            // Choice connections
            if (node.type == DialogueNodeType::Choice) {
                for (usize i = 0; i < node.choices.size(); ++i) {
                    if (node.choices[i].targetNodeId == 0) continue;
                    f32 yOff = (static_cast<f32>(i) - static_cast<f32>(node.choices.size()) * 0.5f + 0.5f) * NODE_LINE_HEIGHT * m_Zoom;
                    ImVec2 start = getNodeCenter(node.id, true, yOff);
                    ImVec2 end = getNodeCenter(node.choices[i].targetNodeId, false);
                    f32 dist = fabsf(end.x - start.x) * 0.5f;
                    if (dist < 40.0f) dist = 40.0f;
                    drawList->AddBezierCubic(start,
                        ImVec2(start.x + dist, start.y),
                        ImVec2(end.x - dist, end.y),
                        end, IM_COL32(100, 220, 100, 200), CONNECTION_THICKNESS * m_Zoom);
                }
            }

            // Condition connections (true/false)
            if (node.type == DialogueNodeType::Condition) {
                if (node.trueNodeId != 0) {
                    ImVec2 start = getNodeCenter(node.id, true, -8.0f * m_Zoom);
                    ImVec2 end = getNodeCenter(node.trueNodeId, false);
                    f32 dist = fabsf(end.x - start.x) * 0.5f;
                    if (dist < 40.0f) dist = 40.0f;
                    drawList->AddBezierCubic(start,
                        ImVec2(start.x + dist, start.y),
                        ImVec2(end.x - dist, end.y),
                        end, IM_COL32(100, 255, 100, 220), CONNECTION_THICKNESS * m_Zoom);
                }
                if (node.falseNodeId != 0) {
                    ImVec2 start = getNodeCenter(node.id, true, 8.0f * m_Zoom);
                    ImVec2 end = getNodeCenter(node.falseNodeId, false);
                    f32 dist = fabsf(end.x - start.x) * 0.5f;
                    if (dist < 40.0f) dist = 40.0f;
                    drawList->AddBezierCubic(start,
                        ImVec2(start.x + dist, start.y),
                        ImVec2(end.x - dist, end.y),
                        end, IM_COL32(255, 100, 100, 220), CONNECTION_THICKNESS * m_Zoom);
                }
            }
        }
    }

    void DialogueTreeEditor::DrawInspector() {
        ImGui::Text("Dialogue Inspector");
        ImGui::Separator();

        // Tree-level properties
        char nameBuffer[256] = {};
        strncpy(nameBuffer, m_Tree->treeName.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Tree Name", nameBuffer, sizeof(nameBuffer))) {
            m_Tree->treeName = nameBuffer;
        }

        i32 rootId = static_cast<i32>(m_Tree->rootNodeId);
        if (ImGui::InputInt("Root Node ID", &rootId)) {
            m_Tree->rootNodeId = static_cast<u32>(std::max(0, rootId));
        }

        ImGui::Separator();

        if (m_SelectedNodeId == 0) {
            ImGui::TextWrapped("Select a node to edit its properties.");
            return;
        }

        DialogueNode* node = m_Tree->GetNode(m_SelectedNodeId);
        if (!node) {
            ImGui::Text("Node not found.");
            m_SelectedNodeId = 0;
            return;
        }

        ImGui::Text("Node #%u", node->id);
        ImGui::Text("Type: %s", NodeTypeToString(node->type));
        ImGui::Separator();

        // Common: editor position
        ImGui::DragFloat2("Position", &node->editorPosition.x, 1.0f);

        switch (node->type) {
            case DialogueNodeType::Text: {
                char speakerBuf[256] = {};
                strncpy(speakerBuf, node->speakerName.c_str(), sizeof(speakerBuf) - 1);
                if (ImGui::InputText("Speaker", speakerBuf, sizeof(speakerBuf))) {
                    node->speakerName = speakerBuf;
                }

                ImGui::ColorEdit3("Speaker Color", &node->speakerColor.x);

                char textBuf[1024] = {};
                strncpy(textBuf, node->text.c_str(), sizeof(textBuf) - 1);
                if (ImGui::InputTextMultiline("Text", textBuf, sizeof(textBuf), ImVec2(-1, 100))) {
                    node->text = textBuf;
                }

                i32 nextId = static_cast<i32>(node->nextNodeId);
                if (ImGui::InputInt("Next Node", &nextId)) {
                    node->nextNodeId = static_cast<u32>(std::max(0, nextId));
                }
                break;
            }

            case DialogueNodeType::Choice: {
                char speakerBuf[256] = {};
                strncpy(speakerBuf, node->speakerName.c_str(), sizeof(speakerBuf) - 1);
                if (ImGui::InputText("Speaker", speakerBuf, sizeof(speakerBuf))) {
                    node->speakerName = speakerBuf;
                }

                char textBuf[1024] = {};
                strncpy(textBuf, node->text.c_str(), sizeof(textBuf) - 1);
                if (ImGui::InputTextMultiline("Prompt", textBuf, sizeof(textBuf), ImVec2(-1, 60))) {
                    node->text = textBuf;
                }

                ImGui::Separator();
                ImGui::Text("Choices:");

                i32 removeIdx = -1;
                for (usize i = 0; i < node->choices.size(); ++i) {
                    ImGui::PushID(static_cast<i32>(i));

                    char choiceBuf[256] = {};
                    strncpy(choiceBuf, node->choices[i].text.c_str(), sizeof(choiceBuf) - 1);
                    char label[32];
                    snprintf(label, sizeof(label), "Choice %zu", i);
                    if (ImGui::InputText(label, choiceBuf, sizeof(choiceBuf))) {
                        node->choices[i].text = choiceBuf;
                    }

                    i32 targetId = static_cast<i32>(node->choices[i].targetNodeId);
                    if (ImGui::InputInt("Target", &targetId)) {
                        node->choices[i].targetNodeId = static_cast<u32>(std::max(0, targetId));
                    }

                    ImGui::Checkbox("Has Condition", &node->choices[i].hasCondition);
                    if (node->choices[i].hasCondition) {
                        char condVarBuf[256] = {};
                        strncpy(condVarBuf, node->choices[i].showCondition.variable.c_str(), sizeof(condVarBuf) - 1);
                        if (ImGui::InputText("Cond Var", condVarBuf, sizeof(condVarBuf))) {
                            node->choices[i].showCondition.variable = condVarBuf;
                        }

                        i32 opIdx = static_cast<i32>(node->choices[i].showCondition.op);
                        const char* opNames[] = {"Equals", "NotEquals", "GreaterThan", "LessThan"};
                        if (ImGui::Combo("Cond Op", &opIdx, opNames, 4)) {
                            node->choices[i].showCondition.op = static_cast<DialogueCondition::Op>(opIdx);
                        }

                        char condValBuf[256] = {};
                        strncpy(condValBuf, node->choices[i].showCondition.value.c_str(), sizeof(condValBuf) - 1);
                        if (ImGui::InputText("Cond Value", condValBuf, sizeof(condValBuf))) {
                            node->choices[i].showCondition.value = condValBuf;
                        }
                    }

                    if (ImGui::Button("Remove")) {
                        removeIdx = static_cast<i32>(i);
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }

                if (removeIdx >= 0) {
                    node->choices.erase(node->choices.begin() + removeIdx);
                }

                if (ImGui::Button("Add Choice")) {
                    node->choices.push_back(DialogueChoice{});
                }
                break;
            }

            case DialogueNodeType::Condition: {
                char condVarBuf[256] = {};
                strncpy(condVarBuf, node->condition.variable.c_str(), sizeof(condVarBuf) - 1);
                if (ImGui::InputText("Variable", condVarBuf, sizeof(condVarBuf))) {
                    node->condition.variable = condVarBuf;
                }

                i32 opIdx = static_cast<i32>(node->condition.op);
                const char* opNames[] = {"Equals", "NotEquals", "GreaterThan", "LessThan"};
                if (ImGui::Combo("Operator", &opIdx, opNames, 4)) {
                    node->condition.op = static_cast<DialogueCondition::Op>(opIdx);
                }

                char condValBuf[256] = {};
                strncpy(condValBuf, node->condition.value.c_str(), sizeof(condValBuf) - 1);
                if (ImGui::InputText("Value", condValBuf, sizeof(condValBuf))) {
                    node->condition.value = condValBuf;
                }

                i32 trueId = static_cast<i32>(node->trueNodeId);
                if (ImGui::InputInt("True Node", &trueId)) {
                    node->trueNodeId = static_cast<u32>(std::max(0, trueId));
                }

                i32 falseId = static_cast<i32>(node->falseNodeId);
                if (ImGui::InputInt("False Node", &falseId)) {
                    node->falseNodeId = static_cast<u32>(std::max(0, falseId));
                }
                break;
            }

            case DialogueNodeType::SetVariable: {
                char varNameBuf[256] = {};
                strncpy(varNameBuf, node->variableName.c_str(), sizeof(varNameBuf) - 1);
                if (ImGui::InputText("Variable", varNameBuf, sizeof(varNameBuf))) {
                    node->variableName = varNameBuf;
                }

                char varValBuf[256] = {};
                strncpy(varValBuf, node->variableValue.c_str(), sizeof(varValBuf) - 1);
                if (ImGui::InputText("Value", varValBuf, sizeof(varValBuf))) {
                    node->variableValue = varValBuf;
                }

                i32 nextId = static_cast<i32>(node->nextNodeId);
                if (ImGui::InputInt("Next Node", &nextId)) {
                    node->nextNodeId = static_cast<u32>(std::max(0, nextId));
                }
                break;
            }

            case DialogueNodeType::Event: {
                char eventBuf[256] = {};
                strncpy(eventBuf, node->eventName.c_str(), sizeof(eventBuf) - 1);
                if (ImGui::InputText("Event Name", eventBuf, sizeof(eventBuf))) {
                    node->eventName = eventBuf;
                }

                i32 nextId = static_cast<i32>(node->nextNodeId);
                if (ImGui::InputInt("Next Node", &nextId)) {
                    node->nextNodeId = static_cast<u32>(std::max(0, nextId));
                }
                break;
            }

            case DialogueNodeType::Root: {
                i32 nextId = static_cast<i32>(node->nextNodeId);
                if (ImGui::InputInt("Next Node", &nextId)) {
                    node->nextNodeId = static_cast<u32>(std::max(0, nextId));
                }
                break;
            }

            case DialogueNodeType::End: {
                ImGui::TextWrapped("This node ends the dialogue.");
                break;
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Delete Node")) {
            m_Tree->RemoveNode(m_SelectedNodeId);
            m_SelectedNodeId = 0;
        }
    }

} // namespace Enjin::GUI
