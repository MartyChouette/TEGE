#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <deque>

namespace Enjin {
namespace Accessibility {

// Visual indicator for an audio event
struct AudioIndicator {
    std::string label;          // Short description (e.g. "Error", "Compile OK", "Audio Playing")
    Math::Vector3 color;        // Indicator color
    f32 lifetime = 2.0f;        // Total display time
    f32 timeRemaining = 2.0f;   // Time until fade out
    f32 intensity = 1.0f;       // 0..1 pulse intensity
    bool isPulse = false;       // Pulsing (continuous) vs one-shot
};

// Configuration for audio visual indicators
struct AudioIndicatorConfig {
    bool enabled = false;
    f32 indicatorSize = 12.0f;     // Radius of indicator dots
    f32 positionX = 0.97f;         // Screen position (0=left, 1=right)
    f32 positionY = 0.05f;         // Screen position (0=top, 1=bottom)
    bool showLabels = true;
};

// Renders visual indicators for audio events
// Shown as colored dots/circles on the edge of the viewport
class ENJIN_API AudioVisualIndicatorSystem {
public:
    AudioVisualIndicatorSystem() = default;
    ~AudioVisualIndicatorSystem() = default;

    // Show a one-shot indicator (notification, error, completion)
    void ShowIndicator(const std::string& label, const Math::Vector3& color, f32 duration = 2.0f);

    // Show a pulsing indicator (continuous audio, recording, etc.)
    void ShowPulse(const std::string& label, const Math::Vector3& color);

    // Remove a named pulse indicator
    void StopPulse(const std::string& label);

    // Update timers
    void Update(f32 dt);

    // Render indicators overlay
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

    // Clear all indicators
    void Clear();

    AudioIndicatorConfig& GetConfig() { return m_Config; }
    const AudioIndicatorConfig& GetConfig() const { return m_Config; }

private:
    AudioIndicatorConfig m_Config;
    std::deque<AudioIndicator> m_Indicators;
    f32 m_PulsePhase = 0.0f; // For pulsing animation
};

} // namespace Accessibility
} // namespace Enjin
