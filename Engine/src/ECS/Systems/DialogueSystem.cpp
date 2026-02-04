#include "Enjin/ECS/Systems/DialogueSystem.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace ECS {

void DialogueSystem::SyncNodeToComponent(DialogueComponent& dlg, GUI::DialoguePlayer& player) {
    const GUI::DialogueNode* node = player.GetCurrentNode();
    if (!node) {
        dlg.currentSpeaker.clear();
        dlg.currentText.clear();
        dlg.currentSpeakerColor = Math::Vector3(1, 1, 1);
        dlg.currentChoices.clear();
        return;
    }

    dlg.currentNodeId = node->id;
    dlg.currentSpeaker = node->speakerName;
    dlg.currentSpeakerColor = node->speakerColor;
    dlg.currentChoices.clear();

    if (node->type == GUI::DialogueNodeType::Text) {
        dlg.currentText = node->text;
        // Reset typewriter for new text
        dlg.currentChar = 0;
        dlg.charTimer = 0.0f;
        dlg.isTyping = true;
        dlg.waitingForInput = false;
        dlg.selectedChoice = 0;
    } else if (node->type == GUI::DialogueNodeType::Choice) {
        dlg.currentText = node->text;
        dlg.currentChoices = player.GetAvailableChoices();
        // Reset typewriter for prompt text
        dlg.currentChar = 0;
        dlg.charTimer = 0.0f;
        dlg.isTyping = !node->text.empty();
        dlg.waitingForInput = node->text.empty();
        dlg.selectedChoice = 0;
    } else {
        dlg.currentText.clear();
    }
}

void DialogueSystem::StartDialogue(World* world, Entity entity) {
    if (!world) return;
    auto* dlg = world->GetComponent<DialogueComponent>(entity);
    if (!dlg || !dlg->IsTreeMode()) return;

    auto& player = m_Players[entity];

    // Copy persisted variables into the player
    for (const auto& [k, v] : dlg->variables) {
        player.SetVariable(k, v);
    }

    // Wire event callback
    if (m_EventCallback) {
        player.SetEventCallback([this, entity](const std::string& eventName) {
            m_EventCallback(entity, eventName);
        });
    }

    player.Start(dlg->dialogueTree);
    dlg->treeActive = true;

    SyncNodeToComponent(*dlg, player);

    ENJIN_LOG_INFO(Script, "Dialogue tree started on entity %llu", static_cast<u64>(entity));
}

void DialogueSystem::Advance(World* world, Entity entity) {
    if (!world) return;
    auto it = m_Players.find(entity);
    if (it == m_Players.end()) return;

    auto* dlg = world->GetComponent<DialogueComponent>(entity);
    if (!dlg) return;

    auto& player = it->second;
    player.Advance();

    if (!player.IsActive()) {
        // Dialogue ended — copy variables back and clean up
        for (const auto& [k, v] : dlg->variables) {
            dlg->variables[k] = player.GetVariable(k);
        }
        dlg->treeActive = false;
        dlg->currentText.clear();
        dlg->currentSpeaker.clear();
        dlg->currentChoices.clear();
        m_Players.erase(it);
        if (m_ActiveEntity == entity) m_ActiveEntity = INVALID_ENTITY;
    } else {
        SyncNodeToComponent(*dlg, player);
    }
}

void DialogueSystem::SelectChoice(World* world, Entity entity, u32 choiceIndex) {
    if (!world) return;
    auto it = m_Players.find(entity);
    if (it == m_Players.end()) return;

    auto* dlg = world->GetComponent<DialogueComponent>(entity);
    if (!dlg) return;

    auto& player = it->second;
    player.SelectChoice(choiceIndex);

    if (!player.IsActive()) {
        dlg->treeActive = false;
        dlg->currentText.clear();
        dlg->currentSpeaker.clear();
        dlg->currentChoices.clear();
        m_Players.erase(it);
        if (m_ActiveEntity == entity) m_ActiveEntity = INVALID_ENTITY;
    } else {
        SyncNodeToComponent(*dlg, player);
    }
}

void DialogueSystem::SetVariable(World* world, Entity entity, const std::string& name, const std::string& value) {
    if (!world) return;
    auto* dlg = world->GetComponent<DialogueComponent>(entity);
    if (dlg) dlg->variables[name] = value;

    auto it = m_Players.find(entity);
    if (it != m_Players.end()) {
        it->second.SetVariable(name, value);
    }
}

std::string DialogueSystem::GetVariable(World* world, Entity entity, const std::string& name) const {
    if (!world) return "";
    auto it = m_Players.find(entity);
    if (it != m_Players.end()) {
        return it->second.GetVariable(name);
    }
    auto* dlg = world->GetComponent<DialogueComponent>(entity);
    if (dlg) {
        auto vit = dlg->variables.find(name);
        if (vit != dlg->variables.end()) return vit->second;
    }
    return "";
}

bool DialogueSystem::IsActive(World* world, Entity entity) const {
    if (!world) return false;
    auto* dlg = world->GetComponent<DialogueComponent>(entity);
    if (!dlg) return false;

    if (dlg->IsTreeMode()) return dlg->treeActive;

    // Legacy mode: active if has lines and not complete
    return !dlg->dialogueLines.empty() && !dlg->IsComplete();
}

void DialogueSystem::Clear() {
    m_Players.clear();
    m_ActiveEntity = INVALID_ENTITY;
}

void DialogueSystem::ProcessLegacy(World* world, Entity entity, DialogueComponent& d, f32 deltaTime) {
    // Advance typewriter
    if (d.isTyping && d.currentLine < d.dialogueLines.size()) {
        d.charTimer += deltaTime;
        while (d.charTimer >= d.charDelay && d.currentChar < d.dialogueLines[d.currentLine].size()) {
            d.currentChar++;
            d.charTimer -= d.charDelay;
        }
        if (d.currentChar >= d.dialogueLines[d.currentLine].size()) {
            d.isTyping = false;
            d.waitingForInput = true;
        }
    }

    // Input: advance dialogue
    if (d.waitingForInput) {
        if (Input::IsKeyPressed(KeyCode::Space) || Input::IsKeyPressed(KeyCode::Enter) ||
            Input::IsMouseButtonPressed(MouseButton::Left)) {
            if (d.currentLine + 1 >= d.dialogueLines.size() && !d.choices.empty()) {
                // Wait for choice selection
            } else {
                d.currentLine++;
                d.currentChar = 0;
                d.charTimer = 0.0f;
                d.waitingForInput = false;
                if (!d.IsComplete()) {
                    d.isTyping = true;
                }
            }
        }
    }

    // Skip to end of line while typing
    if (d.isTyping) {
        if (Input::IsKeyPressed(KeyCode::Space) || Input::IsKeyPressed(KeyCode::Enter)) {
            d.currentChar = static_cast<u32>(d.dialogueLines[d.currentLine].size());
            d.isTyping = false;
            d.waitingForInput = true;
        }
    }

    // Choice navigation
    if (d.waitingForInput && !d.choices.empty() &&
        d.currentLine + 1 >= d.dialogueLines.size()) {
        if (Input::IsKeyPressed(KeyCode::Up) || Input::IsKeyPressed(KeyCode::W)) {
            d.selectedChoice--;
            if (d.selectedChoice < 0) d.selectedChoice = static_cast<i32>(d.choices.size()) - 1;
        }
        if (Input::IsKeyPressed(KeyCode::Down) || Input::IsKeyPressed(KeyCode::S)) {
            d.selectedChoice++;
            if (d.selectedChoice >= static_cast<i32>(d.choices.size())) d.selectedChoice = 0;
        }
        if (Input::IsKeyPressed(KeyCode::Enter) || Input::IsKeyPressed(KeyCode::Space)) {
            d.currentLine = static_cast<u32>(d.dialogueLines.size());
            d.waitingForInput = false;
        }
    }
}

void DialogueSystem::Update(World* world, f32 deltaTime) {
    if (!world) return;

    Entity activeDialogue = INVALID_ENTITY;

    for (Entity entity : world->GetAllEntities()) {
        if (!world->HasComponent<DialogueComponent>(entity)) continue;
        auto* dlg = world->GetComponent<DialogueComponent>(entity);
        if (!dlg) continue;

        if (dlg->IsTreeMode()) {
            // Tree mode
            if (!dlg->treeActive) continue;

            activeDialogue = entity;

            // Advance typewriter on currentText
            if (dlg->isTyping && !dlg->currentText.empty()) {
                dlg->charTimer += deltaTime;
                while (dlg->charTimer >= dlg->charDelay &&
                       dlg->currentChar < static_cast<u32>(dlg->currentText.size())) {
                    dlg->currentChar++;
                    dlg->charTimer -= dlg->charDelay;
                }
                if (dlg->currentChar >= static_cast<u32>(dlg->currentText.size())) {
                    dlg->isTyping = false;
                    dlg->waitingForInput = true;
                }
            }

            // Input handling for tree mode
            auto it = m_Players.find(entity);
            if (it == m_Players.end()) continue;
            auto& player = it->second;
            const GUI::DialogueNode* currentNode = player.GetCurrentNode();
            if (!currentNode) continue;

            if (dlg->isTyping) {
                // Skip to end of text
                if (Input::IsKeyPressed(KeyCode::Space) || Input::IsKeyPressed(KeyCode::Enter)) {
                    dlg->currentChar = static_cast<u32>(dlg->currentText.size());
                    dlg->isTyping = false;
                    dlg->waitingForInput = true;
                }
            } else if (dlg->waitingForInput) {
                if (currentNode->type == GUI::DialogueNodeType::Text) {
                    // Advance to next node
                    if (Input::IsKeyPressed(KeyCode::Space) || Input::IsKeyPressed(KeyCode::Enter)) {
                        Advance(world, entity);
                    }
                } else if (currentNode->type == GUI::DialogueNodeType::Choice) {
                    // Choice navigation
                    if (Input::IsKeyPressed(KeyCode::Up) || Input::IsKeyPressed(KeyCode::W)) {
                        dlg->selectedChoice--;
                        if (dlg->selectedChoice < 0)
                            dlg->selectedChoice = static_cast<i32>(dlg->currentChoices.size()) - 1;
                    }
                    if (Input::IsKeyPressed(KeyCode::Down) || Input::IsKeyPressed(KeyCode::S)) {
                        dlg->selectedChoice++;
                        if (dlg->selectedChoice >= static_cast<i32>(dlg->currentChoices.size()))
                            dlg->selectedChoice = 0;
                    }
                    if (Input::IsKeyPressed(KeyCode::Enter) || Input::IsKeyPressed(KeyCode::Space)) {
                        if (dlg->selectedChoice >= 0 &&
                            dlg->selectedChoice < static_cast<i32>(dlg->currentChoices.size())) {
                            SelectChoice(world, entity, static_cast<u32>(dlg->selectedChoice));
                        }
                    }
                }
            }

            break;  // Only one active dialogue at a time
        } else {
            // Legacy mode
            if (dlg->dialogueLines.empty() || dlg->IsComplete()) continue;

            activeDialogue = entity;
            ProcessLegacy(world, entity, *dlg, deltaTime);
            break;  // Only one active dialogue at a time
        }
    }

    m_ActiveEntity = activeDialogue;
}

} // namespace ECS
} // namespace Enjin
