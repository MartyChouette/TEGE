// Tutorial mode is Creative mode with a walkthrough over it.
//
// The editor had three modes in Marty's head and two in the code, and neither
// of the two was presented as a mode: Creative had a "Full Editor" button to
// leave by, so the way OUT was visible from inside it, while the full editor
// said nothing at all about another mode existing. Tutorial did not exist -- no
// code, no flag, no setting, nothing in any doc.
//
// These cover the walkthrough's logic rather than its copy: which step you are
// on, when it advances, and the two properties that decide whether a tutorial
// helps or nags.
#include "EnjinTest.h"
#include "Enjin/Editor/Walkthrough.h"
#include "Enjin/Editor/EditorMode.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/GrassVolume.h"

using namespace Enjin;
using namespace Enjin::Editor;

// ---------------------------------------------------------------------------
// Modes
// ---------------------------------------------------------------------------

ENJIN_TEST(EditorModes, TutorialRaisesTheBuildSurfaceJustLikeCreative) {
    // Tutorial IS Creative with a walkthrough on top -- same rail, same tools.
    // Anything asking "is the build surface up" has to accept both, or the
    // tutorial loses the thing it is teaching.
    ENJIN_EXPECT_TRUE(ModeUsesBuildSurface(EditorMode::Creative));
    ENJIN_EXPECT_TRUE(ModeUsesBuildSurface(EditorMode::Tutorial));
    ENJIN_EXPECT_FALSE(ModeUsesBuildSurface(EditorMode::Developer));
}

ENJIN_TEST(EditorModes, EveryModeIsNamedAndExplained) {
    // The switcher shows the name and the description. A mode with an empty
    // description is one you pick by guessing.
    for (u8 i = 0; i < static_cast<u8>(EditorMode::Count); ++i) {
        const EditorMode m = static_cast<EditorMode>(i);
        ENJIN_ASSERT_TRUE(EditorModeName(m) != nullptr);
        ENJIN_EXPECT_TRUE(EditorModeName(m)[0] != '\0');
        ENJIN_ASSERT_TRUE(EditorModeDescription(m) != nullptr);
        ENJIN_EXPECT_TRUE(EditorModeDescription(m)[0] != '\0');
    }
}

ENJIN_TEST(EditorModes, ModesRoundTripThroughTheirSavedName) {
    // Saved by NAME, so reordering the enum cannot silently move everybody to a
    // different mode on the next launch.
    for (u8 i = 0; i < static_cast<u8>(EditorMode::Count); ++i) {
        const EditorMode m = static_cast<EditorMode>(i);
        ENJIN_EXPECT_TRUE(EditorModeFromName(EditorModeName(m)) == m);
    }
}

ENJIN_TEST(EditorModes, AnUnreadableModeFallsBackToTheOneWithEverythingInIt) {
    // A settings file from a newer build should drop you somewhere you can still
    // work, not somewhere with fewer tools.
    ENJIN_EXPECT_TRUE(EditorModeFromName("Sculpt") == EditorMode::Developer);
    ENJIN_EXPECT_TRUE(EditorModeFromName("") == EditorMode::Developer);
    ENJIN_EXPECT_TRUE(EditorModeFromName(nullptr) == EditorMode::Developer);
}

// ---------------------------------------------------------------------------
// The walkthrough
// ---------------------------------------------------------------------------

ENJIN_TEST(Walkthrough, EveryStepSaysWhatToDoAndWhy) {
    // The instruction is what, the guidance is why. An instruction you follow
    // without knowing why teaches a sequence rather than a tool, and a step with
    // no detectable completion has no business claiming one.
    const auto& steps = DefaultWalkthrough();
    ENJIN_ASSERT_TRUE(!steps.empty());
    for (const auto& s : steps) {
        ENJIN_EXPECT_FALSE(s.instruction.empty());
        ENJIN_EXPECT_FALSE(s.guidance.empty());
        ENJIN_EXPECT_TRUE(static_cast<bool>(s.isComplete));
    }
}

ENJIN_TEST(Walkthrough, ItStartsOnTheFirstStepAndIsNotFinished) {
    WalkthroughState w;
    ENJIN_EXPECT_EQ(w.CurrentStep(), static_cast<usize>(0));
    ENJIN_EXPECT_FALSE(w.IsFinished());
    ENJIN_EXPECT_TRUE(w.StepCount() == DefaultWalkthrough().size());
}

ENJIN_TEST(Walkthrough, AStepAdvancesWhenItsConditionIsMet) {
    // The first step asks you to pick the Wall tool.
    ECS::World world;
    WalkthroughState w;

    WalkthroughContext ctx;
    ctx.world = &world;
    ctx.currentTool = BuildTool::Floor;

    ENJIN_EXPECT_FALSE(w.Update(ctx));
    ENJIN_EXPECT_EQ(w.CurrentStep(), static_cast<usize>(0));

    ctx.currentTool = BuildTool::Wall;
    ENJIN_EXPECT_TRUE(w.Update(ctx));            // true on the frame it completes
    ENJIN_EXPECT_EQ(w.CurrentStep(), static_cast<usize>(1));
}

ENJIN_TEST(Walkthrough, CompletionIsReportedOnceNotEveryFrame) {
    // The caller reacts to this -- a sound, an announcement. Returning true
    // every frame after would repeat it for as long as the condition held.
    ECS::World world;
    WalkthroughState w;

    WalkthroughContext ctx;
    ctx.world = &world;
    ctx.currentTool = BuildTool::Wall;

    ENJIN_ASSERT_TRUE(w.Update(ctx));
    const usize after = w.CurrentStep();
    // The tool is still Wall, but step 0 is done and step 1 wants something else.
    ENJIN_EXPECT_FALSE(w.Update(ctx));
    ENJIN_EXPECT_EQ(w.CurrentStep(), after);
}

ENJIN_TEST(Walkthrough, ItNeverGoesBackwards) {
    // Delete the wall you built and the tutorial must not drag you back through
    // it. The point of a completed step is that you did it once.
    ECS::World world;
    WalkthroughState w;

    WalkthroughContext ctx;
    ctx.world = &world;
    ctx.currentTool = BuildTool::Wall;
    ENJIN_ASSERT_TRUE(w.Update(ctx));
    ENJIN_ASSERT_EQ(w.CurrentStep(), static_cast<usize>(1));

    // Condition for step 0 no longer holds.
    ctx.currentTool = BuildTool::Terrain;
    for (int i = 0; i < 10; ++i) w.Update(ctx);
    ENJIN_EXPECT_TRUE(w.CurrentStep() >= 1);
}

ENJIN_TEST(Walkthrough, RunningEveryStepFinishesIt) {
    // Drives each step's own predicate rather than asserting a fixed count, so
    // adding a step to the list does not break this.
    ECS::World world;
    WalkthroughState w;

    WalkthroughContext ctx;
    ctx.world = &world;

    // A context that satisfies everything the steps can ask of the editor. The
    // world-content steps are satisfied below by adding the components.
    ctx.isPlaying = true;

    world.AddComponent<ECS::BrushSolidComponent>(world.CreateEntity());
    world.AddComponent<ECS::WaterVolumeComponent>(world.CreateEntity());
    world.AddComponent<ECS::GrassVolumeComponent>(world.CreateEntity());
    world.AddComponent<ECS::LightComponent>(world.CreateEntity());

    // Walk the tool-dependent steps by offering each tool in turn, repeatedly,
    // until nothing advances.
    const BuildTool tools[] = { BuildTool::Wall, BuildTool::Floor };
    for (int pass = 0; pass < 64 && !w.IsFinished(); ++pass) {
        bool advanced = false;
        for (BuildTool t : tools) {
            ctx.currentTool = t;
            if (w.Update(ctx)) advanced = true;
        }
        if (!advanced) break;
    }

    ENJIN_EXPECT_TRUE(w.IsFinished());
    ENJIN_EXPECT_EQ(w.CurrentStep(), w.StepCount());
}

ENJIN_TEST(Walkthrough, ResetPutsYouBackAtTheStart) {
    ECS::World world;
    WalkthroughState w;
    WalkthroughContext ctx;
    ctx.world = &world;
    ctx.currentTool = BuildTool::Wall;

    w.Update(ctx);
    ENJIN_ASSERT_TRUE(w.CurrentStep() > 0);

    w.Reset();
    ENJIN_EXPECT_EQ(w.CurrentStep(), static_cast<usize>(0));
    ENJIN_EXPECT_FALSE(w.IsFinished());
}

ENJIN_TEST(Walkthrough, TheCurrentStepIsReadableEvenWhenFinished) {
    // The panel asks for a step to render every frame, including the frame after
    // the last one completes. Reading off the end there would be the crash.
    ECS::World world;
    WalkthroughState w;
    WalkthroughContext ctx;
    ctx.world = &world;
    ctx.isPlaying = true;

    world.AddComponent<ECS::BrushSolidComponent>(world.CreateEntity());
    world.AddComponent<ECS::WaterVolumeComponent>(world.CreateEntity());
    world.AddComponent<ECS::GrassVolumeComponent>(world.CreateEntity());
    world.AddComponent<ECS::LightComponent>(world.CreateEntity());

    const BuildTool tools[] = { BuildTool::Wall, BuildTool::Floor };
    for (int pass = 0; pass < 64 && !w.IsFinished(); ++pass) {
        for (BuildTool t : tools) { ctx.currentTool = t; w.Update(ctx); }
    }

    ENJIN_ASSERT_TRUE(w.IsFinished());
    ENJIN_EXPECT_FALSE(w.Current().instruction.empty());
}

ENJIN_TEST_MAIN()
