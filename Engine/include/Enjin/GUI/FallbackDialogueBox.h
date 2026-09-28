#pragma once
// The built-in dialogue box: speaker, typed-out text and choices, drawn with
// ImGui for a dialogue whose entity has no DialogueBoxComponent.
//
// It lived in the desktop player, so on web a dialogue without its own box
// showed nothing at all. Both players call this inside their ImGui frame.

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace ECS { class World; }
namespace InputSystem { class InputActionMap; }
namespace GUI {

// Draw the box for `active` (0 = no dialogue). Does nothing when the entity
// has a DialogueBoxComponent, which the DialogueSystem draws instead. `map`
// names the Advance Dialogue binding in the prompt; null shows "Space".
ENJIN_API void DrawFallbackDialogueBox(ECS::World* world, ECS::Entity active,
                                       const InputSystem::InputActionMap* map);

} // namespace GUI
} // namespace Enjin
