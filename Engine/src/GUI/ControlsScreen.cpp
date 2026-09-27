#include "Enjin/GUI/ControlsScreen.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Platform/Input.h"
#include <cmath>

namespace Enjin {
namespace GUI {

namespace {
    // A pad capture with nothing pressed gives up after this long, so a player
    // with only a pad is never stuck on an armed row (Escape is the other way out)
    constexpr f32 kPadCaptureTimeout = 6.0f;

    bool PadAtRest() {
        for (i32 gp = 0; gp < 4; ++gp) {
            if (!Input::IsGamepadConnected(gp)) continue;
            for (i32 b = 0; b <= static_cast<i32>(GamepadButton::DPadLeft); ++b) {
                if (Input::IsGamepadButtonDown(static_cast<GamepadButton>(b), gp)) return false;
            }
            for (i32 a = 0; a <= 3; ++a) {
                if (std::fabs(Input::GetGamepadAxis(static_cast<GamepadAxis>(a), gp)) > 0.3f) return false;
            }
            for (i32 a = 4; a <= 5; ++a) {
                if (Input::GetGamepadAxis(static_cast<GamepadAxis>(a), gp) > 0.0f) return false;
            }
        }
        return true;
    }

    bool MouseUp() {
        return !Input::IsMouseButtonDown(MouseButton::Left) &&
               !Input::IsMouseButtonDown(MouseButton::Right) &&
               !Input::IsMouseButtonDown(MouseButton::Middle);
    }
}

ControlsScreen::~ControlsScreen() {
    RemoveListeners();
}

void ControlsScreen::Attach(ECS::World* world, UISystem* ui, InputSystem::InputActionMap* map) {
    if (world == m_World && ui == m_UI && map == m_Map) return;   // cheap to call every frame
    const bool reopen = m_Open;
    if (m_Open) Close();
    RemoveListeners();
    m_World = world;
    m_UI = ui;
    m_Map = map;
    m_Entity = ECS::INVALID_ENTITY;   // an entity of the old world means nothing in the new one
    if (reopen) Open();
}

void ControlsScreen::Open() {
    if (!m_World || !m_UI || !m_Map) return;
    m_Open = true;
    Disarm();
    RegisterListeners();
    Rebuild();
    Input::SetMouseCaptured(false);
}

void ControlsScreen::Close() {
    if (!m_Open) return;
    m_Open = false;
    // Abandon any capture in progress, or the next key pressed anywhere would
    // silently rebind an action the player has stopped looking at
    Disarm();
    if (m_World && m_Entity != ECS::INVALID_ENTITY && m_World->IsValid(m_Entity)) m_World->DestroyEntity(m_Entity);
    m_Entity = ECS::INVALID_ENTITY;
    RemoveListeners();
    if (m_Dirty) Save();
}

void ControlsScreen::Drop() {
    m_Open = false;
    Disarm();
    m_Entity = ECS::INVALID_ENTITY;
    RemoveListeners();
}

void ControlsScreen::Save() {
    m_Dirty = false;
    if (onSave) onSave();
}

void ControlsScreen::Rebuild() {
    if (!m_World || !m_Map) return;
    // Rebuilt rather than patched: each row carries its binding in its text
    if (m_Entity != ECS::INVALID_ENTITY && m_World->IsValid(m_Entity)) m_World->DestroyEntity(m_Entity);
    auto canvas = UITemplates::CreateControlsMenu(*m_Map, m_Armed, m_ArmedPad);
    m_Entity = m_World->CreateEntity();
    m_World->AddComponent<ECS::NameComponent>(m_Entity, "Controls Menu UI");
    m_World->AddComponent<UICanvasComponent>(m_Entity, std::move(canvas));
}

void ControlsScreen::Arm(i32 action, bool pad) {
    // While a row is armed EVERY click belongs to the capture, including one on
    // another row: binding a mouse button means clicking, and every click on
    // this screen lands on a row
    if (m_Armed >= 0) return;
    if (m_Frame == m_CaptureFrame) return;   // the press the capture just consumed
    m_Armed = action;
    m_ArmedPad = pad;
    m_WaitForRelease = true;
    m_ArmedSeconds = 0.0f;
    Rebuild();
}

void ControlsScreen::Disarm() {
    m_Armed = -1;
    m_ArmedPad = false;
    m_WaitForRelease = false;
    m_ArmedSeconds = 0.0f;
}

void ControlsScreen::RegisterListeners() {
    if (!m_UI || !m_Listeners.empty()) return;
    auto& bus = m_UI->GetEventBus();
    auto listen = [&](const std::string& name, UIEventBus::Callback cb) {
        m_Listeners.push_back(bus.Listen(name, std::move(cb)));
    };
    // One listener per row, for every action the map has right now: a script
    // may define a project action after boot, so this happens on each Open
    for (i32 a = 0; a < m_Map->GetActionCount(); ++a) {
        listen(UITemplates::ControlsRebindEvent(a), [this, a](const UIEventData&) { Arm(a, false); });
        listen(UITemplates::ControlsRebindPadEvent(a), [this, a](const UIEventData&) { Arm(a, true); });
    }
    listen("controls_back", [this](const UIEventData&) {
        Close();
        if (onBack) onBack();
    });
    listen("controls_reset", [this](const UIEventData&) {
        m_Map->ResetToDefaults();
        Disarm();
        Save();
        Rebuild();
    });
    auto preset = [this](InputSystem::BindingPreset p) {
        m_Map->TogglePreset(p);
        Disarm();
        Save();
        Rebuild();
    };
    listen("controls_preset_left_hand", [preset](const UIEventData&) { preset(InputSystem::BindingPreset::LeftHand); });
    listen("controls_preset_right_hand", [preset](const UIEventData&) { preset(InputSystem::BindingPreset::RightHand); });
    listen("controls_preset_gamepad", [preset](const UIEventData&) { preset(InputSystem::BindingPreset::GamepadOnly); });
    // Sliders dispatch every frame while dragged, so these are saved on Close
    listen("controls_sensitivity", [this](const UIEventData& e) { m_Map->SetMouseSensitivity(e.floatValue); m_Dirty = true; });
    listen("controls_invert_y", [this](const UIEventData& e) { m_Map->SetInvertY(e.boolValue); m_Dirty = true; });
    listen("controls_sprint_mode", [this](const UIEventData& e) { m_Map->SetSprintToggle(e.intValue == 1); m_Dirty = true; });
    listen("controls_crouch_mode", [this](const UIEventData& e) { m_Map->SetCrouchToggle(e.intValue == 1); m_Dirty = true; });
}

void ControlsScreen::RemoveListeners() {
    if (m_UI) {
        for (u32 id : m_Listeners) m_UI->GetEventBus().RemoveListener(id);
    }
    m_Listeners.clear();
}

void ControlsScreen::Update(f32 dt) {
    ++m_Frame;
    m_EscapeSpent = false;
    if (!m_Open || m_Armed < 0 || !m_Map) return;

    // Escape cancels a capture, which is why it cannot be bound: while a row
    // is armed every other input is the binding
    if (Input::IsKeyPressed(KeyCode::Escape)) {
        Disarm();
        m_EscapeSpent = true;
        Rebuild();
        return;
    }

    if (m_ArmedPad) {
        m_ArmedSeconds += dt;
        if (m_ArmedSeconds > kPadCaptureTimeout) { Disarm(); Rebuild(); return; }
        // The confirm press that armed the row, and any resting stick, first
        if (m_WaitForRelease) {
            if (PadAtRest()) m_WaitForRelease = false;
            return;
        }
        InputSystem::InputBinding b;
        if (!m_Map->PollNextGamepadInput(b)) return;
        m_Map->RebindGamepad(m_Armed, b);
    } else {
        if (m_WaitForRelease && MouseUp()) m_WaitForRelease = false;
        i32 key = m_Map->PollNextKeyPress();
        if (key == static_cast<i32>(KeyCode::Escape)) key = -1;
        const i32 mouse = m_WaitForRelease ? -1 : m_Map->PollNextMouseButton();
        if (key < 0 && mouse < 0) return;
        if (key >= 0) m_Map->RebindAction(m_Armed, InputSystem::BindingType::Key, key);
        else m_Map->RebindAction(m_Armed, InputSystem::BindingType::MouseButton, mouse);
    }
    m_CaptureFrame = m_Frame;
    Disarm();
    Save();
    Rebuild();
}

} // namespace GUI
} // namespace Enjin
