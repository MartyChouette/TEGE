#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include <string>

namespace Enjin {
namespace ECS { class World; struct SaveLoadMenuComponent; }
namespace Gameplay {

class TieredSaveSystem;

// SaveLoadMenuComponent USED TO BE DECLARED HERE, a second time.
//
// An identical copy lives in Enjin::ECS (ECS/Components/Gameplay.h) and that is
// the one the inspector adds, the serializer round-trips and every scene on disk
// contains. This copy was in Enjin::Gameplay and was the one DrawSaveLoadMenu
// took, so the function could never be called with a component any entity
// actually had -- the two types can not meet. That is why it had no caller in
// the entire repo: there could not be one.
//
// Same shape as Enjin/Math/Vector.h existing in two include roots. A duplicate
// declaration does not fail to build; it quietly splits a feature in half.
//
// Forward-declared rather than including ECS/Components/Gameplay.h, which is a
// very large header and is not needed to take a reference. The declaration sits
// beside the existing `namespace ECS { class World; }` at Enjin scope, NOT
// inside Gameplay -- writing it there declares Enjin::Gameplay::ECS, which then
// shadows the real Enjin::ECS for every header included after this one.

// Render the save/load menu using ImGui (called during play mode UI pass)
// maxManualSlots is the scene-wide ceiling from SaveSystemComponent, which had
// no reader: the menu listed every slot the save system reports regardless of
// what the game said it allowed. 0 means "no ceiling", which is what a caller
// with no SaveSystemComponent in the scene passes.
ENJIN_API void DrawSaveLoadMenu(ECS::SaveLoadMenuComponent& menu,
                                 TieredSaveSystem* saveSystem,
                                 ECS::World* world,
                                 u32 maxManualSlots = 0);

} // namespace Gameplay
} // namespace Enjin
