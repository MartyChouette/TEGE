#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <functional>

namespace Enjin {
namespace Accessibility {

// Supported alternative input device types
enum class AlternativeInputType : u8 {
    None = 0,
    SwitchAccess,       // Binary input switches (1-4 switches)
    EyeTracking,        // Eye/gaze tracking devices
    SipAndPuff,         // Pneumatic switch (sip = activate, puff = secondary)
    HeadTracking,       // Head movement for cursor control
    VoiceControl        // Voice command input
};

// Switch access configuration
struct SwitchAccessConfig {
    bool enabled = false;
    bool scanningMode = true;       // Auto-scan through UI elements
    f32 scanSpeed = 1.5f;           // Seconds per element (0.5-5.0)
    f32 scanStartDelay = 0.5f;      // Delay before scanning starts
    bool autoScanReverse = false;   // Reverse direction on wrap
    i32 switchCount = 1;            // 1 = single switch (scan+select), 2 = select+next

    // Switch mappings (key codes for hardware switches)
    i32 selectKey = 0;              // Primary action (Enter by default)
    i32 nextKey = 0;                // Move to next (Tab by default)
    i32 backKey = 0;                // Move to previous (Shift+Tab)
    i32 escapeKey = 0;              // Cancel/escape
};

// Eye tracking configuration
struct EyeTrackingConfig {
    bool enabled = false;
    f32 dwellTime = 1.0f;           // Seconds to dwell for click (0.3-3.0)
    f32 smoothingFactor = 0.3f;     // Cursor smoothing (0.0-1.0)
    f32 deadZone = 5.0f;            // Pixels of movement ignored (reduces jitter)
    bool showGazeIndicator = true;  // Show where the user is looking
    f32 gazeIndicatorSize = 20.0f;  // Size of gaze circle
};

// Sip and puff configuration
struct SipAndPuffConfig {
    bool enabled = false;
    f32 sipThreshold = 0.3f;        // Normalized pressure for sip detection
    f32 puffThreshold = 0.3f;       // Normalized pressure for puff detection
    f32 hardSipThreshold = 0.7f;    // Hard sip = secondary action
    f32 hardPuffThreshold = 0.7f;   // Hard puff = escape/cancel
};

// Head tracking configuration
struct HeadTrackingConfig {
    bool enabled = false;
    f32 sensitivity = 1.0f;         // Movement multiplier (0.1-5.0)
    f32 smoothing = 0.5f;           // Cursor smoothing (0.0-1.0)
    f32 deadZone = 3.0f;            // Degrees of head movement ignored
    bool invertX = false;
    bool invertY = false;
};

// A scannable UI element for switch access
struct ScanTarget {
    std::string label;              // Accessible label for the element
    std::string group;              // Group name for hierarchical scanning
    f32 x = 0, y = 0, w = 0, h = 0; // Screen bounds
    std::function<void()> onActivate; // Called when selected
    bool isFocused = false;
};

// Alternative input device manager
// Provides abstraction layer for switch access, eye tracking,
// sip-and-puff, head tracking, and voice control devices
class ENJIN_API AlternativeInputManager {
public:
    AlternativeInputManager() = default;
    ~AlternativeInputManager() = default;

    // Per-frame update (call from editor Update)
    void Update(f32 dt);

    // Render overlays (scanning highlight, gaze indicator)
    void RenderOverlay();

    // Register/unregister scannable UI elements
    void RegisterScanTarget(const ScanTarget& target);
    void ClearScanTargets();

    // Switch access
    void SetSwitchConfig(const SwitchAccessConfig& config) { m_SwitchConfig = config; }

    // Drive the switch scanner from the ONE accessibility setting the player
    // sees. This scanner covers editor/app chrome while UISystem covers game
    // UI elements; they are different target lists on purpose, but they must
    // never disagree about being on or scan at different speeds.
    // Device-specific fields (switch count, key mapping) stay in the config.
    void ApplyAccessibilitySettings(bool switchEnabled, f32 scanSpeed) {
        m_SwitchConfig.enabled = switchEnabled;
        if (scanSpeed > 0.1f) m_SwitchConfig.scanSpeed = scanSpeed;
    }
    const SwitchAccessConfig& GetSwitchConfig() const { return m_SwitchConfig; }

    // Eye tracking
    void SetEyeTrackingConfig(const EyeTrackingConfig& config) { m_EyeConfig = config; }
    const EyeTrackingConfig& GetEyeTrackingConfig() const { return m_EyeConfig; }
    void UpdateGazePosition(f32 x, f32 y); // Called by eye tracker driver/API

    // Sip and puff
    void SetSipAndPuffConfig(const SipAndPuffConfig& config) { m_SipPuffConfig = config; }
    const SipAndPuffConfig& GetSipAndPuffConfig() const { return m_SipPuffConfig; }
    void UpdatePressure(f32 pressure); // Positive = puff, negative = sip

    // Head tracking
    void SetHeadTrackingConfig(const HeadTrackingConfig& config) { m_HeadConfig = config; }
    const HeadTrackingConfig& GetHeadTrackingConfig() const { return m_HeadConfig; }
    void UpdateHeadRotation(f32 yaw, f32 pitch); // Called by head tracker driver/API

    // State
    bool IsAnyAlternativeInputActive() const;
    i32 GetCurrentScanIndex() const { return m_ScanIndex; }
    usize GetScanTargetCount() const { return m_ScanTargets.size(); }

private:
    // Configs
    SwitchAccessConfig m_SwitchConfig;
    EyeTrackingConfig m_EyeConfig;
    SipAndPuffConfig m_SipPuffConfig;
    HeadTrackingConfig m_HeadConfig;

    // Switch scanning state
    std::vector<ScanTarget> m_ScanTargets;
    i32 m_ScanIndex = -1;
    f32 m_ScanTimer = 0.0f;
    bool m_ScanForward = true;

    // Eye tracking state
    f32 m_GazeX = 0.0f, m_GazeY = 0.0f;
    // Set by UpdateGazePosition so a real tracker overrides the pointer
    // fallback for that frame; cleared each Update.
    bool m_GazeFedThisFrame = false;
    f32 m_SmoothedGazeX = 0.0f, m_SmoothedGazeY = 0.0f;
    f32 m_GazeDwellTimer = 0.0f;
    i32 m_GazeDwellTarget = -1;  // Index of scan target being dwelled on

    // Sip/puff state
    f32 m_CurrentPressure = 0.0f;
    bool m_SipActive = false;
    bool m_PuffActive = false;

    // Head tracking state
    f32 m_HeadYaw = 0.0f, m_HeadPitch = 0.0f;
    f32 m_SmoothedCursorX = 0.0f, m_SmoothedCursorY = 0.0f;

    void UpdateSwitchScanning(f32 dt);
    void UpdateEyeTracking(f32 dt);
    void RenderScanHighlight();
    void RenderGazeIndicator();
};

} // namespace Accessibility
} // namespace Enjin
