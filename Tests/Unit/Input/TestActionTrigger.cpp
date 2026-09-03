#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/ActionTrigger.h"
#include "Enjin/ECS/Systems/ActionTriggerSystem.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Input/InputProjectSettings.h"
#include "Enjin/Input/TouchActionBridge.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include <cstring>
#include <string>

using namespace Enjin;
using namespace Enjin::InputSystem;

// The components-only control path: a project names a Custom action, a scene
// wires it up with an ActionTriggerComponent, and that action becomes a real
// control everywhere (menus, hint, touch button) with no script.

namespace {

// Drive the action map from a synthetic key state. Replay injection is the
// engine's own headless input path, so this exercises the real edge detection.
class FakeInput {
public:
    FakeInput() {
        Input::SetReplayInjection(true);
        std::memset(m_Keys, 0, sizeof(m_Keys));
        std::memset(m_Mouse, 0, sizeof(m_Mouse));
        Step();   // establish a released baseline so the next press is an edge
    }
    ~FakeInput() { Input::SetReplayInjection(false); }

    void SetKey(KeyCode key, bool down) { m_Keys[static_cast<int>(key)] = down; }

    void Step() {
        Input::InjectFrameState(m_Keys, m_Mouse, Math::Vector2(0.0f, 0.0f));
        Input::Update();
    }

private:
    bool m_Keys[512];
    bool m_Mouse[8];
};

// One press-and-release of a key, with the map updated each frame.
void PressKey(FakeInput& input, InputActionMap& map, KeyCode key) {
    input.SetKey(key, true);
    input.Step();
    map.Update(0.016f);
}

void ReleaseKey(FakeInput& input, InputActionMap& map, KeyCode key) {
    input.SetKey(key, false);
    input.Step();
    map.Update(0.016f);
}

} // namespace

ENJIN_TEST(ActionTrigger, ToggleTimeScaleSlowsAndRestoresTheWorld) {
    // Arrange: a project-named custom action bound to B, and a scene entity
    // that toggles bullet time with it. No script anywhere.
    InputProjectSettings project;
    CustomActionDef slomo;
    slomo.slot = 0;
    slomo.name = "SLO-MO";
    slomo.key = static_cast<i32>(KeyCode::B);
    slomo.mode = static_cast<u32>(ActionMode::Press);
    project.customActions.push_back(slomo);

    InputActionMap map;
    map.LoadDefaults();
    project.ApplyTo(map);

    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    ECS::ActionTriggerComponent trigger;
    trigger.action = static_cast<i32>(GameAction::Custom0);
    trigger.mode = ECS::ActionTriggerMode::Toggle;
    trigger.effect = ECS::ActionEffect::TimeScale;
    trigger.timeScale = 0.25f;
    trigger.keepPlayerSpeed = false;   // no controllers in this world
    world.AddComponent<ECS::ActionTriggerComponent>(e, trigger);

    ECS::ActionTriggerSystem system;
    system.SetInputActionMap(&map);

    Scripting::SetTimeScale(1.0f);
    FakeInput input;

    // Act: press B once.
    PressKey(input, map, KeyCode::B);
    system.Update(&world, 0.016f);

    // Assert: the world slowed.
    ENJIN_EXPECT_FLOAT_EQ(Scripting::GetTimeScale(), 0.25f);
    ENJIN_EXPECT_TRUE(world.GetComponent<ECS::ActionTriggerComponent>(e)->active);

    // Act: release, then press again.
    ReleaseKey(input, map, KeyCode::B);
    system.Update(&world, 0.016f);
    ENJIN_EXPECT_FLOAT_EQ(Scripting::GetTimeScale(), 0.25f);   // toggle holds

    PressKey(input, map, KeyCode::B);
    system.Update(&world, 0.016f);

    // Assert: back to normal time.
    ENJIN_EXPECT_FLOAT_EQ(Scripting::GetTimeScale(), 1.0f);
    ENJIN_EXPECT_FALSE(world.GetComponent<ECS::ActionTriggerComponent>(e)->active);

    system.Reset(&world);
    Scripting::SetTimeScale(1.0f);
}

ENJIN_TEST(ActionTrigger, ResetRestoresTimeScaleWhenPlayStops) {
    // Leaving play mode with bullet time on must not strand the editor's clock.
    InputActionMap map;
    map.LoadDefaults();
    map.SetCustomActionName(GameAction::Custom0, "SLO-MO");
    map.RebindAction(static_cast<i32>(GameAction::Custom0), static_cast<i32>(KeyCode::B));

    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    ECS::ActionTriggerComponent trigger;
    trigger.action = static_cast<i32>(GameAction::Custom0);
    trigger.mode = ECS::ActionTriggerMode::Toggle;
    trigger.effect = ECS::ActionEffect::TimeScale;
    trigger.timeScale = 0.1f;
    trigger.keepPlayerSpeed = false;
    world.AddComponent<ECS::ActionTriggerComponent>(e, trigger);

    ECS::ActionTriggerSystem system;
    system.SetInputActionMap(&map);
    Scripting::SetTimeScale(1.0f);

    FakeInput input;
    PressKey(input, map, KeyCode::B);
    system.Update(&world, 0.016f);
    ENJIN_ASSERT_TRUE(Scripting::GetTimeScale() < 0.5f);

    system.Reset(&world);
    ENJIN_EXPECT_FLOAT_EQ(Scripting::GetTimeScale(), 1.0f);
    ENJIN_EXPECT_FALSE(world.GetComponent<ECS::ActionTriggerComponent>(e)->active);
}

ENJIN_TEST(ActionTrigger, SceneTriggerAddsItsOwnTouchButton) {
    // Dropping the component into a scene is what puts the control on mobile:
    // no script, no touch code.
    InputActionMap map;
    map.LoadDefaults();
    map.SetCustomActionName(GameAction::Custom0, "SLO-MO");
    map.RebindAction(static_cast<i32>(GameAction::Custom0), static_cast<i32>(KeyCode::B));
    SetTouchActionMap(&map);
    SetTouchProjectSettings(nullptr);

    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    ECS::ActionTriggerComponent trigger;
    trigger.action = static_cast<i32>(GameAction::Custom0);
    trigger.touchButton = true;
    trigger.touchRow = 2.0f;
    world.AddComponent<ECS::ActionTriggerComponent>(e, trigger);

    ResetTouchPresetTracking();
    ENJIN_ASSERT_TRUE(ApplyTouchPresetForWorld(&world));

    const Input::TouchScheme& scheme = Input::GetTouchScheme();
    bool sawCustom = false;
    for (int i = 0; i < scheme.buttonCount; ++i) {
        if (scheme.buttons[i].action == static_cast<int>(GameAction::Custom0)) {
            sawCustom = true;
            // The button is labelled with the action's NAME, not its key.
            ENJIN_EXPECT_TRUE(std::strcmp(scheme.buttons[i].label, "SLO-MO") == 0);
        }
    }
    ENJIN_EXPECT_TRUE(sawCustom);

    // It presses the action's live binding, so a rebind moves the button too.
    ENJIN_EXPECT_EQ(TouchActionKey(static_cast<int>(GameAction::Custom0)),
                    static_cast<int>(KeyCode::B));

    // Turning the button off removes it, and the change is picked up without a
    // scene reload (the scheme is fingerprinted, not cached on preset alone).
    world.GetComponent<ECS::ActionTriggerComponent>(e)->touchButton = false;
    ENJIN_EXPECT_TRUE(ApplyTouchPresetForWorld(&world));
    const Input::TouchScheme& after = Input::GetTouchScheme();
    for (int i = 0; i < after.buttonCount; ++i) {
        ENJIN_EXPECT_TRUE(after.buttons[i].action != static_cast<int>(GameAction::Custom0));
    }

    SetTouchActionMap(nullptr);
}

ENJIN_TEST(InputProjectSettings, RoundTripsAndNamesCustomActions) {
    InputProjectSettings settings;
    CustomActionDef def;
    def.slot = 2;
    def.name = "Horn";
    def.key = static_cast<i32>(KeyCode::H);
    def.gamepad = 3;   // Y
    def.mode = static_cast<u32>(ActionMode::Press);
    settings.customActions.push_back(def);
    settings.touchLeftHanded = true;
    settings.touchButtonScale = 1.5f;
    settings.touchLook = TouchLookMode::AlwaysOff;
    settings.customTouchLayout = true;
    TouchButtonLayout btn;
    btn.action = static_cast<i32>(GameAction::Custom2);
    btn.col = 1.0f; btn.row = 2.0f; btn.size = 0.09f;
    settings.touchButtons.push_back(btn);

    ENJIN_EXPECT_FALSE(settings.IsEmpty());

    InputProjectSettings loaded;
    ENJIN_ASSERT_TRUE(loaded.FromJson(settings.ToJson()));
    ENJIN_ASSERT_TRUE(loaded.customActions.size() == 1);
    ENJIN_EXPECT_EQ(loaded.customActions[0].slot, 2);
    ENJIN_EXPECT_TRUE(loaded.customActions[0].name == "Horn");
    ENJIN_EXPECT_EQ(loaded.customActions[0].key, static_cast<i32>(KeyCode::H));
    ENJIN_EXPECT_EQ(loaded.customActions[0].gamepad, 3);
    ENJIN_EXPECT_TRUE(loaded.touchLeftHanded);
    ENJIN_EXPECT_FLOAT_EQ(loaded.touchButtonScale, 1.5f);
    ENJIN_EXPECT_TRUE(loaded.touchLook == TouchLookMode::AlwaysOff);
    ENJIN_ASSERT_TRUE(loaded.touchButtons.size() == 1);
    ENJIN_EXPECT_EQ(loaded.touchButtons[0].action, static_cast<i32>(GameAction::Custom2));

    // Applying it names and binds the slot, which is what makes the action
    // visible to menus, the controls hint and touch.
    InputActionMap map;
    map.LoadDefaults();
    const i32 custom2 = static_cast<i32>(GameAction::Custom2);
    ENJIN_EXPECT_FALSE(map.IsActionListed(custom2));
    loaded.ApplyTo(map);
    ENJIN_EXPECT_TRUE(map.IsActionListed(custom2));
    ENJIN_EXPECT_TRUE(std::string(map.GetActionName(custom2)) == "Horn");
    ENJIN_EXPECT_EQ(static_cast<i32>(map.GetActionConfig(GameAction::Custom2).bindings.size()), 2);
}

ENJIN_TEST(InputProjectSettings, EmptyDefaultsAndBadJsonAreSafe) {
    InputProjectSettings settings;
    ENJIN_EXPECT_TRUE(settings.IsEmpty());          // clean projects write nothing
    ENJIN_EXPECT_FALSE(settings.FromJson(""));
    ENJIN_EXPECT_FALSE(settings.FromJson("{{{not json"));

    // An unnamed slot stays hidden and unbound even after Apply.
    InputProjectSettings unnamed;
    CustomActionDef def;
    def.slot = 0;
    def.key = static_cast<i32>(KeyCode::B);
    unnamed.customActions.push_back(def);
    InputActionMap map;
    map.LoadDefaults();
    unnamed.ApplyTo(map);
    ENJIN_EXPECT_FALSE(map.IsActionListed(static_cast<i32>(GameAction::Custom0)));
    ENJIN_EXPECT_TRUE(map.GetActionConfig(GameAction::Custom0).bindings.empty());

    // Out-of-range slots are ignored rather than corrupting the map.
    InputProjectSettings bad;
    CustomActionDef oob;
    oob.slot = 99;
    oob.name = "Nope";
    bad.customActions.push_back(oob);
    bad.ApplyTo(map);   // must not crash or write anywhere
    ENJIN_EXPECT_TRUE(true);
}

ENJIN_TEST_MAIN()
