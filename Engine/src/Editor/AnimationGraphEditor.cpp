#include "Enjin/Editor/AnimationGraphEditor.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Name.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Enjin {
namespace Editor {

// Header colors for different node types
static const Math::Vector3 COLOR_ENTRY   = Math::Vector3(0.2f, 0.6f, 0.3f);  // Green
static const Math::Vector3 COLOR_STATE   = Math::Vector3(0.3f, 0.4f, 0.7f);  // Blue
static const Math::Vector3 COLOR_CURRENT = Math::Vector3(0.7f, 0.5f, 0.2f);  // Orange (active in play)

// ============================================================================
// Target Management
// ============================================================================

void AnimationGraphEditor::SetTarget(ECS::World* world, ECS::Entity entity) {
    if (m_World == world && m_TargetEntity == entity) return;
    m_World = world;
    m_TargetEntity = entity;
    m_NeedsSync = true;
    m_SelectedTransition.valid = false;
}

void AnimationGraphEditor::ClearTarget() {
    m_World = nullptr;
    m_TargetEntity = ECS::INVALID_ENTITY;
    m_GraphData.Clear();
    m_StateNames.clear();
    m_StateToNodeId.clear();
    m_EntryNodeId = 0;
    m_NeedsSync = true;
    m_SelectedTransition.valid = false;
}

// ============================================================================
// Sync From Component
// ============================================================================

void AnimationGraphEditor::SyncFromComponent() {
    m_GraphData.Clear();
    m_StateNames.clear();
    m_StateToNodeId.clear();
    m_EntryNodeId = 0;

    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    // Try StateMachineComponent first
    auto* sm = m_World->GetComponent<ECS::StateMachineComponent>(m_TargetEntity);

    if (sm) {
        // Create Entry pseudo-node
        m_EntryNodeId = m_GraphData.AddNode("Entry", Math::Vector2(-150, 100), COLOR_ENTRY);
        auto* entryNode = m_GraphData.FindNode(m_EntryNodeId);
        if (entryNode) {
            entryNode->flags = NodeFlags::NoDelete;
            m_GraphData.AddPin(m_EntryNodeId, "Out", PinType::Flow, PinKind::Output);
        }

        // Create a node per state
        for (usize i = 0; i < sm->states.size(); i++) {
            auto& state = sm->states[i];
            Math::Vector2 pos = state.editorPosition;
            // Auto-position if at origin and not first sync
            if (pos.x == 0.0f && pos.y == 0.0f) {
                pos = Math::Vector2(100.0f + static_cast<f32>(i) * 220.0f, 100.0f);
            }

            NodeId nid = m_GraphData.AddNode(state.name, pos, COLOR_STATE);
            m_StateNames.push_back(state.name);
            m_StateToNodeId[state.name] = nid;

            // Add input/output pins for transitions
            m_GraphData.AddPin(nid, "In", PinType::Flow, PinKind::Input);
            m_GraphData.AddPin(nid, "Out", PinType::Flow, PinKind::Output);
        }

        // Create links for transitions
        for (auto& state : sm->states) {
            auto fromIt = m_StateToNodeId.find(state.name);
            if (fromIt == m_StateToNodeId.end()) continue;

            auto* fromNode = m_GraphData.FindNode(fromIt->second);
            if (!fromNode || fromNode->outputs.empty()) continue;
            PinId fromPin = fromNode->outputs[0].id;

            for (auto& trans : state.transitions) {
                auto toIt = m_StateToNodeId.find(trans.toState);
                if (toIt == m_StateToNodeId.end()) continue;

                auto* toNode = m_GraphData.FindNode(toIt->second);
                if (!toNode || toNode->inputs.empty()) continue;
                PinId toPin = toNode->inputs[0].id;

                m_GraphData.AddLink(fromPin, toPin);
            }
        }

        // Link Entry to current/default state
        std::string defaultState = sm->currentState;
        if (defaultState.empty() && !sm->states.empty())
            defaultState = sm->states[0].name;

        if (!defaultState.empty()) {
            auto it = m_StateToNodeId.find(defaultState);
            if (it != m_StateToNodeId.end()) {
                auto* toNode = m_GraphData.FindNode(it->second);
                if (toNode && !toNode->inputs.empty() && entryNode && !entryNode->outputs.empty()) {
                    m_GraphData.AddLink(entryNode->outputs[0].id, toNode->inputs[0].id);
                }
            }
        }
    }

    m_NeedsSync = false;
}

// ============================================================================
// Sync To Component
// ============================================================================

void AnimationGraphEditor::SyncToComponent() {
    if (!m_World || m_TargetEntity == ECS::INVALID_ENTITY) return;

    auto* sm = m_World->GetComponent<ECS::StateMachineComponent>(m_TargetEntity);
    if (!sm) return;

    // Write back editor positions from graph nodes to SM states
    for (auto& state : sm->states) {
        auto it = m_StateToNodeId.find(state.name);
        if (it != m_StateToNodeId.end()) {
            auto* node = m_GraphData.FindNode(it->second);
            if (node) {
                state.editorPosition = node->position;
            }
        }
    }
}

// ============================================================================
// Callbacks Setup
// ============================================================================

void AnimationGraphEditor::SetupCallbacks() {
    m_Callbacks = {};

    // Validation: only allow Flow->Flow connections, no self-links
    m_Callbacks.CanCreateLink = [](const Pin& from, const Pin& to) -> bool {
        if (from.type != PinType::Flow || to.type != PinType::Flow) return false;
        if (from.nodeId == to.nodeId) return false;
        return true;
    };

    // When a link is created, add the corresponding SM transition
    m_Callbacks.OnLinkCreated = [this](LinkId linkId) {
        auto* link = m_GraphData.FindLink(linkId);
        if (!link) return;

        auto* startPin = m_GraphData.FindPin(link->startPinId);
        auto* endPin = m_GraphData.FindPin(link->endPinId);
        if (!startPin || !endPin) return;

        // Skip if source is Entry node
        if (startPin->nodeId == m_EntryNodeId) return;

        auto* sm = m_World ? m_World->GetComponent<ECS::StateMachineComponent>(m_TargetEntity) : nullptr;
        if (!sm) return;

        // Find state names
        std::string fromState, toState;
        for (auto& [name, nid] : m_StateToNodeId) {
            if (nid == startPin->nodeId) fromState = name;
            if (nid == endPin->nodeId) toState = name;
        }

        if (fromState.empty() || toState.empty()) return;

        // Add transition to the SM state
        for (auto& state : sm->states) {
            if (state.name == fromState) {
                // Check if transition already exists
                bool exists = false;
                for (auto& t : state.transitions) {
                    if (t.toState == toState) { exists = true; break; }
                }
                if (!exists) {
                    ECS::SMTransition t;
                    t.toState = toState;
                    state.transitions.push_back(t);
                }
                break;
            }
        }
    };

    // When a link is deleted, remove the SM transition
    m_Callbacks.OnLinkDeleted = [this](LinkId linkId) {
        auto* link = m_GraphData.FindLink(linkId);
        if (!link) return;

        auto* startPin = m_GraphData.FindPin(link->startPinId);
        auto* endPin = m_GraphData.FindPin(link->endPinId);
        if (!startPin || !endPin) return;

        if (startPin->nodeId == m_EntryNodeId) return;

        auto* sm = m_World ? m_World->GetComponent<ECS::StateMachineComponent>(m_TargetEntity) : nullptr;
        if (!sm) return;

        std::string fromState, toState;
        for (auto& [name, nid] : m_StateToNodeId) {
            if (nid == startPin->nodeId) fromState = name;
            if (nid == endPin->nodeId) toState = name;
        }

        if (fromState.empty() || toState.empty()) return;

        for (auto& state : sm->states) {
            if (state.name == fromState) {
                state.transitions.erase(
                    std::remove_if(state.transitions.begin(), state.transitions.end(),
                        [&](const ECS::SMTransition& t) { return t.toState == toState; }),
                    state.transitions.end());
                break;
            }
        }
    };

    // When a node is selected, determine if a transition is selected for the inspector
    m_Callbacks.OnNodeSelected = [this](NodeId id) {
        m_SelectedTransition.valid = false;
    };

    m_Callbacks.OnLinkSelected = [this](LinkId id) {
        auto* link = m_GraphData.FindLink(id);
        if (!link) { m_SelectedTransition.valid = false; return; }

        auto* startPin = m_GraphData.FindPin(link->startPinId);
        auto* endPin = m_GraphData.FindPin(link->endPinId);
        if (!startPin || !endPin) { m_SelectedTransition.valid = false; return; }

        m_SelectedTransition.fromState.clear();
        m_SelectedTransition.toState.clear();
        for (auto& [name, nid] : m_StateToNodeId) {
            if (nid == startPin->nodeId) m_SelectedTransition.fromState = name;
            if (nid == endPin->nodeId) m_SelectedTransition.toState = name;
        }
        m_SelectedTransition.valid = !m_SelectedTransition.fromState.empty() &&
                                     !m_SelectedTransition.toState.empty();
    };

    m_Callbacks.OnSelectionCleared = [this]() {
        m_SelectedTransition.valid = false;
    };

    // When a node is deleted, remove the corresponding SM state
    m_Callbacks.OnNodeDeleted = [this](NodeId id) {
        auto* sm = m_World ? m_World->GetComponent<ECS::StateMachineComponent>(m_TargetEntity) : nullptr;
        if (!sm) return;

        // Find state name for this node
        std::string stateName;
        for (auto& [name, nid] : m_StateToNodeId) {
            if (nid == id) { stateName = name; break; }
        }
        if (stateName.empty()) return;

        // Remove state from SM
        sm->states.erase(
            std::remove_if(sm->states.begin(), sm->states.end(),
                [&](const ECS::SMState& s) { return s.name == stateName; }),
            sm->states.end());

        // Remove all transitions referencing this state
        for (auto& state : sm->states) {
            state.transitions.erase(
                std::remove_if(state.transitions.begin(), state.transitions.end(),
                    [&](const ECS::SMTransition& t) { return t.toState == stateName; }),
                state.transitions.end());
        }

        m_StateToNodeId.erase(stateName);
        if (sm->currentState == stateName) {
            sm->currentState = sm->states.empty() ? "" : sm->states[0].name;
        }
    };

    // Context menu: add new state
    ContextMenuCategory statesCat;
    statesCat.name = "States";
    statesCat.items.push_back({"Add State", [this](Math::Vector2 pos) {
        auto* sm = m_World ? m_World->GetComponent<ECS::StateMachineComponent>(m_TargetEntity) : nullptr;
        if (!sm) return;

        // Generate unique name
        std::string baseName = "State";
        std::string name = baseName;
        int counter = 1;
        while (sm->HasState(name)) {
            name = baseName + std::to_string(counter++);
        }

        ECS::SMState newState;
        newState.name = name;
        newState.editorPosition = pos;
        sm->states.push_back(newState);

        // If this is the first state, set it as current
        if (sm->states.size() == 1) {
            sm->currentState = name;
        }

        m_NeedsSync = true;
    }});
    m_Callbacks.contextMenuCategories.push_back(statesCat);
}

// ============================================================================
// Render
// ============================================================================

void AnimationGraphEditor::Render(const EditorSettings& settings, bool isPlaying) {
    if (!HasTarget()) {
        ImGui::TextDisabled("Select an entity with a State Machine component");
        return;
    }

    auto* sm = m_World->GetComponent<ECS::StateMachineComponent>(m_TargetEntity);
    if (!sm) {
        ImGui::TextDisabled("Selected entity has no State Machine component");
        return;
    }

    if (m_NeedsSync) {
        SyncFromComponent();
        SetupCallbacks();
        m_Colors = NodeGraphColors::FromTheme(settings);
    }

    if (isPlaying) {
        UpdatePlayModeHighlight();
    }

    DrawToolbar();

    // Layout: canvas on left, inspector on right
    f32 inspectorWidth = 260.0f * settings.uiScale;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    f32 canvasWidth = avail.x - inspectorWidth - 8.0f;
    if (canvasWidth < 200.0f) canvasWidth = avail.x;  // Don't show inspector if too narrow

    // Canvas
    ImGui::BeginChild("##AnimGraphCanvas", ImVec2(canvasWidth, avail.y), false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    m_GraphEditor.Render(m_GraphData, m_Callbacks, m_Colors, settings.uiScale);
    ImGui::EndChild();

    // Write back positions continuously
    SyncToComponent();

    // Inspector sidebar
    if (canvasWidth < avail.x) {
        ImGui::SameLine();
        ImGui::BeginChild("##AnimGraphInspector", ImVec2(inspectorWidth, avail.y), true);
        DrawInspector(isPlaying);
        ImGui::EndChild();
    }
}

// ============================================================================
// Toolbar
// ============================================================================

void AnimationGraphEditor::DrawToolbar() {
    auto* sm = m_World ? m_World->GetComponent<ECS::StateMachineComponent>(m_TargetEntity) : nullptr;
    if (!sm) return;

    if (ImGui::Button("+ Add State")) {
        std::string baseName = "State";
        std::string name = baseName;
        int counter = 1;
        while (sm->HasState(name)) {
            name = baseName + std::to_string(counter++);
        }

        ECS::SMState newState;
        newState.name = name;
        // Place near center of current view
        Math::Vector2 viewCenter = m_GraphEditor.GetScrollOffset().x == 0 &&
                                    m_GraphEditor.GetScrollOffset().y == 0
            ? Math::Vector2(200, 100)
            : Math::Vector2(
                (-m_GraphEditor.GetScrollOffset().x + 200.0f) / m_GraphEditor.GetZoom(),
                (-m_GraphEditor.GetScrollOffset().y + 100.0f) / m_GraphEditor.GetZoom()
              );
        newState.editorPosition = viewCenter;
        sm->states.push_back(newState);

        if (sm->states.size() == 1)
            sm->currentState = name;

        m_NeedsSync = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Auto Layout")) {
        AutoLayout();
    }

    ImGui::SameLine();
    if (ImGui::Button("Fit All")) {
        m_GraphEditor.FitAllNodes(m_GraphData);
    }

    // Entity name
    if (m_World) {
        auto* nameComp = m_World->GetComponent<ECS::NameComponent>(m_TargetEntity);
        if (nameComp) {
            ImGui::SameLine();
            ImGui::TextDisabled("Entity: %s", nameComp->name.c_str());
        }
    }

    ImGui::Separator();
}

// ============================================================================
// Inspector Panel
// ============================================================================

void AnimationGraphEditor::DrawInspector(bool isPlaying) {
    auto* sm = m_World ? m_World->GetComponent<ECS::StateMachineComponent>(m_TargetEntity) : nullptr;
    if (!sm) return;

    NodeId selectedNode = m_GraphEditor.GetSelectedNodeId();
    LinkId selectedLink = m_GraphEditor.GetSelectedLinkId();

    // --- State Inspector ---
    if (selectedNode != 0 && selectedNode != m_EntryNodeId) {
        // Find state name
        std::string stateName;
        for (auto& [name, nid] : m_StateToNodeId) {
            if (nid == selectedNode) { stateName = name; break; }
        }

        if (!stateName.empty()) {
            ImGui::Text("State");
            ImGui::Separator();

            // Find the SM state
            ECS::SMState* smState = nullptr;
            for (auto& s : sm->states) {
                if (s.name == stateName) { smState = &s; break; }
            }

            if (smState) {
                // Editable name
                char nameBuf[128];
                std::strncpy(nameBuf, smState->name.c_str(), sizeof(nameBuf) - 1);
                nameBuf[sizeof(nameBuf) - 1] = '\0';
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                    std::string newName(nameBuf);
                    if (!newName.empty() && !sm->HasState(newName)) {
                        // Update all references
                        std::string oldName = smState->name;
                        smState->name = newName;

                        // Update transitions referencing this state
                        for (auto& state : sm->states) {
                            for (auto& t : state.transitions) {
                                if (t.toState == oldName) t.toState = newName;
                            }
                        }

                        if (sm->currentState == oldName) sm->currentState = newName;
                        if (sm->previousState == oldName) sm->previousState = newName;

                        m_NeedsSync = true;
                    }
                }

                // Set as default state
                bool isDefault = (sm->currentState == smState->name);
                if (ImGui::Checkbox("Default State", &isDefault)) {
                    if (isDefault) sm->currentState = smState->name;
                }

                // AnimatorComponent state machine animation name (if available)
                auto* animator = m_World->GetComponent<ECS::AnimatorComponent>(m_TargetEntity);
                if (animator) {
                    auto& asmStates = animator->stateMachine.GetStates();
                    auto it = asmStates.find(stateName);
                    if (it != asmStates.end()) {
                        ImGui::Separator();
                        ImGui::Text("Animation");
                        ImGui::Text("Clip: %s", it->second.animationName.c_str());
                        ImGui::Text("Speed: %.2f", it->second.speed);
                    }
                }

                // Script callbacks
                ImGui::Separator();
                ImGui::Text("Callbacks");
                char enterBuf[128], updateBuf[128], exitBuf[128];
                std::strncpy(enterBuf, smState->onEnter.c_str(), sizeof(enterBuf) - 1);
                enterBuf[sizeof(enterBuf) - 1] = '\0';
                std::strncpy(updateBuf, smState->onUpdate.c_str(), sizeof(updateBuf) - 1);
                updateBuf[sizeof(updateBuf) - 1] = '\0';
                std::strncpy(exitBuf, smState->onExit.c_str(), sizeof(exitBuf) - 1);
                exitBuf[sizeof(exitBuf) - 1] = '\0';
                if (ImGui::InputText("On Enter", enterBuf, sizeof(enterBuf)))
                    smState->onEnter = enterBuf;
                if (ImGui::InputText("On Update", updateBuf, sizeof(updateBuf)))
                    smState->onUpdate = updateBuf;
                if (ImGui::InputText("On Exit", exitBuf, sizeof(exitBuf)))
                    smState->onExit = exitBuf;

                // Transitions list
                ImGui::Separator();
                ImGui::Text("Transitions (%zu)", smState->transitions.size());
                for (usize i = 0; i < smState->transitions.size(); i++) {
                    auto& t = smState->transitions[i];
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::TreeNode("", "-> %s", t.toState.c_str())) {
                        ImGui::Text("Conditions: %zu", t.conditions.size());
                        for (usize c = 0; c < t.conditions.size(); c++) {
                            auto& cond = t.conditions[c];
                            const char* typeNames[] = {
                                "Bool True", "Bool False", "Float >", "Float <",
                                "Int ==", "Int !=", "Trigger"
                            };
                            i32 typeIdx = static_cast<i32>(cond.type);
                            if (typeIdx < 0 || typeIdx >= static_cast<i32>(ECS::SMConditionType::COUNT))
                                typeIdx = 0;
                            ImGui::BulletText("%s: %s", cond.paramName.c_str(),
                                typeNames[typeIdx]);
                        }
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }
        }
    }
    // --- Transition Inspector (link selected) ---
    else if (selectedLink != 0 && m_SelectedTransition.valid) {
        ImGui::Text("Transition");
        ImGui::Separator();
        ImGui::Text("%s -> %s", m_SelectedTransition.fromState.c_str(),
            m_SelectedTransition.toState.c_str());

        // Find the transition
        ECS::SMTransition* transition = nullptr;
        for (auto& state : sm->states) {
            if (state.name == m_SelectedTransition.fromState) {
                for (auto& t : state.transitions) {
                    if (t.toState == m_SelectedTransition.toState) {
                        transition = &t;
                        break;
                    }
                }
                break;
            }
        }

        if (transition) {
            ImGui::Separator();
            ImGui::Text("Conditions");

            for (usize i = 0; i < transition->conditions.size(); i++) {
                auto& cond = transition->conditions[i];
                ImGui::PushID(static_cast<int>(i));

                const char* typeNames[] = {
                    "Bool True", "Bool False", "Float >", "Float <",
                    "Int ==", "Int !=", "Trigger"
                };
                i32 typeIdx = static_cast<i32>(cond.type);
                if (typeIdx < 0 || typeIdx >= static_cast<i32>(ECS::SMConditionType::COUNT))
                    typeIdx = 0;

                ImGui::Text("[%zu] %s %s", i, cond.paramName.c_str(), typeNames[typeIdx]);

                if (cond.type == ECS::SMConditionType::FloatGreater ||
                    cond.type == ECS::SMConditionType::FloatLess) {
                    ImGui::SameLine();
                    ImGui::Text("%.2f", cond.threshold);
                }
                if (cond.type == ECS::SMConditionType::IntEquals ||
                    cond.type == ECS::SMConditionType::IntNotEquals) {
                    ImGui::SameLine();
                    ImGui::Text("%d", cond.intValue);
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    transition->conditions.erase(transition->conditions.begin() + i);
                    ImGui::PopID();
                    break;
                }

                ImGui::PopID();
            }

            // Add condition
            ImGui::Separator();
            ImGui::Text("Add Condition");

            const char* condTypes[] = {
                "Bool True", "Bool False", "Float >", "Float <",
                "Int ==", "Int !=", "Trigger"
            };
            ImGui::Combo("Type", &m_NewCondType, condTypes, 7);
            ImGui::InputText("Param", m_NewCondParam, sizeof(m_NewCondParam));

            if (m_NewCondType == 2 || m_NewCondType == 3) {
                ImGui::DragFloat("Threshold", &m_NewCondThreshold, 0.1f);
            }
            if (m_NewCondType == 4 || m_NewCondType == 5) {
                ImGui::DragInt("Value", &m_NewCondIntValue);
            }

            if (ImGui::Button("Add##Cond") && std::strlen(m_NewCondParam) > 0) {
                ECS::SMTransitionCondition cond;
                cond.paramName = m_NewCondParam;
                cond.type = static_cast<ECS::SMConditionType>(m_NewCondType);
                cond.threshold = m_NewCondThreshold;
                cond.intValue = m_NewCondIntValue;
                transition->conditions.push_back(cond);
                m_NewCondParam[0] = '\0';
            }
        }
    }
    // --- No selection ---
    else if (selectedNode == m_EntryNodeId && m_EntryNodeId != 0) {
        ImGui::Text("Entry Node");
        ImGui::Separator();
        ImGui::TextWrapped("The Entry node connects to the default/initial state.");

        // Show which state is the default
        if (!sm->currentState.empty()) {
            ImGui::Text("Default: %s", sm->currentState.c_str());
        }
    }
    else {
        ImGui::TextDisabled("Select a state or transition");
    }

    // --- Parameters Section (always visible) ---
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Bool params
        for (auto& [name, val] : sm->boolParams) {
            ImGui::PushID(name.c_str());
            bool v = val;
            if (ImGui::Checkbox(name.c_str(), &v)) val = v;
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                sm->boolParams.erase(name);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        // Float params
        for (auto& [name, val] : sm->floatParams) {
            ImGui::PushID(name.c_str());
            f32 v = val;
            if (ImGui::DragFloat(name.c_str(), &v, 0.1f)) val = v;
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                sm->floatParams.erase(name);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        // Int params
        for (auto& [name, val] : sm->intParams) {
            ImGui::PushID(name.c_str());
            i32 v = val;
            if (ImGui::DragInt(name.c_str(), &v)) val = v;
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                sm->intParams.erase(name);
                ImGui::PopID();
                break;
            }
            ImGui::PopID();
        }

        // Add new parameter
        ImGui::Spacing();
        ImGui::InputText("##NewParam", m_NewParamName, sizeof(m_NewParamName));
        ImGui::SameLine();
        const char* paramTypes[] = { "Bool", "Float", "Int" };
        ImGui::SetNextItemWidth(60);
        ImGui::Combo("##ParamType", &m_NewParamType, paramTypes, 3);
        ImGui::SameLine();
        if (ImGui::Button("Add") && std::strlen(m_NewParamName) > 0) {
            std::string pname(m_NewParamName);
            switch (m_NewParamType) {
                case 0: sm->boolParams[pname] = false; break;
                case 1: sm->floatParams[pname] = 0.0f; break;
                case 2: sm->intParams[pname] = 0; break;
            }
            m_NewParamName[0] = '\0';
        }
    }

    // --- Play Mode: Send Trigger ---
    if (isPlaying) {
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Play Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Current: %s", sm->currentState.c_str());
            ImGui::Text("Previous: %s", sm->previousState.empty() ? "(none)" : sm->previousState.c_str());
            ImGui::Text("State Time: %.2f", sm->stateTime);

            ImGui::Separator();
            ImGui::InputText("##TriggerName", m_TriggerNameBuf, sizeof(m_TriggerNameBuf));
            ImGui::SameLine();
            if (ImGui::Button("Send Trigger") && std::strlen(m_TriggerNameBuf) > 0) {
                sm->SendTrigger(std::string(m_TriggerNameBuf));
            }
        }
    }
}

// ============================================================================
// Auto Layout
// ============================================================================

void AnimationGraphEditor::AutoLayout() {
    auto& nodes = m_GraphData.GetNodes();
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 colWidth = 220.0f;
    f32 rowHeight = 100.0f;
    int cols = 3;
    int idx = 0;

    for (auto& node : nodes) {
        if (node.id == m_EntryNodeId) {
            node.position = Math::Vector2(-200.0f, 100.0f);
            continue;
        }

        node.position = Math::Vector2(
            x + static_cast<f32>(idx % cols) * colWidth,
            y + static_cast<f32>(idx / cols) * rowHeight
        );
        idx++;
    }

    // Write back
    SyncToComponent();
    m_GraphEditor.FitAllNodes(m_GraphData);
}

// ============================================================================
// Play Mode Highlight
// ============================================================================

void AnimationGraphEditor::UpdatePlayModeHighlight() {
    auto* sm = m_World ? m_World->GetComponent<ECS::StateMachineComponent>(m_TargetEntity) : nullptr;
    if (!sm) return;

    // Clear all highlights
    for (auto& node : m_GraphData.GetNodes()) {
        node.flags = static_cast<NodeFlags>(
            static_cast<u32>(node.flags) & ~static_cast<u32>(NodeFlags::Highlighted));
    }

    // Highlight current state
    if (!sm->currentState.empty()) {
        auto it = m_StateToNodeId.find(sm->currentState);
        if (it != m_StateToNodeId.end()) {
            auto* node = m_GraphData.FindNode(it->second);
            if (node) {
                node->flags = node->flags | NodeFlags::Highlighted;
            }
        }
    }
}

} // namespace Editor
} // namespace Enjin
