#include "EnjinTest.h"
#include "Enjin/Editor/PlayMode.h"
#include "Enjin/Editor/PlayModeDiff.h"

using namespace Enjin;
using namespace Enjin::Editor;

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

ENJIN_TEST(PlayModeSystems, SimpleAudio) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetSimpleAudio());
}

ENJIN_TEST(PlayModeSystems, InputActionMap) {
    PlayMode pm;
    ENJIN_ASSERT_NOT_NULL(pm.GetInputActionMap());
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
