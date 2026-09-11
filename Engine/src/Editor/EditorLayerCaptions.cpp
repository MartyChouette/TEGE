// The Caption Track panel: a timeline, a coverage figure, and the lint findings.
//
// A caption track is a table of numbers whose faults only exist at runtime, in one
// place, for a moment. An overlap puts two lines on screen at once; a cue that
// outlives the audio hangs over silence; a stretch with nothing in it is invisible
// in a list and obvious as a hole. Reading the numbers cannot show you any of that,
// and scrubbing the whole clip to find it is the reason nobody checks.
//
// So: draw the cues against time, mark what the linter found where it is, and put
// the number of seconds that have no caption at the top. Clicking a finding moves
// the playhead to it.
//
// ImGui rules this file obeys, each of which has cost a session before:
//   - Everything here runs in the panel-drawing phase. EditorLayer::Update happens
//     BEFORE ImGui::NewFrame, and drawing from there dereferences a null font.
//   - Colours are authored in sRGB and converted to linear before packing into
//     IM_COL32, because the swapchain is B8G8R8A8_SRGB and the hardware converts
//     again. Picking darker constants instead would hide the transfer error in the
//     numbers.
//   - io.FontGlobalScale does not reach ImDrawList::AddText, so every hand-drawn
//     string passes an explicit size scaled by hand.

#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Assets/CaptionLint.h"
#include "Enjin/Assets/DataAsset.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace Enjin {
namespace Editor {

namespace {

// sRGB -> linear, then pack. Same reasoning as Authored() in
// EditorLayerCreative.cpp: ImGui writes vertex colours straight through and the
// hardware encodes them a second time, so an authored #1b212b samples on screen as
// RGB(91,101,114) unless it is converted here.
f32 SrgbToLinear(f32 c) {
    return (c <= 0.04045f) ? (c / 12.92f)
                           : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

ImU32 Authored(u8 r, u8 g, u8 b, u8 a = 255) {
    return IM_COL32(static_cast<int>(SrgbToLinear(r / 255.0f) * 255.0f + 0.5f),
                    static_cast<int>(SrgbToLinear(g / 255.0f) * 255.0f + 0.5f),
                    static_cast<int>(SrgbToLinear(b / 255.0f) * 255.0f + 0.5f),
                    a);
}

std::string FormatTime(f32 seconds) {
    if (seconds < 0.0f) seconds = 0.0f;
    const int total = static_cast<int>(seconds);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d:%02d.%02d", total / 60, total % 60,
                  static_cast<int>((seconds - total) * 100.0f));
    return buf;
}

} // namespace

void EditorLayer::DrawCaptionTrackPanel() {
    bool open = true;
    ImGui::Begin("Caption Track", &open);
    if (!open) {
        SetPanelVisibility(EditorPanel::CaptionTrack, false);
        ImGui::End();
        return;
    }

    const f32 s = ImGui::GetIO().FontGlobalScale;
    auto& registry = Assets::DataAssetRegistry::Get();

    // --- Which track -------------------------------------------------------
    //
    // Only assets that actually have a cue_t array are offered. Listing every data
    // asset and failing on the ones that are not caption tracks would make the
    // picker a guessing game.
    std::vector<std::string> tracks;
    for (const Assets::DataAsset* asset : registry.GetAllAssets()) {
        if (!asset) continue;
        if (registry.GetArrayLength(asset->name, "cue_t") > 0) {
            tracks.push_back(asset->name);
        }
    }
    std::sort(tracks.begin(), tracks.end());

    if (tracks.empty()) {
        // Three-state emptiness, not one: nothing loaded at all is a different
        // situation from data assets present but none of them a caption track, and
        // the fix is different too.
        const usize assetCount = registry.GetAllAssets().size();
        if (assetCount == 0) {
            ImGui::TextWrapped("No data assets are loaded.");
            ImGui::Spacing();
            ImGui::TextWrapped(
                "Drop a .srt on this window, or use Tools > Scripting & Logic > "
                "Import Captions, to make one.");
        } else {
            ImGui::TextWrapped("%zu data asset(s) are loaded, and none of them is a "
                               "caption track.", assetCount);
            ImGui::Spacing();
            ImGui::TextWrapped(
                "A caption track has a cue_t array. Import a .srt to build one.");
        }
        ImGui::End();
        return;
    }

    if (m_CaptionTrackName.empty() ||
        std::find(tracks.begin(), tracks.end(), m_CaptionTrackName) == tracks.end()) {
        m_CaptionTrackName = tracks.front();
    }

    if (ImGui::BeginCombo("Track", m_CaptionTrackName.c_str())) {
        for (const std::string& name : tracks) {
            const bool selected = (name == m_CaptionTrackName);
            if (ImGui::Selectable(name.c_str(), selected)) {
                m_CaptionTrackName = name;
                m_CaptionPlayhead = 0.0f;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    std::string problem;
    const std::vector<Assets::SrtCue> cues =
        Assets::ReadCaptionTrack(m_CaptionTrackName, &problem);
    if (cues.empty()) {
        ImGui::TextColored(ImVec4(0.95f, 0.55f, 0.35f, 1.0f), "%s", problem.c_str());
        ImGui::End();
        return;
    }

    // --- How long the audio is --------------------------------------------
    //
    // Two of the lint rules need it and there is no way to derive it, so it is
    // asked for rather than assumed. Deriving it from the last cue's end would make
    // coverage read near 100% on every track, including one that stops captioning
    // halfway through -- which is the case the number exists to catch. Leaving it
    // at 0 is allowed, and the panel then names the checks that did not run.
    ImGui::DragFloat("Clip length (s)", &m_CaptionClipLength, 0.1f, 0.0f, 100000.0f,
                     "%.2f");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "How long the audio this track captions is.\n"
            "Leave it at 0 and two checks are listed as not run:\n"
            "cues past the end of the clip, and coverage.");
    }

    Assets::CaptionLintOptions options;
    options.clipLength = m_CaptionClipLength;
    const Assets::CaptionLintResult lint = Assets::LintCaptionTrack(cues, options);

    // --- The headline numbers ---------------------------------------------
    const f32 span = (m_CaptionClipLength > 0.0f)
                   ? m_CaptionClipLength
                   : std::max(0.001f, cues.back().end);

    ImGui::Separator();
    ImGui::Text("%zu cues, %s captioned of %s",
                lint.cueCount,
                FormatTime(lint.captionedSeconds).c_str(),
                FormatTime(span).c_str());

    if (lint.Coverage() >= 0.0f) {
        ImGui::SameLine();
        const f32 pct = lint.Coverage() * 100.0f;
        const ImVec4 colour = (pct >= 80.0f) ? ImVec4(0.55f, 0.85f, 0.60f, 1.0f)
                            : (pct >= 50.0f) ? ImVec4(0.90f, 0.80f, 0.45f, 1.0f)
                                             : ImVec4(0.95f, 0.55f, 0.35f, 1.0f);
        ImGui::TextColored(colour, "(%.0f%%)", pct);
    } else {
        ImGui::SameLine();
        ImGui::TextDisabled("(coverage needs the clip length)");
    }

    // --- The timeline -----------------------------------------------------
    const f32 rowHeight = 22.0f * s;
    const f32 timelineHeight = rowHeight * 2.0f + 26.0f * s;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const f32 width = std::max(120.0f * s, ImGui::GetContentRegionAvail().x);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImU32 kGround = Authored(0x14, 0x18, 0x1f);
    const ImU32 kRail = Authored(0x0f, 0x13, 0x1a);
    const ImU32 kCue = Authored(0x3f, 0x7a, 0x9e);
    const ImU32 kCueSpoken = Authored(0x4d, 0x93, 0xbd);
    const ImU32 kFault = Authored(0xd0, 0x6b, 0x3f);
    const ImU32 kGap = Authored(0x2a, 0x22, 0x1c);
    const ImU32 kPlayhead = Authored(0x1a, 0xff, 0xcc);   // electric teal, the house probe colour
    const ImU32 kTick = Authored(0x4a, 0x52, 0x5e);
    const ImU32 kText = Authored(0xc8, 0xcf, 0xd8);

    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + timelineHeight),
                        kGround, 3.0f * s);

    auto xAt = [&](f32 seconds) {
        return origin.x + (seconds / span) * width;
    };

    // Second ticks, at whatever spacing keeps them readable.
    {
        f32 step = 1.0f;
        const f32 minPixels = 60.0f * s;
        while ((step / span) * width < minPixels) step *= (step < 10.0f ? 5.0f : 2.0f);
        for (f32 t = 0.0f; t <= span; t += step) {
            const f32 x = xAt(t);
            draw->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + timelineHeight),
                          kTick, 1.0f);
            draw->AddText(nullptr, 11.0f * s, ImVec2(x + 3.0f * s,
                          origin.y + timelineHeight - 15.0f * s), kText,
                          FormatTime(t).c_str());
        }
    }

    // Gaps first, underneath, so a hole reads as a hole rather than as absence of
    // drawing -- an empty stretch and a stretch the panel failed to draw look the
    // same otherwise.
    {
        f32 covered = 0.0f;
        std::vector<std::pair<f32, f32>> windows;
        for (const Assets::SrtCue& cue : cues) {
            if (cue.end > cue.start) windows.emplace_back(cue.start, cue.end);
        }
        std::sort(windows.begin(), windows.end());
        for (const auto& w : windows) {
            if (w.first > covered) {
                draw->AddRectFilled(ImVec2(xAt(covered), origin.y + 2.0f * s),
                                    ImVec2(xAt(w.first), origin.y + rowHeight * 2.0f),
                                    kGap);
            }
            covered = std::max(covered, w.second);
        }
        if (covered < span) {
            draw->AddRectFilled(ImVec2(xAt(covered), origin.y + 2.0f * s),
                                ImVec2(xAt(span), origin.y + rowHeight * 2.0f), kGap);
        }
    }

    // Cues. Alternating rows so an overlap is visible as two stacked blocks rather
    // than one wider one.
    int hovered = -1;
    for (usize i = 0; i < cues.size(); ++i) {
        const Assets::SrtCue& cue = cues[i];
        const f32 x0 = xAt(std::min(cue.start, span));
        const f32 x1 = std::max(x0 + 2.0f, xAt(std::min(cue.end, span)));
        const f32 y0 = origin.y + 3.0f * s + (i % 2) * rowHeight;
        const f32 y1 = y0 + rowHeight - 4.0f * s;

        const bool faulty = [&] {
            for (const Assets::CaptionFinding& f : lint.findings) {
                if (f.cueIndex == i) return true;
            }
            return false;
        }();

        draw->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1),
                            faulty ? kFault : (cue.speaker.empty() ? kCue : kCueSpoken),
                            2.0f * s);

        const ImVec2 mouse = ImGui::GetIO().MousePos;
        if (mouse.x >= x0 && mouse.x <= x1 && mouse.y >= y0 && mouse.y <= y1) {
            hovered = static_cast<int>(i);
        }
    }

    // Playhead last, over everything.
    if (m_CaptionPlayhead > 0.0f) {
        const f32 x = xAt(std::min(m_CaptionPlayhead, span));
        draw->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + timelineHeight),
                      kPlayhead, 2.0f * s);
    }
    draw->AddRect(origin, ImVec2(origin.x + width, origin.y + timelineHeight), kRail,
                  3.0f * s);

    // An invisible button over the whole strip, so clicking scrubs and the window
    // does not start dragging instead.
    ImGui::InvisibleButton("##captiontimeline", ImVec2(width, timelineHeight));
    if (ImGui::IsItemActive() || ImGui::IsItemClicked()) {
        const f32 local = (ImGui::GetIO().MousePos.x - origin.x) / width;
        m_CaptionPlayhead = std::max(0.0f, std::min(1.0f, local)) * span;
    }

    if (hovered >= 0 && ImGui::IsItemHovered()) {
        const Assets::SrtCue& cue = cues[static_cast<usize>(hovered)];
        ImGui::BeginTooltip();
        ImGui::Text("Cue %d  %s -> %s  (%.2fs)", hovered,
                    FormatTime(cue.start).c_str(), FormatTime(cue.end).c_str(),
                    cue.end - cue.start);
        if (!cue.speaker.empty()) ImGui::TextDisabled("%s", cue.speaker.c_str());
        ImGui::PushTextWrapPos(360.0f * s);
        ImGui::TextWrapped("%s", cue.text.empty() ? "(no text)" : cue.text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    // --- What is on screen at the playhead --------------------------------
    ImGui::Spacing();
    {
        bool any = false;
        for (const Assets::SrtCue& cue : cues) {
            if (m_CaptionPlayhead >= cue.start && m_CaptionPlayhead < cue.end) {
                if (!any) ImGui::Text("At %s:", FormatTime(m_CaptionPlayhead).c_str());
                any = true;
                ImGui::BulletText("%s%s",
                                  cue.speaker.empty() ? "" : (cue.speaker + ": ").c_str(),
                                  cue.text.empty() ? "(no text)" : cue.text.c_str());
            }
        }
        if (!any) {
            ImGui::TextDisabled("At %s: no caption",
                                FormatTime(m_CaptionPlayhead).c_str());
        }
    }

    // --- Findings ---------------------------------------------------------
    ImGui::Separator();
    if (!lint.notRun.empty()) {
        // Said before the findings, not after. "No problems found" under a list of
        // checks that did not run is how an unchecked track becomes a checked one
        // in the author's head.
        ImGui::TextColored(ImVec4(0.90f, 0.80f, 0.45f, 1.0f), "Not checked:");
        for (const std::string& why : lint.notRun) {
            ImGui::Bullet();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextWrapped("%s", why.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::Spacing();
    }

    if (lint.findings.empty()) {
        if (lint.notRun.empty()) {
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.60f, 1.0f),
                               "Every check passed.");
        } else {
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.60f, 1.0f),
                               "The checks that ran all passed.");
        }
    } else {
        ImGui::Text("%zu finding(s):", lint.findings.size());
        if (ImGui::BeginChild("##captionfindings", ImVec2(0, 0), true)) {
            for (usize i = 0; i < lint.findings.size(); ++i) {
                const Assets::CaptionFinding& f = lint.findings[i];
                ImGui::PushID(static_cast<int>(i));
                const bool isGap = (f.issue == Assets::CaptionIssue::LongGap);
                ImGui::TextColored(isGap ? ImVec4(0.90f, 0.80f, 0.45f, 1.0f)
                                         : ImVec4(0.95f, 0.55f, 0.35f, 1.0f),
                                   "%s", Assets::CaptionIssueName(f.issue));
                ImGui::SameLine();
                if (ImGui::SmallButton(FormatTime(f.at).c_str())) {
                    m_CaptionPlayhead = f.at;
                }
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextWrapped("%s", f.message.c_str());
                ImGui::PopTextWrapPos();
                ImGui::Separator();
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

} // namespace Editor
} // namespace Enjin
