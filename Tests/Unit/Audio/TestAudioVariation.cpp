// What one play of an AudioSource sounds like.
//
// pitchMin/pitchMax, volumeMin/volumeMax, clipVariations and noRepeat were
// authored in the inspector, written to the scene, and checked by the asset
// validator -- and no audio code read any of them, so every play used the same
// clip at the same pitch. A footstep sounded like the same recording sixty
// times a minute, which is the one thing the Randomization section exists to
// prevent. These tests are here so it cannot quietly stop working again.
//
// ChooseVariation touches no device: it reads the component and the engine's
// generator, so it runs in CI with no sound card.

#include "EnjinTest.h"
#include "Enjin/Audio/AudioEngine.h"
#include "Enjin/ECS/Components/Gameplay.h"

#include <set>
#include <string>

using namespace Enjin;
using namespace Enjin::Audio;

namespace {

ECS::AudioSourceComponent MakeSource() {
    ECS::AudioSourceComponent src;
    src.clipPath = "assets/step_a.wav";
    src.volume = 1.0f;
    src.pitch = 1.0f;
    return src;
}

} // namespace

ENJIN_TEST(AudioVariation, DefaultsChangeNothing) {
    // 1.0/1.0 is "no variation", and a source with no alternates must come back
    // exactly as authored -- otherwise every existing project starts wobbling.
    AudioEngine engine;
    ECS::AudioSourceComponent src = MakeSource();
    src.volume = 0.8f;
    src.pitch = 1.2f;

    for (int i = 0; i < 32; ++i) {
        auto v = engine.ChooseVariation(src);
        ENJIN_EXPECT_EQ(v.clipPath, std::string("assets/step_a.wav"));
        ENJIN_EXPECT_FLOAT_NEAR(v.pitch, 1.2f, 0.0001f);
        ENJIN_EXPECT_FLOAT_NEAR(v.volume, 0.8f, 0.0001f);
    }
}

ENJIN_TEST(AudioVariation, PitchStaysInsideTheAuthoredRange) {
    AudioEngine engine;
    ECS::AudioSourceComponent src = MakeSource();
    src.pitchMin = 0.9f;
    src.pitchMax = 1.1f;

    bool sawTwoValues = false;
    f32 first = 0.0f;
    for (int i = 0; i < 64; ++i) {
        auto v = engine.ChooseVariation(src);
        ENJIN_ASSERT_TRUE(v.pitch >= 0.9f - 0.0001f);
        ENJIN_ASSERT_TRUE(v.pitch <= 1.1f + 0.0001f);
        if (i == 0) first = v.pitch;
        else if (v.pitch != first) sawTwoValues = true;
    }
    // A range that never varies is the bug this whole file is about.
    ENJIN_EXPECT_TRUE(sawTwoValues);
}

ENJIN_TEST(AudioVariation, VolumeScalesTheAuthoredVolume) {
    // The range MULTIPLIES the source volume rather than replacing it, so a
    // quiet source stays quiet.
    AudioEngine engine;
    ECS::AudioSourceComponent src = MakeSource();
    src.volume = 0.5f;
    src.volumeMin = 0.8f;
    src.volumeMax = 1.0f;

    for (int i = 0; i < 64; ++i) {
        auto v = engine.ChooseVariation(src);
        ENJIN_ASSERT_TRUE(v.volume >= 0.5f * 0.8f - 0.0001f);
        ENJIN_ASSERT_TRUE(v.volume <= 0.5f * 1.0f + 0.0001f);
    }
}

ENJIN_TEST(AudioVariation, TheAuthoredClipIsInTheDraw) {
    // The list is the source's own clip PLUS its alternates. Drawing only from
    // the alternates would make the clip you actually assigned the one you
    // never hear.
    AudioEngine engine;
    ECS::AudioSourceComponent src = MakeSource();
    src.clipVariations = { "assets/step_b.wav", "assets/step_c.wav" };
    src.noRepeat = false;

    std::set<std::string> seen;
    for (int i = 0; i < 200; ++i) seen.insert(engine.ChooseVariation(src).clipPath);

    ENJIN_EXPECT_EQ(seen.size(), (usize)3);
    ENJIN_EXPECT_TRUE(seen.count("assets/step_a.wav") == 1);
    ENJIN_EXPECT_TRUE(seen.count("assets/step_b.wav") == 1);
    ENJIN_EXPECT_TRUE(seen.count("assets/step_c.wav") == 1);
}

ENJIN_TEST(AudioVariation, NoRepeatNeverPicksTheSameClipTwiceRunning) {
    AudioEngine engine;
    ECS::AudioSourceComponent src = MakeSource();
    src.clipVariations = { "assets/step_b.wav", "assets/step_c.wav" };
    src.noRepeat = true;

    std::string previous = engine.ChooseVariation(src).clipPath;
    for (int i = 0; i < 400; ++i) {
        std::string next = engine.ChooseVariation(src).clipPath;
        ENJIN_ASSERT_TRUE(next != previous);
        previous = next;
    }
}

ENJIN_TEST(AudioVariation, NoRepeatStillReachesEveryClip) {
    // Avoiding the last one must not collapse into alternating between two.
    AudioEngine engine;
    ECS::AudioSourceComponent src = MakeSource();
    src.clipVariations = { "assets/step_b.wav", "assets/step_c.wav" };
    src.noRepeat = true;

    std::set<std::string> seen;
    for (int i = 0; i < 300; ++i) seen.insert(engine.ChooseVariation(src).clipPath);
    ENJIN_EXPECT_EQ(seen.size(), (usize)3);
}

ENJIN_TEST(AudioVariation, NoRepeatWithOneAlternateAlternates) {
    // Two clips and "never the same twice" leaves exactly one legal answer
    // each time. It must not spin or throw looking for a third.
    AudioEngine engine;
    ECS::AudioSourceComponent src = MakeSource();
    src.clipVariations = { "assets/step_b.wav" };
    src.noRepeat = true;

    std::string a = engine.ChooseVariation(src).clipPath;
    std::string b = engine.ChooseVariation(src).clipPath;
    std::string c = engine.ChooseVariation(src).clipPath;
    ENJIN_EXPECT_TRUE(a != b);
    ENJIN_EXPECT_EQ(a, c);
}

ENJIN_TEST_MAIN()
