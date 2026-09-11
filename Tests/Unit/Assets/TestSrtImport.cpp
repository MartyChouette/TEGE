// Reading a .srt so nobody retypes timecodes as floats.
//
// Every captioning tool and every transcription service emits SubRip, and the
// engine could not read one, so the authoring path for timed captions was to read
// the timecodes off the screen and retype each as a float into a .enjdata. A
// ten-minute broadcast is several hundred numbers typed twice, and a typo in any
// of them surfaces as one line arriving early with nothing to explain it.
//
// The properties these tests pin, which are the ones worth arguing about:
//
//   cue_end is carried, not inferred. A player can usually guess a cue's end from
//   the next cue's start, and that guess is wrong wherever the file has a gap --
//   which is every pause in a conversation. The inferred version holds the
//   previous caption on screen through the silence.
//
//   A "NAME:" prefix is NOT split by default. "WGRB: Night desk." and "Look: over
//   there." are the same shape, so splitting on a colon rewrites some captions and
//   not others with no way to tell which. The parse counts them and reports the
//   count instead, so the author is told the option exists.
//
//   All four arrays are always written, always the same length. A player walks
//   them by index off one GetArrayLength, so a track that omitted cue_who because
//   no cue had a speaker would turn "nobody named" into a failed lookup.
//
//   A malformed block is reported and skipped, not fatal. One bad block in a
//   200-cue file still yields a usable track, and staying silent about it is how
//   it stays bad.
#include "EnjinTest.h"
#include "Enjin/Assets/SrtImport.h"
#include "Enjin/Assets/DataAsset.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace Enjin;
using namespace Enjin::Assets;
namespace fs = std::filesystem;

namespace {

// A hand-written track with the things that actually show up in real files: a
// gap between cues, a two-line cue, a speaker tag, and an empty caption.
const char* kTrack =
    "1\n"
    "00:00:00,500 --> 00:00:04,000\n"
    "<v WGRB>Night desk.\n"
    "\n"
    "2\n"
    "00:00:08,000 --> 00:00:11,500\n"
    "[CALLER] You there?\n"
    "\n"
    "3\n"
    "00:00:11,500 --> 00:00:14,000\n"
    "Go ahead,\n"
    "I'm listening.\n"
    "\n";

fs::path TempDir() {
    fs::path dir = fs::temp_directory_path() / "enjin_srt_test";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

fs::path WriteTemp(const char* leaf, const std::string& contents) {
    const fs::path p = TempDir() / leaf;
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << contents;
    return p;
}

bool Near(f32 a, f32 b) { return (a - b) < 0.002f && (b - a) < 0.002f; }

} // namespace

ENJIN_TEST(SrtImport, TimecodesBecomeSeconds) {
    // Arrange / act
    const SrtParseResult r = ParseSrt(kTrack);

    // Assert
    ENJIN_ASSERT_EQ(r.cues.size(), static_cast<usize>(3));
    ENJIN_EXPECT_EQ(r.blocksSeen, static_cast<usize>(3));
    ENJIN_EXPECT_TRUE(r.problems.empty());

    ENJIN_EXPECT_TRUE(Near(r.cues[0].start, 0.5f));
    ENJIN_EXPECT_TRUE(Near(r.cues[0].end, 4.0f));
    ENJIN_EXPECT_TRUE(Near(r.cues[1].start, 8.0f));
    ENJIN_EXPECT_TRUE(Near(r.cues[2].end, 14.0f));
}

ENJIN_TEST(SrtImport, TheGapBetweenCuesSurvives) {
    // The reason cue_end exists. Cue 1 ends at 4.0 and cue 2 starts at 8.0: four
    // seconds of silence. A track that only stored start times would put cue 1 on
    // screen for eight seconds, because "ends when the next one starts" is the only
    // rule available to it.
    const SrtParseResult r = ParseSrt(kTrack);
    ENJIN_ASSERT_EQ(r.cues.size(), static_cast<usize>(3));

    ENJIN_EXPECT_TRUE(Near(r.cues[0].end, 4.0f));
    ENJIN_EXPECT_TRUE(Near(r.cues[1].start, 8.0f));
    ENJIN_EXPECT_TRUE(r.cues[1].start - r.cues[0].end > 3.9f);
}

ENJIN_TEST(SrtImport, SpeakerComesFromTagsNotFromGuessing) {
    const SrtParseResult r = ParseSrt(kTrack);
    ENJIN_ASSERT_EQ(r.cues.size(), static_cast<usize>(3));

    // Both forms are real notations, so reading them is not a guess.
    ENJIN_EXPECT_EQ(r.cues[0].speaker, std::string("WGRB"));
    ENJIN_EXPECT_EQ(r.cues[1].speaker, std::string("CALLER"));
    // And the tag is removed from the text, not left in it.
    ENJIN_EXPECT_EQ(r.cues[0].text, std::string("Night desk."));
    ENJIN_EXPECT_EQ(r.cues[1].text, std::string("You there?"));

    // A cue with no tag has no speaker, rather than inheriting the previous one.
    ENJIN_EXPECT_EQ(r.cues[2].speaker, std::string(""));
}

ENJIN_TEST(SrtImport, AColonPrefixIsCountedNotSplit) {
    // The plausible-wrong-answer case. Two cues, same shape, different meaning.
    const char* ambiguous =
        "1\n00:00:01,000 --> 00:00:03,000\nWGRB: Night desk.\n"
        "\n"
        "2\n00:00:03,000 --> 00:00:05,000\nLook: over there.\n";

    const SrtParseResult off = ParseSrt(ambiguous);
    ENJIN_ASSERT_EQ(off.cues.size(), static_cast<usize>(2));

    // Default: nothing is rewritten, and the author is told how many lines look
    // like a speaker prefix so they can decide for their own file.
    ENJIN_EXPECT_EQ(off.cues[0].speaker, std::string(""));
    ENJIN_EXPECT_EQ(off.cues[0].text, std::string("WGRB: Night desk."));
    ENJIN_EXPECT_EQ(off.cuesWithColonPrefix, static_cast<usize>(2));

    // Opted in: both split, including the one that should not have. That is the
    // author's call to make with their file in front of them, which is exactly why
    // it is not the engine's call to make silently.
    SrtImportOptions opts;
    opts.splitColonSpeaker = true;
    const SrtParseResult on = ParseSrt(ambiguous, opts);
    ENJIN_ASSERT_EQ(on.cues.size(), static_cast<usize>(2));
    ENJIN_EXPECT_EQ(on.cues[0].speaker, std::string("WGRB"));
    ENJIN_EXPECT_EQ(on.cues[0].text, std::string("Night desk."));
}

ENJIN_TEST(SrtImport, AMultiLineCueKeepsItsLineBreak) {
    const SrtParseResult r = ParseSrt(kTrack);
    ENJIN_ASSERT_EQ(r.cues.size(), static_cast<usize>(3));
    ENJIN_EXPECT_EQ(r.cues[2].text, std::string("Go ahead,\nI'm listening."));
}

ENJIN_TEST(SrtImport, CommaAndDotAndMissingMillisAllParse) {
    // Three spellings of the same instant, all produced by tools in the wild.
    const char* variants =
        "1\n00:00:01,500 --> 00:00:02,000\na\n"
        "\n"
        "2\n00:00:03.250 --> 00:00:04,000\nb\n"
        "\n"
        "3\n00:00:05 --> 00:00:06\nc\n"
        "\n"
        "4\n01:02,500 --> 01:03,000\nd\n";     // MM:SS,mmm, no hour field

    const SrtParseResult r = ParseSrt(variants);
    ENJIN_ASSERT_EQ(r.cues.size(), static_cast<usize>(4));
    ENJIN_EXPECT_TRUE(Near(r.cues[0].start, 1.5f));
    ENJIN_EXPECT_TRUE(Near(r.cues[1].start, 3.25f));
    ENJIN_EXPECT_TRUE(Near(r.cues[2].start, 5.0f));
    ENJIN_EXPECT_TRUE(Near(r.cues[3].start, 62.5f));
}

ENJIN_TEST(SrtImport, AByteOrderMarkDoesNotEatTheFirstCaption) {
    // Notepad and several captioning tools write a UTF-8 BOM. Left in place it
    // makes the first block's index line unparseable, and the first caption is the
    // one an author checks -- so losing it reads as "the importer is broken".
    const std::string withBom = std::string("\xEF\xBB\xBF") +
        "1\n00:00:00,500 --> 00:00:02,000\nFirst line.\n";

    const SrtParseResult r = ParseSrt(withBom);
    ENJIN_ASSERT_EQ(r.cues.size(), static_cast<usize>(1));
    ENJIN_EXPECT_EQ(r.cues[0].text, std::string("First line."));
    ENJIN_EXPECT_TRUE(r.problems.empty());
}

ENJIN_TEST(SrtImport, AnIndexLineIsOptional) {
    // Not every writer emits the numbering.
    const char* noIndex =
        "00:00:01,000 --> 00:00:02,000\nalpha\n"
        "\n"
        "00:00:02,000 --> 00:00:03,000\nbeta\n";
    const SrtParseResult r = ParseSrt(noIndex);
    ENJIN_ASSERT_EQ(r.cues.size(), static_cast<usize>(2));
    ENJIN_EXPECT_EQ(r.cues[1].text, std::string("beta"));
}

ENJIN_TEST(SrtImport, TrailingCueSettingsDoNotBreakTheTiming) {
    // WebVTT-flavoured files append positioning after the end timecode. A SubRip
    // reader ignores it; failing on it would reject the whole file.
    const char* withSettings =
        "1\n00:00:01,000 --> 00:00:02,000 align:middle line:90%\nplaced\n";
    const SrtParseResult r = ParseSrt(withSettings);
    ENJIN_ASSERT_EQ(r.cues.size(), static_cast<usize>(1));
    ENJIN_EXPECT_TRUE(Near(r.cues[0].end, 2.0f));
}

ENJIN_TEST(SrtImport, ABadBlockIsReportedAndSkippedNotFatal) {
    // One malformed block in the middle. The cues either side must survive, and
    // the problem must be named with its line number -- a silently shorter track
    // is the failure this whole importer exists to avoid.
    const char* broken =
        "1\n00:00:01,000 --> 00:00:02,000\nkeep me\n"
        "\n"
        "2\nnot a timing line at all\nlost\n"
        "\n"
        "3\n00:00:05,000 --> 00:00:06,000\nkeep me too\n";

    const SrtParseResult r = ParseSrt(broken);
    ENJIN_ASSERT_EQ(r.cues.size(), static_cast<usize>(2));
    ENJIN_EXPECT_EQ(r.cues[0].text, std::string("keep me"));
    ENJIN_EXPECT_EQ(r.cues[1].text, std::string("keep me too"));

    ENJIN_ASSERT_EQ(r.problems.size(), static_cast<usize>(1));
    ENJIN_EXPECT_TRUE(r.problems[0].find("line 6") != std::string::npos);
    ENJIN_EXPECT_FALSE(r.Ok());   // usable, but not clean
    // blocksSeen counts what the file appeared to contain, so the caller can say
    // "2 of 3" rather than reporting an unqualified success.
    ENJIN_EXPECT_EQ(r.blocksSeen, static_cast<usize>(3));
}

ENJIN_TEST(SrtImport, ACueThatEndsBeforeItStartsIsRejected) {
    const char* backwards =
        "1\n00:00:09,000 --> 00:00:02,000\nimpossible\n";
    const SrtParseResult r = ParseSrt(backwards);
    ENJIN_EXPECT_TRUE(r.cues.empty());
    ENJIN_ASSERT_EQ(r.problems.size(), static_cast<usize>(1));
    ENJIN_EXPECT_TRUE(r.problems[0].find("before it starts") != std::string::npos);
}

ENJIN_TEST(SrtImport, TheCueLimitStopsRatherThanAllocatingForever) {
    std::string many;
    for (int i = 0; i < 40; ++i) {
        many += std::to_string(i + 1) + "\n00:00:0" + std::to_string(i % 10) +
                ",000 --> 00:00:09,000\nline\n\n";
    }
    SrtImportOptions opts;
    opts.maxCues = 10;
    const SrtParseResult r = ParseSrt(many, opts);
    ENJIN_EXPECT_EQ(r.cues.size(), static_cast<usize>(10));
    ENJIN_ASSERT_TRUE(!r.problems.empty());
    ENJIN_EXPECT_TRUE(r.problems.back().find("limit") != std::string::npos);
}

ENJIN_TEST(SrtImport, TheBuiltAssetHasFourEqualLengthArrays) {
    const SrtParseResult r = ParseSrt(kTrack);
    const DataAsset asset = BuildCaptionTrack("wgrb_night", r);

    ENJIN_EXPECT_EQ(asset.name, std::string("wgrb_night"));
    ENJIN_EXPECT_EQ(asset.schemaName, std::string("CaptionTrack"));

    // All four present, all the same length, even though no cue in this track has
    // an empty speaker problem -- a player indexes them together.
    for (const char* key : { "cue_t", "cue_end", "cue_who", "cue_line" }) {
        ENJIN_ASSERT_TRUE(asset.values.count(key) == 1);
    }
    ENJIN_EXPECT_EQ(std::get<std::vector<f32>>(asset.values.at("cue_t")).size(),
                    static_cast<usize>(3));
    ENJIN_EXPECT_EQ(std::get<std::vector<f32>>(asset.values.at("cue_end")).size(),
                    static_cast<usize>(3));
    ENJIN_EXPECT_EQ(std::get<std::vector<std::string>>(asset.values.at("cue_who")).size(),
                    static_cast<usize>(3));
    ENJIN_EXPECT_EQ(std::get<std::vector<std::string>>(asset.values.at("cue_line")).size(),
                    static_cast<usize>(3));
}

ENJIN_TEST(SrtImport, AnEmptySpeakerColumnIsStillWritten) {
    // A track where nobody is named. cue_who must exist and be full of empty
    // strings, not absent: absent turns "nobody named" into a failed lookup that
    // warns once per read, which is noise pointing at nothing wrong.
    const char* anonymous = "1\n00:00:01,000 --> 00:00:02,000\njust text\n";
    const DataAsset asset = BuildCaptionTrack("anon", ParseSrt(anonymous));

    ENJIN_ASSERT_TRUE(asset.values.count("cue_who") == 1);
    const auto& who = std::get<std::vector<std::string>>(asset.values.at("cue_who"));
    ENJIN_ASSERT_EQ(who.size(), static_cast<usize>(1));
    ENJIN_EXPECT_EQ(who[0], std::string(""));
}

ENJIN_TEST(SrtImport, AnImportedFileRoundTripsThroughTheRegistry) {
    // End to end, the way a caption player will actually reach it: import writes
    // a .enjdata, the registry loads it, and the script accessors read it back.
    auto& registry = DataAssetRegistry::Get();
    registry.Clear();

    const fs::path src = WriteTemp("roundtrip.srt", kTrack);
    const fs::path out = TempDir() / "wgrb_night.enjdata";
    std::error_code ec;
    fs::remove(out, ec);

    SrtParseResult parsed;
    std::string error;
    ENJIN_ASSERT_TRUE(ImportSrtFile(src.string(), out.string(), {}, &parsed, &error));
    ENJIN_EXPECT_TRUE(error.empty());
    ENJIN_ASSERT_TRUE(fs::exists(out));

    // Reload from disk into a clean registry, so this tests the FILE and not the
    // in-memory asset the import happened to leave behind.
    registry.Clear();
    ENJIN_ASSERT_TRUE(registry.LoadAsset(out.string()));

    ENJIN_EXPECT_EQ(registry.GetArrayLength("wgrb_night", "cue_t"),
                    static_cast<usize>(3));
    ENJIN_EXPECT_TRUE(registry.GetFloatAt("wgrb_night", "cue_t", 1) > 7.99f);
    ENJIN_EXPECT_TRUE(registry.GetFloatAt("wgrb_night", "cue_end", 0) > 3.99f);
    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_who", 1),
                    std::string("CALLER"));
    ENJIN_EXPECT_EQ(registry.GetStringAt("wgrb_night", "cue_line", 0),
                    std::string("Night desk."));
}

ENJIN_TEST(SrtImport, ImportingAFileWithNoCuesFailsLoudly) {
    auto& registry = DataAssetRegistry::Get();
    registry.Clear();

    const fs::path src = WriteTemp("prose.srt", "this is not a subtitle file at all\n");
    const fs::path out = TempDir() / "prose.enjdata";
    std::error_code ec;
    fs::remove(out, ec);

    SrtParseResult parsed;
    std::string error;
    ENJIN_EXPECT_FALSE(ImportSrtFile(src.string(), out.string(), {}, &parsed, &error));
    ENJIN_EXPECT_TRUE(!error.empty());
    // And it must not leave a valid-looking empty track behind for a script to
    // read zero cues out of.
    ENJIN_EXPECT_FALSE(fs::exists(out));
}

ENJIN_TEST(SrtImport, TheSchemaNamesEveryFieldTheImportWrites) {
    // The Data Asset panel renders a schema. One that omitted a field would show a
    // generated track with rows the editor cannot name or edit.
    const DataAssetSchema schema = CaptionTrackSchema();
    ENJIN_EXPECT_EQ(schema.name, std::string("CaptionTrack"));
    ENJIN_ASSERT_EQ(schema.fields.size(), static_cast<usize>(4));

    const DataAsset asset = BuildCaptionTrack("x", ParseSrt(kTrack));
    for (const DataAssetField& f : schema.fields) {
        ENJIN_EXPECT_TRUE(asset.values.count(f.name) == 1);
    }
    ENJIN_EXPECT_EQ(asset.values.size(), schema.fields.size());
}

ENJIN_TEST_MAIN()
