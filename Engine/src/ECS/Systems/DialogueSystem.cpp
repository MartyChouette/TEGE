#include "Enjin/ECS/Systems/DialogueSystem.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include <algorithm>

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

    // Wire event callback (fires both user callback and EntityEventBus)
    player.SetEventCallback([this, entity](const std::string& eventName) {
        if (m_EventCallback) {
            m_EventCallback(entity, eventName);
        }
        BroadcastEvent(entity, eventName);
    });

    player.Start(dlg->dialogueTree);
    dlg->treeActive = true;

    SyncNodeToComponent(*dlg, player);

    // Show subtitle for the first text node
    ShowSubtitle(*dlg);

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
        ShowSubtitle(*dlg);
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
        ShowSubtitle(*dlg);
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
    if (!m_Enabled || !world) return;

    Entity activeDialogue = INVALID_ENTITY;

    for (Entity entity : world->GetEntitiesWithComponent<DialogueComponent>()) {
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

    // Sync DialogueBoxComponent → UICanvasComponent
    for (Entity entity : world->GetEntitiesWithComponent<DialogueBoxComponent>()) {
        auto* box = world->GetComponent<DialogueBoxComponent>(entity);
        auto* canvas = world->GetComponent<GUI::UICanvasComponent>(entity);
        auto* dlg = world->GetComponent<DialogueComponent>(entity);
        if (!box || !canvas || !dlg) continue;

        if (!box->initialized) {
            BuildDialogueBoxUI(*canvas, *box);
        }
        SyncDialogueBoxUI(*canvas, *box, *dlg, deltaTime);
    }
}

void DialogueSystem::BuildDialogueBoxUI(GUI::UICanvasComponent& canvas, DialogueBoxComponent& box) {
    // Build the UI element tree for the dialogue box
    f32 dw = canvas.designWidth;
    f32 dh = canvas.designHeight;
    f32 m = box.boxMargin;

    // Root panel (anchored to bottom of screen)
    u32 panelId = canvas.AddElement(GUI::UIWidgetType::Panel, "DialogueBox");
    auto* panel = canvas.GetElement(panelId);
    panel->anchor.anchorMin = Math::Vector2(0.0f, 1.0f);
    panel->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
    panel->anchor.pivot = Math::Vector2(0.5f, 1.0f);
    panel->anchor.offsetLeft = m;
    panel->anchor.offsetRight = m;
    panel->anchor.offsetTop = -(dh - box.boxHeight - m);
    panel->anchor.offsetBottom = m;
    panel->style.bgColor = box.boxColor;
    panel->style.bgAlpha = box.boxAlpha;
    panel->style.borderRadius = box.boxBorderRadius;
    panel->visible = false;
    box.panelElementId = panelId;

    f32 pad = box.boxPadding;
    f32 textLeft = pad;

    // Portrait image (left side of panel)
    if (box.showPortrait) {
        u32 portraitId = canvas.AddElement(GUI::UIWidgetType::Image, "Portrait", panelId);
        auto* portrait = canvas.GetElement(portraitId);
        portrait->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        portrait->anchor.anchorMax = Math::Vector2(0.0f, 0.0f);
        portrait->anchor.pivot = Math::Vector2(0.0f, 0.0f);
        portrait->anchor.offsetLeft = pad;
        portrait->anchor.offsetTop = pad;
        portrait->anchor.offsetRight = -(pad + box.portraitSize);
        portrait->anchor.offsetBottom = -(pad + box.portraitSize);
        box.portraitElementId = portraitId;
        textLeft = pad + box.portraitSize + pad;
    }

    // Speaker name label
    u32 speakerId = canvas.AddElement(GUI::UIWidgetType::Label, "SpeakerName", panelId);
    auto* speaker = canvas.GetElement(speakerId);
    speaker->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
    speaker->anchor.anchorMax = Math::Vector2(1.0f, 0.0f);
    speaker->anchor.pivot = Math::Vector2(0.0f, 0.0f);
    speaker->anchor.offsetLeft = textLeft;
    speaker->anchor.offsetTop = pad;
    speaker->anchor.offsetRight = pad;
    speaker->anchor.offsetBottom = -(pad + box.speakerFontSize + 4.0f);
    speaker->style.fontSize = box.speakerFontSize;
    speaker->data.textAlignH = 0; // left
    speaker->data.textAlignV = 1;
    box.speakerElementId = speakerId;

    // Dialogue text label
    f32 textTop = pad + box.speakerFontSize + 8.0f;
    u32 textId = canvas.AddElement(GUI::UIWidgetType::Label, "DialogueText", panelId);
    auto* text = canvas.GetElement(textId);
    text->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
    text->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
    text->anchor.pivot = Math::Vector2(0.0f, 0.0f);
    text->anchor.offsetLeft = textLeft;
    text->anchor.offsetTop = textTop;
    text->anchor.offsetRight = pad;
    text->anchor.offsetBottom = pad + 24.0f;
    text->style.fontSize = box.textFontSize;
    text->style.textColor = box.textColor;
    text->data.textAlignH = 0;
    text->data.textAlignV = 0; // top
    box.textElementId = textId;

    // Continue indicator (bottom-right)
    u32 contId = canvas.AddElement(GUI::UIWidgetType::Label, "ContinueIndicator", panelId);
    auto* cont = canvas.GetElement(contId);
    cont->anchor.anchorMin = Math::Vector2(1.0f, 1.0f);
    cont->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
    cont->anchor.pivot = Math::Vector2(1.0f, 1.0f);
    cont->anchor.offsetLeft = -(pad + 60.0f);
    cont->anchor.offsetTop = -(pad + 20.0f);
    cont->anchor.offsetRight = pad;
    cont->anchor.offsetBottom = pad;
    cont->data.text = box.continueText;
    cont->data.textAlignH = 2; // right
    cont->data.textAlignV = 1;
    cont->visible = false;
    box.continueElementId = contId;

    // Choice buttons
    box.choiceCount = 0;
    for (u32 i = 0; i < DialogueBoxComponent::MAX_CHOICES; ++i) {
        u32 btnId = canvas.AddElement(GUI::UIWidgetType::Button, "Choice" + std::to_string(i), panelId);
        auto* btn = canvas.GetElement(btnId);
        f32 btnTop = textTop + (static_cast<f32>(i) * (box.textFontSize + box.choiceSpacing + 8.0f));
        btn->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
        btn->anchor.anchorMax = Math::Vector2(1.0f, 0.0f);
        btn->anchor.pivot = Math::Vector2(0.0f, 0.0f);
        btn->anchor.offsetLeft = textLeft;
        btn->anchor.offsetTop = btnTop;
        btn->anchor.offsetRight = pad;
        btn->anchor.offsetBottom = -(btnTop + box.textFontSize + 8.0f);
        btn->style.bgColor = box.choiceColor;
        btn->style.bgAlpha = 0.8f;
        btn->style.textColor = box.choiceTextColor;
        btn->style.fontSize = box.textFontSize;
        btn->style.borderRadius = 4.0f;
        btn->data.textAlignH = 0;
        btn->visible = false;
        box.choiceElementIds[i] = btnId;
    }

    box.initialized = true;
}

void DialogueSystem::SyncDialogueBoxUI(GUI::UICanvasComponent& canvas, DialogueBoxComponent& box,
                                         const DialogueComponent& dlg, f32 deltaTime) {
    bool active = dlg.treeActive || (!dlg.dialogueLines.empty() && !dlg.IsComplete());

    // Show/hide the panel
    auto* panel = canvas.GetElement(box.panelElementId);
    if (panel) panel->visible = active;
    if (!active) return;

    // Speaker name (use const ref to avoid copy when possible)
    auto* speaker = canvas.GetElement(box.speakerElementId);
    if (speaker) {
        const std::string& name = dlg.currentSpeaker.empty() ? dlg.speakerName : dlg.currentSpeaker;
        if (speaker->data.text != name) {
            speaker->data.text = name;
        }
        speaker->style.textColor = dlg.IsTreeMode() ? dlg.currentSpeakerColor : box.defaultSpeakerColor;
    }

    // Dialogue text (typewriter)
    auto* text = canvas.GetElement(box.textElementId);
    if (text) {
        text->data.text = dlg.IsTreeMode() ? dlg.GetTreeVisibleText() : dlg.GetVisibleText();
    }

    // Portrait
    if (box.showPortrait) {
        auto* portrait = canvas.GetElement(box.portraitElementId);
        if (portrait) {
            portrait->data.imagePath = dlg.portraitPath;
            portrait->visible = !dlg.portraitPath.empty();
        }
    }

    // Continue indicator (blink when waiting for input, no choices)
    auto* cont = canvas.GetElement(box.continueElementId);
    if (cont) {
        bool showContinue = dlg.waitingForInput && dlg.currentChoices.empty();
        if (showContinue) {
            box.blinkTimer += deltaTime;
            f32 blink = std::sin(box.blinkTimer * box.continueBlinkSpeed * 3.14159f);
            cont->visible = blink > 0.0f;
        } else {
            cont->visible = false;
            box.blinkTimer = 0.0f;
        }
    }

    // Choices
    bool hasChoices = dlg.waitingForInput && !dlg.currentChoices.empty();
    u32 numChoices = hasChoices ? static_cast<u32>(dlg.currentChoices.size()) : 0;
    if (numChoices > DialogueBoxComponent::MAX_CHOICES)
        numChoices = DialogueBoxComponent::MAX_CHOICES;

    // Hide text label when showing choices, show choice buttons instead
    if (text) text->visible = !hasChoices;

    for (u32 i = 0; i < DialogueBoxComponent::MAX_CHOICES; ++i) {
        auto* btn = canvas.GetElement(box.choiceElementIds[i]);
        if (!btn) continue;
        if (i < numChoices) {
            btn->visible = true;
            // Build choice text in-place to minimize string allocations
            bool isSelected = (static_cast<i32>(i) == dlg.selectedChoice);
            const char* prefix = isSelected ? "> " : "  ";
            const std::string& choiceText = dlg.currentChoices[i].text;
            // Only rebuild if content changed (avoids per-frame string concatenation)
            if (btn->data.text.size() != 2 + choiceText.size() ||
                btn->data.text.compare(2, std::string::npos, choiceText) != 0 ||
                btn->data.text[0] != prefix[0]) {
                btn->data.text.clear();
                btn->data.text.reserve(2 + choiceText.size());
                btn->data.text.append(prefix);
                btn->data.text.append(choiceText);
            }
            btn->style.bgAlpha = isSelected ? 1.0f : 0.6f;
        } else {
            btn->visible = false;
        }
    }
}

void DialogueSystem::BroadcastEvent(Entity entity, const std::string& eventName) {
    if (!m_EventBus) return;

    EntityEvent evt;
    evt.name.reserve(9 + eventName.size());
    evt.name.append("Dialogue_");
    evt.name.append(eventName);
    evt.sender = entity;
    evt.strings["event"] = eventName;
    m_EventBus->Broadcast(evt);

    ENJIN_LOG_DEBUG(Script, "Dialogue event broadcast: %s from entity %llu",
                    evt.name.c_str(), static_cast<u64>(entity));
}

void DialogueSystem::ShowSubtitle(const DialogueComponent& dlg) {
    if (!m_SubtitleSystem) return;
    if (!m_SubtitleSystem->GetConfig().enabled) return;
    if (dlg.currentText.empty()) return;

    // Estimate duration: at least 3 seconds, ~0.05s per character for reading pace
    f32 duration = std::max(3.0f, static_cast<f32>(dlg.currentText.length()) * 0.05f);

    m_SubtitleSystem->ShowSubtitle(
        dlg.currentText,
        dlg.currentSpeaker,
        dlg.currentSpeakerColor,
        duration
    );
}

} // namespace ECS
} // namespace Enjin
