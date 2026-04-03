#pragma once

#include "Enjin/Platform/Platform.h"
#include <imgui.h>

namespace Enjin::Editor {

// Centralized editor theme constants.
// Replaces magic numbers scattered across 466+ IM_COL32 calls and hardcoded
// padding/rounding values. Change these to re-theme the entire editor.
namespace Theme {

    // =========================================================================
    // Colors
    // =========================================================================

    // Backgrounds
    constexpr ImU32 PanelBg             = IM_COL32(30, 33, 42, 255);
    constexpr ImU32 PanelBgDark         = IM_COL32(20, 22, 28, 255);
    constexpr ImU32 CardBg              = IM_COL32(30, 30, 30, 255);
    constexpr ImU32 OverlayBg           = IM_COL32(50, 50, 50, 80);
    constexpr ImU32 TooltipBg           = IM_COL32(0, 0, 0, 180);

    // Text
    constexpr ImU32 TextPrimary         = IM_COL32(220, 225, 245, 255);
    constexpr ImU32 TextSecondary       = IM_COL32(180, 185, 205, 255);
    constexpr ImU32 TextMuted           = IM_COL32(120, 125, 145, 200);
    constexpr ImU32 TextWhite           = IM_COL32(255, 255, 255, 255);
    constexpr ImU32 TextWhiteSoft       = IM_COL32(255, 255, 255, 230);
    constexpr ImU32 TextWhiteFaded      = IM_COL32(255, 255, 255, 200);
    constexpr ImU32 TextDisabled        = IM_COL32(150, 150, 150, 200);
    constexpr ImU32 TextGray            = IM_COL32(180, 180, 180, 255);
    constexpr ImU32 TextDark            = IM_COL32(100, 100, 100, 255);
    constexpr ImU32 Transparent         = IM_COL32(0, 0, 0, 0);

    // Borders / Separators
    constexpr ImU32 Border              = IM_COL32(80, 80, 80, 255);
    constexpr ImU32 BorderSubtle        = IM_COL32(60, 65, 80, 150);
    constexpr ImU32 Separator           = IM_COL32(200, 200, 200, 200);

    // Accents
    constexpr ImU32 AccentBlue          = IM_COL32(80, 110, 180, 200);
    constexpr ImU32 AccentGreen         = IM_COL32(50, 255, 80, 255);
    constexpr ImU32 AccentYellow        = IM_COL32(255, 200, 50, 255);
    constexpr ImU32 AccentRed           = IM_COL32(255, 60, 60, 200);
    constexpr ImU32 AccentOrange        = IM_COL32(255, 165, 0, 255);
    constexpr ImU32 AccentCyan          = IM_COL32(100, 220, 255, 230);

    // Status colors
    constexpr ImU32 Success             = IM_COL32(50, 200, 80, 255);
    constexpr ImU32 Warning             = IM_COL32(255, 200, 50, 255);
    constexpr ImU32 Error               = IM_COL32(255, 80, 80, 255);
    constexpr ImU32 Info                = IM_COL32(80, 160, 255, 255);

    // Selection
    constexpr ImU32 Selected            = IM_COL32(255, 255, 100, 255);
    constexpr ImU32 Hovered             = IM_COL32(80, 80, 120, 200);

    // Bone visualization
    constexpr ImU32 BoneNormal          = IM_COL32(255, 255, 255, 200);
    constexpr ImU32 BoneSelected        = IM_COL32(50, 255, 80, 255);
    constexpr ImU32 BoneIKTarget        = IM_COL32(255, 220, 50, 220);
    constexpr ImU32 BoneChain           = IM_COL32(100, 220, 255, 230);
    constexpr ImU32 BoneDimmed          = IM_COL32(180, 180, 180, 100);

    // Audio bus colors
    constexpr ImU32 BusSFX              = IM_COL32(80, 180, 100, 255);
    constexpr ImU32 BusMusic            = IM_COL32(130, 100, 220, 255);
    constexpr ImU32 BusUI               = IM_COL32(60, 150, 220, 255);
    constexpr ImU32 BusVoice            = IM_COL32(220, 150, 60, 255);

    // Audio bus colors (ImVec4 for PushStyleColor)
    constexpr ImVec4 BusSFXV            = ImVec4(0.3f, 0.7f, 0.4f, 1.0f);
    constexpr ImVec4 BusMusicV          = ImVec4(0.5f, 0.4f, 0.9f, 1.0f);
    constexpr ImVec4 BusUIV             = ImVec4(0.2f, 0.6f, 0.9f, 1.0f);
    constexpr ImVec4 BusVoiceV          = ImVec4(0.9f, 0.6f, 0.2f, 1.0f);

    // Gizmo
    constexpr ImU32 GizmoX              = IM_COL32(255, 50, 50, 255);
    constexpr ImU32 GizmoY              = IM_COL32(50, 255, 50, 255);
    constexpr ImU32 GizmoZ              = IM_COL32(50, 50, 255, 255);

    // =========================================================================
    // Spacing & Sizing
    // =========================================================================

    constexpr f32 WindowRounding        = 8.0f;
    constexpr f32 FrameRounding         = 4.0f;     // Button/input field rounding
    constexpr f32 CardRounding          = 4.0f;
    constexpr f32 TooltipRounding       = 3.0f;

    constexpr f32 ItemSpacing           = 8.0f;
    constexpr f32 IndentWidth           = 8.0f;
    constexpr f32 SectionSpacing        = 16.0f;

    constexpr f32 ButtonHeightSmall     = 26.0f;
    constexpr f32 ButtonHeightNormal    = 32.0f;
    constexpr f32 ButtonHeightLarge     = 40.0f;

    constexpr f32 ThumbnailPadding      = 16.0f;
    constexpr f32 PanelMinWidth         = 340.0f;

    // =========================================================================
    // Font sizes (for custom text rendering, not ImGui default font)
    // =========================================================================

    constexpr f32 FontSizeSmall         = 12.0f;
    constexpr f32 FontSizeNormal        = 14.0f;
    constexpr f32 FontSizeLarge         = 18.0f;
    constexpr f32 FontSizeHeading       = 24.0f;

    // =========================================================================
    // ImVec4 variants (for PushStyleColor)
    // =========================================================================

    constexpr ImVec4 TextPrimaryV       = ImVec4(0.86f, 0.88f, 0.96f, 1.0f);
    constexpr ImVec4 TextSecondaryV     = ImVec4(0.71f, 0.73f, 0.80f, 1.0f);
    constexpr ImVec4 TextMutedV         = ImVec4(0.47f, 0.49f, 0.57f, 0.78f);
    constexpr ImVec4 SuccessV           = ImVec4(0.2f, 0.8f, 0.3f, 1.0f);
    constexpr ImVec4 WarningV           = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
    constexpr ImVec4 ErrorV             = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
    constexpr ImVec4 InfoV              = ImVec4(0.3f, 0.6f, 1.0f, 1.0f);
    constexpr ImVec4 HeadingV           = ImVec4(0.7f, 0.85f, 1.0f, 1.0f);

} // namespace Theme
} // namespace Enjin::Editor
