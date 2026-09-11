#pragma once

// Turn a subtitle file into a caption track a script can read.
//
// .srt is what every captioning tool exports and what every transcription service
// hands back, and the engine could not read one. The authoring path for timed
// captions was therefore: open the .srt, read the timecodes off the screen, and
// retype each one as a float into a .enjdata by hand. For a ten-minute radio
// broadcast that is several hundred numbers typed twice (start and end), and a
// typo in any of them shows up as one line arriving early and nothing explaining
// why.
//
// The output is the parallel-array shape DataAsset already round-trips and
// DataAsset_GetFloatAt / DataAsset_GetStringAt already read:
//
//     cue_t    FloatArray    start of each cue, seconds
//     cue_end  FloatArray    end of each cue, seconds
//     cue_who  StringArray   speaker, "" when the file does not say
//     cue_line StringArray   the caption text
//
// cue_end is carried even though a player can usually infer the end from the next
// start, because inferring it is wrong wherever the file has a GAP -- and silence
// between two lines is the normal case in a conversation. Dropping a column the
// source file actually contains is data loss, and the inferred version holds the
// previous caption on screen through the pause.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Assets/DataAsset.h"

#include <string>
#include <vector>

namespace Enjin {
namespace Assets {

struct SrtCue {
    f32 start = 0.0f;          // seconds
    f32 end = 0.0f;            // seconds
    std::string speaker;       // "" when the file does not name one
    std::string text;          // multi-line cues keep their newlines
};

// What the parse actually did.
//
// Returned rather than logged, for the same reason DataAssetScanResult is: an
// import that produced 40 cues out of a 60-cue file and one that produced 40 out
// of 40 are otherwise the same success, and the difference is twenty missing
// captions nobody is looking for.
struct SrtParseResult {
    std::vector<SrtCue> cues;
    usize blocksSeen = 0;          // subtitle blocks the file appeared to contain
    usize cuesWithColonPrefix = 0; // how many lines start "NAME: ..." (see options)
    std::vector<std::string> problems;  // one line each, with the source line number

    bool Ok() const { return !cues.empty() && problems.empty(); }
};

struct SrtImportOptions {
    // Split a leading "NAME: " off the caption text into the speaker column.
    //
    // OFF by default, and that is the whole point. "WGRB: Night desk." and
    // "Look: over there." are the same shape, so splitting on a colon rewrites
    // some captions and not others with no way to tell which. Instead the parse
    // COUNTS the lines that look like a speaker prefix and reports it, so the
    // author is told the option exists and can decide for their own file.
    bool splitColonSpeaker = false;

    // Cap, so a malformed or hostile file cannot allocate without bound. Ten
    // thousand cues is about nine hours of dialogue.
    usize maxCues = 10000;
};

// Parse SubRip text. Accepts both "," and "." as the millisecond separator, a
// missing millisecond field, and CRLF or LF. Never throws.
ENJIN_API SrtParseResult ParseSrt(const std::string& text,
                                  const SrtImportOptions& options = {});

// The schema the generated assets declare, so the Data Asset panel shows named
// fields of the right types rather than four untyped rows.
ENJIN_API DataAssetSchema CaptionTrackSchema();

// Build the asset. `name` is what a script passes as the asset name to
// DataAsset_GetArrayLength and friends.
ENJIN_API DataAsset BuildCaptionTrack(const std::string& name,
                                      const SrtParseResult& parsed);

// Read `srtPath`, write `outPath`. Returns false and fills `error` on failure;
// `result` receives the parse report either way, so a caller can show the problem
// lines alongside a partial success.
ENJIN_API bool ImportSrtFile(const std::string& srtPath,
                             const std::string& outPath,
                             const SrtImportOptions& options,
                             SrtParseResult* result,
                             std::string* error);

} // namespace Assets
} // namespace Enjin
