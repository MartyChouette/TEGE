// Animation LOD: the band table, and the fidelity knob it drives.
//
// This replaced four `constexpr f32` distances and a 1/2/4/8 FRAME interval buried in
// RenderSystem. Two things about that are worth pinning down here, because neither was
// visible before: the band a distance resolves to, and the fact that dropping
// interpolation snaps to the PRECEDING key rather than the nearest one.

#include "EnjinTest.h"
#include "Enjin/ECS/Components/AnimationLOD.h"
#include "Enjin/Animation/Animation.h"

using namespace Enjin;
using namespace Enjin::ECS;

// ---------------------------------------------------------------------------
// Band resolution
// ---------------------------------------------------------------------------

ENJIN_TEST(AnimationLODBands, DefaultsReproduceTheOldHardcodedDistances) {
    // The hardcoded version switched at 30, 70 and 140 metres. A project that adds this
    // component and leaves it alone must get exactly what it had.
    AnimationLODComponent lod;
    ENJIN_EXPECT_EQ(lod.bandCount, 4);
    ENJIN_EXPECT_FLOAT_EQ(lod.bands[1].beginDistance, 30.0f);
    ENJIN_EXPECT_FLOAT_EQ(lod.bands[2].beginDistance, 70.0f);
    ENJIN_EXPECT_FLOAT_EQ(lod.bands[3].beginDistance, 140.0f);
    // ... and at 60fps the old 1/2/4/8 frame intervals are these rates.
    ENJIN_EXPECT_FLOAT_EQ(lod.bands[0].updateHz, 0.0f);   // every frame
    ENJIN_EXPECT_FLOAT_EQ(lod.bands[1].updateHz, 30.0f);
    ENJIN_EXPECT_FLOAT_EQ(lod.bands[2].updateHz, 15.0f);
    ENJIN_EXPECT_FLOAT_EQ(lod.bands[3].updateHz, 7.5f);
}

ENJIN_TEST(AnimationLODBands, ResolvesTheBandADistanceFallsIn) {
    AnimationLODComponent lod;
    ENJIN_EXPECT_EQ(lod.ResolveBand(0.0f), 0);
    ENJIN_EXPECT_EQ(lod.ResolveBand(29.9f), 0);
    ENJIN_EXPECT_EQ(lod.ResolveBand(30.0f), 1);   // inclusive at the boundary
    ENJIN_EXPECT_EQ(lod.ResolveBand(69.9f), 1);
    ENJIN_EXPECT_EQ(lod.ResolveBand(70.0f), 2);
    ENJIN_EXPECT_EQ(lod.ResolveBand(139.9f), 2);
    ENJIN_EXPECT_EQ(lod.ResolveBand(140.0f), 3);
    ENJIN_EXPECT_EQ(lod.ResolveBand(10000.0f), 3);   // never past the last band
}

ENJIN_TEST(AnimationLODBands, HonoursAReducedBandCount) {
    AnimationLODComponent lod;
    lod.bandCount = 2;
    ENJIN_EXPECT_EQ(lod.ResolveBand(10.0f), 0);
    ENJIN_EXPECT_EQ(lod.ResolveBand(1000.0f), 1);   // band 2 and 3 are not in play
}

ENJIN_TEST(AnimationLODBands, OutOfRangeBandCountCannotIndexOutOfBounds) {
    // Authored data and old scenes both reach this; clamping in ResolveBand is what
    // stops a bad bandCount being an out-of-bounds read on a fixed array.
    AnimationLODComponent lod;
    lod.bandCount = 99;
    const i32 b = lod.ResolveBand(1e9f);
    ENJIN_EXPECT_TRUE(b >= 0 && b < AnimationLODComponent::MAX_BANDS);

    lod.bandCount = 0;
    ENJIN_EXPECT_EQ(lod.ResolveBand(1e9f), 0);

    lod.bandCount = -5;
    ENJIN_EXPECT_EQ(lod.ResolveBand(1e9f), 0);
}

ENJIN_TEST(AnimationLODBands, FidelityDropsOffWithDistance) {
    AnimationLODComponent lod;
    ENJIN_EXPECT_TRUE(lod.bands[lod.ResolveBand(0.0f)].ik);
    ENJIN_EXPECT_TRUE(lod.bands[lod.ResolveBand(0.0f)].blendTrees);
    // Far enough away, a hand placed exactly on a door handle stops being worth a solve.
    ENJIN_EXPECT_FALSE(lod.bands[lod.ResolveBand(100.0f)].ik);
    ENJIN_EXPECT_FALSE(lod.bands[lod.ResolveBand(100.0f)].blendTrees);
    // Interpolation survives one band longer than IK does: it is cheaper to keep.
    ENJIN_EXPECT_TRUE(lod.bands[lod.ResolveBand(100.0f)].interpolate);
    ENJIN_EXPECT_FALSE(lod.bands[lod.ResolveBand(200.0f)].interpolate);
}

ENJIN_TEST(AnimationLODQuality, DefaultsToFullFidelity) {
    // AnimatorComponent::Update takes this by default, so a caller that knows nothing
    // about LOD keeps the behaviour it had.
    AnimationQuality q;
    ENJIN_EXPECT_TRUE(q.blendTrees);
    ENJIN_EXPECT_TRUE(q.interpolate);
}

// ---------------------------------------------------------------------------
// The fidelity knob actually changes the sample
// ---------------------------------------------------------------------------

namespace {
Animation::BoneTrack TwoKeyTrack() {
    Animation::BoneTrack t;
    t.positionTimes = { 0.0f, 1.0f };
    t.positions = { Math::Vector3(0.0f, 0.0f, 0.0f), Math::Vector3(10.0f, 0.0f, 0.0f) };
    return t;
}
}  // namespace

ENJIN_TEST(AnimationLODSampling, InterpolatedSampleBlendsBetweenKeys) {
    const Animation::BoneTrack t = TwoKeyTrack();
    ENJIN_EXPECT_FLOAT_EQ(t.SamplePosition(0.5f, true).x, 5.0f);
}

ENJIN_TEST(AnimationLODSampling, NonInterpolatedSampleHoldsThePrecedingKey) {
    const Animation::BoneTrack t = TwoKeyTrack();
    // NOT the nearest key. Rounding to whichever is closer would let a pose arrive
    // before its keyframe does, which reads as the animation running early.
    ENJIN_EXPECT_FLOAT_EQ(t.SamplePosition(0.5f, false).x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(t.SamplePosition(0.9f, false).x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(t.SamplePosition(1.0f, false).x, 10.0f);
}

ENJIN_TEST(AnimationLODSampling, ClampingIsUnaffectedByFidelity) {
    const Animation::BoneTrack t = TwoKeyTrack();
    ENJIN_EXPECT_FLOAT_EQ(t.SamplePosition(-1.0f, false).x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(t.SamplePosition(99.0f, false).x, 10.0f);
}

ENJIN_TEST_MAIN()
