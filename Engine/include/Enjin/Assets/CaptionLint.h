#pragma once

// Check a caption track against the clip it captions.
//
// A caption track is data that is only wrong at RUNTIME, in one place, for a
// moment: a cue that overlaps the next one puts two lines on screen at once, a cue
// that outlives the audio leaves a line hanging over silence, and a cue nobody can
// read in the time it is up is technically present and practically absent. None of
// those is visible in a table of numbers, and all of them are obvious on a
// timeline -- which is why this ships with one.
//
// The rule that shapes the whole thing: when the clip length is unknown, the
// checks that need it are reported as NOT RUN, by name. A linter that quietly
// skips half its rules and prints "no problems found" is worse than no linter,
// because it converts an unchecked track into a checked one in the author's head.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Assets/SrtImport.h"

#include <string>
#include <vector>

namespace Enjin {
namespace Assets {

enum class CaptionIssue : u8 {
    Overlap,        // this cue is still up when the next one starts
    OutOfOrder,     // this cue starts before the one before it
    ZeroLength,     // end == start, so it is never on screen
    Empty,          // no text
    TooFast,        // more characters than a reader gets through in that time
    PastClipEnd,    // still on screen after the audio has finished
    LongGap,        // a stretch with no caption at all -- worth a look, not wrong
};

ENJIN_API const char* CaptionIssueName(CaptionIssue issue);

struct CaptionFinding {
    CaptionIssue issue = CaptionIssue::Overlap;
    usize cueIndex = 0;         // the cue it is about; for LongGap, the cue BEFORE the gap
    f32 at = 0.0f;              // seconds, where to look
    std::string message;        // one line, with the numbers in it
};

struct CaptionLintOptions {
    // How long the audio is, in seconds. 0 means "not known", and the checks that
    // need it are then listed in notRun rather than silently skipped.
    f32 clipLength = 0.0f;

    // Reading speed ceiling. 20 characters per second is around the upper end of
    // published subtitle guidance (the BBC's is 160-180 words per minute, roughly
    // 15-17 cps; Netflix allows 20). The default is the permissive end on purpose:
    // this is a lint, and a rule that fires on a third of a normal track gets
    // switched off and then catches nothing.
    f32 maxCharsPerSecond = 20.0f;

    // A caption shorter than this is hard to read whatever its length, because the
    // eye needs time to find it. Below it, TooFast is reported regardless of
    // character count.
    f32 minCueSeconds = 0.5f;

    // Silence longer than this is reported as a LongGap. Not an error: a pause in
    // a conversation is normal, and so is music. It is the one rule that most
    // often finds captions that were never written, so it is worth surfacing.
    f32 longGapSeconds = 10.0f;
};

// What the lint could and could not check.
struct CaptionLintResult {
    std::vector<CaptionFinding> findings;

    // Checks that were not run, and why. Empty means every rule ran.
    std::vector<std::string> notRun;

    usize cueCount = 0;
    f32 captionedSeconds = 0.0f;   // union of the cue windows, overlaps counted once
    f32 clipLength = 0.0f;         // as supplied; 0 = unknown

    // Fraction of the clip with a caption on screen, 0..1. -1 when the clip length
    // is unknown -- a coverage number computed against the last cue's end instead
    // would always be close to 1.0 and would mean nothing.
    f32 Coverage() const {
        if (clipLength <= 0.0f) return -1.0f;
        return captionedSeconds / clipLength;
    }

    bool Clean() const { return findings.empty(); }
};

ENJIN_API CaptionLintResult LintCaptionTrack(const std::vector<SrtCue>& cues,
                                            const CaptionLintOptions& options = {});

// Read the four parallel arrays back out of a loaded asset so a panel can lint
// what is actually in the project rather than only what an import just parsed.
// Returns an empty vector when the asset is missing or the arrays disagree in
// length; `problem` says which.
ENJIN_API std::vector<SrtCue> ReadCaptionTrack(const std::string& assetName,
                                               std::string* problem = nullptr);

} // namespace Assets
} // namespace Enjin
