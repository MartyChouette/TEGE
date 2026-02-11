#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include <string>

namespace Enjin {
namespace ECS { class World; }
namespace Gameplay {

class TieredSaveSystem;

// In-game save/load menu component (renders as UICanvas overlay)
struct SaveLoadMenuComponent {
    bool showOnPause = true;       // Auto-show when game is paused
    bool allowManualSave = true;   // Show "Save" buttons
    bool allowManualLoad = true;   // Show "Load" buttons
    bool allowDelete = true;       // Show "Delete" buttons
    bool showAutoSaves = true;     // Show auto-save slots
    i32 columnsPerRow = 4;         // Grid layout columns
    std::string headerText = "Save / Load Game";

    // Runtime state (not serialized)
    bool isOpen = false;
    i32 selectedSlot = -1;
    bool confirmOverwrite = false;
    bool confirmDelete = false;

    enum class Mode : u8 { Save, Load } mode = Mode::Save;
};

// Render the save/load menu using ImGui (called during play mode UI pass)
ENJIN_API void DrawSaveLoadMenu(SaveLoadMenuComponent& menu,
                                 TieredSaveSystem* saveSystem,
                                 ECS::World* world);

} // namespace Gameplay
} // namespace Enjin
