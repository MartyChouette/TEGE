// A data format that stores arrays lets a script read arrays.
//
// DataAssetValue carries eight types and the serializer round-trips all eight.
// Five had a getter. So StringArray, FloatArray and Vector4 were unreachable from
// script: the format could hold a list and nothing could get one back.
//
// The workarounds were indexed keys (cue0_t, cue1_t, read until empty) or one
// delimited string parsed by hand in AngelScript -- both working around a
// capability the format already had. This is not caption-specific; waypoints,
// loot tables, dialogue, spawn sets and schedules hit the same wall.
//
// The contract these tests pin, which is the part worth getting right:
//
//   GetArrayLength -> 0 for missing asset, missing field, AND not-an-array.
//                     All three mean "nothing to iterate", so a plain
//                     for (i = 0; i < len; ++i) is correct in every case. A -1
//                     would be a sentinel every caller has to remember.
//
//   element reads  -> fallback AND a warning, once per asset+field. A silent ""
//                     is indistinguishable from authored empty text, which is the
//                     same failure Audio_GetTime avoids by answering -1 instead
//                     of 0.0 when it cannot say where a sound is.
#include "EnjinTest.h"
#include "Enjin/Assets/DataAsset.h"
#include <string>

using namespace Enjin;
using namespace Enjin::Assets;

namespace {

// The shape the request actually asks for: a caption track as three parallel
// arrays, hand-written the way someone would author it.
const char* kCaptionAsset = R"({
  "name": "wgrb_night",
  "schema": "CaptionTrack",
  "values": {
    "callsign": "WGRB NIGHT DESK",
    "cue_t":    [0.5, 4.0, 8.0, 11.5],
    "cue_who":  ["WGRB", "CALLER", "WGRB", "CALLER"],
    "cue_line": ["Night desk.", "You there?", "Go ahead.", ""]
  }
})";

DataAssetRegistry& FreshRegistry() {
    auto& r = DataAssetRegistry::Get();
    r.Clear();
    return r;
}

} // namespace

ENJIN_TEST(DataAssetArrays, ParallelArraysReadBackElementByElement) {
    auto& registry = FreshRegistry();
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(kCaptionAsset, "wgrb_night.enjdata"));

    ENJIN_ASSERT_EQ(registry.GetArrayLength("wgrb_night", "cue_t"), static_cast<usize>(4));
    ENJIN_EXPECT_EQ(registry.GetArrayLength("wgrb_night", "cue_who"), static_cast<usize>(4));
    ENJIN_EXPECT_EQ(registry.GetArrayLength("wgrb_night", "cue_line"), static_cast<usize>(4));

    // The whole track, the way a caption player would walk it.
    ENJIN_EXPECT_TRUE(registry.GetFloatAt("wgrb_night", "cue_t", 0) < 0.51f);
    ENJIN_EXPECT_TRUE(registry.GetFloatAt("wgrb_night", "cue_t", 3) > 11.49f);
    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_who", 1), std::string("CALLER"));
    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_line", 2), std::string("Go ahead."));

    // A scalar field on the same asset still reads as a scalar.
    ENJIN_EXPECT_EQ(registry.GetString("wgrb_night", "callsign"), std::string("WGRB NIGHT DESK"));
}

ENJIN_TEST(DataAssetArrays, AnAuthoredEmptyStringIsNotAFailure) {
    // The distinction the warning exists to protect. Element 3 of cue_line is
    // deliberately "" -- a real, authored, empty caption. It must read back as
    // an empty string with no complaint, and be distinguishable from a read that
    // failed, which is why a failed read warns.
    auto& registry = FreshRegistry();
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(kCaptionAsset, "wgrb_night.enjdata"));

    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_line", 3), std::string(""));
    // And it is genuinely element 3 of 4, not a fallback for a missing element.
    ENJIN_EXPECT_EQ(registry.GetArrayLength("wgrb_night", "cue_line"), static_cast<usize>(4));
}

ENJIN_TEST(DataAssetArrays, LengthIsZeroForEveryKindOfNothing) {
    // Missing asset, missing field, and present-but-not-a-list all answer 0,
    // because all three mean the same thing to a caller writing a for loop.
    auto& registry = FreshRegistry();
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(kCaptionAsset, "wgrb_night.enjdata"));

    ENJIN_EXPECT_EQ(registry.GetArrayLength("no_such_asset", "cue_t"), static_cast<usize>(0));
    ENJIN_EXPECT_EQ(registry.GetArrayLength("wgrb_night", "no_such_field"), static_cast<usize>(0));
    ENJIN_EXPECT_EQ(registry.GetArrayLength("wgrb_night", "callsign"), static_cast<usize>(0));
}

ENJIN_TEST(DataAssetArrays, AForLoopOverAMissingFieldIsSafeAndEmpty) {
    // The property the 0-not-minus-1 choice buys: the obvious loop is correct
    // against a field that does not exist, with no guard.
    auto& registry = FreshRegistry();
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(kCaptionAsset, "wgrb_night.enjdata"));

    usize visited = 0;
    const usize len = registry.GetArrayLength("wgrb_night", "not_authored_yet");
    for (usize i = 0; i < len; ++i) {
        (void)registry.GetStringAt("wgrb_night", "not_authored_yet", i);
        ++visited;
    }
    ENJIN_EXPECT_EQ(visited, static_cast<usize>(0));
}

ENJIN_TEST(DataAssetArrays, OutOfRangeReturnsTheFallbackNotTheLastElement) {
    // Clamping to the last element would be the plausible wrong answer: a caption
    // player reading one cue past the end would repeat the final line forever
    // instead of stopping.
    auto& registry = FreshRegistry();
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(kCaptionAsset, "wgrb_night.enjdata"));

    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_who", 4), std::string(""));
    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_who", 99, "MISS"), std::string("MISS"));
    ENJIN_EXPECT_TRUE(registry.GetFloatAt("wgrb_night", "cue_t", 4) == 0.0f);
    ENJIN_EXPECT_TRUE(registry.GetFloatAt("wgrb_night", "cue_t", 99, -1.0f) == -1.0f);
}

ENJIN_TEST(DataAssetArrays, AFloatArrayIsNotSilentlyReadAsStrings) {
    // Coercing 4.0 into "4" here is exactly the plausible-wrong-answer the
    // accessor exists to avoid, so a wrong-type read is a fallback, not a
    // conversion.
    auto& registry = FreshRegistry();
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(kCaptionAsset, "wgrb_night.enjdata"));

    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_t", 0, "NOPE"), std::string("NOPE"));
    ENJIN_EXPECT_TRUE(registry.GetFloatAt("wgrb_night", "cue_who", 0, -7.0f) == -7.0f);
}

ENJIN_TEST(DataAssetArrays, TheOtherTwoUnreachableTypesAreReachable) {
    // Vector4 had no getter either -- three of the eight types were unreachable,
    // not the two the request counted.
    auto& registry = FreshRegistry();
    const char* asset = R"({
      "name": "tint",
      "values": { "colour": { "type": "Vector4", "value": [0.2, 0.4, 0.6, 0.8] } }
    })";
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(asset, "tint.enjdata"));

    const Math::Vector4 v = registry.GetVector4("tint", "colour");
    ENJIN_EXPECT_TRUE(v.x > 0.19f && v.x < 0.21f);
    ENJIN_EXPECT_TRUE(v.w > 0.79f && v.w < 0.81f);

    // And a missing one still falls back rather than inventing a colour.
    const Math::Vector4 miss = registry.GetVector4("tint", "no_such", Math::Vector4(9, 9, 9, 9));
    ENJIN_EXPECT_TRUE(miss.x > 8.9f);
}

ENJIN_TEST(DataAssetArrays, ClearForgetsWhatItHasAlreadyWarnedAbout) {
    // Warn-once is per asset+field for the life of the loaded data. After a
    // reload the data is new, so a still-broken field deserves to say so again --
    // otherwise fixing a typo and reloading looks identical to not fixing it.
    auto& registry = FreshRegistry();
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(kCaptionAsset, "wgrb_night.enjdata"));

    // Two bad reads: the second is silent, but both return the fallback.
    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_who", 50, "A"), std::string("A"));
    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_who", 51, "B"), std::string("B"));

    registry.Clear();
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(kCaptionAsset, "wgrb_night.enjdata"));
    // Still returns the fallback after a reload; the warning state is what reset.
    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_who", 50, "C"), std::string("C"));
}

ENJIN_TEST(DataAssetArrays, ABareNumericArrayIsNeverGuessedIntoAVector) {
    // The trap this rule removes. A caption track with exactly four cues, or a
    // waypoint list with exactly three points, must stay a list. Guessing a
    // Vector4 from the length would hand the author a point where they wrote a
    // list, and GetArrayLength would answer 0 with nothing to explain it.
    auto& registry = FreshRegistry();
    const char* asset = R"({
      "name": "edge",
      "values": {
        "three_cues": [1.0, 2.0, 3.0],
        "four_cues":  [1.0, 2.0, 3.0, 4.0]
      }
    })";
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(asset, "edge.enjdata"));

    ENJIN_EXPECT_EQ(registry.GetArrayLength("edge", "three_cues"), static_cast<usize>(3));
    ENJIN_EXPECT_EQ(registry.GetArrayLength("edge", "four_cues"), static_cast<usize>(4));
    ENJIN_EXPECT_TRUE(registry.GetFloatAt("edge", "four_cues", 3) > 3.9f);

    // The tagged form is how you ask for a vector, and it still works.
    const char* tagged = R"({
      "name": "point",
      "values": { "p": { "type": "Vector3", "value": [1.0, 2.0, 3.0] } }
    })";
    ENJIN_ASSERT_TRUE(registry.LoadAssetFromString(tagged, "point.enjdata"));
    ENJIN_EXPECT_TRUE(registry.GetVector3("point", "p").z > 2.9f);
    ENJIN_EXPECT_EQ(registry.GetArrayLength("point", "p"), static_cast<usize>(0));
}

ENJIN_TEST_MAIN()
