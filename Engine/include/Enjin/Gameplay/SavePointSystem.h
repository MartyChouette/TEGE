#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include <string>

namespace Enjin {
namespace InputSystem { class InputActionMap; }
namespace Accessibility { class AccessibilityAnnouncer; }

namespace Gameplay {

class TieredSaveSystem;
class SaveIndicator;

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

    // Where the prompt and the confirmation are SHOWN and SPOKEN.
    //
    // Two wrong homes before this one, and both are worth not repeating. An
    // ImGui chip drawn by this system bypassed the HUD font, the accessibility
    // text scale and the editor's game-view origin. The SUBTITLE system fixed
    // all three and introduced a worse problem: ShowCaption is opt-in, because
    // a caption describes a SOUND for someone who cannot hear it, so a save
    // became invisible to anyone without captions turned on.
    //
    // SaveIndicator is the save system's own feedback, configured by
    // SaveSystemComponent's showSaveIndicator and saveIndicatorDuration -- the
    // settings that were written for this and had nothing reading them.
    //
    // The ANNOUNCER stays. A save is exactly the kind of event a screen-reader
    // user needs and cannot otherwise perceive, and that is what an announcer
    // is for. Both are optional: a runtime that wants neither passes neither.
    void SetIndicator(SaveIndicator* indicator) { m_Indicator = indicator; }
    void SetAnnouncer(Accessibility::AccessibilityAnnouncer* announcer) { m_Announcer = announcer; }

    // The scene a save records. The player already tracks this for
    // TieredSaveSystem; the same string comes here.
    void SetSceneName(const std::string& scene) { m_SceneName = scene; }

    void Update(f32 deltaTime);

    // What the system last put on screen, for tests. The DISPLAY goes through
    // SaveIndicator; these are observables, not a drawing contract.
    const std::string& GetPrompt() const { return m_Prompt; }
    const std::string& GetMessage() const { return m_Message; }

private:
    ECS::Entity FindPlayer() const;

    ECS::World* m_World = nullptr;
    TieredSaveSystem* m_SaveSystem = nullptr;
    InputSystem::InputActionMap* m_InputMap = nullptr;
    SaveIndicator* m_Indicator = nullptr;
    Accessibility::AccessibilityAnnouncer* m_Announcer = nullptr;
    std::string m_SceneName;

    std::string m_Prompt;
    std::string m_Message;
    // Which point is currently prompting, so the caption is shown once on
    // entering range rather than re-issued every frame.
    ECS::Entity m_PromptingPoint = ECS::INVALID_ENTITY;
};

} // namespace Gameplay
} // namespace Enjin
