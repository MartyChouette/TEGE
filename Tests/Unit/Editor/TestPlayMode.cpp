#include "EnjinTest.h"
#include "Enjin/Editor/PlayMode.h"
#include "Enjin/Editor/PlayModeDiff.h"
#include <cstdio>
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

// ===========================================================================
// PlayMode subsystem accessors
//
// This was 12 separate tests, each of the shape:
//
//     PlayMode pm;
//     ENJIN_ASSERT_NOT_NULL(pm.GetXSystem());
//
// which cannot fail unless somebody writes `return nullptr;` in a getter that
// hands back the address of a member. Twelve tests, one tautology, and nothing
// said about whether PlayMode wires, ticks or resets any of them.
//
// Folded into one test asserting three properties that CAN fail:
//   - every accessor is non-null
//   - each returns the SAME address twice (a member, not a temporary)
//   - no two return the same address (a copy-pasted getter naming the wrong
//     member is the realistic bug here, and it is invisible to a null check)
// ===========================================================================

ENJIN_TEST(PlayModeSystems, EveryAccessorIsADistinctStableSubsystem) {
    PlayMode pm;

    struct Accessor { const char* name; const void* first; const void* second; };
    const Accessor accessors[] = {
        { "ControllerSystem",   pm.GetControllerSystem(),   pm.GetControllerSystem()   },
        { "FlowerSystem",       pm.GetFlowerSystem(),       pm.GetFlowerSystem()       },
        { "ScriptEngine",       pm.GetScriptEngine(),       pm.GetScriptEngine()       },
        { "ScriptSystem",       pm.GetScriptSystem(),       pm.GetScriptSystem()       },
        { "CoroutineScheduler", pm.GetCoroutineScheduler(), pm.GetCoroutineScheduler() },
        { "EventBus",           pm.GetEventBus(),           pm.GetEventBus()           },
        { "ObjectPool",         pm.GetObjectPool(),         pm.GetObjectPool()         },
        { "QuestSystem",        pm.GetQuestSystem(),        pm.GetQuestSystem()        },
        { "CinematicSystem",    pm.GetCinematicSystem(),    pm.GetCinematicSystem()    },
        { "NetworkSystem",      pm.GetNetworkSystem(),      pm.GetNetworkSystem()      },
        { "AudioEngine",        pm.GetAudioEngine(),        pm.GetAudioEngine()        },
        { "TweenSystem",        pm.GetTweenSystem(),        pm.GetTweenSystem()        },
    };
    constexpr size_t kCount = sizeof(accessors) / sizeof(accessors[0]);

    for (size_t i = 0; i < kCount; ++i) {
        ENJIN_EXPECT_NOT_NULL(accessors[i].first);
        // Stable identity: a getter returning the address of a temporary, or
        // lazily constructing a new object each call, fails here.
        ENJIN_EXPECT_EQ(accessors[i].first, accessors[i].second);
    }

    // Distinctness. Two accessors returning the same address means one of them
    // names the wrong member -- a copy-paste that a null check cannot see.
    for (size_t i = 0; i < kCount; ++i) {
        for (size_t j = i + 1; j < kCount; ++j) {
            if (accessors[i].first != nullptr && accessors[i].first == accessors[j].first) {
                char buf[256];
                std::snprintf(buf, sizeof(buf),
                    "Get%s() and Get%s() return the same object",
                    accessors[i].name, accessors[j].name);
                EnjinTest::ReportFailureMsg(__FILE__, __LINE__, buf);
            }
        }
    }
    // The double loop above reports its own failures, so count it as one
    // assertion regardless of outcome.
    ENJIN_EXPECT_TRUE(kCount == 12);
}

ENJIN_TEST(PlayModeSystems, InputActionMapIsNotOwnedByPlayMode) {
    // The one accessor that is deliberately different: EditorLayer owns the map
    // and PlayMode BORROWS it, because PlayMode used to keep its own and a
    // rebind moved only half the readers. With nothing injected it must be null
    // rather than quietly manufacturing a second map.
    PlayMode pm;
    ENJIN_EXPECT_NULL(pm.GetInputActionMap());
}

ENJIN_TEST_MAIN()
