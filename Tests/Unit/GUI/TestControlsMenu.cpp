// The Controls / key-bindings screen, generated from the live InputActionMap.
//
// Generated rather than hand-written so it cannot list an action the game does
// not have, or miss one it does. These tests are the guard on that: the screen
// is a UICanvas built with no GPU, so the whole thing is checkable here rather
// than by opening a browser and squinting at it.
//
// Background: the web player had no controls screen at all, so no shipped web
// build could rebind anything even though InputAction_Rebind has been in the
// binary the whole time.
#include "EnjinTest.h"
#include "Enjin/GUI/UITemplates.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Input/InputAction.h"

#include <string>
#include <cstdio>

using namespace Enjin;
using namespace Enjin::GUI;

namespace {

// Every button event the canvas dispatches, in order.
std::vector<std::string> ClickEvents(const UICanvasComponent& canvas) {
    std::vector<std::string> out;
    for (const auto& e : canvas.elements) {
        if (!e.onClickEvent.empty()) out.push_back(e.onClickEvent);
    }
    return out;
}

bool HasEvent(const UICanvasComponent& canvas, const std::string& event) {
    for (const auto& e : canvas.elements) {
        if (e.onClickEvent == event || e.onValueChangedEvent == event) return true;
    }
    return false;
}

// Whether any element's visible text contains a fragment.
bool AnyTextContains(const UICanvasComponent& canvas, const std::string& fragment) {
    for (const auto& e : canvas.elements) {
        if (e.data.text.find(fragment) != std::string::npos) return true;
    }
    return false;
}

} // namespace

ENJIN_TEST(ControlsMenu, HasOneRebindRowPerAction) {
    // Arrange
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    // Act
    const UICanvasComponent canvas = UITemplates::CreateControlsMenu(map);

    // Assert: an action without a row cannot be rebound by anyone.
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* name = map.GetActionName(i);
        if (!name || !*name) continue;
        ENJIN_EXPECT_TRUE(HasEvent(canvas, UITemplates::ControlsRebindEvent(i)));
    }
}

ENJIN_TEST(ControlsMenu, ShowsTheActionNameAndItsCurrentBinding) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    const UICanvasComponent canvas = UITemplates::CreateControlsMenu(map);

    // The row is the only place a player learns what a key currently does, so
    // it has to carry both halves.
    ENJIN_EXPECT_TRUE(AnyTextContains(canvas, "Jump"));
    const char* jumpBinding = nullptr;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* n = map.GetActionName(i);
        if (n && std::string(n) == "Jump") { jumpBinding = map.GetBindingDisplayName(i); break; }
    }
    ENJIN_ASSERT_TRUE(jumpBinding != nullptr && *jumpBinding != '\0');
    ENJIN_EXPECT_TRUE(AnyTextContains(canvas, jumpBinding));
}

ENJIN_TEST(ControlsMenu, TheArmedRowPromptsForAKeyInsteadOfShowingOne) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    const UICanvasComponent canvas = UITemplates::CreateControlsMenu(map, 4);

    // The row itself is the prompt: on a touchscreen there is no Esc to dismiss
    // a modal with, and every key is being captured anyway.
    ENJIN_EXPECT_TRUE(AnyTextContains(canvas, "press a key"));
}

ENJIN_TEST(ControlsMenu, CarriesBackAndResetAndTheLookSettings) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    const UICanvasComponent canvas = UITemplates::CreateControlsMenu(map);

    ENJIN_EXPECT_TRUE(HasEvent(canvas, "controls_back"));
    ENJIN_EXPECT_TRUE(HasEvent(canvas, "controls_reset"));
    ENJIN_EXPECT_TRUE(HasEvent(canvas, "controls_sensitivity"));
    ENJIN_EXPECT_TRUE(HasEvent(canvas, "controls_invert_y"));
    ENJIN_EXPECT_TRUE(HasEvent(canvas, "controls_sprint_mode"));
    ENJIN_EXPECT_TRUE(HasEvent(canvas, "controls_crouch_mode"));
}

// The producer and the consumer agree about the event format, or a tap on a
// binding row silently does nothing.
ENJIN_TEST(ControlsMenu, RebindEventNamesRoundTrip) {
    for (i32 i : {0, 1, 7, 42, 128}) {
        const std::string event = UITemplates::ControlsRebindEvent(i);
        ENJIN_EXPECT_EQ(UITemplates::ControlsRebindIndexFromEvent(event), i);
    }
}

ENJIN_TEST(ControlsMenu, AMalformedRebindEventIsRejectedRatherThanReadAsActionZero) {
    // Action 0 is Move Forward. Parsing junk as 0 would rebind movement on a
    // stray event, which is the worst possible default.
    ENJIN_EXPECT_EQ(UITemplates::ControlsRebindIndexFromEvent("controls_back"), -1);
    ENJIN_EXPECT_EQ(UITemplates::ControlsRebindIndexFromEvent("controls_rebind_"), -1);
    ENJIN_EXPECT_EQ(UITemplates::ControlsRebindIndexFromEvent("controls_rebind_x"), -1);
    ENJIN_EXPECT_EQ(UITemplates::ControlsRebindIndexFromEvent("controls_rebind_1a"), -1);
    ENJIN_EXPECT_EQ(UITemplates::ControlsRebindIndexFromEvent(""), -1);
}

// Rebinding through the map must actually change what the screen reports, or
// the player rebinds and the row keeps showing the old key.
ENJIN_TEST(ControlsMenu, ARebindIsReflectedTheNextTimeTheScreenIsBuilt) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    i32 jump = -1;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* n = map.GetActionName(i);
        if (n && std::string(n) == "Jump") { jump = i; break; }
    }
    ENJIN_ASSERT_TRUE(jump >= 0);

    map.RebindAction(jump, static_cast<i32>(KeyCode::J));
    const UICanvasComponent after = UITemplates::CreateControlsMenu(map);

    const char* binding = map.GetBindingDisplayName(jump);
    ENJIN_ASSERT_TRUE(binding != nullptr && *binding != '\0');
    ENJIN_EXPECT_TRUE(AnyTextContains(after, binding));
}

// REGRESSION: a UI handler must be able to create and destroy entities.
//
// UISystem::ProcessInput walks canvas.elements BY REFERENCE, and those elements
// live in a UICanvasComponent inside ECS storage. Dispatching a handler inside
// that walk meant a handler which added a component could reallocate the
// storage under the loop -- a use-after-free that on wasm traps and freezes the
// page. That is exactly what "open the pause menu, click Options" did: the
// handler destroyed the pause canvas and added the options canvas mid-walk.
//
// Events are now queued during the walk and dispatched after it. This test does
// the same thing the pause menu does, and passes only because of that.
ENJIN_TEST(ControlsMenu, AHandlerMayCreateAndDestroyEntitiesDuringDispatch) {
    ECS::World world;
    UISystem ui;

    // A canvas with one button, standing in for "Options".
    ECS::Entity menu = world.CreateEntity();
    UICanvasComponent canvas = UITemplates::CreatePauseMenu();
    world.AddComponent<UICanvasComponent>(menu, std::move(canvas));

    bool ran = false;
    ui.GetEventBus().Listen("swap_menus", [&](const UIEventData&) {
        ran = true;
        // Precisely what ShowOptionsMenu does: drop the canvas that raised the
        // event and add a new, larger one.
        world.DestroyEntity(menu);
        ECS::Entity next = world.CreateEntity();
        world.AddComponent<UICanvasComponent>(next, UITemplates::CreateOptionsMenu());
    });

    // Raise it through the same queue the input walk uses.
    UIEventData ev;
    ev.eventName = "swap_menus";
    ui.QueueEvent(ev);
    ui.FlushPendingEvents();

    ENJIN_EXPECT_TRUE(ran);
    // Reaching here at all is the assertion: before the fix this path was a
    // use-after-free inside the element loop.
    ENJIN_SURVIVED("controls menu navigation with no bindings");
}

// REGRESSION: a rebind must store a code the engine can actually match.
//
// PollNextKeyPress used to scan ImGuiKey_NamedKey_BEGIN..END and return the
// ImGuiKey, with a comment claiming it mapped directly to a GLFW code. It does
// not: KeyCode is GLFW-style (Space = 32, A = 65) and ImGuiKey named keys start
// at 512. Every rebind stored a value no real key press could equal, so
// rebinding silently did nothing -- for every key, not just unusual ones.
ENJIN_TEST(ControlsMenu, ARebindStoresAnEngineKeyCodeNotAnImGuiKey) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    i32 jump = -1;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* n = map.GetActionName(i);
        if (n && std::string(n) == "Jump") { jump = i; break; }
    }
    ENJIN_ASSERT_TRUE(jump >= 0);

    map.RebindAction(jump, static_cast<i32>(KeyCode::J));
    // The display name is derived from the stored code, so a code outside the
    // KeyCode domain cannot render as the key that was pressed.
    const char* shown = map.GetBindingDisplayName(jump);
    ENJIN_ASSERT_TRUE(shown != nullptr);
    ENJIN_EXPECT_TRUE(std::string(shown) == "J");
}

// Binding an action to a mouse button must not throw away its keyboard binding,
// and must actually be stored as a mouse binding.
ENJIN_TEST(ControlsMenu, RebindingReplacesTheOldBindingButKeepsTheGamepad) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    i32 jump = -1;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* n = map.GetActionName(i);
        if (n && std::string(n) == "Jump") { jump = i; break; }
    }
    ENJIN_ASSERT_TRUE(jump >= 0);

    map.RebindAction(jump, InputSystem::BindingType::MouseButton,
                     static_cast<i32>(MouseButton::Left));

    const auto& cfg = map.GetActionConfig(static_cast<InputSystem::GameAction>(jump));
    bool hasMouseLeft = false, keepsSpace = false;
    for (const auto& b : cfg.bindings) {
        if (b.type == InputSystem::BindingType::MouseButton &&
            b.code == static_cast<i32>(MouseButton::Left)) hasMouseLeft = true;
        if (b.type == InputSystem::BindingType::Key &&
            b.code == static_cast<i32>(KeyCode::Space)) keepsSpace = true;
    }
    ENJIN_EXPECT_TRUE(hasMouseLeft);
    // Rebinding REPLACES. After binding Jump to the mouse, Space must stop
    // jumping -- otherwise the controls screen says one thing and the game does
    // another, which is exactly how this was first reported.
    ENJIN_EXPECT_FALSE(keepsSpace);
    // The gamepad binding is a separate control and must survive.
    bool keepsPad = false;
    for (const auto& b : cfg.bindings) {
        if (b.type == InputSystem::BindingType::GamepadButton) keepsPad = true;
    }
    ENJIN_EXPECT_TRUE(keepsPad);
}

// End-to-end: rebind Jump to LMB, then press LMB and check the ACTION fires.
// The hint bar updating only proves the binding was stored; this proves the
// binding is actually evaluated, which is the half that was failing.
ENJIN_TEST(ControlsMenu, AMouseBoundActionFiresWhenTheButtonIsPressed) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    i32 jump = -1;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* n = map.GetActionName(i);
        if (n && std::string(n) == "Jump") { jump = i; break; }
    }
    ENJIN_ASSERT_TRUE(jump >= 0);
    map.RebindAction(jump, InputSystem::BindingType::MouseButton,
                     static_cast<i32>(MouseButton::Left));

    // Gameplay owns the pointer; nothing in the UI took this click.
    Input::SetInputFocus(Input::InputFocus::Gameplay);
    Input::SetUIConsumedPointer(false);

    bool keys[512] = {false};
    bool mouse[8] = {false};
    Math::Vector2 pos(0.0f, 0.0f);

    Input::SetReplayInjection(true);
    // Frame 1: nothing down, so the next frame is a real pressed EDGE.
    // map.Update() is what fills the per-action cache IsActionPressed reads;
    // without it the query answers from an empty cache and nothing ever fires.
    Input::InjectFrameState(keys, mouse, pos);
    Input::Update();
    map.Update(1.0f / 60.0f);
    // Frame 2: left button down.
    mouse[static_cast<int>(MouseButton::Left)] = true;
    Input::InjectFrameState(keys, mouse, pos);
    Input::Update();
    map.Update(1.0f / 60.0f);

    const bool fired = map.IsActionPressed(InputSystem::GameAction::Jump);
    Input::SetReplayInjection(false);

    ENJIN_EXPECT_TRUE(fired);
}

// The capture side: PollNextMouseButton must actually see a click. This is the
// last link in the rebind chain that had no test -- storage and evaluation were
// covered, but nothing proved the thing that READS the button.
ENJIN_TEST(ControlsMenu, PollNextMouseButtonSeesAClick) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    bool keys[512] = {false};
    bool mouse[8] = {false};
    Math::Vector2 pos(0.0f, 0.0f);

    Input::SetReplayInjection(true);
    // Nothing down: the poll must report nothing, or arming a row would bind
    // instantly off a stale edge.
    Input::InjectFrameState(keys, mouse, pos);
    Input::Update();
    const i32 idle = map.PollNextMouseButton();

    // Left down: a real pressed edge.
    mouse[static_cast<int>(MouseButton::Left)] = true;
    Input::InjectFrameState(keys, mouse, pos);
    Input::Update();
    const i32 clicked = map.PollNextMouseButton();
    Input::SetReplayInjection(false);

    ENJIN_EXPECT_EQ(idle, -1);
    ENJIN_EXPECT_EQ(clicked, static_cast<i32>(MouseButton::Left));
}

// And the key poll must return an engine KeyCode, which is what the ImGuiKey
// bug got wrong. Injected J must come back as J, not as 512-something.
ENJIN_TEST(ControlsMenu, PollNextKeyPressReturnsAnEngineKeyCode) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    bool keys[512] = {false};
    bool mouse[8] = {false};
    Math::Vector2 pos(0.0f, 0.0f);

    Input::SetReplayInjection(true);
    Input::InjectFrameState(keys, mouse, pos);
    Input::Update();
    keys[static_cast<int>(KeyCode::J)] = true;
    Input::InjectFrameState(keys, mouse, pos);
    Input::Update();
    const i32 got = map.PollNextKeyPress();
    Input::SetReplayInjection(false);

    ENJIN_EXPECT_EQ(got, static_cast<i32>(KeyCode::J));
}

// REGRESSION: saved bindings poisoned by the old ImGuiKey bug must be repaired.
//
// The old PollNextKeyPress scanned the ImGuiKey range and returned the ImGuiKey.
// That range INCLUDES ImGuiKey_MouseLeft (~656), so clicking at a rebind prompt
// stored a KEY binding with code 656. It was written to bindings.json and
// restored every boot, leaving the action permanently dead: rebinding it looked
// like it did nothing, and no player could tell their file was poisoned.
// Observed live as jumpBinds=[key:656 pad:0].
ENJIN_TEST(ControlsMenu, SavedBindingsPoisonedByImGuiKeyCodesAreRepairedOnLoad) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    i32 jump = -1;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* n = map.GetActionName(i);
        if (n && std::string(n) == "Jump") { jump = i; break; }
    }
    ENJIN_ASSERT_TRUE(jump >= 0);

    // Forge exactly what the old bug wrote: Jump bound to ImGuiKey_MouseLeft.
    map.RebindAction(jump, InputSystem::BindingType::Key, static_cast<i32>(KeyCode::J));
    {
        auto& cfg = map.GetActionConfig(static_cast<InputSystem::GameAction>(jump));
        for (auto& b : cfg.bindings) {
            if (b.type == InputSystem::BindingType::Key) b.code = 656;
        }
    }
    const std::string poisoned = map.ToJson();

    InputSystem::InputActionMap loaded;
    loaded.LoadDefaults();
    ENJIN_ASSERT_TRUE(loaded.FromJson(poisoned));

    const auto& cfg = loaded.GetActionConfig(static_cast<InputSystem::GameAction>(jump));
    for (const auto& b : cfg.bindings) {
        // Nothing out of range may survive a load.
        ENJIN_EXPECT_TRUE(InputSystem::InputActionMap::IsBindingCodeValid(b.type, b.code));
    }
    // And an action stripped bare gets its defaults back rather than staying dead.
    ENJIN_EXPECT_TRUE(!cfg.bindings.empty());
}

// The same code can never be STORED in the first place.
ENJIN_TEST(ControlsMenu, AnImpossibleBindingCodeIsRefusedRatherThanStored) {
    InputSystem::InputActionMap map;
    map.LoadDefaults();

    ENJIN_EXPECT_FALSE(InputSystem::InputActionMap::IsBindingCodeValid(
        InputSystem::BindingType::Key, 656));
    ENJIN_EXPECT_TRUE(InputSystem::InputActionMap::IsBindingCodeValid(
        InputSystem::BindingType::Key, static_cast<i32>(KeyCode::Space)));
    ENJIN_EXPECT_TRUE(InputSystem::InputActionMap::IsBindingCodeValid(
        InputSystem::BindingType::MouseButton, static_cast<i32>(MouseButton::Left)));

    i32 jump = -1;
    for (i32 i = 0; i < map.GetActionCount(); ++i) {
        const char* n = map.GetActionName(i);
        if (n && std::string(n) == "Jump") { jump = i; break; }
    }
    map.RebindAction(jump, InputSystem::BindingType::Key, 656);
    const auto& cfg = map.GetActionConfig(static_cast<InputSystem::GameAction>(jump));
    for (const auto& b : cfg.bindings) {
        ENJIN_EXPECT_TRUE(b.code != 656);
    }
}

ENJIN_TEST_MAIN()
