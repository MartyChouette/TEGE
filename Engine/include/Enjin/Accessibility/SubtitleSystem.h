#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <deque>

namespace Enjin {
namespace Accessibility {

// Direction indicator for spatial audio captions
enum class SubtitleDirection : u32 {
    None = 0,
    Left,
    Right,
    Above,
    Below,
    Behind
};

// A single subtitle entry
struct SubtitleEntry {
    std::string text;
    std::string speaker;
    Math::Vector3 speakerColor = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 duration = 3.0f;
    f32 timeRemaining = 3.0f;
    bool isCaption = false;       // Closed caption (sound effect) vs subtitle (dialogue)
    SubtitleDirection direction = SubtitleDirection::None;
};

// Subtitle display configuration
struct SubtitleConfig {
    bool enabled = false;
    bool captionsEnabled = false;
    f32 fontSize = 24.0f;         // 16-48
    f32 backgroundOpacity = 0.7f;
    bool showSpeakerNames = true;
    bool showDirectionIndicators = false;
    u32 maxVisibleLines = 4;
    f32 positionY = 0.85f;        // Vertical position (0=top, 1=bottom)
    // Global accessibility text scale (RuntimeAccessibilitySettings::fontScale),
    // multiplied into fontSize at draw time. Subtitles used to ignore it, so a
    // player who scaled up the UI still got small subtitles.
    f32 fontScale = 1.0f;
};

// Subtitle rendering system
// Renders via ImGui foreground draw list for overlay display
class ENJIN_API SubtitleSystem {
public:
    SubtitleSystem() = default;
    ~SubtitleSystem() = default;

    // Show a dialogue subtitle
    void ShowSubtitle(const std::string& text, const std::string& speaker = "",
                      const Math::Vector3& speakerColor = Math::Vector3(1.0f, 1.0f, 1.0f),
                      f32 duration = 3.0f);

    // Show a closed caption (sound effect description)
    void ShowCaption(const std::string& text, SubtitleDirection direction = SubtitleDirection::None,
                     f32 duration = 2.5f);

    // Update timers, remove expired entries
    void Update(f32 dt);

    // Render subtitle overlay (call during ImGui frame, after scene rendering)
    // Origin FIRST, and no default value, on purpose.
    //
    // This used to take a size only, so a caller could not say WHERE the image
    // it is overlaying starts -- 0,0 was baked in. In the docked editor the
    // game is a panel somewhere in the middle of the window, so passing the
    // window size put subtitles at the bottom of the EDITOR rather than the
    // bottom of the game, over whatever panel was docked there. The editor had
    // the right rectangle the whole time and used it correctly twelve lines
    // away (EditorLayer.cpp, the UISystem::Update call).
    //
    // A magic zero that means "assume the top-left corner" gives a caller who
    // does not know about it a wrong ANSWER rather than an error. Origin first
    // means every old two-argument call fails to compile instead.
    void RenderOverlay(f32 originX, f32 originY, u32 viewportWidth, u32 viewportHeight);

    // Clear all active subtitles
    void Clear();

    // How many subtitles are on screen right now.
    //
    // Added because Clear() had no observable at all, so its test could only
    // assert EXPECT_TRUE(true) -- a Clear() with an empty body would have passed.
    // It is also the question a HUD actually asks ("is anything showing?"), so
    // this is not a test-only accessor.
    usize GetActiveCount() const { return m_Entries.size(); }

    // Configuration
    SubtitleConfig& GetConfig() { return m_Config; }
    const SubtitleConfig& GetConfig() const { return m_Config; }
    void SetConfig(const SubtitleConfig& config) { m_Config = config; }

private:
    SubtitleConfig m_Config;
    std::deque<SubtitleEntry> m_Entries;
};

} // namespace Accessibility
} // namespace Enjin
