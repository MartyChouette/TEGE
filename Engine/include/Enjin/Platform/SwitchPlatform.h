#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Logging/Log.h"
#include <string>

/**
 * @file SwitchPlatform.h
 * @brief Nintendo Switch platform abstraction stubs
 *
 * Provides stubs for Joy-Con detection, handheld/docked mode,
 * touch screen input, and performance mode switching.
 * All methods log warnings and return defaults when the real
 * Switch SDK (__SWITCH__) is not available.
 */

namespace Enjin {
namespace Platform {

// Performance mode (handheld vs docked GPU/CPU clocks)
enum class SwitchPerformanceMode : u8 {
    Handheld,       // Lower clocks, battery-saving
    Docked,         // Higher clocks, full performance
    Boost           // Temporary CPU boost (available on Switch 1 firmware 8.0+)
};

// Joy-Con connection state
enum class JoyConConnectionState : u8 {
    Disconnected,
    AttachedToConsole,  // Handheld mode, attached to rails
    Wireless,           // Connected via Bluetooth
    USB                 // Connected via USB (Pro Controller etc.)
};

// Joy-Con type
enum class JoyConType : u8 {
    Left,
    Right,
    ProController,
    Unknown
};

// Joy-Con info
struct JoyConInfo {
    JoyConType type = JoyConType::Unknown;
    JoyConConnectionState connectionState = JoyConConnectionState::Disconnected;
    u8 batteryLevel = 0;      // 0-100
    bool hasGyroscope = false;
    bool hasAccelerometer = false;
    bool hasNFC = false;       // NFC/amiibo reader
    bool hasIRCamera = false;  // Right Joy-Con IR camera
    u32 playerIndex = 0;       // Which player slot (0-7)
};

// Touch point for Switch touch screen
struct SwitchTouchPoint {
    u32 id = 0;
    f32 x = 0.0f;       // Normalized 0..1
    f32 y = 0.0f;       // Normalized 0..1
    f32 diameterX = 0.0f;  // Touch area diameter
    f32 diameterY = 0.0f;
    f32 rotationAngle = 0.0f;
    bool active = false;
};

// Switch console capabilities
struct SwitchCapabilities {
    bool hasTouchScreen = true;      // Always true on Switch 1 (handheld mode)
    bool hasGyroscope = true;        // Joy-Con gyro
    bool hasAccelerometer = true;    // Joy-Con accelerometer
    bool hasNFC = true;              // amiibo support
    bool hasIRCamera = true;         // Right Joy-Con
    bool hasHDRumble = true;         // Linear actuator rumble
    u32 maxControllers = 8;          // Up to 8 players
    u32 touchPointMax = 10;          // Multi-touch on handheld screen
    u32 screenWidthHandheld = 1280;  // 720p handheld screen
    u32 screenHeightHandheld = 720;
    u32 maxOutputWidth = 1920;       // 1080p docked
    u32 maxOutputHeight = 1080;
};

#ifdef ENJIN_PLATFORM_SWITCH

/**
 * @brief Nintendo Switch platform abstraction
 *
 * Provides access to Switch-specific hardware features.
 * Currently a stub -- all methods log warnings and return defaults.
 */
class ENJIN_API SwitchPlatform {
public:
    SwitchPlatform();
    ~SwitchPlatform();

    /**
     * @brief Initialize Switch platform services
     * @return true on success
     */
    bool Initialize();

    /**
     * @brief Shutdown and release Switch platform services
     */
    void Shutdown();

    // --- Joy-Con Management ---

    /**
     * @brief Get number of connected controllers
     */
    u32 GetConnectedControllerCount() const;

    /**
     * @brief Get info for a specific controller
     * @param index Controller index (0-7)
     */
    JoyConInfo GetControllerInfo(u32 index) const;

    /**
     * @brief Check if Joy-Cons are attached to the console (handheld mode)
     */
    bool AreJoyConsAttached() const;

    /**
     * @brief Set HD rumble intensity for a controller
     * @param index Controller index
     * @param lowFreqAmplitude Low-frequency amplitude (0.0-1.0)
     * @param highFreqAmplitude High-frequency amplitude (0.0-1.0)
     */
    void SetRumble(u32 index, f32 lowFreqAmplitude, f32 highFreqAmplitude);

    // --- Display Mode ---

    /**
     * @brief Check if the console is currently docked
     */
    bool IsDockedMode() const;

    /**
     * @brief Check if the console is in handheld mode
     */
    bool IsHandheldMode() const;

    /**
     * @brief Check if the console is in tabletop mode (kickstand, detached Joy-Cons)
     */
    bool IsTabletopMode() const;

    /**
     * @brief Get the current display resolution based on mode
     * @param outWidth Output width in pixels
     * @param outHeight Output height in pixels
     */
    void GetCurrentResolution(u32& outWidth, u32& outHeight) const;

    // --- Touch Screen ---

    /**
     * @brief Check if touch screen is available (only in handheld mode)
     */
    bool IsTouchScreenAvailable() const;

    /**
     * @brief Get number of active touch points
     */
    u32 GetTouchPointCount() const;

    /**
     * @brief Get a specific touch point
     * @param index Touch point index
     */
    SwitchTouchPoint GetTouchPoint(u32 index) const;

    // --- Performance Mode ---

    /**
     * @brief Get current performance mode
     */
    SwitchPerformanceMode GetPerformanceMode() const;

    /**
     * @brief Request a performance mode change
     * @param mode Desired performance mode
     * @return true if the request was accepted
     */
    bool RequestPerformanceMode(SwitchPerformanceMode mode);

    /**
     * @brief Get current GPU clock speed in MHz
     */
    f32 GetGPUClockMHz() const;

    /**
     * @brief Get current CPU clock speed in MHz
     */
    f32 GetCPUClockMHz() const;

    // --- System Info ---

    /**
     * @brief Get console capabilities
     */
    SwitchCapabilities GetCapabilities() const;

    /**
     * @brief Get available system memory in bytes
     */
    u64 GetAvailableMemory() const;

    /**
     * @brief Get console serial number (stub, returns empty)
     */
    std::string GetConsoleSerial() const;

    /**
     * @brief Get system firmware version string (stub)
     */
    std::string GetFirmwareVersion() const;

    /**
     * @brief Check if the console has an active internet connection
     */
    bool IsNetworkAvailable() const;

private:
    bool m_Initialized = false;
    SwitchPerformanceMode m_CurrentMode = SwitchPerformanceMode::Handheld;
};

#endif // ENJIN_PLATFORM_SWITCH

} // namespace Platform
} // namespace Enjin
