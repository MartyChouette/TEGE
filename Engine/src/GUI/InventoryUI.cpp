#include "Enjin/GUI/InventoryUI.h"

#include <imgui.h>
#include <cstdio>

namespace Enjin::GUI {

    void InventoryUI::SetSlots(u32 rows, u32 cols) {
        m_Rows = rows;
        m_Cols = cols;
        m_Slots.resize(static_cast<usize>(m_Rows) * m_Cols);
    }

    ItemSlot* InventoryUI::GetSlot(u32 row, u32 col) {
        if (row >= m_Rows || col >= m_Cols) {
            return nullptr;
        }
        return &m_Slots[static_cast<usize>(row) * m_Cols + col];
    }

    bool InventoryUI::AddItem(i32 itemId, const std::string& name, u32 count,
                              Math::Vector3 color) {
        // First pass: try to stack into existing slots with the same item
        for (auto& slot : m_Slots) {
            if (slot.itemId == itemId && slot.stackable && slot.count < slot.maxStack) {
                u32 canAdd = slot.maxStack - slot.count;
                u32 toAdd = (count < canAdd) ? count : canAdd;
                slot.count += toAdd;
                count -= toAdd;
                if (count == 0) {
                    return true;
                }
            }
        }

        // Second pass: place remaining into empty slots
        while (count > 0) {
            bool foundEmpty = false;
            for (auto& slot : m_Slots) {
                if (slot.itemId == -1) {
                    slot.itemId = itemId;
                    slot.name = name;
                    slot.color = color;
                    slot.stackable = true;
                    u32 toAdd = (count < slot.maxStack) ? count : slot.maxStack;
                    slot.count = toAdd;
                    count -= toAdd;
                    foundEmpty = true;
                    break;
                }
            }
            if (!foundEmpty) {
                return false; // No room left
            }
        }

        return true;
    }

    bool InventoryUI::RemoveItem(i32 itemId, u32 count) {
        // Verify we have enough before removing
        u32 available = GetItemCount(itemId);
        if (available < count) {
            return false;
        }

        for (auto& slot : m_Slots) {
            if (slot.itemId == itemId && count > 0) {
                u32 toRemove = (count < slot.count) ? count : slot.count;
                slot.count -= toRemove;
                count -= toRemove;
                if (slot.count == 0) {
                    slot.itemId = -1;
                    slot.name.clear();
                    slot.tooltip.clear();
                    slot.color = Math::Vector3(0.5f, 0.5f, 0.5f);
                }
                if (count == 0) {
                    break;
                }
            }
        }

        return true;
    }

    u32 InventoryUI::GetItemCount(i32 itemId) const {
        u32 total = 0;
        for (const auto& slot : m_Slots) {
            if (slot.itemId == itemId) {
                total += slot.count;
            }
        }
        return total;
    }

    void InventoryUI::AddRecipe(const CraftingRecipe& recipe) {
        m_Recipes.push_back(recipe);
    }

    bool InventoryUI::CanCraft(const CraftingRecipe& recipe) const {
        for (const auto& [ingredientId, needed] : recipe.ingredients) {
            if (GetItemCount(ingredientId) < needed) {
                return false;
            }
        }
        return true;
    }

    bool InventoryUI::Craft(const CraftingRecipe& recipe) {
        if (!CanCraft(recipe)) {
            return false;
        }

        // Remove ingredients
        for (const auto& [ingredientId, needed] : recipe.ingredients) {
            RemoveItem(ingredientId, needed);
        }

        // Add crafted result
        AddItem(recipe.resultId, recipe.resultName, recipe.resultCount);
        return true;
    }

    void InventoryUI::Render(f32 screenW, f32 screenH) {
        if (!m_Visible) {
            return;
        }

        const f32 slotSize = 60.0f;
        const f32 slotPadding = 4.0f;
        const f32 gridWidth = static_cast<f32>(m_Cols) * (slotSize + slotPadding) + slotPadding;
        const f32 gridHeight = static_cast<f32>(m_Rows) * (slotSize + slotPadding) + slotPadding;

        // Center the inventory window
        ImGui::SetNextWindowSize(ImVec2(gridWidth + 20.0f, gridHeight + 60.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.5f), ImGuiCond_FirstUseEver,
                                ImVec2(0.5f, 0.5f));

        if (!ImGui::Begin("Inventory", &m_Visible)) {
            ImGui::End();
            return;
        }

        // Inventory grid
        ImGui::BeginChild("InventoryGrid", ImVec2(gridWidth, gridHeight), true);

        for (u32 row = 0; row < m_Rows; ++row) {
            for (u32 col = 0; col < m_Cols; ++col) {
                i32 slotIndex = static_cast<i32>(row * m_Cols + col);
                ItemSlot& slot = m_Slots[static_cast<usize>(slotIndex)];

                if (col > 0) {
                    ImGui::SameLine();
                }

                ImGui::PushID(slotIndex);

                // Color the button based on slot contents
                if (slot.itemId >= 0) {
                    ImVec4 btnColor(slot.color.x * 0.6f, slot.color.y * 0.6f,
                                    slot.color.z * 0.6f, 1.0f);
                    ImVec4 btnHovered(slot.color.x * 0.8f, slot.color.y * 0.8f,
                                      slot.color.z * 0.8f, 1.0f);
                    ImVec4 btnActive(slot.color.x, slot.color.y, slot.color.z, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHovered);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, btnActive);
                } else {
                    ImVec4 emptyColor(0.15f, 0.15f, 0.15f, 1.0f);
                    ImVec4 emptyHovered(0.2f, 0.2f, 0.2f, 1.0f);
                    ImVec4 emptyActive(0.25f, 0.25f, 0.25f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, emptyColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, emptyHovered);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, emptyActive);
                }

                // Build label: item name + count
                char label[128];
                if (slot.itemId >= 0 && slot.count > 0) {
                    std::snprintf(label, sizeof(label), "%s\n%u",
                                  slot.name.c_str(), slot.count);
                } else {
                    label[0] = '\0';
                }

                ImGui::Button(label, ImVec2(slotSize, slotSize));

                // Selection highlight
                if (slotIndex == m_SelectedSlot) {
                    ImVec2 min = ImGui::GetItemRectMin();
                    ImVec2 max = ImGui::GetItemRectMax();
                    ImGui::GetWindowDrawList()->AddRect(min, max,
                        IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
                }

                // Click to select
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    m_SelectedSlot = slotIndex;
                }

                // Tooltip on hover
                if (ImGui::IsItemHovered() && slot.itemId >= 0) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", slot.name.c_str());
                    if (!slot.tooltip.empty()) {
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                                           "%s", slot.tooltip.c_str());
                    }
                    ImGui::Text("Count: %u / %u", slot.count, slot.maxStack);
                    ImGui::EndTooltip();
                }

                // Drag source
                if (slot.itemId >= 0 && ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                    m_DragFromSlot = slotIndex;
                    ImGui::SetDragDropPayload("INVENTORY_SLOT", &slotIndex, sizeof(i32));
                    ImGui::Text("%s x%u", slot.name.c_str(), slot.count);
                    ImGui::EndDragDropSource();
                }

                // Drop target
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("INVENTORY_SLOT")) {
                        i32 srcIndex = *static_cast<const i32*>(payload->Data);
                        if (srcIndex != slotIndex && srcIndex >= 0 &&
                            srcIndex < static_cast<i32>(m_Slots.size())) {
                            ItemSlot& srcSlot = m_Slots[static_cast<usize>(srcIndex)];

                            // If same item and stackable, merge stacks
                            if (slot.itemId == srcSlot.itemId && slot.stackable &&
                                srcSlot.stackable) {
                                u32 canAdd = slot.maxStack - slot.count;
                                u32 toMove = (srcSlot.count < canAdd) ? srcSlot.count : canAdd;
                                slot.count += toMove;
                                srcSlot.count -= toMove;
                                if (srcSlot.count == 0) {
                                    srcSlot.itemId = -1;
                                    srcSlot.name.clear();
                                    srcSlot.tooltip.clear();
                                    srcSlot.color = Math::Vector3(0.5f, 0.5f, 0.5f);
                                }
                            } else {
                                // Swap slots
                                std::swap(slot, srcSlot);
                            }
                        }
                        m_DragFromSlot = -1;
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::PopStyleColor(3);
                ImGui::PopID();
            }
        }

        ImGui::EndChild();

        // Crafting section
        if (m_CraftingVisible && !m_Recipes.empty()) {
            ImGui::Separator();
            ImGui::Text("Crafting");
            ImGui::Separator();

            ImGui::BeginChild("CraftingPanel", ImVec2(0, 0), true);

            for (usize i = 0; i < m_Recipes.size(); ++i) {
                const CraftingRecipe& recipe = m_Recipes[i];
                bool canCraft = CanCraft(recipe);

                ImGui::PushID(static_cast<i32>(i));

                // Recipe header
                if (canCraft) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 1.0f, 0.2f, 1.0f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                }

                ImGui::Text("%s x%u", recipe.resultName.c_str(), recipe.resultCount);
                ImGui::PopStyleColor();

                // Ingredients list
                ImGui::Indent();
                for (const auto& [ingredientId, needed] : recipe.ingredients) {
                    u32 have = GetItemCount(ingredientId);
                    // Find a name for this ingredient from inventory
                    const char* ingredientName = "Unknown";
                    for (const auto& slot : m_Slots) {
                        if (slot.itemId == ingredientId) {
                            ingredientName = slot.name.c_str();
                            break;
                        }
                    }
                    ImVec4 textColor = (have >= needed)
                        ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
                        : ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
                    ImGui::TextColored(textColor, "  %s: %u / %u",
                                       ingredientName, have, needed);
                }
                ImGui::Unindent();

                // Craft button
                ImGui::SameLine(gridWidth - 80.0f);
                if (!canCraft) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::Button("Craft", ImVec2(70.0f, 0.0f))) {
                    Craft(recipe);
                }
                if (!canCraft) {
                    ImGui::EndDisabled();
                }

                ImGui::Separator();
                ImGui::PopID();
            }

            ImGui::EndChild();
        }

        ImGui::End();
    }

} // namespace Enjin::GUI
