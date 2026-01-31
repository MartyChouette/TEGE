#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <string>
#include <vector>
#include <utility>

namespace Enjin::GUI {

    struct ItemSlot {
        i32 itemId = -1;
        u32 count = 0;
        std::string name;
        std::string tooltip;
        Math::Vector3 color = Math::Vector3(0.5f, 0.5f, 0.5f);
        bool stackable = true;
        u32 maxStack = 99;
    };

    struct CraftingRecipe {
        std::string resultName;
        i32 resultId;
        u32 resultCount;
        std::vector<std::pair<i32, u32>> ingredients; // itemId, count needed
    };

    class ENJIN_API InventoryUI {
    public:
        InventoryUI() = default;
        ~InventoryUI() = default;

        void Render(f32 screenW, f32 screenH);

        void SetVisible(bool v) { m_Visible = v; }
        bool IsVisible() const { return m_Visible; }

        void SetSlots(u32 rows, u32 cols);

        ItemSlot* GetSlot(u32 row, u32 col);

        bool AddItem(i32 itemId, const std::string& name, u32 count = 1,
                     Math::Vector3 color = Math::Vector3(0.5f, 0.5f, 0.5f));
        bool RemoveItem(i32 itemId, u32 count = 1);
        u32 GetItemCount(i32 itemId) const;

        void AddRecipe(const CraftingRecipe& recipe);
        bool CanCraft(const CraftingRecipe& recipe) const;
        bool Craft(const CraftingRecipe& recipe);

        void SetCraftingVisible(bool v) { m_CraftingVisible = v; }
        bool IsCraftingVisible() const { return m_CraftingVisible; }

    private:
        bool m_Visible = false;
        bool m_CraftingVisible = false;
        u32 m_Rows = 4;
        u32 m_Cols = 8;
        std::vector<ItemSlot> m_Slots = std::vector<ItemSlot>(m_Rows * m_Cols);
        std::vector<CraftingRecipe> m_Recipes;
        i32 m_DragFromSlot = -1;
        i32 m_SelectedSlot = -1;
    };

} // namespace Enjin::GUI
