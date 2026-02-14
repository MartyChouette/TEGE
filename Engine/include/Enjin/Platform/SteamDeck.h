#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <functional>

namespace Enjin::Platform {

// Quality level recommendations for adaptive rendering
enum class DeckQualityLevel : u8 {
    Low = 0,
    Medium = 1,
    High = 2
};

// Thermal state reported by the device
enum class ThermalState : u8 {
    Cool = 0,       // Full performance available
    Warm = 1,       // Sustained load, may throttle soon
    Hot = 2,        // Thermal throttling active
    Critical = 3    // Emergency throttling, reduce load immediately
};

// Steam Deck detection and optimization configuration
struct SteamDeckConfig {
    bool adaptiveQuality = true;    // Automatically adjust quality based on thermal state
    u32 targetFPS = 60;             // Target framerate (30 or 60 typical for Deck)
    bool gyroEnabled = false;       // Enable gyroscope input
    bool hapticFeedback = true;     // Enable haptic feedback on trackpads
    bool suspendResume = true;      // Handle suspend/resume lifecycle events
    f32 renderScale = 1.0f;         // Resolution scale (0.5 to 1.0, Deck native 1280x800)
    bool forceFSR = false;          // Force AMD FidelityFX Super Resolution
};

// Steam Deck platform support and adaptive quality management
class ENJIN_API SteamDeckSupport {
public:
    SteamDeckSupport() = default;
    ~SteamDeckSupport() = default;

    // Initialize Steam Deck support, detecting hardware capabilities
    void Initialize();

    // Shutdown and clean up
    void Shutdown();

    // Update per frame (checks thermal state, handles suspend/resume)
    void Update(f32 dt);

    // =============================================================================
    // Detection
    // =============================================================================

    // Check if we are running on a Steam Deck.
    // Detects via SteamDeck=1 env var, /sys/devices DMI, or gamescope session.
    static bool IsRunningOnDeck();

    // Check if we are running under gamescope (Steam's compositor)
    static bool IsRunningUnderGamescope();

    // Get the Deck's hardware revision string (if available)
    std::string GetHardwareRevision() const;

    // =============================================================================
    // Adaptive Quality
    // =============================================================================

    // Enable or disable adaptive quality adjustments
    void SetAdaptiveQuality(bool enabled) { m_Config.adaptiveQuality = enabled; }
    bool IsAdaptiveQualityEnabled() const { return m_Config.adaptiveQuality; }

    // Get the recommended quality level based on current thermal state and FPS
    DeckQualityLevel GetRecommendedQuality() const { return m_RecommendedQuality; }

    // Get recommended shadow map resolution for current quality level
    u32 GetRecommendedShadowResolution() const;

    // Get recommended render scale (0.5 to 1.0)
    f32 GetRecommendedRenderScale() const;

    // Get recommended max particle count
    u32 GetRecommendedMaxParticles() const;

    // =============================================================================
    // Thermal monitoring
    // =============================================================================

    // Get current thermal state
    ThermalState GetThermalState() const { return m_ThermalState; }

    // Get GPU temperature in Celsius (0 if unavailable)
    f32 GetGPUTemperature() const { return m_GPUTemp; }

    // Get battery percentage (0-100, -1 if on AC or unavailable)
    i32 GetBatteryPercentage() const { return m_BatteryPercentage; }

    // Is the device currently charging?
    bool IsCharging() const { return m_IsCharging; }

    // =============================================================================
    // Suspend/Resume
    // =============================================================================

    // Register a callback for suspend events (save state)
    using SuspendCallback = std::function<void()>;
    void SetSuspendCallback(SuspendCallback cb) { m_SuspendCallback = std::move(cb); }

    // Register a callback for resume events (restore state)
    using ResumeCallback = std::function<void()>;
    void SetResumeCallback(ResumeCallback cb) { m_ResumeCallback = std::move(cb); }

    // Manually trigger suspend handling (called by the engine on focus loss)
    void HandleSuspend();

    // Manually trigger resume handling (called by the engine on focus gain)
    void HandleResume();

    // =============================================================================
    // Gyroscope
    // =============================================================================

    // Enable/disable gyro input processing
    void SetGyroEnabled(bool enabled) { m_Config.gyroEnabled = enabled; }
    bool IsGyroEnabled() const { return m_Config.gyroEnabled; }

    // Update gyroscope state (called internally by SteamInput when available)
    void UpdateGyro(f32 dt);

    // Get gyro rotation rates (degrees/second) — pitch, yaw, roll
    f32 GetGyroPitch() const { return m_GyroPitch; }
    f32 GetGyroYaw() const { return m_GyroYaw; }
    f32 GetGyroRoll() const { return m_GyroRoll; }

    // =============================================================================
    // Configuration
    // =============================================================================

    const SteamDeckConfig& GetConfig() const { return m_Config; }
    void SetConfig(const SteamDeckConfig& config) { m_Config = config; }
    void SetTargetFPS(u32 fps) { m_Config.targetFPS = fps; }

private:
    // Read thermal zone temperatures from /sys/class/thermal
    void UpdateThermalState();

    // Read battery info from /sys/class/power_supply
    void UpdateBatteryInfo();

    // Compute recommended quality based on thermal state and FPS history
    void UpdateRecommendedQuality(f32 currentFPS);

    SteamDeckConfig m_Config;
    bool m_Initialized = false;
    bool m_IsDeck = false;

    // Thermal
    ThermalState m_ThermalState = ThermalState::Cool;
    f32 m_GPUTemp = 0.0f;
    f32 m_ThermalCheckTimer = 0.0f;
    static constexpr f32 THERMAL_CHECK_INTERVAL = 2.0f; // Check every 2 seconds

    // Battery
    i32 m_BatteryPercentage = -1;
    bool m_IsCharging = false;

    // Quality tracking
    DeckQualityLevel m_RecommendedQuality = DeckQualityLevel::High;
    f32 m_FPSHistory[60]{};
    u32 m_FPSHistoryIndex = 0;
    f32 m_QualityAdjustTimer = 0.0f;
    static constexpr f32 QUALITY_ADJUST_INTERVAL = 5.0f; // Re-evaluate every 5 seconds

    // Gyro
    f32 m_GyroPitch = 0.0f;
    f32 m_GyroYaw = 0.0f;
    f32 m_GyroRoll = 0.0f;

    // Suspend/resume
    bool m_WasSuspended = false;
    SuspendCallback m_SuspendCallback;
    ResumeCallback m_ResumeCallback;
};

} // namespace Enjin::Platform
