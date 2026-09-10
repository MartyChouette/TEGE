#include "EnjinTest.h"
#include "Enjin/Editor/PlayMode.h"
#include "Enjin/Editor/PlayModeDiff.h"
#include <string>

using namespace Enjin;
using namespace Enjin::Editor;

namespace {

// A minimal scene, so a diff test states exactly what changed between two of
// them and nothing else.
std::string SceneJson(const char* name, float x, float health) {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        R"({"version":"1.0","entities":[{"id":1,"name":{"name":"%s"},)"
        R"("transform":{"position":[%.3f,0.0,0.0],"rotation":[0.0,0.0,0.0,1.0],"scale":[1.0,1.0,1.0]},)"
        R"("health":{"maxHealth":100.0,"currentHealth":%.3f}}]})",
        name, x, health);
    return buf;
}

} // namespace

// ===========================================================================
// The play-mode diff
//
// This whole feature was three lines short of existing. ComputePlayModeDiff had
// ZERO call sites, m_ShowDiffDialog was only ever assigned false, and
// m_PlayedSceneJson was declared with a public getter and never written -- so
// HasPendingDiff() was a compile-time false and the "what changed during your
// playtest, tick what to keep" dialog could never appear, though every part of
// it (per-property checkboxes, Apply Selected, Apply to Prefab) was built.
//
// It pairs with the play-mode restore: the restore puts everything back so a
// playtest cannot cost you work, and this is the deliberate way to promote a
// change you liked.
// ===========================================================================

ENJIN_TEST(PlayModeDiffCompute, AnUnchangedSceneHasNoChanges) {
    const std::string before = SceneJson("Player", 0.0f, 100.0f);
    PlayModeDiff diff = ComputePlayModeDiff(before, before);
    ENJIN_EXPECT_FALSE(diff.HasChanges());
    ENJIN_EXPECT_EQ(diff.CountModified(), static_cast<u32>(0));
}

ENJIN_TEST(PlayModeDiffCompute, AMovedEntityIsReportedAsModified) {
    PlayModeDiff diff = ComputePlayModeDiff(SceneJson("Player", 0.0f, 100.0f),
                                            SceneJson("Player", 12.5f, 100.0f));
    ENJIN_ASSERT_TRUE(diff.HasChanges());
    ENJIN_EXPECT_EQ(diff.CountModified(), static_cast<u32>(1));
    ENJIN_EXPECT_EQ(diff.CountCreated(), static_cast<u32>(0));
    ENJIN_EXPECT_EQ(diff.CountDeleted(), static_cast<u32>(0));
    ENJIN_EXPECT_EQ(diff.entities[0].entityName, std::string("Player"));
}

ENJIN_TEST(PlayModeDiffCompute, ADamagedEntityIsReportedTooNotJustTheTransform) {
    // The transform was the only thing the old restore handled, so it is the one
    // thing a diff must not be limited to.
    PlayModeDiff diff = ComputePlayModeDiff(SceneJson("Player", 0.0f, 100.0f),
                                            SceneJson("Player", 0.0f, 7.5f));
    ENJIN_ASSERT_TRUE(diff.HasChanges());
    ENJIN_EXPECT_EQ(diff.CountModified(), static_cast<u32>(1));
}

ENJIN_TEST(PlayModeDiffCompute, MalformedInputIsEmptyNotACrash) {
    // Stop() feeds this whatever the serializer produced. An empty diff means
    // "nothing to offer"; a throw here would take the editor down on Stop.
    ENJIN_EXPECT_FALSE(ComputePlayModeDiff("", "").HasChanges());
    ENJIN_EXPECT_FALSE(ComputePlayModeDiff("{not json", SceneJson("A", 0.0f, 1.0f)).HasChanges());
    ENJIN_EXPECT_FALSE(ComputePlayModeDiff(SceneJson("A", 0.0f, 1.0f), "{not json").HasChanges());
}

ENJIN_TEST(PlayModeDiffCompute, EverySelectionFlagStartsOff) {
    // The dialog's contract is opt-in: the restore already happened, so a change
    // is kept only if someone ticks it. A default-on checkbox would silently
    // re-apply the playtest, which is the bug the restore exists to prevent.
    PlayModeDiff diff = ComputePlayModeDiff(SceneJson("Player", 0.0f, 100.0f),
                                            SceneJson("Player", 12.5f, 7.5f));
    ENJIN_ASSERT_TRUE(diff.HasChanges());
    for (const auto& e : diff.entities) {
        ENJIN_EXPECT_FALSE(e.selected);
        for (const auto& c : e.components) {
            ENJIN_EXPECT_FALSE(c.selected);
            for (const auto& p : c.properties) ENJIN_EXPECT_FALSE(p.selected);
        }
    }
}

// ===========================================================================
// PlayState Enum
// ===========================================================================

ENJIN_TEST(PlayStateEnum, Values) {
    ENJIN_EXPECT_EQ((int)PlayState::Stopped, 0);
    ENJIN_EXPECT_EQ((int)PlayState::Playing, 1);
    ENJIN_EXPECT_EQ((int)PlayState::Paused, 2);
}

// ===========================================================================
// PlayMode Initial State
// ===========================================================================

ENJIN_TEST(PlayModeState, DefaultStopped) {
    PlayMode pm;
    ENJIN_EXPECT_TRUE(pm.IsStopped());
    ENJIN_EXPECT_FALSE(pm.IsPlaying());
    ENJIN_EXPECT_FALSE(pm.IsPaused());
}

ENJIN_TEST(PlayModeState, GetStateDefault) {
    PlayMode pm;
    ENJIN_EXPECT_EQ((int)pm.GetState(), (int)PlayState::Stopped);
}

ENJIN_TEST(PlayModeState, NoPendingDiff) {
    PlayMode pm;
    ENJIN_EXPECT_FALSE(pm.HasPendingDiff());
}

ENJIN_TEST(PlayModeState, DismissDiff) {
    PlayMode pm;
    pm.DismissDiff();
    ENJIN_EXPECT_FALSE(pm.HasPendingDiff());
}

// ===========================================================================
// PlayMode Subsystem Accessors
// ===========================================================================

ENJIN_TEST(PlayModeSystems, ControllerSystem) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetControllerSystem());
}

ENJIN_TEST(PlayModeSystems, FlowerSystem) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetFlowerSystem());
}

ENJIN_TEST(PlayModeSystems, ScriptEngine) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetScriptEngine());
}

ENJIN_TEST(PlayModeSystems, ScriptSystem) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetScriptSystem());
}

ENJIN_TEST(PlayModeSystems, CoroutineScheduler) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetCoroutineScheduler());
}

ENJIN_TEST(PlayModeSystems, EventBus) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetEventBus());
}

ENJIN_TEST(PlayModeSystems, QuestSystem) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetQuestSystem());
}

ENJIN_TEST(PlayModeSystems, ObjectPool) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetObjectPool());
}

ENJIN_TEST(PlayModeSystems, CinematicSystem) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetCinematicSystem());
}

ENJIN_TEST(PlayModeSystems, TweenSystem) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetTweenSystem());
}

ENJIN_TEST(PlayModeSystems, NetworkSystem) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetNetworkSystem());
}

ENJIN_TEST(PlayModeSystems, AudioEngine) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetAudioEngine());
}

ENJIN_TEST(PlayModeSystems, InputActionMapIsBorrowedNotOwned) {
    // PlayMode no longer keeps its own action map. The editor owns ONE map and
    // injects it, so ControllerSystem, the Controls menu, script bindings,
    // ActionTriggers and the touch overlay cannot drift apart on a rebind.
    PlayMode pm;
    ENJIN_EXPECT_TRUE(pm.GetInputActionMap() == nullptr);   // nothing injected yet

    InputSystem::InputActionMap map;
    map.LoadDefaults();
    pm.SetInputActionMap(&map);
    ENJIN_ASSERT_NOT_NULL(pm.GetInputActionMap());
    ENJIN_EXPECT_TRUE(pm.GetInputActionMap() == &map);

    // A rebind through the owner is visible through PlayMode: one map.
    map.RebindAction(static_cast<i32>(InputSystem::GameAction::Jump),
                     static_cast<i32>(KeyCode::K));
    const auto& cfg = pm.GetInputActionMap()->GetActionConfig(InputSystem::GameAction::Jump);
    bool sawK = false;
    for (const auto& b : cfg.bindings)
        if (b.type == InputSystem::BindingType::Key && b.code == static_cast<i32>(KeyCode::K)) sawK = true;
    ENJIN_EXPECT_TRUE(sawK);
}

// ===========================================================================
// PlayModeDiff
// ===========================================================================

ENJIN_TEST(PlayModeDiff, EmptyDiff) {
    PlayModeDiff diff;
    ENJIN_EXPECT_FALSE(diff.HasChanges());
    ENJIN_EXPECT_EQ(diff.CountCreated(), 0u);
    ENJIN_EXPECT_EQ(diff.CountDeleted(), 0u);
    ENJIN_EXPECT_EQ(diff.CountModified(), 0u);
}

ENJIN_TEST(PlayModeDiff, CountCreated) {
    PlayModeDiff diff;
    EntityDiff e1;
    e1.action = DiffAction::Created;
    EntityDiff e2;
    e2.action = DiffAction::Modified;
    diff.entities = {e1, e2};
    ENJIN_EXPECT_TRUE(diff.HasChanges());
    ENJIN_EXPECT_EQ(diff.CountCreated(), 1u);
    ENJIN_EXPECT_EQ(diff.CountModified(), 1u);
    ENJIN_EXPECT_EQ(diff.CountDeleted(), 0u);
}

ENJIN_TEST(PlayModeDiff, CountDeleted) {
    PlayModeDiff diff;
    EntityDiff e1;
    e1.action = DiffAction::Deleted;
    EntityDiff e2;
    e2.action = DiffAction::Deleted;
    diff.entities = {e1, e2};
    ENJIN_EXPECT_EQ(diff.CountDeleted(), 2u);
}

// ===========================================================================
// DiffAction Enum
// ===========================================================================

ENJIN_TEST(DiffActionEnum, Values) {
    ENJIN_EXPECT_EQ((int)DiffAction::Created, 0);
    ENJIN_EXPECT_EQ((int)DiffAction::Deleted, 1);
    ENJIN_EXPECT_EQ((int)DiffAction::Modified, 2);
}

// ===========================================================================
// PropertyDiff Defaults
// ===========================================================================

ENJIN_TEST(PropertyDiff, Defaults) {
    PropertyDiff pd;
    ENJIN_EXPECT_FALSE(pd.selected);
    ENJIN_EXPECT_TRUE(pd.name.empty());
    ENJIN_EXPECT_TRUE(pd.oldValue.empty());
    ENJIN_EXPECT_TRUE(pd.newValue.empty());
}

// ===========================================================================
// ComponentDiff Defaults
// ===========================================================================

ENJIN_TEST(ComponentDiff, Defaults) {
    ComponentDiff cd;
    ENJIN_EXPECT_FALSE(cd.selected);
    ENJIN_EXPECT_FALSE(cd.expanded);
    ENJIN_EXPECT_TRUE(cd.componentType.empty());
    ENJIN_EXPECT_EQ(cd.properties.size(), (size_t)0);
}

// ===========================================================================
// EntityDiff Defaults
// ===========================================================================

ENJIN_TEST(EntityDiff, Defaults) {
    EntityDiff ed;
    ENJIN_EXPECT_FALSE(ed.selected);
    ENJIN_EXPECT_FALSE(ed.expanded);
    ENJIN_EXPECT_FALSE(ed.isPrefabInstance);
    ENJIN_EXPECT_TRUE(ed.prefabPath.empty());
    ENJIN_EXPECT_EQ(ed.components.size(), (size_t)0);
}

ENJIN_TEST_MAIN()
