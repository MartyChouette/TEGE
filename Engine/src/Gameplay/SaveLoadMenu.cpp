#include "Enjin/Gameplay/SaveLoadMenu.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/ECS/World.h"
#include <imgui.h>

namespace Enjin {
namespace Gameplay {

void DrawSaveLoadMenu(SaveLoadMenuComponent& menu,
                       TieredSaveSystem* saveSystem,
                       ECS::World* world) {
    if (!menu.isOpen || !saveSystem || !world) return;

    ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    bool open = true;
    ImGui::Begin(menu.headerText.c_str(), &open, flags);
    if (!open) {
        menu.isOpen = false;
        ImGui::End();
        return;
    }

    // Mode tabs
    if (menu.allowManualSave) {
        if (ImGui::RadioButton("Save", menu.mode == SaveLoadMenuComponent::Mode::Save)) {
            menu.mode = SaveLoadMenuComponent::Mode::Save;
        }
        ImGui::SameLine();
    }
    if (menu.allowManualLoad) {
        if (ImGui::RadioButton("Load", menu.mode == SaveLoadMenuComponent::Mode::Load)) {
            menu.mode = SaveLoadMenuComponent::Mode::Load;
        }
    }
    ImGui::Separator();

    auto slots = saveSystem->GetAllSlots();
    i32 cols = menu.columnsPerRow > 0 ? menu.columnsPerRow : 4;

    if (ImGui::BeginTable("SaveLoadGrid", cols, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
        i32 idx = 0;
        for (const auto& slot : slots) {
            // Skip auto-saves if configured
            if (slot.isAutoSave && !menu.showAutoSaves) continue;

            if (idx % cols == 0) ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(slot.slotIndex));

            // Slot card
            ImVec2 cardStart = ImGui::GetCursorPos();
            bool isSelected = (menu.selectedSlot == static_cast<i32>(slot.slotIndex));

            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.4f, 0.6f, 0.3f));
            }

            ImGui::BeginGroup();
            if (slot.isEmpty) {
                ImGui::TextDisabled("--- Empty ---");
                if (slot.isAutoSave) {
                    ImGui::TextDisabled("Auto %u", slot.slotIndex - TieredSaveSystem::AUTO_SAVE_SLOT_START + 1);
                } else {
                    ImGui::TextDisabled("Slot %u", slot.slotIndex + 1);
                }
            } else {
                ImGui::Text("%s", slot.displayName.c_str());
                ImGui::TextDisabled("%s", slot.sceneName.c_str());
                i32 mins = static_cast<i32>(slot.playTime) / 60;
                i32 secs = static_cast<i32>(slot.playTime) % 60;
                ImGui::Text("%d:%02d  %s", mins, secs, slot.timestamp.c_str());
            }

            // Action button
            if (menu.mode == SaveLoadMenuComponent::Mode::Save && menu.allowManualSave && !slot.isAutoSave) {
                if (slot.isEmpty) {
                    if (ImGui::SmallButton("Save Here")) {
                        saveSystem->SaveToSlot(slot.slotIndex, world,
                            saveSystem->GetCurrentScene());
                    }
                } else {
                    if (!menu.confirmOverwrite || menu.selectedSlot != static_cast<i32>(slot.slotIndex)) {
                        if (ImGui::SmallButton("Overwrite")) {
                            menu.selectedSlot = static_cast<i32>(slot.slotIndex);
                            menu.confirmOverwrite = true;
                        }
                    } else {
                        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Confirm?");
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Yes")) {
                            saveSystem->SaveToSlot(slot.slotIndex, world,
                                saveSystem->GetCurrentScene());
                            menu.confirmOverwrite = false;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("No")) {
                            menu.confirmOverwrite = false;
                        }
                    }
                }
            } else if (menu.mode == SaveLoadMenuComponent::Mode::Load && menu.allowManualLoad && !slot.isEmpty) {
                if (ImGui::SmallButton("Load")) {
                    saveSystem->LoadFromSlot(slot.slotIndex, world);
                    menu.isOpen = false;
                }
            }

            // Delete button
            if (menu.allowDelete && !slot.isEmpty) {
                ImGui::SameLine();
                if (!menu.confirmDelete || menu.selectedSlot != static_cast<i32>(slot.slotIndex)) {
                    if (ImGui::SmallButton("X")) {
                        menu.selectedSlot = static_cast<i32>(slot.slotIndex);
                        menu.confirmDelete = true;
                    }
                } else {
                    ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Delete?");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Yes")) {
                        saveSystem->DeleteSlot(slot.slotIndex);
                        menu.confirmDelete = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("No")) {
                        menu.confirmDelete = false;
                    }
                }
            }

            ImGui::EndGroup();
            if (isSelected) {
                ImGui::PopStyleColor();
            }

            ImGui::PopID();
            idx++;
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace Gameplay
} // namespace Enjin
