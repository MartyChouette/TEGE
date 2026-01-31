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
    void RenderOverlay(u32 viewportWidth, u32 viewportHeight);

    // Clear all active subtitles
    void Clear();

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
