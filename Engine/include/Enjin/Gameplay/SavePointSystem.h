#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include <string>

namespace Enjin {
namespace InputSystem { class InputActionMap; }

namespace Gameplay {

class TieredSaveSystem;

// Makes SavePointComponent actually save.
//
// It did not, until 2026-09-16. `SavePointComponent` had five script setters
// (SetSlot, SetSaveOnEnter, SetRadius, SetMessage) and no reader anywhere: none
// of SaveSystem.cpp, SaveBackend.cpp, SaveLoadMenu.cpp or TieredSaveSystem.cpp
// mentioned save points at all. `used` was never written, so SavePoint_IsUsed()
// could only ever return false, and `playerInRange` was never touched. A save
// point configured through the API compiled, ran, logged nothing, and did not
// save. That is the silent-stub shape: a setter storing a value nothing reads.
//
// WHAT THIS HONOURS, and nothing more. From SavePointComponent: slotTarget,
// saveOnEnter, oneTimeUse, radius, saveMessage, and the two runtime fields used
// and playerInRange. From SaveSystemComponent, which is scene-level
// configuration: allowInWorldSavePoints, savePointRadius (the default when a
// point's own radius is 0), savePointRequiresInput, savePointShowPrompt,
// showSaveIndicator and saveIndicatorDuration.
//
// WHAT IT DOES NOT. SaveSystemComponent also carries auto-save (timer, scene
// transition, rotation), meta-progression, cloud sync and the pause-menu save
// UI. Those fields are still read by nothing. Wiring save points does not make
// that component live, and saying otherwise would swap one silent stub for a
// misleading one.
//
// INPUT comes from the ACTION MAP, not from SaveSystemComponent::savePointKey.
// Gameplay input is actions here (GameAction::Interact), so a player's rebind
// and the touch button both work; a hardcoded keycode would honour neither.
// That leaves savePointKey read by nothing, which is recorded rather than
// hidden -- it wants removing or turning into an action override, and that is a
// decision rather than a cleanup.
class ENJIN_API SavePointSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    // TieredSaveSystem, not the plain SaveSystem: it is what the runtimes
    // actually own, and what SaveSystemComponent's own header says handles
    // the persistence.
    void SetSaveSystem(TieredSaveSystem* saveSystem) { m_SaveSystem = saveSystem; }
    void SetInputActionMap(InputSystem::InputActionMap* map) { m_InputMap = map; }

    // The scene a save records. The player already tracks this for
    // TieredSaveSystem; the same string comes here.
    void SetSceneName(const std::string& scene) { m_SceneName = scene; }

    void Update(f32 deltaTime);

    // For a HUD. Empty when there is nothing to show.
    //
    // The system does not draw: it has no business knowing about fonts or
    // canvases, and a runtime that wants no prompt should not have to suppress
    // one. It reports, and whoever draws decides.
    const std::string& GetPrompt() const { return m_Prompt; }
    const std::string& GetMessage() const { return m_Message; }
    f32 GetMessageTimer() const { return m_MessageTimer; }

private:
    ECS::Entity FindPlayer() const;

    ECS::World* m_World = nullptr;
    TieredSaveSystem* m_SaveSystem = nullptr;
    InputSystem::InputActionMap* m_InputMap = nullptr;
    std::string m_SceneName;

    std::string m_Prompt;
    std::string m_Message;
    f32 m_MessageTimer = 0.0f;
};

} // namespace Gameplay
} // namespace Enjin
