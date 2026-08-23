// Shared animated TEGE splash. Extracted from the editor so built games can
// show the same intro (the optional per-build "Made with TEGE" card).
#include "Enjin/GUI/EngineSplash.h"
#include "Enjin/Core/Version.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace Enjin {
namespace GUI {

// Per-art-style splash palette: same composition, restyled. Indices match the
// Art Style Preset dropdown (SceneRenderSettings artStylePreset 0-6).
namespace {
struct C3 { int r, g, b; };
struct SplashPalette {
    C3 bg;         // opaque backdrop
    C3 structure;  // wireframe hexagons / rings / cool geometry
    C3 accent;     // glows, mantra, flares (the "sage" family in the default)
    C3 title;      // the TEGE wordmark
    C3 spark;      // bright particles / paper tones
};
const SplashPalette kSplashPalettes[7] = {
    // 0 Realistic PBR - the original steel-and-sage look
    { {13,13,20},  {100,140,180}, {160,200,160}, {199,218,196}, {240,240,230} },
    // 1 Classic Blinn-Phong - warm brass and amber
    { {20,15,10},  {180,140,90},  {220,180,120}, {235,215,180}, {255,245,220} },
    // 2 Hand-Painted - canvas and terracotta
    { {24,18,16},  {170,120,100}, {200,160,120}, {230,205,180}, {250,240,225} },
    // 3 Toon / Anime - ink night with a bold pop
    { {10,10,14},  {90,110,220},  {255,90,140},  {250,250,255}, {255,255,255} },
    // 4 Low-Poly Retro - PS1 dusk grays
    { {14,16,22},  {110,120,150}, {130,180,190}, {200,210,220}, {230,235,240} },
    // 5 Pixel Art - console palette greens and golds
    { {8,8,12},    {80,200,120},  {240,160,60},  {240,240,220}, {255,255,255} },
    // 6 NPR Sketch - graphite and warm paper
    { {18,17,15},  {150,150,140}, {200,195,180}, {235,230,215}, {250,248,240} },
};
} // namespace

void DrawEngineSplash(f32 timeSeconds, f32 duration, f32 fadeStart, const char* creditLine,
                      ImFont* titleFont) {
    DrawEngineSplash(timeSeconds, duration, fadeStart, creditLine, titleFont, SplashOptions{});
}

void DrawEngineSplash(f32 timeSeconds, f32 duration, f32 fadeStart, const char* creditLine,
                      ImFont* titleFont, const SplashOptions& opts) {
    const SplashPalette& P = kSplashPalettes[opts.artStylePreset < 7u ? opts.artStylePreset : 0u];
    const bool reduced = opts.reducedMotion;
    ImGuiIO& io = ImGui::GetIO();
    const f32 t = timeSeconds;

    // --- Utility lambdas ---
    auto smoothstep = [](f32 x) -> f32 { x = std::clamp(x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); };
    auto easeOutCubic = [](f32 x) -> f32 { x = std::clamp(x, 0.0f, 1.0f); f32 inv = 1.0f - x; return 1.0f - inv * inv * inv; };
    auto timerRamp = [&](f32 start, f32 end) -> f32 {
        if (t <= start) return 0.0f;
        if (t >= end) return 1.0f;
        return (t - start) / (end - start);
    };
    auto hashFloat = [](u32 seed) -> f32 {
        seed = seed * 2654435761u;
        seed ^= seed >> 16;
        seed *= 0x45d9f3bu;
        seed ^= seed >> 16;
        return static_cast<f32>(seed & 0xFFFFu) / 65535.0f;
    };

    // Global fade: 0-0.3 fade-in, 3.0-4.0 fade-out
    f32 globalAlpha = 1.0f;
    if (t < 0.3f) globalAlpha = smoothstep(t / 0.3f);
    else if (t > fadeStart) {
        f32 fadeProgress = (t - fadeStart) / (duration - fadeStart);
        globalAlpha = 1.0f - smoothstep(fadeProgress);
    }
    f32 ga = globalAlpha;

    // Full-screen overlay
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::SetNextWindowBgAlpha(0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    if (ImGui::Begin("##Splash", nullptr, flags)) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        ImFont* tfont = titleFont ? titleFont : font;  // "TEGE" title = Playfair when provided
        f32 W = io.DisplaySize.x;
        f32 H = io.DisplaySize.y;
        ImVec2 center(W * 0.5f, H * 0.5f);

        // ========== LAYER 1: Background ==========

        // 1a. Solid dark fill — stays fully opaque even during fade-out
        //     so the 3D scene never shows through
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H),
            IM_COL32(P.bg.r, P.bg.g, P.bg.b, 255));

        // 1b. Radial vignette with subtle breathing
        f32 breath = reduced ? 1.0f : 1.0f + 0.03f * std::sin(t * 0.8f);
        for (int ring = 0; ring < 3; ring++) {
            f32 radius = (0.35f + ring * 0.15f) * std::min(W, H) * breath;
            int vigAlpha = static_cast<int>((20 - ring * 6) * ga);
            if (vigAlpha > 0) {
                dl->AddCircle(center, radius,
                    IM_COL32(P.structure.r, P.structure.g, P.structure.b, vigAlpha), 64, 1.5f);
            }
        }

        // 1c. 12 wireframe hexagons — slowly rotating, drifting upward
        // (decorative motion: skipped for reduced-motion players)
        for (int h = 0; h < (reduced ? 0 : 12); h++) {
            f32 hx = hashFloat(h * 7 + 1) * W;
            f32 hy = hashFloat(h * 7 + 2) * H;
            hy = std::fmod(hy - t * (15.0f + hashFloat(h * 7 + 3) * 10.0f), H + 100.0f);
            if (hy < -50.0f) hy += H + 100.0f;
            f32 hexR = 20.0f + hashFloat(h * 7 + 4) * 30.0f;
            f32 rot = t * (0.2f + hashFloat(h * 7 + 5) * 0.3f);
            int hexAlpha = static_cast<int>(15 * ga);
            if (hexAlpha > 0) {
                ImVec2 pts[6];
                for (int v = 0; v < 6; v++) {
                    f32 angle = rot + v * 3.14159265f / 3.0f;
                    pts[v] = ImVec2(hx + std::cos(angle) * hexR, hy + std::sin(angle) * hexR);
                }
                for (int v = 0; v < 6; v++) {
                    dl->AddLine(pts[v], pts[(v + 1) % 6],
                        IM_COL32(P.structure.r, P.structure.g, P.structure.b, hexAlpha), 1.0f);
                }
            }
        }

        // ========== LAYER 2: Floating Light Orbs (24 total) ==========

        auto orbColor = [&](int idx) -> ImVec4 {
            f32 pick = hashFloat(idx * 13 + 100);
            if (pick < 0.4f) return ImVec4((f32)P.accent.r, (f32)P.accent.g, (f32)P.accent.b, 1);
            if (pick < 0.8f) return ImVec4((f32)P.structure.r, (f32)P.structure.g, (f32)P.structure.b, 1);
            return ImVec4((f32)P.spark.r, (f32)P.spark.g, (f32)P.spark.b, 1);
        };

        for (int i = 0; i < (reduced ? 0 : 24); i++) {
            bool foreground = (i >= 16);
            f32 baseRadius = foreground ? (5.0f + hashFloat(i * 11 + 50) * 9.0f)
                                        : (3.0f + hashFloat(i * 11 + 50) * 5.0f);
            f32 brightness = foreground ? 1.0f : 0.4f;
            f32 speed = foreground ? (0.6f + hashFloat(i * 11 + 51) * 0.4f)
                                   : (0.3f + hashFloat(i * 11 + 51) * 0.3f);

            f32 phaseX = hashFloat(i * 11 + 52) * 6.28f;
            f32 phaseY = hashFloat(i * 11 + 53) * 6.28f;
            f32 ampX = 0.15f + hashFloat(i * 11 + 54) * 0.25f;
            f32 ampY = 0.1f + hashFloat(i * 11 + 55) * 0.2f;
            f32 freqRatioX = 1.0f + hashFloat(i * 11 + 56) * 2.0f;
            f32 freqRatioY = 1.0f + hashFloat(i * 11 + 57) * 1.5f;

            f32 ox = center.x + std::sin(t * speed * freqRatioX + phaseX) * W * ampX;
            f32 oy = center.y + std::cos(t * speed * freqRatioY + phaseY) * H * ampY;

            if (foreground) {
                f32 converge = smoothstep(timerRamp(0.5f, 1.5f));
                f32 disperse = smoothstep(timerRamp(2.0f, 3.0f));
                f32 pull = converge * (1.0f - disperse);
                ox = ox + (center.x - ox) * pull * 0.6f;
                oy = oy + (center.y - oy) * pull * 0.6f;
            }

            f32 orbAlpha = foreground ? smoothstep(timerRamp(0.3f, 0.8f)) : 1.0f;

            ImVec4 col = orbColor(i);
            f32 r = baseRadius * (0.9f + 0.1f * std::sin(t * 2.0f + i));

            int a3 = static_cast<int>(20 * brightness * orbAlpha * ga);
            int a2 = static_cast<int>(50 * brightness * orbAlpha * ga);
            int a1 = static_cast<int>(180 * brightness * orbAlpha * ga);
            if (a3 > 0) dl->AddCircleFilled(ImVec2(ox, oy), r * 3.0f,
                IM_COL32(static_cast<int>(col.x), static_cast<int>(col.y), static_cast<int>(col.z), a3), 16);
            if (a2 > 0) dl->AddCircleFilled(ImVec2(ox, oy), r * 1.8f,
                IM_COL32(static_cast<int>(col.x), static_cast<int>(col.y), static_cast<int>(col.z), a2), 16);
            if (a1 > 0) dl->AddCircleFilled(ImVec2(ox, oy), r,
                IM_COL32(static_cast<int>(col.x + (255 - col.x) * 0.3f),
                         static_cast<int>(col.y + (255 - col.y) * 0.3f),
                         static_cast<int>(col.z + (255 - col.z) * 0.3f), a1), 16);
        }

        // ========== LAYER 3: Geometric Shapes ==========

        // 3a. 4 diamonds in compass formation
        {
            f32 diamondScale = easeOutCubic(timerRamp(0.4f, 1.0f));
            f32 diamondDrift = smoothstep(timerRamp(1.8f, 3.0f)) * 40.0f;
            f32 diamondR = 12.0f * diamondScale;
            int dAlpha = static_cast<int>(120 * diamondScale * ga);
            if (dAlpha > 0) {
                f32 dist = 120.0f + diamondDrift;
                ImVec2 dPos[4] = {
                    ImVec2(center.x, center.y - dist),
                    ImVec2(center.x + dist, center.y),
                    ImVec2(center.x, center.y + dist),
                    ImVec2(center.x - dist, center.y)
                };
                for (int d = 0; d < 4; d++) {
                    ImVec2 dp[4] = {
                        ImVec2(dPos[d].x, dPos[d].y - diamondR),
                        ImVec2(dPos[d].x + diamondR, dPos[d].y),
                        ImVec2(dPos[d].x, dPos[d].y + diamondR),
                        ImVec2(dPos[d].x - diamondR, dPos[d].y)
                    };
                    dl->AddConvexPolyFilled(dp, 4,
                        IM_COL32(P.accent.r, P.accent.g, P.accent.b, dAlpha / 3));
                    dl->AddPolyline(dp, 4, IM_COL32(P.accent.r, P.accent.g, P.accent.b, dAlpha), ImDrawFlags_Closed, 1.5f);
                }
            }
        }

        // 3b. 6 small triangles orbiting center
        {
            f32 triAlphaF = smoothstep(timerRamp(0.6f, 1.0f));
            int triAlpha = static_cast<int>(90 * triAlphaF * ga);
            if (triAlpha > 0) {
                f32 orbitR = 180.0f;
                for (int ti = 0; ti < 6; ti++) {
                    f32 orbitAngle = t * 0.4f + ti * 3.14159265f / 3.0f;
                    f32 tx = center.x + std::cos(orbitAngle) * orbitR;
                    f32 ty = center.y + std::sin(orbitAngle) * orbitR;
                    f32 selfRot = t * 1.5f + ti * 1.0f;
                    f32 triR = 8.0f;
                    ImVec2 tp[3];
                    for (int v = 0; v < 3; v++) {
                        f32 a = selfRot + v * 2.0944f;
                        tp[v] = ImVec2(tx + std::cos(a) * triR, ty + std::sin(a) * triR);
                    }
                    dl->AddConvexPolyFilled(tp, 3,
                        IM_COL32(P.accent.r, P.accent.g, P.accent.b, triAlpha / 2));
                    dl->AddPolyline(tp, 3, IM_COL32(P.accent.r, P.accent.g, P.accent.b, triAlpha), ImDrawFlags_Closed, 1.0f);
                }
            }
        }

        // 3c. 2 pulsing concentric rings
        {
            f32 ringAlphaF = smoothstep(timerRamp(0.5f, 1.0f));
            f32 pulse1 = 200.0f + 5.0f * std::sin(t * 1.2f);
            f32 pulse2 = 250.0f + 5.0f * std::sin(t * 1.2f + 1.5f);
            int ringA = static_cast<int>(40 * ringAlphaF * ga);
            if (ringA > 0) {
                if (!reduced) dl->AddCircle(center, pulse1, IM_COL32(P.structure.r, P.structure.g, P.structure.b, ringA), 64, 1.0f);
                if (!reduced) dl->AddCircle(center, pulse2, IM_COL32(P.structure.r, P.structure.g, P.structure.b, ringA / 2), 64, 1.0f);
            }
        }

        // 3d. 4 corner L-bracket lines
        {
            f32 bracketT = easeOutCubic(timerRamp(0.8f, 1.5f));
            int bAlpha = static_cast<int>(80 * bracketT * ga);
            if (bAlpha > 0) {
                f32 margin = 60.0f;
                f32 bLen = 40.0f;
                f32 slideOff = (1.0f - bracketT) * 80.0f;
                u32 bCol = IM_COL32(P.structure.r, P.structure.g, P.structure.b, bAlpha);
                dl->AddLine(ImVec2(margin - slideOff, margin - slideOff),
                            ImVec2(margin - slideOff + bLen, margin - slideOff), bCol, 1.5f);
                dl->AddLine(ImVec2(margin - slideOff, margin - slideOff),
                            ImVec2(margin - slideOff, margin - slideOff + bLen), bCol, 1.5f);
                dl->AddLine(ImVec2(W - margin + slideOff, margin - slideOff),
                            ImVec2(W - margin + slideOff - bLen, margin - slideOff), bCol, 1.5f);
                dl->AddLine(ImVec2(W - margin + slideOff, margin - slideOff),
                            ImVec2(W - margin + slideOff, margin - slideOff + bLen), bCol, 1.5f);
                dl->AddLine(ImVec2(margin - slideOff, H - margin + slideOff),
                            ImVec2(margin - slideOff + bLen, H - margin + slideOff), bCol, 1.5f);
                dl->AddLine(ImVec2(margin - slideOff, H - margin + slideOff),
                            ImVec2(margin - slideOff, H - margin + slideOff - bLen), bCol, 1.5f);
                dl->AddLine(ImVec2(W - margin + slideOff, H - margin + slideOff),
                            ImVec2(W - margin + slideOff - bLen, H - margin + slideOff), bCol, 1.5f);
                dl->AddLine(ImVec2(W - margin + slideOff, H - margin + slideOff),
                            ImVec2(W - margin + slideOff, H - margin + slideOff - bLen), bCol, 1.5f);
            }
        }

        // ========== LAYER 3.5: Mantra — "Collaborate Compromise Create" ==========
        // Three words cycle as ghostly watermark text behind the title.
        // Slow crossfade, multi-layer glow, positioned below title area.
        {
            const char* mantraWords[3] = { "Collaborate", "Compromise", "Create" };
            // Slower timing: each word spans ~1.4s with 0.4s overlap for crossfade
            f32 wordStart[3] = { 0.5f, 1.5f, 2.5f };
            f32 wordEnd[3]   = { 1.9f, 2.9f, 3.9f };
            f32 mantraFontSize = 16.0f;

            for (int w = 0; w < 3; w++) {
                f32 wt = timerRamp(wordStart[w], wordEnd[w]);
                if (wt <= 0.0f || wt >= 1.0f) continue;

                // Slow bell-curve: fade in 25%, hold 50%, fade out 25%
                f32 wordAlpha;
                if (wt < 0.25f) wordAlpha = smoothstep(wt / 0.25f);
                else if (wt > 0.75f) wordAlpha = smoothstep((1.0f - wt) / 0.25f);
                else wordAlpha = 1.0f;

                // Gentle upward drift
                f32 driftY = -8.0f * wt;

                // Position: centered, well below the title
                ImVec2 wordSz = font->CalcTextSizeA(mantraFontSize, FLT_MAX, 0.0f, mantraWords[w]);
                f32 wordX = center.x - wordSz.x * 0.5f;
                f32 wordY = center.y + 145.0f + driftY;

                int mAlpha = static_cast<int>(38 * wordAlpha * ga);
                if (mAlpha > 0) {
                    // Multi-layer glow (3 passes at increasing offsets)
                    for (int g = 3; g >= 1; g--) {
                        f32 off = g * 2.0f;
                        int glowA = static_cast<int>((6 + (3 - g) * 3) * wordAlpha * ga);
                        if (glowA > 0) {
                            u32 glowCol = IM_COL32(P.accent.r, P.accent.g, P.accent.b, glowA);
                            dl->AddText(nullptr, mantraFontSize, ImVec2(wordX + off, wordY), glowCol, mantraWords[w]);
                            dl->AddText(nullptr, mantraFontSize, ImVec2(wordX - off, wordY), glowCol, mantraWords[w]);
                            dl->AddText(nullptr, mantraFontSize, ImVec2(wordX, wordY + off), glowCol, mantraWords[w]);
                            dl->AddText(nullptr, mantraFontSize, ImVec2(wordX, wordY - off), glowCol, mantraWords[w]);
                        }
                    }
                    // Main text — sage green
                    dl->AddText(nullptr, mantraFontSize, ImVec2(wordX, wordY),
                        IM_COL32(P.accent.r, P.accent.g, P.accent.b, mAlpha), mantraWords[w]);
                }
            }
        }

        // ========== LAYER 4: Title "TEGE" ==========

        const char* title = "TEGE";
        f32 splashFontSize = 72.0f;
        ImVec2 titleSz = tfont->CalcTextSizeA(splashFontSize, FLT_MAX, 0.0f, title);

        {
            f32 revealT = easeOutCubic(timerRamp(1.0f, 1.6f));
            f32 titleScale = 0.9f + 0.1f * revealT;
            f32 scaledFontSize = splashFontSize * titleScale;
            ImVec2 scaledSz = tfont->CalcTextSizeA(scaledFontSize, FLT_MAX, 0.0f, title);
            ImVec2 titlePos(center.x - scaledSz.x * 0.5f, center.y - scaledSz.y * 0.5f - 10.0f);
            int titleAlpha = static_cast<int>(255 * revealT * ga);

            if (titleAlpha > 0) {
                // Circle halo behind title
                int haloA = static_cast<int>(30 * revealT * ga);
                if (haloA > 0) {
                    dl->AddCircleFilled(ImVec2(center.x, center.y - 10.0f), scaledSz.x * 0.6f,
                        IM_COL32(P.accent.r, P.accent.g, P.accent.b, haloA), 32);
                }

                // Multi-layer glow: 3 layers in 4 directions
                for (int layer = 2; layer >= 0; layer--) {
                    f32 off = (layer + 1) * 2.5f;
                    int glowA = static_cast<int>((25 - layer * 7) * revealT * ga);
                    if (glowA > 0) {
                        u32 glowCol = IM_COL32(P.accent.r, P.accent.g, P.accent.b, glowA);
                        dl->AddText(tfont, scaledFontSize, ImVec2(titlePos.x + off, titlePos.y), glowCol, title);
                        dl->AddText(tfont, scaledFontSize, ImVec2(titlePos.x - off, titlePos.y), glowCol, title);
                        dl->AddText(tfont, scaledFontSize, ImVec2(titlePos.x, titlePos.y + off), glowCol, title);
                        dl->AddText(tfont, scaledFontSize, ImVec2(titlePos.x, titlePos.y - off), glowCol, title);
                    }
                }

                // Main title text — sage green
                dl->AddText(tfont, scaledFontSize, titlePos,
                    IM_COL32(P.title.r, P.title.g, P.title.b, titleAlpha), title);

                // Shimmer: white highlight sweep at t=1.8→2.4
                f32 shimmerT = timerRamp(1.8f, 2.4f);
                if (shimmerT > 0.0f && shimmerT < 1.0f) {
                    f32 shimmerX = titlePos.x + scaledSz.x * shimmerT;
                    f32 shimmerW = scaledSz.x * 0.12f;
                    int shimmerA = static_cast<int>(140 * std::sin(shimmerT * 3.14159265f) * ga);
                    if (shimmerA > 0) {
                        // Draw shimmer as a bright vertical band clipped to title region
                        ImVec2 shimmerMin(shimmerX - shimmerW * 0.5f, titlePos.y);
                        ImVec2 shimmerMax(shimmerX + shimmerW * 0.5f, titlePos.y + scaledSz.y);
                        dl->AddRectFilledMultiColor(shimmerMin, shimmerMax,
                            IM_COL32(255, 255, 255, 0),
                            IM_COL32(255, 255, 255, 0),
                            IM_COL32(255, 255, 255, shimmerA),
                            IM_COL32(255, 255, 255, shimmerA));
                        // Re-draw title on top so shimmer is blended behind letters
                        dl->AddText(tfont, scaledFontSize, titlePos,
                            IM_COL32(P.title.r, P.title.g, P.title.b, titleAlpha), title);
                    }
                }
            }
        }

        // ========== LAYER 5: Accent Lines & Flares ==========

        // 5a. Horizontal rules flanking title — grow from center at t=1.2→1.8
        {
            f32 lineGrow = easeOutCubic(timerRamp(1.2f, 1.8f));
            f32 lineHalf = 160.0f * lineGrow;
            int lineA = static_cast<int>(140 * lineGrow * ga);
            if (lineA > 0) {
                f32 lineY1 = center.y - 50.0f;
                f32 lineY2 = center.y + 40.0f;
                // Glow line (wider, dimmer)
                dl->AddLine(ImVec2(center.x - lineHalf, lineY1),
                            ImVec2(center.x + lineHalf, lineY1),
                            IM_COL32(P.accent.r, P.accent.g, P.accent.b, lineA / 3), 4.0f);
                dl->AddLine(ImVec2(center.x - lineHalf, lineY2),
                            ImVec2(center.x + lineHalf, lineY2),
                            IM_COL32(P.accent.r, P.accent.g, P.accent.b, lineA / 3), 4.0f);
                // Sharp line
                dl->AddLine(ImVec2(center.x - lineHalf, lineY1),
                            ImVec2(center.x + lineHalf, lineY1),
                            IM_COL32(P.accent.r, P.accent.g, P.accent.b, lineA), 1.5f);
                dl->AddLine(ImVec2(center.x - lineHalf, lineY2),
                            ImVec2(center.x + lineHalf, lineY2),
                            IM_COL32(P.accent.r, P.accent.g, P.accent.b, lineA), 1.5f);

                // 5b. Diamond caps at endpoints
                f32 capR = 4.0f;
                auto drawDiamondCap = [&](f32 cx2, f32 cy2) {
                    ImVec2 dp[4] = {
                        ImVec2(cx2, cy2 - capR), ImVec2(cx2 + capR, cy2),
                        ImVec2(cx2, cy2 + capR), ImVec2(cx2 - capR, cy2)
                    };
                    dl->AddConvexPolyFilled(dp, 4, IM_COL32(P.accent.r, P.accent.g, P.accent.b, lineA));
                };
                drawDiamondCap(center.x - lineHalf, lineY1);
                drawDiamondCap(center.x + lineHalf, lineY1);
                drawDiamondCap(center.x - lineHalf, lineY2);
                drawDiamondCap(center.x + lineHalf, lineY2);
            }
        }

        // 5c. Bezier S-curves flanking title area
        {
            f32 bezierAlpha = smoothstep(timerRamp(1.0f, 1.6f));
            int bA = static_cast<int>(60 * bezierAlpha * ga);
            if (bA > 0) {
                // Left curve
                dl->AddBezierCubic(
                    ImVec2(center.x - 200, center.y - 80),
                    ImVec2(center.x - 240, center.y - 20),
                    ImVec2(center.x - 240, center.y + 20),
                    ImVec2(center.x - 200, center.y + 80),
                    IM_COL32(P.accent.r, P.accent.g, P.accent.b, bA), 1.5f, 20);
                // Right curve
                dl->AddBezierCubic(
                    ImVec2(center.x + 200, center.y - 80),
                    ImVec2(center.x + 240, center.y - 20),
                    ImVec2(center.x + 240, center.y + 20),
                    ImVec2(center.x + 200, center.y + 80),
                    IM_COL32(P.accent.r, P.accent.g, P.accent.b, bA), 1.5f, 20);
            }
        }

        // 5d. 12-sparkle burst from center at t=1.5→2.1
        {
            f32 sparkT = timerRamp(1.5f, 2.1f);
            if (sparkT > 0.0f && sparkT < 1.0f) {
                f32 sparkFade = std::sin(sparkT * 3.14159265f);
                for (int s = 0; s < 12; s++) {
                    f32 angle = s * 3.14159265f / 6.0f + 0.2f;
                    f32 dist = 30.0f + sparkT * 120.0f;
                    f32 sx = center.x + std::cos(angle) * dist;
                    f32 sy = (center.y - 10.0f) + std::sin(angle) * dist;
                    bool isSage = (s % 2 == 0);
                    int sA = static_cast<int>((isSage ? 180 : 220) * sparkFade * ga);
                    u32 sCol = isSage ? IM_COL32(P.accent.r, P.accent.g, P.accent.b, sA)
                                      : IM_COL32(P.spark.r, P.spark.g, P.spark.b, sA);
                    // Sparkle dot
                    dl->AddCircleFilled(ImVec2(sx, sy), 2.5f, sCol, 8);
                    // Trail line back toward center
                    f32 trailDist = dist - 15.0f;
                    if (trailDist > 0.0f) {
                        f32 tx = center.x + std::cos(angle) * trailDist;
                        f32 ty = (center.y - 10.0f) + std::sin(angle) * trailDist;
                        dl->AddLine(ImVec2(tx, ty), ImVec2(sx, sy),
                            IM_COL32(isSage ? P.accent.r : P.spark.r, isSage ? P.accent.g : P.spark.g, isSage ? P.accent.b : P.spark.b, sA / 2), 1.0f);
                    }
                }
            }
        }

        // ========== LAYER 6: Info Text ==========

        // "by marty64" — fade in + slide up at t=1.8→2.2
        {
            f32 creditT = easeOutCubic(timerRamp(1.8f, 2.2f));
            int creditA = static_cast<int>(160 * creditT * ga);
            if (creditA > 0 && creditLine != nullptr) {
                const char* credit = creditLine;
                ImVec2 creditSz = ImGui::CalcTextSize(credit);
                f32 slideY = center.y + 60.0f + (1.0f - creditT) * 15.0f;
                dl->AddText(ImVec2(center.x - creditSz.x * 0.5f, slideY),
                    IM_COL32(160, 165, 180, creditA), credit);
            }
        }

        // Accessibility statement — under the credit, accent-tinted.
        // "Every player" is the point; it fades in with the credit line.
        {
            f32 a11yT = easeOutCubic(timerRamp(2.0f, 2.4f));
            int a11yA = static_cast<int>(150 * a11yT * ga);
            if (a11yA > 0 && opts.a11yLine != nullptr) {
                ImVec2 sz = ImGui::CalcTextSize(opts.a11yLine);
                f32 y = center.y + 84.0f + (1.0f - a11yT) * 10.0f;
                dl->AddText(ImVec2(center.x - sz.x * 0.5f, y),
                    IM_COL32(P.accent.r, P.accent.g, P.accent.b, a11yA), opts.a11yLine);
            }
        }

        // Version string — fade in at t=2.0→2.4
        {
            f32 verT = smoothstep(timerRamp(2.0f, 2.4f));
            int verA = static_cast<int>(140 * verT * ga);
            if (verA > 0) {
                const char* version = "v" ENJIN_VERSION_STRING;
                ImVec2 verSz = ImGui::CalcTextSize(version);
                dl->AddText(ImVec2(center.x - verSz.x * 0.5f, H - 45.0f),
                    IM_COL32(100, 110, 140, verA), version);
            }
        }

        // Spinning arc loader — smooth rotating 120-degree arc
        {
            f32 loaderT = smoothstep(timerRamp(0.5f, 0.8f));
            int loaderA = static_cast<int>(120 * loaderT * ga);
            // Fade out loader when title is fully revealed
            f32 loaderFadeOut = smoothstep(timerRamp(2.2f, 2.6f));
            loaderA = static_cast<int>(loaderA * (1.0f - loaderFadeOut));
            if (loaderA > 0) {
                f32 loaderY = center.y + 100.0f;
                f32 loaderR = 12.0f;
                f32 arcStart = reduced ? -1.5f : t * 4.0f;  // static arc under reduced motion
                f32 arcLen = 2.0944f;      // 120 degrees
                int segments = 20;
                for (int s = 0; s < segments; s++) {
                    f32 a1 = arcStart + arcLen * s / segments;
                    f32 a2 = arcStart + arcLen * (s + 1) / segments;
                    // Gradient alpha along arc
                    f32 segAlpha = static_cast<f32>(s) / segments;
                    int sA = static_cast<int>(loaderA * (0.3f + 0.7f * segAlpha));
                    dl->AddLine(
                        ImVec2(center.x + std::cos(a1) * loaderR, loaderY + std::sin(a1) * loaderR),
                        ImVec2(center.x + std::cos(a2) * loaderR, loaderY + std::sin(a2) * loaderR),
                        IM_COL32(P.accent.r, P.accent.g, P.accent.b, sA), 2.0f);
                }
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

} // namespace GUI
} // namespace Enjin
