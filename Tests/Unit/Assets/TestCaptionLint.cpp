// A caption track is only wrong at runtime, in one place, for a moment.
//
// Two cues that overlap put two lines on screen at once. A cue that outlives the
// audio leaves a line hanging over silence. A cue nobody can read in the time it
// is up is technically present and practically absent. None of that is visible in
// a table of numbers, which is why the linter exists and why it ships beside a
// timeline.
//
// The property worth defending above all the individual rules: when the clip
// length is unknown, the two checks that need it are reported as NOT RUN, by name.
// A linter that quietly skips half its rules and then says "no problems found"
// converts an unchecked track into a checked one in the author's head, which is
// worse than having no linter at all.
#include "EnjinTest.h"
#include "Enjin/Assets/CaptionLint.h"
#include "Enjin/Assets/SrtImport.h"
#include "Enjin/Assets/DataAsset.h"
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::Assets;

namespace {

SrtCue Cue(f32 start, f32 end, const char* text, const char* who = "") {
    SrtCue c;
    c.start = start;
    c.end = end;
    c.text = text;
    c.speaker = who;
    return c;
}

// A clean three-cue track: in order, no overlaps, comfortable reading speed.
std::vector<SrtCue> CleanTrack() {
    return {
        Cue(0.5f, 4.0f, "Night desk."),
        Cue(4.0f, 8.0f, "You there?"),
        Cue(8.0f, 12.0f, "Go ahead."),
    };
}

bool Has(const CaptionLintResult& r, CaptionIssue issue) {
    for (const CaptionFinding& f : r.findings) if (f.issue == issue) return true;
    return false;
}

usize CountOf(const CaptionLintResult& r, CaptionIssue issue) {
    usize n = 0;
    for (const CaptionFinding& f : r.findings) if (f.issue == issue) ++n;
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// What it refuses to pretend it checked
// ---------------------------------------------------------------------------

ENJIN_TEST(CaptionLint, WithoutAClipLengthItSaysWhatItCouldNotCheck) {
    // Arrange: a clean track, no clip length.
    CaptionLintOptions opts;
    opts.clipLength = 0.0f;

    // Act
    const CaptionLintResult r = LintCaptionTrack(CleanTrack(), opts);

    // Assert: clean, but explicitly not fully checked. Both of the rules that need
    // the audio length are named.
    ENJIN_EXPECT_TRUE(r.Clean());
    ENJIN_ASSERT_EQ(r.notRun.size(), static_cast<usize>(2));
    ENJIN_EXPECT_TRUE(r.notRun[0].find("Past the end") != std::string::npos);
    ENJIN_EXPECT_TRUE(r.notRun[1].find("Coverage") != std::string::npos);
}

ENJIN_TEST(CaptionLint, CoverageIsMinusOneNotZeroWhenTheClipLengthIsUnknown) {
    // 0.0 would read as "none of the clip is captioned", which is a specific and
    // wrong claim. -1 cannot be mistaken for a measurement.
    CaptionLintOptions opts;
    opts.clipLength = 0.0f;
    const CaptionLintResult r = LintCaptionTrack(CleanTrack(), opts);

    ENJIN_EXPECT_TRUE(r.Coverage() < 0.0f);
    // The captioned seconds ARE known without the clip length, and are reported.
    ENJIN_EXPECT_TRUE(r.captionedSeconds > 11.4f && r.captionedSeconds < 11.6f);
}

ENJIN_TEST(CaptionLint, WithAClipLengthEveryRuleRuns) {
    CaptionLintOptions opts;
    opts.clipLength = 12.0f;
    const CaptionLintResult r = LintCaptionTrack(CleanTrack(), opts);

    ENJIN_EXPECT_TRUE(r.notRun.empty());
    ENJIN_EXPECT_TRUE(r.Clean());
    // 11.5s of caption in a 12s clip.
    ENJIN_EXPECT_TRUE(r.Coverage() > 0.95f && r.Coverage() <= 1.0f);
}

// ---------------------------------------------------------------------------
// The rules
// ---------------------------------------------------------------------------

ENJIN_TEST(CaptionLint, AnOverlapIsFoundAndSaysHowLong) {
    std::vector<SrtCue> cues = {
        Cue(0.0f, 5.0f, "first line here"),
        Cue(3.5f, 8.0f, "second line here"),   // starts 1.5s before the first ends
    };
    const CaptionLintResult r = LintCaptionTrack(cues);

    ENJIN_ASSERT_TRUE(Has(r, CaptionIssue::Overlap));
    ENJIN_EXPECT_EQ(CountOf(r, CaptionIssue::Overlap), static_cast<usize>(1));
    // The number is in the message, because "there is an overlap" does not tell an
    // author whether it is a rounding error or a real mistake.
    for (const CaptionFinding& f : r.findings) {
        if (f.issue == CaptionIssue::Overlap) {
            ENJIN_EXPECT_TRUE(f.message.find("1.50s of both") != std::string::npos);
        }
    }
}

ENJIN_TEST(CaptionLint, CuesOutOfOrderAreNotReportedAsAnOverlap) {
    // Distinct faults with distinct fixes: an overlap is a timing tweak, an
    // out-of-order pair means the track was assembled wrong.
    std::vector<SrtCue> cues = {
        Cue(10.0f, 12.0f, "later line"),
        Cue(2.0f, 4.0f, "earlier line"),
    };
    const CaptionLintResult r = LintCaptionTrack(cues);

    ENJIN_EXPECT_TRUE(Has(r, CaptionIssue::OutOfOrder));
    ENJIN_EXPECT_FALSE(Has(r, CaptionIssue::Overlap));
}

ENJIN_TEST(CaptionLint, AZeroLengthCueIsReportedAsNeverAppearing) {
    std::vector<SrtCue> cues = { Cue(3.0f, 3.0f, "blink") };
    const CaptionLintResult r = LintCaptionTrack(cues);

    ENJIN_ASSERT_TRUE(Has(r, CaptionIssue::ZeroLength));
    // And it contributes nothing to coverage, rather than counting as a cue.
    ENJIN_EXPECT_TRUE(r.captionedSeconds < 0.001f);
}

ENJIN_TEST(CaptionLint, AnEmptyCueIsFoundButItsTimingIsStillChecked) {
    // An authored empty caption is legal in a .srt but almost always a mistake in a
    // track, and reporting only the emptiness would hide that it also overlaps.
    std::vector<SrtCue> cues = {
        Cue(0.0f, 5.0f, ""),
        Cue(4.0f, 8.0f, "next"),
    };
    const CaptionLintResult r = LintCaptionTrack(cues);

    ENJIN_EXPECT_TRUE(Has(r, CaptionIssue::Empty));
    ENJIN_EXPECT_TRUE(Has(r, CaptionIssue::Overlap));
}

ENJIN_TEST(CaptionLint, TooMuchTextForTheTimeIsFlaggedWithTheRate) {
    // 60 characters in one second is three times the default ceiling.
    std::vector<SrtCue> cues = {
        Cue(0.0f, 1.0f, "a line of roughly sixty characters, which is far too long"),
    };
    const CaptionLintResult r = LintCaptionTrack(cues);

    ENJIN_ASSERT_TRUE(Has(r, CaptionIssue::TooFast));
    for (const CaptionFinding& f : r.findings) {
        if (f.issue == CaptionIssue::TooFast) {
            ENJIN_EXPECT_TRUE(f.message.find("chars/s") != std::string::npos);
        }
    }
}

ENJIN_TEST(CaptionLint, AVeryBriefCueIsFlaggedEvenWhenItIsShort) {
    // Two characters in a tenth of a second passes any chars-per-second test and is
    // still unreadable: the eye needs time to find the text at all.
    std::vector<SrtCue> cues = { Cue(1.0f, 1.1f, "Hi") };
    const CaptionLintResult r = LintCaptionTrack(cues);

    ENJIN_ASSERT_TRUE(Has(r, CaptionIssue::TooFast));
    for (const CaptionFinding& f : r.findings) {
        if (f.issue == CaptionIssue::TooFast) {
            ENJIN_EXPECT_TRUE(f.message.find("floor") != std::string::npos);
        }
    }
}

ENJIN_TEST(CaptionLint, ALineBreakDoesNotCountAgainstReadingSpeed) {
    // A two-line caption is not harder to read for having a newline in it, and
    // counting the newline would make wrapping a caption a lint failure.
    std::vector<SrtCue> a = { Cue(0.0f, 2.0f, "one line of text here now") };
    std::vector<SrtCue> b = { Cue(0.0f, 2.0f, "one line of\ntext here now") };

    CaptionLintOptions tight;
    tight.maxCharsPerSecond = 12.5f;   // just above 25 chars / 2s

    ENJIN_EXPECT_FALSE(Has(LintCaptionTrack(a, tight), CaptionIssue::TooFast));
    ENJIN_EXPECT_FALSE(Has(LintCaptionTrack(b, tight), CaptionIssue::TooFast));
}

ENJIN_TEST(CaptionLint, ACuePastTheEndOfTheClipIsFound) {
    CaptionLintOptions opts;
    opts.clipLength = 10.0f;
    std::vector<SrtCue> cues = {
        Cue(2.0f, 6.0f, "fine"),
        Cue(8.0f, 14.0f, "hangs over the silence"),
    };
    const CaptionLintResult r = LintCaptionTrack(cues, opts);

    ENJIN_ASSERT_TRUE(Has(r, CaptionIssue::PastClipEnd));
    ENJIN_EXPECT_EQ(CountOf(r, CaptionIssue::PastClipEnd), static_cast<usize>(1));
}

ENJIN_TEST(CaptionLint, GapsAreFoundAtTheStartInTheMiddleAndAtTheEnd) {
    // The middle gap has a cue either side to hang off. The first and last do not,
    // and a linter that only checked between cues would miss a silent opening
    // minute and a silent closing one -- the two places captions most often stop.
    CaptionLintOptions opts;
    opts.clipLength = 100.0f;
    opts.longGapSeconds = 10.0f;

    std::vector<SrtCue> cues = {
        Cue(30.0f, 32.0f, "late start"),     // 30s of nothing before this
        Cue(60.0f, 62.0f, "after a hole"),   // 28s gap
    };                                       // 38s of nothing after this
    const CaptionLintResult r = LintCaptionTrack(cues, opts);

    ENJIN_EXPECT_EQ(CountOf(r, CaptionIssue::LongGap), static_cast<usize>(3));
    ENJIN_EXPECT_TRUE(r.Coverage() < 0.05f);
}

ENJIN_TEST(CaptionLint, AShortPauseIsNotAFinding) {
    // Silence between lines is how conversation works. A rule that fires on every
    // pause gets switched off, and then catches nothing.
    std::vector<SrtCue> cues = {
        Cue(0.0f, 2.0f, "one"),
        Cue(4.0f, 6.0f, "two"),     // 2s pause
        Cue(9.0f, 11.0f, "three"),  // 3s pause
    };
    const CaptionLintResult r = LintCaptionTrack(cues);
    ENJIN_EXPECT_FALSE(Has(r, CaptionIssue::LongGap));
}

// ---------------------------------------------------------------------------
// Coverage arithmetic
// ---------------------------------------------------------------------------

ENJIN_TEST(CaptionLint, OverlappingCuesDoNotPushCoveragePastOneHundredPercent) {
    // Counting overlaps twice would report 150% coverage, which reads as a broken
    // tool rather than as the overlap it actually is -- and the overlap is already
    // reported on its own.
    CaptionLintOptions opts;
    opts.clipLength = 10.0f;
    std::vector<SrtCue> cues = {
        Cue(0.0f, 10.0f, "the whole clip"),
        Cue(0.0f, 10.0f, "the whole clip again"),
    };
    const CaptionLintResult r = LintCaptionTrack(cues, opts);

    ENJIN_EXPECT_TRUE(r.captionedSeconds > 9.9f && r.captionedSeconds < 10.1f);
    ENJIN_EXPECT_TRUE(r.Coverage() <= 1.001f);
    ENJIN_EXPECT_TRUE(Has(r, CaptionIssue::Overlap));
}

ENJIN_TEST(CaptionLint, CoverageIsTheUnionOfTheWindowsNotTheirSum) {
    CaptionLintOptions opts;
    opts.clipLength = 20.0f;
    std::vector<SrtCue> cues = {
        Cue(0.0f, 6.0f, "a"),
        Cue(4.0f, 10.0f, "b"),    // overlaps a by 2s: union is 0..10
        Cue(14.0f, 16.0f, "c"),   // separate: +2s
    };
    const CaptionLintResult r = LintCaptionTrack(cues, opts);

    // 10 + 2 = 12, not 6 + 6 + 2 = 14.
    ENJIN_EXPECT_TRUE(r.captionedSeconds > 11.9f && r.captionedSeconds < 12.1f);
}

ENJIN_TEST(CaptionLint, AnEmptyTrackIsReportedAsEmptyNotAsClean) {
    CaptionLintOptions opts;
    opts.clipLength = 30.0f;
    const CaptionLintResult r = LintCaptionTrack({}, opts);

    ENJIN_EXPECT_EQ(r.cueCount, static_cast<usize>(0));
    ENJIN_EXPECT_TRUE(r.captionedSeconds < 0.001f);
    // Zero coverage of a known clip is a real measurement, and the honest one.
    ENJIN_EXPECT_TRUE(r.Coverage() < 0.001f && r.Coverage() >= 0.0f);
    ENJIN_EXPECT_TRUE(r.notRun.empty());
}

ENJIN_TEST(CaptionLint, FindingsComeBackInTimeOrder) {
    // The order an author walks them in. Grouping by rule would make them jump
    // around the timeline.
    CaptionLintOptions opts;
    opts.clipLength = 40.0f;
    std::vector<SrtCue> cues = {
        Cue(0.0f, 0.1f, "too brief"),          // TooFast at 0.0
        Cue(5.0f, 12.0f, ""),                  // Empty at 5.0
        Cue(11.0f, 20.0f, "overlaps"),         // Overlap reported at 12.0
        Cue(35.0f, 45.0f, "past the end"),     // PastClipEnd at 35.0
    };
    const CaptionLintResult r = LintCaptionTrack(cues, opts);

    ENJIN_ASSERT_TRUE(r.findings.size() >= 4);
    for (usize i = 1; i < r.findings.size(); ++i) {
        ENJIN_EXPECT_TRUE(r.findings[i].at >= r.findings[i - 1].at);
    }
}

// ---------------------------------------------------------------------------
// Reading a track back out of a loaded asset
// ---------------------------------------------------------------------------

ENJIN_TEST(CaptionLint, ALoadedTrackReadsBackIntoCues) {
    auto& registry = DataAssetRegistry::Get();
    registry.Clear();

    const SrtParseResult parsed = ParseSrt(
        "1\n00:00:01,000 --> 00:00:03,000\n<v HOST>Welcome.\n"
        "\n"
        "2\n00:00:04,000 --> 00:00:06,000\nAnd we are live.\n");
    registry.CreateAsset(BuildCaptionTrack("show", parsed));

    std::string problem = "unset";
    const std::vector<SrtCue> cues = ReadCaptionTrack("show", &problem);

    ENJIN_ASSERT_EQ(cues.size(), static_cast<usize>(2));
    ENJIN_EXPECT_TRUE(problem.empty());
    ENJIN_EXPECT_EQ(cues[0].speaker, std::string("HOST"));
    ENJIN_EXPECT_TRUE(cues[1].end > 5.99f);

    // And it lints clean against a clip that contains it.
    CaptionLintOptions opts;
    opts.clipLength = 7.0f;
    ENJIN_EXPECT_TRUE(LintCaptionTrack(cues, opts).Clean());
}

ENJIN_TEST(CaptionLint, AMissingAssetSaysSoRatherThanReturningAnEmptyTrack) {
    auto& registry = DataAssetRegistry::Get();
    registry.Clear();

    std::string problem;
    const std::vector<SrtCue> cues = ReadCaptionTrack("not_loaded", &problem);

    ENJIN_EXPECT_TRUE(cues.empty());
    ENJIN_ASSERT_TRUE(!problem.empty());
    ENJIN_EXPECT_TRUE(problem.find("not_loaded") != std::string::npos);
}

ENJIN_TEST(CaptionLint, ColumnsOfDifferentLengthsAreRejectedNotPaddedOut) {
    // The accessors fall back to "" and 0.0 for a read past the end, so a short
    // column would read as a track with blank captions at 0.0 seconds -- which
    // lints as a pile of overlaps and zero-length cues pointing nowhere near the
    // actual fault. Refusing it, and saying which column is short, is the fix.
    auto& registry = DataAssetRegistry::Get();
    registry.Clear();

    DataAsset asset;
    asset.name = "ragged";
    asset.schemaName = "CaptionTrack";
    asset.values["cue_t"] = std::vector<f32>{ 0.0f, 2.0f, 4.0f };
    asset.values["cue_end"] = std::vector<f32>{ 1.0f, 3.0f, 5.0f };
    asset.values["cue_who"] = std::vector<std::string>{ "", "" };        // short
    asset.values["cue_line"] = std::vector<std::string>{ "a", "b", "c" };
    registry.CreateAsset(asset);

    std::string problem;
    const std::vector<SrtCue> cues = ReadCaptionTrack("ragged", &problem);

    ENJIN_EXPECT_TRUE(cues.empty());
    ENJIN_ASSERT_TRUE(!problem.empty());
    ENJIN_EXPECT_TRUE(problem.find("cue_who 2") != std::string::npos);
}

ENJIN_TEST(CaptionLint, AnAssetThatIsNotACaptionTrackIsRejectedByName) {
    auto& registry = DataAssetRegistry::Get();
    registry.Clear();

    DataAsset asset;
    asset.name = "sword";
    asset.values["damage"] = 12.0f;
    registry.CreateAsset(asset);

    std::string problem;
    ENJIN_EXPECT_TRUE(ReadCaptionTrack("sword", &problem).empty());
    ENJIN_EXPECT_TRUE(problem.find("cue_t") != std::string::npos);
}

ENJIN_TEST_MAIN()
