#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Enjin::Platform {

// Type of Steam Input action
enum class SteamInputActionType : u8 {
    Button = 0,     // Digital on/off (A, B, X, Y, bumpers, etc.)
    Axis = 1,       // Analog 1D (trigger, stick axis)
    StickPad = 2,   // Analog 2D (stick, trackpad)
    Gyro = 3        // Gyroscope 3-axis rotation
};

// Represents a Steam Input action binding
struct SteamInputAction {
    std::string name;               // Action name (e.g., "Jump", "Move")
    SteamInputActionType type = SteamInputActionType::Button;
    u64 handle = 0;                 // Steam Input action handle (opaque)
};

// Steam Input action set (groups of actions for different contexts)
struct SteamInputActionSet {
    std::string name;               // Set name (e.g., "InGame", "Menu")
    u64 handle = 0;                 // Steam Input action set handle
    std::vector<SteamInputAction> actions;
};

// State of a digital (button) action
struct DigitalActionState {
    bool active = false;            // Is this action currently bound?
    bool pressed = false;           // Is the button currently pressed?
};

// State of an analog action (1D or 2D)
struct AnalogActionState {
    bool active = false;            // Is this action currently bound?
    f32 x = 0.0f;                  // X axis (-1.0 to 1.0)
    f32 y = 0.0f;                  // Y axis (-1.0 to 1.0, 0 for 1D actions)
};

// Gyroscope callback signature: (pitch, yaw, roll) in degrees/second
using GyroCallback = std::function<void(f32 pitch, f32 yaw, f32 roll)>;

// Steam Input manager for controller input via Steam's remapping system.
// When ENJIN_STEAM is defined and the Steamworks SDK is available, this
// calls the real Steam Input API. Otherwise all methods are safe no-ops.
class ENJIN_API SteamInputManager {
public:
    SteamInputManager() = default;
    ~SteamInputManager() = default;

    // =============================================================================
    // Lifecycle
    // =============================================================================

    // Initialize Steam Input subsystem.
    // Returns true if Steam Input is available and initialized.
    bool Initialize();

    // Shut down Steam Input and release handles.
    void Shutdown();

    // Is Steam Input initialized and available?
    bool IsAvailable() const { return m_Initialized; }

    // =============================================================================
    // Input polling
    // =============================================================================

    // Poll all connected controllers for input state.
    // Call once per frame before querying action states.
    void PollInput();

    // Get the number of connected Steam Input controllers
    u32 GetConnectedControllerCount() const { return m_ControllerCount; }

    // =============================================================================
    // Action sets
    // =============================================================================

    // Register an action set by name. Returns a handle (0 on failure).
    u64 RegisterActionSet(const std::string& name);

    // Activate an action set for a specific controller (0 = first controller)
    void ActivateActionSet(u64 setHandle, u32 controllerIndex = 0);

    // Get the currently active action set handle for a controller
    u64 GetActiveActionSet(u32 controllerIndex = 0) const;

    // =============================================================================
    // Digital actions (buttons)
    // =============================================================================

    // Register a digital action by name. Returns a handle (0 on failure).
    u64 RegisterDigitalAction(const std::string& name);

    // Get the state of a digital action for a controller
    DigitalActionState GetDigitalActionState(u64 actionHandle, u32 controllerIndex = 0) const;

    // Convenience: is a named action currently pressed?
    bool IsActionPressed(const std::string& name, u32 controllerIndex = 0) const;

    // =============================================================================
    // Analog actions (sticks, triggers, trackpads)
    // =============================================================================

    // Register an analog action by name. Returns a handle (0 on failure).
    u64 RegisterAnalogAction(const std::string& name);

    // Get the state of an analog action for a controller
    AnalogActionState GetAnalogActionState(u64 actionHandle, u32 controllerIndex = 0) const;

    // Convenience: get a named action's X value (or X,Y as pair)
    f32 GetActionValue(const std::string& name, u32 controllerIndex = 0) const;

    // =============================================================================
    // Gyroscope
    // =============================================================================

    // Set a callback for gyroscope data. Called during PollInput() if gyro is active.
    void SetGyroCallback(GyroCallback cb) { m_GyroCallback = std::move(cb); }

    // =============================================================================
    // Steam overlay
    // =============================================================================

    // Show Steam's controller binding configuration panel
    void ShowBindingPanel(u32 controllerIndex = 0);

    // Show Steam overlay at a specific URL/page
    void ShowOverlay(const std::string& page = "");

    // =============================================================================
    // Haptics
    // =============================================================================

    // Trigger haptic feedback on a controller's trackpad or actuator
    // pad: 0 = left, 1 = right
    void TriggerHapticPulse(u32 controllerIndex, u32 pad, u16 durationMicroseconds);

    // Trigger repeated haptic pulses
    void TriggerRepeatedHapticPulse(u32 controllerIndex, u32 pad,
                                     u16 durationMicroseconds, u16 offMicroseconds,
                                     u16 repeat);

private:
    bool m_Initialized = false;
    u32 m_ControllerCount = 0;

    // Action registration maps (name -> handle)
    std::unordered_map<std::string, u64> m_ActionSetHandles;
    std::unordered_map<std::string, u64> m_DigitalActionHandles;
    std::unordered_map<std::string, u64> m_AnalogActionHandles;

    // Currently active action set per controller
    u64 m_ActiveActionSets[4]{};

    // Controller handles (up to 4 controllers)
    u64 m_ControllerHandles[4]{};

    GyroCallback m_GyroCallback;
};

} // namespace Enjin::Platform
