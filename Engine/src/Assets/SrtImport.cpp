#include "Enjin/Assets/SrtImport.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace Enjin {
namespace Assets {

namespace {

std::string Trim(const std::string& in) {
    const auto first = in.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return in.substr(first, in.find_last_not_of(" \t\r\n") - first + 1);
}

// "00:01:23,456" -> 83.456. Also accepts "." for the separator, a missing
// millisecond field, and a missing hour field (some tools emit MM:SS,mmm).
bool ParseTimecode(const std::string& raw, f32& outSeconds) {
    const std::string s = Trim(raw);
    if (s.empty()) return false;

    // Split on ':' into 2 or 3 parts; the last part may carry ",mmm" or ".mmm".
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == ':') { parts.push_back(cur); cur.clear(); }
        else          { cur.push_back(c); }
    }
    parts.push_back(cur);
    if (parts.size() < 2 || parts.size() > 3) return false;

    std::string last = parts.back();
    f64 millis = 0.0;
    const auto sep = last.find_first_of(",.");
    if (sep != std::string::npos) {
        std::string ms = last.substr(sep + 1);
        last = last.substr(0, sep);
        if (ms.empty() || ms.size() > 6) return false;
        for (char c : ms) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        // Scale by however many digits there are: "5" is 500ms, not 5ms.
        f64 value = 0.0;
        for (char c : ms) value = value * 10.0 + (c - '0');
        f64 scale = 1.0;
        for (usize i = 0; i < ms.size(); ++i) scale *= 10.0;
        millis = value / scale;
    }
    parts.back() = last;

    f64 seconds = 0.0;
    for (const std::string& p : parts) {
        const std::string t = Trim(p);
        if (t.empty() || t.size() > 4) return false;
        for (char c : t) if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        f64 v = 0.0;
        for (char c : t) v = v * 10.0 + (c - '0');
        seconds = seconds * 60.0 + v;
    }

    outSeconds = static_cast<f32>(seconds + millis);
    return true;
}

// "00:00:00,500 --> 00:00:04,000" (with optional trailing cue settings, which
// WebVTT-style files append and SubRip readers ignore).
bool ParseTimingLine(const std::string& line, f32& start, f32& end) {
    const auto arrow = line.find("-->");
    if (arrow == std::string::npos) return false;

    const std::string lhs = line.substr(0, arrow);
    std::string rhs = line.substr(arrow + 3);

    // Stop the right-hand timecode at the first space after it, so trailing
    // "align:middle line:90%" settings do not make the parse fail.
    const std::string rhsTrimmed = Trim(rhs);
    const auto space = rhsTrimmed.find_first_of(" \t");
    rhs = (space == std::string::npos) ? rhsTrimmed : rhsTrimmed.substr(0, space);

    return ParseTimecode(lhs, start) && ParseTimecode(rhs, end);
}

// WebVTT voice tag: "<v Speaker>text" or "<v.loud Speaker>text". A real standard,
// so reading it is not a guess.
bool ExtractVoiceTag(std::string& text, std::string& speaker) {
    const std::string t = Trim(text);
    if (t.rfind("<v", 0) != 0) return false;
    const auto close = t.find('>');
    if (close == std::string::npos) return false;

    std::string inner = t.substr(2, close - 2);      // ".loud Speaker" or " Speaker"
    const auto space = inner.find(' ');
    if (space == std::string::npos) return false;    // "<v>" with no name
    speaker = Trim(inner.substr(space + 1));
    if (speaker.empty()) return false;

    text = Trim(t.substr(close + 1));
    return true;
}

// "[Speaker] text" -- the other unambiguous form, because brackets are not
// punctuation that appears mid-sentence the way a colon is.
bool ExtractBracketSpeaker(std::string& text, std::string& speaker) {
    const std::string t = Trim(text);
    if (t.empty() || t[0] != '[') return false;
    const auto close = t.find(']');
    if (close == std::string::npos) return false;
    speaker = Trim(t.substr(1, close - 1));
    if (speaker.empty()) return false;
    text = Trim(t.substr(close + 1));
    return true;
}

// Does the line open with something that COULD be a speaker prefix? Used only to
// count, and to split when the caller has opted in. Deliberately narrow: a single
// run of letters, digits, spaces, dots and apostrophes before a colon, no more
// than 24 characters, and no sentence-ending punctuation in it.
bool LooksLikeColonSpeaker(const std::string& text, usize& colonAt) {
    const auto colon = text.find(':');
    if (colon == std::string::npos || colon == 0 || colon > 24) return false;
    for (usize i = 0; i < colon; ++i) {
        const char c = text[i];
        const bool allowed = std::isalnum(static_cast<unsigned char>(c)) ||
                             c == ' ' || c == '.' || c == '\'' || c == '-' || c == '_';
        if (!allowed) return false;
    }
    colonAt = colon;
    return true;
}

} // namespace

SrtParseResult ParseSrt(const std::string& text, const SrtImportOptions& options) {
    SrtParseResult out;

    // Split into lines, keeping a 1-based number for every message.
    std::vector<std::string> lines;
    {
        std::istringstream in(text);
        std::string line;
        while (std::getline(in, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            lines.push_back(line);
        }
    }

    // Strip a UTF-8 BOM, which Notepad and several captioning tools write and
    // which otherwise makes the first block's index line unparseable -- and the
    // first caption is the one an author checks, so losing it reads as "the
    // importer does not work" rather than "the file starts with three bytes".
    if (!lines.empty() && lines[0].rfind("\xEF\xBB\xBF", 0) == 0) {
        lines[0] = lines[0].substr(3);
    }

    usize i = 0;
    const usize n = lines.size();
    while (i < n) {
        // Skip blank lines between blocks.
        while (i < n && Trim(lines[i]).empty()) ++i;
        if (i >= n) break;

        const usize blockLine = i + 1;   // 1-based, for messages
        ++out.blocksSeen;

        // An optional index line, then the timing line. Some files omit the
        // index; accept either order of appearance by looking for the arrow.
        if (Trim(lines[i]).find("-->") == std::string::npos) {
            ++i;   // that was the index line
        }
        if (i >= n) {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "line %zu: block ends before its timing line", blockLine);
            out.problems.push_back(buf);
            break;
        }

        SrtCue cue;
        if (!ParseTimingLine(lines[i], cue.start, cue.end)) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                          "line %zu: not a timing line: \"%.80s\"",
                          i + 1, Trim(lines[i]).c_str());
            out.problems.push_back(buf);
            // Skip to the next blank line so one bad block does not derail the
            // rest of the file.
            while (i < n && !Trim(lines[i]).empty()) ++i;
            continue;
        }
        ++i;

        // Text runs to the next blank line.
        std::vector<std::string> body;
        while (i < n && !Trim(lines[i]).empty()) {
            body.push_back(lines[i]);
            ++i;
        }
        if (body.empty()) {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "line %zu: cue has no text", blockLine);
            out.problems.push_back(buf);
            continue;
        }

        // A speaker tag only counts on the FIRST line of the cue.
        if (!ExtractVoiceTag(body[0], cue.speaker)) {
            ExtractBracketSpeaker(body[0], cue.speaker);
        }

        std::string joined;
        for (usize b = 0; b < body.size(); ++b) {
            if (b) joined += "\n";
            joined += Trim(body[b]);
        }
        joined = Trim(joined);

        if (cue.speaker.empty()) {
            usize colonAt = 0;
            if (LooksLikeColonSpeaker(joined, colonAt)) {
                ++out.cuesWithColonPrefix;
                if (options.splitColonSpeaker) {
                    cue.speaker = Trim(joined.substr(0, colonAt));
                    joined = Trim(joined.substr(colonAt + 1));
                }
            }
        }

        cue.text = joined;

        if (cue.end < cue.start) {
            char buf[200];
            std::snprintf(buf, sizeof(buf),
                          "line %zu: cue ends (%.3f) before it starts (%.3f)",
                          blockLine, cue.end, cue.start);
            out.problems.push_back(buf);
            continue;
        }

        if (out.cues.size() >= options.maxCues) {
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "line %zu: stopped at the %zu-cue limit", blockLine,
                          options.maxCues);
            out.problems.push_back(buf);
            break;
        }
        out.cues.push_back(std::move(cue));
    }

    return out;
}

DataAssetSchema CaptionTrackSchema() {
    DataAssetSchema schema;
    schema.name = "CaptionTrack";
    schema.description =
        "Timed captions as parallel arrays: cue_t/cue_end are seconds, cue_who is "
        "the speaker (empty when the source did not name one), cue_line is the "
        "text. Read with DataAsset_GetArrayLength + DataAsset_GetFloatAt / "
        "DataAsset_GetStringAt.";
    schema.fields = {
        { "cue_t",    DataFieldType::FloatArray,  std::vector<f32>{} },
        { "cue_end",  DataFieldType::FloatArray,  std::vector<f32>{} },
        { "cue_who",  DataFieldType::StringArray, std::vector<std::string>{} },
        { "cue_line", DataFieldType::StringArray, std::vector<std::string>{} },
    };
    return schema;
}

DataAsset BuildCaptionTrack(const std::string& name, const SrtParseResult& parsed) {
    DataAsset asset;
    asset.name = name;
    asset.schemaName = "CaptionTrack";

    std::vector<f32> starts, ends;
    std::vector<std::string> who, lines;
    starts.reserve(parsed.cues.size());
    ends.reserve(parsed.cues.size());
    who.reserve(parsed.cues.size());
    lines.reserve(parsed.cues.size());

    for (const SrtCue& cue : parsed.cues) {
        starts.push_back(cue.start);
        ends.push_back(cue.end);
        who.push_back(cue.speaker);
        lines.push_back(cue.text);
    }

    // All four arrays are written even when empty, and all four are the same
    // length. A caption player walks them by index off one GetArrayLength, so a
    // track that omitted cue_who because no cue had a speaker would read the
    // speaker of every line as a failed lookup instead of as "nobody named".
    asset.values["cue_t"] = starts;
    asset.values["cue_end"] = ends;
    asset.values["cue_who"] = who;
    asset.values["cue_line"] = lines;
    return asset;
}

bool ImportSrtFile(const std::string& srtPath, const std::string& outPath,
                   const SrtImportOptions& options, SrtParseResult* result,
                   std::string* error) {
    auto fail = [&](const std::string& msg) {
        if (error) *error = msg;
        ENJIN_LOG_ERROR(Assets, "SRT import failed: %s", msg.c_str());
        return false;
    };

    std::ifstream file(srtPath, std::ios::binary);
    if (!file) return fail("cannot open " + srtPath);

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();
    if (text.empty()) return fail(srtPath + " is empty");

    const SrtParseResult parsed = ParseSrt(text, options);
    if (result) *result = parsed;

    if (parsed.cues.empty()) {
        std::string msg = "no cues found in " + srtPath;
        if (!parsed.problems.empty()) msg += " (" + parsed.problems.front() + ")";
        return fail(msg);
    }

    // Asset name = output file stem, which is what a script will pass.
    std::string stem = outPath;
    const auto slash = stem.find_last_of("/\\");
    if (slash != std::string::npos) stem = stem.substr(slash + 1);
    const auto dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem = stem.substr(0, dot);

    auto& registry = DataAssetRegistry::Get();
    registry.RegisterSchema(CaptionTrackSchema());

    const DataAsset asset = BuildCaptionTrack(stem, parsed);
    if (!registry.SaveAsset(asset, outPath)) {
        return fail("cannot write " + outPath);
    }

    ENJIN_LOG_INFO(Assets, "Imported %zu cues from %s to %s (%zu blocks seen)",
                   parsed.cues.size(), srtPath.c_str(), outPath.c_str(),
                   parsed.blocksSeen);
    // Problems are not fatal once at least one cue survived: a file with one bad
    // block still yields a usable track, and saying nothing about the bad block
    // is how it stays bad.
    for (const std::string& p : parsed.problems) {
        ENJIN_LOG_WARN(Assets, "SRT %s: %s", srtPath.c_str(), p.c_str());
    }
    if (error) error->clear();
    return true;
}

} // namespace Assets
} // namespace Enjin
