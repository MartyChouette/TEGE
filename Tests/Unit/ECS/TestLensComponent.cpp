#include "EnjinTest.h"
#include <cmath>
#include "Enjin/ECS/Components/Lens.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Renderer/CameraLens.h"
#include "Enjin/Renderer/PostProcessing.h"

using namespace Enjin;
using namespace Enjin::ECS;

ENJIN_TEST(LensDefaults, NeutralByDefault) {
    LensComponent lens;
    ENJIN_EXPECT_TRUE(lens.enabled);
    ENJIN_EXPECT_EQ((int)lens.type, (int)LensType::Standard);
    ENJIN_EXPECT_FLOAT_EQ(lens.distortion, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(lens.anamorphicSqueeze, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(lens.chromaticAberration, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(lens.vignetteIntensity, 0.0f);
}

ENJIN_TEST(LensPreset, FisheyeBendsOutward) {
    // Arrange / Act
    LensComponent lens;
    lens.ApplyPreset(LensType::Fisheye);
    // Assert: strong barrel (negative) distortion and the type is recorded.
    ENJIN_EXPECT_TRUE(lens.distortion < -0.3f);
    ENJIN_EXPECT_EQ((int)lens.type, (int)LensType::Fisheye);
}

ENJIN_TEST(LensPreset, TelephotoPinchesAndVignettes) {
    LensComponent lens;
    lens.ApplyPreset(LensType::Telephoto);
    ENJIN_EXPECT_TRUE(lens.distortion > 0.0f);          // pincushion
    ENJIN_EXPECT_TRUE(lens.vignetteIntensity > 0.2f);   // tighter vignette
}

ENJIN_TEST(LensPreset, AnamorphicSqueezesHorizontally) {
    LensComponent lens;
    lens.ApplyPreset(LensType::Anamorphic);
    ENJIN_EXPECT_TRUE(lens.anamorphicSqueeze > 1.0f);
    ENJIN_EXPECT_TRUE(lens.chromaticAberration > 0.0f);
}

ENJIN_TEST(LensPreset, StandardResetsToNeutral) {
    LensComponent lens;
    lens.ApplyPreset(LensType::Fisheye);   // dirty it
    lens.ApplyPreset(LensType::Standard);  // reset
    ENJIN_EXPECT_FLOAT_EQ(lens.distortion, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(lens.anamorphicSqueeze, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(lens.vignetteIntensity, 0.0f);
}

ENJIN_TEST(LensPreset, CustomLeavesValuesAlone) {
    // Custom is the "I tweaked it myself" mode: ApplyPreset must not stomp values.
    LensComponent lens;
    lens.distortion = -0.22f;
    lens.vignetteIntensity = 0.33f;
    lens.ApplyPreset(LensType::Custom);
    ENJIN_EXPECT_FLOAT_EQ(lens.distortion, -0.22f);
    ENJIN_EXPECT_FLOAT_EQ(lens.vignetteIntensity, 0.33f);
    ENJIN_EXPECT_EQ((int)lens.type, (int)LensType::Custom);
}

ENJIN_TEST(LensPreset, EveryPresetIsApplicable) {
    // No preset should crash or leave an out-of-range squeeze.
    LensComponent lens;
    for (int i = 0; i < (int)LensType::COUNT; ++i) {
        lens.ApplyPreset((LensType)i);
        ENJIN_EXPECT_TRUE(lens.anamorphicSqueeze > 0.0f);
        ENJIN_EXPECT_TRUE(lens.vignetteIntensity >= 0.0f);
    }
}

// ===========================================================================
// Applied to post-processing: nothing read the component (SD-27)
// ===========================================================================

namespace {
Entity AddCamera(World& w, i32 priority) {
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    CameraComponent c;
    c.isActive = true;
    c.priority = priority;
    w.AddComponent<CameraComponent>(e, c);
    return e;
}
}

ENJIN_TEST(LensApply, AFisheyeBendsAndFringes) {
    LensComponent lens;
    lens.ApplyPreset(LensType::Fisheye);
    Renderer::PostProcessSettings s;
    Renderer::ApplyLensToSettings(lens, s);
    ENJIN_EXPECT_FLOAT_EQ(s.lensDistortion, lens.distortion);
    ENJIN_EXPECT_TRUE(s.lensDistortion < 0.0f);
    ENJIN_EXPECT_EQ(s.chromaticAberrationEnabled, 1u);
    ENJIN_EXPECT_FLOAT_EQ(s.chromaticAberrationIntensity, lens.chromaticAberration);
}

ENJIN_TEST(LensApply, AStandardLensKeepsTheScenesVignette) {
    // Zero on the lens means "not the lens's business", not "switch it off"
    LensComponent lens;
    Renderer::PostProcessSettings s;
    s.vignetteEnabled = 1;
    s.vignetteIntensity = 0.4f;
    Renderer::ApplyLensToSettings(lens, s);
    ENJIN_EXPECT_EQ(s.vignetteEnabled, 1u);
    ENJIN_EXPECT_FLOAT_EQ(s.vignetteIntensity, 0.4f);
    ENJIN_EXPECT_FLOAT_EQ(s.lensDistortion, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(s.lensSqueeze, 1.0f);
}

ENJIN_TEST(LensApply, ReadsTheCameraTheGameRendersThrough) {
    World w;
    Entity low = AddCamera(w, 0);
    Entity high = AddCamera(w, 5);
    LensComponent wide;
    wide.anamorphicSqueeze = 1.33f;
    w.AddComponent<LensComponent>(low, wide);

    Renderer::PostProcessSettings s;
    // The lens is on the camera nobody is looking through
    ENJIN_EXPECT_FALSE(Renderer::ApplyCameraLens(&w, s));
    ENJIN_EXPECT_FLOAT_EQ(s.lensSqueeze, 1.0f);

    w.AddComponent<LensComponent>(high, wide);
    ENJIN_EXPECT_TRUE(Renderer::ApplyCameraLens(&w, s));
    ENJIN_EXPECT_FLOAT_EQ(s.lensSqueeze, 1.33f);

    Renderer::PostProcessSettings off;
    w.GetComponent<LensComponent>(high)->enabled = false;
    ENJIN_EXPECT_FALSE(Renderer::ApplyCameraLens(&w, off));
    ENJIN_EXPECT_FLOAT_EQ(off.lensSqueeze, 1.0f);
}

ENJIN_TEST(LensApply, FocalLengthAndFieldOfViewAreOneNumber) {
    // Full frame: a 50 mm lens is about 27 degrees tall
    ENJIN_EXPECT_TRUE(std::abs(Renderer::FovFromFocalLength(50.0f) - 26.99f) < 0.05f);
    ENJIN_EXPECT_TRUE(std::abs(Renderer::FocalLengthFromFov(Renderer::FovFromFocalLength(85.0f)) - 85.0f) < 0.01f);
}

ENJIN_TEST(LensApply, DepthOfFieldFollowsTheOptics) {
    LensComponent lens;
    Renderer::PostProcessSettings off;
    Renderer::ApplyLensToSettings(lens, off, 40.0f);
    ENJIN_EXPECT_EQ(off.dofEnabled, 0u);   // off until asked for

    lens.depthOfField = true;
    lens.focusDistance = 5.0f;
    lens.apertureTStop = 2.8f;
    Renderer::PostProcessSettings a;
    Renderer::ApplyLensToSettings(lens, a, Renderer::FovFromFocalLength(35.0f));
    ENJIN_EXPECT_EQ(a.dofEnabled, 1u);
    ENJIN_EXPECT_FLOAT_EQ(a.dofFocalDistance, 5.0f);
    // 2 N c s^2 / f^2 = 2 * 2.8 * 0.00003 * 25 / 0.035^2 = 3.43 m
    ENJIN_EXPECT_TRUE(std::abs(a.dofFocalRange - 3.43f) < 0.02f);

    // Stopping down deepens it; a longer lens thins it
    lens.apertureTStop = 11.0f;
    Renderer::PostProcessSettings b;
    Renderer::ApplyLensToSettings(lens, b, Renderer::FovFromFocalLength(35.0f));
    ENJIN_EXPECT_TRUE(b.dofFocalRange > a.dofFocalRange);
    Renderer::PostProcessSettings c;
    Renderer::ApplyLensToSettings(lens, c, Renderer::FovFromFocalLength(135.0f));
    ENJIN_EXPECT_TRUE(c.dofFocalRange < b.dofFocalRange);
}

ENJIN_TEST_MAIN()
