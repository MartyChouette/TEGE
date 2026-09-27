#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Entity.h"
#include <functional>
#include <vector>

namespace Enjin {
namespace ECS { class World; }
namespace InputSystem { class InputActionMap; }
namespace GUI {

class UISystem;

// The one controls screen (IN-16). The desktop player, the web player and
// editor play all open this; before it there were two screens that disagreed
// about which actions exist, which inputs can be rebound and which presets
// are offered.
//
// Built as a UICanvas (UITemplates::CreateControlsMenu) so it works under a
// thumb and a pad as well as a mouse. Each action row has a key button and a
// pad button; pressing one arms it and the next key, mouse button or pad
// input becomes the binding. A binding that clashes with another action in
// the same context is named under the row (IN-9).
//
// The owner calls Update every frame BEFORE its own Escape / back handling,
// and checks IsCapturing: while a row is armed, Escape cancels the capture
// instead of leaving the screen.
class ENJIN_API ControlsScreen {
public:
    ControlsScreen() = default;
    ~ControlsScreen();
    ControlsScreen(const ControlsScreen&) = delete;
    ControlsScreen& operator=(const ControlsScreen&) = delete;

    // The world may change on a scene load; call Attach again with the new one
    // (it does nothing when nothing changed, so every frame is fine)
    void Attach(ECS::World* world, UISystem* ui, InputSystem::InputActionMap* map);

    void Open();
    void Close();
    // Forget the screen without destroying its entity: for a world that was
    // just replaced wholesale (editor Stop restores the pre-play scene, where
    // that id may now be a different entity)
    void Drop();
    bool IsOpen() const { return m_Open; }
    // A row is armed, or this frame's input was spent on one: Escape cancelled
    // it, or a capture took a key or button that is also Back (B, Backspace)
    bool IsCapturing() const { return m_Armed >= 0 || m_EscapeSpent || m_Frame == m_CaptureFrame; }

    void Update(f32 dt);

    // Back pressed on the screen. The owner reopens whatever led here.
    std::function<void()> onBack;
    // Bindings or look settings changed and should be saved. Called on each
    // rebind, reset and preset, and on Close when a slider was moved.
    std::function<void()> onSave;

private:
    void Rebuild();
    void Arm(i32 action, bool pad);
    void Disarm();
    void RegisterListeners();
    void RemoveListeners();
    void Save();

    ECS::World* m_World = nullptr;
    UISystem* m_UI = nullptr;
    InputSystem::InputActionMap* m_Map = nullptr;
    ECS::Entity m_Entity = ECS::INVALID_ENTITY;
    std::vector<u32> m_Listeners;
    bool m_Open = false;
    bool m_Dirty = false;           // a slider moved; saved on Close

    i32 m_Armed = -1;               // action awaiting input, -1 none
    bool m_ArmedPad = false;        // the pad button was pressed, not the key one
    bool m_WaitForRelease = false;  // the click or press that armed the row is still down
    f32 m_ArmedSeconds = 0.0f;      // a pad capture times out, for a player with no Escape key
    bool m_EscapeSpent = false;
    // Capture runs in Update, and the UI dispatches the click for that same
    // press later in the frame; a row click in the capture's frame is that
    // press, not a new request
    u64 m_Frame = 0;
    u64 m_CaptureFrame = ~0ull;
};

} // namespace GUI
} // namespace Enjin
