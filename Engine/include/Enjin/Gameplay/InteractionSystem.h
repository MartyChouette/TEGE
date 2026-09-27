#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"
#include <string>

namespace Enjin {
namespace ECS { class World; class EntityEventBus; class RenderSystem; }
namespace InputSystem { class InputActionMap; }
namespace Accessibility { class AccessibilityAnnouncer; }
namespace Gameplay {

// The player using the things around them.
//
// InteractableComponent had a prompt, a range, a look-at check, single use, a
// highlight and a notify target, and there was no system: nothing chose one,
// drew its prompt or did anything on Interact, so every "Press E" in a game
// was a script drawing its own UI (SD-27).
//
// Each frame it picks the nearest usable interactable within Interaction
// Range of the player (the character controller camera zones follow; the
// camera when there is none) and, when Requires Look At is on, within Look At
// Angle of where the game camera faces. The focused one:
//   - shows its prompt, with {Interact} replaced by the live binding, and
//     announces it once for the screen reader
//   - is outlined in its Highlight Color when Highlight On Hover is on
//   - on the Interact action sends its Interact Event through the entity event
//     bus (sender = the interactable, target = On Interact Notify, "interactor"
//     = the player), which reaches scripts (Events_Listen) and visual scripts
//     (Custom Event); a single-use one is then spent
//
// Same overlay shape as SaveIndicator: Update per frame, RenderOverlay beside
// the other overlays, Reset when play stops.
class ENJIN_API InteractionSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    void SetInputActionMap(InputSystem::InputActionMap* map) { m_InputMap = map; }
    void SetEventBus(ECS::EntityEventBus* bus) { m_EventBus = bus; }
    void SetRenderSystem(ECS::RenderSystem* rs) { m_RenderSystem = rs; }
    void SetAnnouncer(Accessibility::AccessibilityAnnouncer* a) { m_Announcer = a; }

    void Update(f32 deltaTime);
    // The same with the press supplied, for tests and callers without a map
    void Update(f32 deltaTime, bool interactPressed);

    void RenderOverlay(f32 originX, f32 originY, u32 viewportWidth, u32 viewportHeight);

    // Drop the focus and its highlight (editor Stop)
    void Reset();

    ECS::Entity GetFocused() const { return m_Focused; }
    const std::string& GetPrompt() const { return m_Prompt; }

private:
    ECS::World* m_World = nullptr;
    InputSystem::InputActionMap* m_InputMap = nullptr;
    ECS::EntityEventBus* m_EventBus = nullptr;
    ECS::RenderSystem* m_RenderSystem = nullptr;
    Accessibility::AccessibilityAnnouncer* m_Announcer = nullptr;

    ECS::Entity m_Focused = ECS::INVALID_ENTITY;
    std::string m_Prompt;
};

} // namespace Gameplay
} // namespace Enjin
