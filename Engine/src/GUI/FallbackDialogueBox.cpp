#include "Enjin/GUI/FallbackDialogueBox.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Input/InputAction.h"
#include <imgui.h>
#include <cmath>
#include <string>

namespace Enjin {
namespace GUI {

void DrawFallbackDialogueBox(ECS::World* world, ECS::Entity active,
                             const InputSystem::InputActionMap* map) {
    if (!world || active == 0) return;

    // Defer to the data-driven ECS dialogue box when this entity defines one:
    // the DialogueSystem renders a styled, per-entity DialogueBoxComponent, and
    // this built-in ImGui overlay is only the fallback for entities without a
    // box. That keeps the two from doubling up on screen.
    if (world->GetComponent<Enjin::ECS::DialogueBoxComponent>(active)) return;

    auto* dlg = world->GetComponent<Enjin::ECS::DialogueComponent>(active);
    if (!dlg) return;

    // Determine speaker, visible text, and choices based on mode
    std::string speaker;
    std::string visibleText;
    bool isTyping = dlg->isTyping;
    bool waiting = dlg->waitingForInput;
    bool hasChoices = false;
    Enjin::i32 selectedChoice = dlg->selectedChoice;
    Enjin::i32 choiceCount = 0;

    if (dlg->IsTreeMode()) {
        if (!dlg->treeActive) return;
        speaker = dlg->currentSpeaker;
        visibleText = dlg->GetTreeVisibleText();
        hasChoices = waiting && !dlg->currentChoices.empty();
        choiceCount = static_cast<Enjin::i32>(dlg->currentChoices.size());
    } else {
        if (dlg->IsComplete()) return;
        speaker = dlg->speakerName;
        visibleText = dlg->GetVisibleText();
        hasChoices = waiting && !dlg->choices.empty() &&
                     dlg->currentLine + 1 >= dlg->dialogueLines.size();
        choiceCount = static_cast<Enjin::i32>(dlg->choices.size());
    }

    ImGuiIO& io = ImGui::GetIO();
    Enjin::f32 screenW = io.DisplaySize.x;
    Enjin::f32 screenH = io.DisplaySize.y;

    Enjin::f32 boxW = screenW * 0.75f;
    Enjin::f32 boxH = 140.0f;
    Enjin::f32 boxX = (screenW - boxW) * 0.5f;
    Enjin::f32 boxY = screenH - boxH - 30.0f;
    Enjin::f32 padding = 16.0f;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padding, padding));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.1f, 0.92f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.45f, 0.65f, 0.8f));

    ImGui::SetNextWindowPos(ImVec2(boxX, boxY));
    ImGui::SetNextWindowSize(ImVec2(boxW, boxH));

    if (ImGui::Begin("##DialogueBox", nullptr, flags)) {
        if (!speaker.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.75f, 1.0f, 1.0f));
            ImGui::Text("%s", speaker.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::Spacing();
        }

        if (!visibleText.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.98f, 1.0f));
            ImGui::TextWrapped("%s", visibleText.c_str());
            ImGui::PopStyleColor();
        }

        if (isTyping) {
            ImGui::SameLine(0, 0);
            Enjin::f32 blink = std::fmod(static_cast<Enjin::f32>(ImGui::GetTime()) * 3.0f, 2.0f);
            if (blink < 1.0f) {
                ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 0.8f), "_");
            }
        }

        if (waiting && !hasChoices) {
            Enjin::f32 bounce = std::sin(static_cast<Enjin::f32>(ImGui::GetTime()) * 4.0f) * 0.3f + 0.7f;
            ImGui::SetCursorPosY(boxH - padding - ImGui::GetTextLineHeight());
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.6f, 0.8f, bounce));
            // The Advance Dialogue action's binding, which is what advancing
            // reads; it said "[Space]" whatever that was bound to (IN-40).
            ImGui::Text("[%s]", (map ? map->GetBindingDisplayName(
                static_cast<Enjin::i32>(Enjin::InputSystem::GameAction::DialogueAdvance)) : "Space"));
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();

    if (hasChoices) {
        Enjin::f32 choiceH = static_cast<Enjin::f32>(choiceCount) * 28.0f + padding * 2.0f;
        Enjin::f32 choiceW = 300.0f;
        Enjin::f32 choiceX = boxX + boxW - choiceW - 10.0f;
        Enjin::f32 choiceY = boxY - choiceH - 8.0f;

        ImGui::SetNextWindowPos(ImVec2(choiceX, choiceY));
        ImGui::SetNextWindowSize(ImVec2(choiceW, choiceH));

        if (ImGui::Begin("##ChoiceBox", nullptr, flags)) {
            if (dlg->IsTreeMode()) {
                for (Enjin::usize i = 0; i < dlg->currentChoices.size(); ++i) {
                    bool sel = (static_cast<Enjin::i32>(i) == selectedChoice);
                    if (sel) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.6f, 1.0f));
                        ImGui::Text("> %s", dlg->currentChoices[i].text.c_str());
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.75f, 1.0f));
                        ImGui::Text("  %s", dlg->currentChoices[i].text.c_str());
                        ImGui::PopStyleColor();
                    }
                }
            } else {
                for (Enjin::usize i = 0; i < dlg->choices.size(); ++i) {
                    bool sel = (static_cast<Enjin::i32>(i) == selectedChoice);
                    if (sel) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.6f, 1.0f));
                        ImGui::Text("> %s", dlg->choices[i].text.c_str());
                        ImGui::PopStyleColor();
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.75f, 1.0f));
                        ImGui::Text("  %s", dlg->choices[i].text.c_str());
                        ImGui::PopStyleColor();
                    }
                }
            }
        }
        ImGui::End();
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

} // namespace GUI
} // namespace Enjin
