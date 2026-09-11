#include "Enjin/Assets/CaptionLint.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
#include <cstdio>
#include <cstdarg>

namespace Enjin {
namespace Assets {

const char* CaptionIssueName(CaptionIssue issue) {
    switch (issue) {
        case CaptionIssue::Overlap:     return "Overlap";
        case CaptionIssue::OutOfOrder:  return "Out of order";
        case CaptionIssue::ZeroLength:  return "Zero length";
        case CaptionIssue::Empty:       return "Empty";
        case CaptionIssue::TooFast:     return "Too fast to read";
        case CaptionIssue::PastClipEnd: return "Past the end of the clip";
        case CaptionIssue::LongGap:     return "Long gap";
    }
    return "Unknown";
}

namespace {

// Characters a reader has to get through. Counts the visible text, so a line break
// does not inflate the number.
usize ReadableLength(const std::string& text) {
    usize count = 0;
    for (char c : text) {
        if (c != '\n' && c != '\r') ++count;
    }
    return count;
}

CaptionFinding Make(CaptionIssue issue, usize index, f32 at, const char* fmt, ...) {
    CaptionFinding f;
    f.issue = issue;
    f.cueIndex = index;
    f.at = at;

    char buf[320];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    f.message = buf;
    return f;
}

} // namespace

CaptionLintResult LintCaptionTrack(const std::vector<SrtCue>& cues,
                                   const CaptionLintOptions& options) {
    CaptionLintResult out;
    out.cueCount = cues.size();
    out.clipLength = options.clipLength;

    if (options.clipLength <= 0.0f) {
        // Named, not silent. These are the two rules an author most wants and they
        // are exactly the two that need to know how long the audio is.
        out.notRun.push_back(
            "Past the end of the clip: needs the audio length. Select the entity "
            "playing this track, or type the length in.");
        out.notRun.push_back(
            "Coverage: needs the audio length, and measuring it against the last "
            "cue instead would always read near 100% and mean nothing.");
    }

    for (usize i = 0; i < cues.size(); ++i) {
        const SrtCue& cue = cues[i];
        const f32 duration = cue.end - cue.start;

        if (ReadableLength(cue.text) == 0) {
            out.findings.push_back(Make(CaptionIssue::Empty, i, cue.start,
                "cue %zu at %.2fs has no text", i, cue.start));
            // Still worth checking its timing, so no continue.
        }

        if (duration <= 0.0f) {
            out.findings.push_back(Make(CaptionIssue::ZeroLength, i, cue.start,
                "cue %zu at %.2fs is %.3fs long, so it never appears",
                i, cue.start, duration));
        } else {
            const usize chars = ReadableLength(cue.text);
            const f32 cps = static_cast<f32>(chars) / duration;
            if (duration < options.minCueSeconds && chars > 0) {
                out.findings.push_back(Make(CaptionIssue::TooFast, i, cue.start,
                    "cue %zu is up for %.2fs, under the %.2fs floor -- too brief to "
                    "find on screen whatever it says",
                    i, duration, options.minCueSeconds));
            } else if (cps > options.maxCharsPerSecond) {
                out.findings.push_back(Make(CaptionIssue::TooFast, i, cue.start,
                    "cue %zu needs %.0f chars/s (%zu chars in %.2fs), over the %.0f "
                    "chars/s ceiling",
                    i, cps, chars, duration, options.maxCharsPerSecond));
            }
        }

        if (i > 0) {
            const SrtCue& prev = cues[i - 1];
            if (cue.start < prev.start) {
                out.findings.push_back(Make(CaptionIssue::OutOfOrder, i, cue.start,
                    "cue %zu starts at %.2fs, before cue %zu at %.2fs",
                    i, cue.start, i - 1, prev.start));
            } else if (cue.start < prev.end) {
                out.findings.push_back(Make(CaptionIssue::Overlap, i - 1, prev.end,
                    "cue %zu is still up at %.2fs when cue %zu starts at %.2fs "
                    "(%.2fs of both on screen)",
                    i - 1, prev.end, i, cue.start, prev.end - cue.start));
            } else if (cue.start - prev.end > options.longGapSeconds) {
                out.findings.push_back(Make(CaptionIssue::LongGap, i - 1, prev.end,
                    "%.1fs with no caption, between cue %zu ending at %.2fs and cue "
                    "%zu starting at %.2fs",
                    cue.start - prev.end, i - 1, prev.end, i, cue.start));
            }
        }

        if (options.clipLength > 0.0f && cue.end > options.clipLength) {
            out.findings.push_back(Make(CaptionIssue::PastClipEnd, i, cue.start,
                "cue %zu runs to %.2fs, past the %.2fs clip",
                i, cue.end, options.clipLength));
        }
    }

    // Captioned time is the UNION of the windows: counting overlaps twice could
    // report more than 100% coverage, which reads as a bug in the tool rather than
    // as the overlap it actually is.
    {
        std::vector<std::pair<f32, f32>> windows;
        windows.reserve(cues.size());
        for (const SrtCue& cue : cues) {
            if (cue.end > cue.start) windows.emplace_back(cue.start, cue.end);
        }
        std::sort(windows.begin(), windows.end());

        f32 total = 0.0f;
        f32 openStart = 0.0f, openEnd = -1.0f;
        for (const auto& w : windows) {
            if (openEnd < 0.0f) {
                openStart = w.first;
                openEnd = w.second;
            } else if (w.first <= openEnd) {
                openEnd = std::max(openEnd, w.second);
            } else {
                total += openEnd - openStart;
                openStart = w.first;
                openEnd = w.second;
            }
        }
        if (openEnd >= 0.0f) total += openEnd - openStart;
        out.captionedSeconds = total;
    }

    // A gap at the START of the clip is as much a hole as one in the middle, and it
    // has no preceding cue to hang off, so it is checked separately.
    if (!cues.empty() && cues.front().start > options.longGapSeconds) {
        out.findings.push_back(Make(CaptionIssue::LongGap, 0, 0.0f,
            "%.1fs with no caption before the first cue at %.2fs",
            cues.front().start, cues.front().start));
    }
    // And so is one at the end, which only the clip length can reveal.
    if (options.clipLength > 0.0f && !cues.empty()) {
        const f32 trailing = options.clipLength - cues.back().end;
        if (trailing > options.longGapSeconds) {
            out.findings.push_back(Make(CaptionIssue::LongGap, cues.size() - 1,
                cues.back().end,
                "%.1fs with no caption after the last cue ends at %.2fs "
                "(clip is %.2fs)",
                trailing, cues.back().end, options.clipLength));
        }
    }

    // Ordered by time, because that is the order an author will walk them in.
    std::stable_sort(out.findings.begin(), out.findings.end(),
                     [](const CaptionFinding& a, const CaptionFinding& b) {
                         return a.at < b.at;
                     });
    return out;
}

std::vector<SrtCue> ReadCaptionTrack(const std::string& assetName,
                                     std::string* problem) {
    auto setProblem = [&](const std::string& msg) {
        if (problem) *problem = msg;
        return std::vector<SrtCue>{};
    };

    auto& registry = DataAssetRegistry::Get();
    if (!registry.FindAsset(assetName)) {
        return setProblem("no data asset named '" + assetName + "' is loaded");
    }

    const usize starts = registry.GetArrayLength(assetName, "cue_t");
    if (starts == 0) {
        return setProblem("'" + assetName + "' has no cue_t array, so it is not a "
                          "caption track");
    }

    // Every column must be the same length. A shorter one would silently read as
    // empty strings or 0.0 from the accessors' fallbacks, which is how a truncated
    // track looks identical to a complete one with blank lines in it.
    const usize ends = registry.GetArrayLength(assetName, "cue_end");
    const usize who  = registry.GetArrayLength(assetName, "cue_who");
    const usize line = registry.GetArrayLength(assetName, "cue_line");
    if (ends != starts || who != starts || line != starts) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "'%s' columns disagree: cue_t %zu, cue_end %zu, cue_who %zu, "
                      "cue_line %zu", assetName.c_str(), starts, ends, who, line);
        return setProblem(buf);
    }

    std::vector<SrtCue> cues;
    cues.reserve(starts);
    for (usize i = 0; i < starts; ++i) {
        SrtCue cue;
        cue.start = registry.GetFloatAt(assetName, "cue_t", i);
        cue.end = registry.GetFloatAt(assetName, "cue_end", i);
        cue.speaker = registry.GetStringAt(assetName, "cue_who", i);
        cue.text = registry.GetStringAt(assetName, "cue_line", i);
        cues.push_back(std::move(cue));
    }
    if (problem) problem->clear();
    return cues;
}

} // namespace Assets
} // namespace Enjin
