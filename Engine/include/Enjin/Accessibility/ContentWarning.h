#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Accessibility {

// Content warning type flags (bitmask)
enum class ContentWarningType : u32 {
    None = 0,
    FlashingLights = 1 << 0,
    RapidMotion = 1 << 1,
    Violence = 1 << 2,
    Heights = 1 << 3,
    LoudSounds = 1 << 4,
    Spiders = 1 << 5,
    Gore = 1 << 6,
    Drowning = 1 << 7
};

inline ContentWarningType operator|(ContentWarningType a, ContentWarningType b) {
    return static_cast<ContentWarningType>(static_cast<u32>(a) | static_cast<u32>(b));
}

inline ContentWarningType operator&(ContentWarningType a, ContentWarningType b) {
    return static_cast<ContentWarningType>(static_cast<u32>(a) & static_cast<u32>(b));
}

inline bool HasWarning(ContentWarningType flags, ContentWarningType warning) {
    return (static_cast<u32>(flags) & static_cast<u32>(warning)) != 0;
}

// Content flags for a scene
struct SceneContentFlags {
    ContentWarningType flags = ContentWarningType::None;
    std::vector<std::string> customWarnings;
};

// Content warning overlay system
class ENJIN_API ContentWarningSystem {
public:
    ContentWarningSystem() = default;
    ~ContentWarningSystem() = default;

    // Set content flags for current scene
    void SetSceneFlags(const SceneContentFlags& flags);

    // Show warning overlay (returns true while overlay is visible)
    bool IsVisible() const { return m_Visible; }

    // Render warning overlay (call during ImGui frame)
    void RenderWarningOverlay(u32 viewportWidth, u32 viewportHeight);

    // Dismiss the warning
    void Dismiss();

    // Check if scene has any warnings
    bool HasWarnings() const;

private:
    SceneContentFlags m_CurrentFlags;
    bool m_Visible = false;
    bool m_Dismissed = false;

    std::string GetWarningText(ContentWarningType type) const;
};

} // namespace Accessibility
} // namespace Enjin
