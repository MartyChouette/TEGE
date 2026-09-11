// rewindTint and rewindVignetteStrength were authored, saved, documented, and
// rendered nowhere.
//
// Both fields sit on RecordRewindComponent and SceneRewindComponent under a
// "Visual Feedback" heading in the inspector. Both are written to the scene file
// by SceneSerializer. The user manual calls rewindTint a "screen tint". The only
// consumer anywhere in the engine was the colour of an ImGui progress bar in an
// editor panel -- not something a player can see, on any platform.
//
// So a designer authoring a Sands of Time rewind picked a gold, saved it, ran the
// game and got nothing. The cruel part is that the feature reads as switched off
// rather than missing, and the obvious next move is to turn the number up.
//
// These test the applier rather than the screen, because what has to be right is
// the save/restore: an applier that saved its own output would put the rewind
// look back when the rewind ended and leave it there forever.
#include "EnjinTest.h"
#include "Enjin/Gameplay/RewindFeedback.h"
#include "Enjin/Gameplay/RecordRewindSystem.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"

using namespace Enjin;
using namespace Enjin::Gameplay;

namespace {

// A world with one entity carrying a scene rewind set to a strong gold, wound up
// so it has something to rewind through.
struct Rig {
    ECS::World world;
    ECS::Entity manager = ECS::INVALID_ENTITY;
    RecordRewindSystem system;

    Rig() {
        manager = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(manager);
        world.AddComponent<ECS::SceneRewindComponent>(manager);
        auto* sr = world.GetComponent<ECS::SceneRewindComponent>(manager);
        sr->enabled = true;
        sr->recordInterval = 0.05f;
        sr->rewindSpeed = 1.0f;
        sr->cooldown = 0.0f;
        sr->rewindVignetteStrength = 0.75f;
        sr->rewindTint = Math::Vector3(0.8f, 0.6f, 0.2f);   // the Sands of Time gold
        system.SetWorld(&world);
    }

    void Record(int frames) {
        for (int i = 0; i < frames; ++i) system.Update(0.05f);
    }
};

} // namespace

ENJIN_TEST(RewindFeedback, NothingIsAppliedWhileNoRewindIsRunning) {
    // A default that might not apply is worse than an absence: a tint applied
    // when nothing is rewinding is a wash nobody can trace to a cause.
    Rig rig;
    rig.Record(10);

    Renderer::PostProcessSettings pp;
    pp.colorFilter = Math::Vector3(1.0f, 1.0f, 1.0f);
    pp.vignetteEnabled = 0;

    RewindFeedbackApplier applier;
    applier.Apply(rig.system, pp);

    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.x, 1.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.y, 1.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.z, 1.0f, 0.0001f);
    ENJIN_EXPECT_EQ(pp.vignetteEnabled, 0u);
}

ENJIN_TEST(RewindFeedback, TheAuthoredTintReachesPostProcessingDuringARewind) {
    // The regression. Everything below here used to be unreachable: the system
    // had no way to report what a rewind wanted the screen to look like, because
    // nothing had ever asked.
    Rig rig;
    rig.Record(10);
    rig.system.StartSceneRewind();
    rig.system.Update(0.05f);
    ENJIN_ASSERT_TRUE(rig.system.IsSceneRewinding());

    Renderer::PostProcessSettings pp;
    pp.colorFilter = Math::Vector3(1.0f, 1.0f, 1.0f);
    pp.vignetteEnabled = 0;
    pp.vignetteIntensity = 0.0f;

    RewindFeedbackApplier applier;
    applier.Apply(rig.system, pp);

    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.x, 0.8f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.y, 0.6f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.z, 0.2f, 0.0001f);
    ENJIN_EXPECT_EQ(pp.vignetteEnabled, 1u);
    ENJIN_EXPECT_FLOAT_NEAR(pp.vignetteIntensity, 0.75f, 0.0001f);
}

ENJIN_TEST(RewindFeedback, EndingARewindPutsTheOriginalSettingsBack) {
    Rig rig;
    rig.Record(10);

    Renderer::PostProcessSettings pp;
    pp.colorFilter = Math::Vector3(0.9f, 0.95f, 1.05f);   // the game's own cool grade
    pp.vignetteEnabled = 1;
    pp.vignetteIntensity = 0.2f;

    RewindFeedbackApplier applier;

    rig.system.StartSceneRewind();
    rig.system.Update(0.05f);
    applier.Apply(rig.system, pp);
    ENJIN_ASSERT_TRUE(pp.colorFilter.x < 0.9f);   // it did something

    rig.system.StopSceneRewind();
    rig.system.Update(0.05f);
    applier.Apply(rig.system, pp);

    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.x, 0.9f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.y, 0.95f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.z, 1.05f, 0.0001f);
    ENJIN_EXPECT_EQ(pp.vignetteEnabled, 1u);
    ENJIN_EXPECT_FLOAT_NEAR(pp.vignetteIntensity, 0.2f, 0.0001f);
}

ENJIN_TEST(RewindFeedback, ManyFramesOfRewindDoNotCompound) {
    // The applier saves on the RISING EDGE only. Saving every frame would capture
    // the values it had just written, so the "restore" would put the rewind look
    // back and the tint would multiply into itself until the screen went black.
    Rig rig;
    rig.Record(40);

    Renderer::PostProcessSettings pp;
    pp.colorFilter = Math::Vector3(1.0f, 1.0f, 1.0f);

    RewindFeedbackApplier applier;
    rig.system.StartSceneRewind();
    for (int i = 0; i < 10; ++i) {
        rig.system.Update(0.05f);
        applier.Apply(rig.system, pp);
    }

    // Still exactly the authored tint after ten frames, not 0.8^10.
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.x, 0.8f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.z, 0.2f, 0.0001f);
}

ENJIN_TEST(RewindFeedback, TheTintMultipliesIntoTheGamesOwnGrade) {
    // A rewind should tint the game's look, not discard it. Replacing the colour
    // filter would throw away a grade the game spent its own effort on.
    Rig rig;
    rig.Record(10);
    rig.system.StartSceneRewind();
    rig.system.Update(0.05f);

    Renderer::PostProcessSettings pp;
    pp.colorFilter = Math::Vector3(0.5f, 0.5f, 0.5f);

    RewindFeedbackApplier applier;
    applier.Apply(rig.system, pp);

    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.x, 0.5f * 0.8f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.y, 0.5f * 0.6f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.z, 0.5f * 0.2f, 0.0001f);
}

ENJIN_TEST(RewindFeedback, AGameThatAlreadyHasAStrongerVignetteKeepsIt) {
    // Taking the stronger of the two rather than replacing: a game shipping a
    // heavy vignette should not have it LIFTED for the duration of a rewind.
    Rig rig;
    rig.Record(10);
    rig.system.StartSceneRewind();
    rig.system.Update(0.05f);

    Renderer::PostProcessSettings pp;
    pp.vignetteEnabled = 1;
    pp.vignetteIntensity = 0.9f;      // stronger than the rewind's 0.75

    RewindFeedbackApplier applier;
    applier.Apply(rig.system, pp);
    ENJIN_EXPECT_FLOAT_NEAR(pp.vignetteIntensity, 0.9f, 0.0001f);
}

ENJIN_TEST(RewindFeedback, ResetUndoesTheLookWithoutARewindSystem) {
    // Play can stop mid-rewind. Without this the editor carries a gold wash into
    // edit mode with nothing on screen to explain it.
    Rig rig;
    rig.Record(10);
    rig.system.StartSceneRewind();
    rig.system.Update(0.05f);

    Renderer::PostProcessSettings pp;
    pp.colorFilter = Math::Vector3(1.0f, 1.0f, 1.0f);
    pp.vignetteEnabled = 0;

    RewindFeedbackApplier applier;
    applier.Apply(rig.system, pp);
    ENJIN_ASSERT_TRUE(pp.colorFilter.z < 0.5f);

    applier.Reset(pp);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.x, 1.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.z, 1.0f, 0.0001f);
    ENJIN_EXPECT_EQ(pp.vignetteEnabled, 0u);

    // A second Reset is a no-op, not a second restore of stale values.
    pp.colorFilter = Math::Vector3(0.3f, 0.3f, 0.3f);
    applier.Reset(pp);
    ENJIN_EXPECT_FLOAT_NEAR(pp.colorFilter.x, 0.3f, 0.0001f);
}

ENJIN_TEST(RewindFeedback, AZeroStrengthVignetteIsLeftAlone) {
    // Off means off. A component with the vignette at zero must not switch one on.
    Rig rig;
    auto* sr = rig.world.GetComponent<ECS::SceneRewindComponent>(rig.manager);
    sr->rewindVignetteStrength = 0.0f;
    rig.Record(10);
    rig.system.StartSceneRewind();
    rig.system.Update(0.05f);

    Renderer::PostProcessSettings pp;
    pp.vignetteEnabled = 0;

    RewindFeedbackApplier applier;
    applier.Apply(rig.system, pp);
    ENJIN_EXPECT_EQ(pp.vignetteEnabled, 0u);
}

ENJIN_TEST_MAIN()
