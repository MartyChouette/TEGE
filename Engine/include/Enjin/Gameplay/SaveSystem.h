#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Gameplay {

struct SaveSlot {
    i32 slotIndex = -1;
    std::string sceneName;
    std::string timestamp;
    f32 playTime = 0.0f;
    std::string displayName;
    bool isEmpty = true;
};

class ENJIN_API SaveSystem {
public:
    static constexpr i32 MAX_SLOTS = 10;

    bool SaveToSlot(i32 slot, ECS::World* world, const std::string& sceneName);
    bool LoadFromSlot(i32 slot, ECS::World* world);
    bool DeleteSlot(i32 slot);
    SaveSlot GetSlotInfo(i32 slot) const;
    std::vector<SaveSlot> GetAllSlots() const;
    bool QuickSave(ECS::World* world, const std::string& sceneName);
    bool QuickLoad(ECS::World* world);

    static std::string GetSaveDirectory();

private:
    std::string GetSlotPath(i32 slot) const;
    std::string GetQuickSavePath() const;
};

} // namespace Gameplay
} // namespace Enjin
