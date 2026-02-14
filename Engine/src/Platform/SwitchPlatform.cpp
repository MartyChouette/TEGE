#include "Enjin/Platform/SwitchPlatform.h"

#ifdef ENJIN_PLATFORM_SWITCH

namespace Enjin {
namespace Platform {

SwitchPlatform::SwitchPlatform() = default;
SwitchPlatform::~SwitchPlatform() {
    if (m_Initialized) {
        Shutdown();
    }
}

bool SwitchPlatform::Initialize() {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: Initialize() — Switch SDK not available");
    m_Initialized = false;
    return false;
}

void SwitchPlatform::Shutdown() {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: Shutdown() — Switch SDK not available");
    m_Initialized = false;
}

// --- Joy-Con Management ---

u32 SwitchPlatform::GetConnectedControllerCount() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetConnectedControllerCount() — returning 0");
    return 0;
}

JoyConInfo SwitchPlatform::GetControllerInfo(u32 index) const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetControllerInfo(%u) — returning default", index);
    return JoyConInfo{};
}

bool SwitchPlatform::AreJoyConsAttached() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: AreJoyConsAttached() — returning false");
    return false;
}

void SwitchPlatform::SetRumble(u32 index, f32 lowFreqAmplitude, f32 highFreqAmplitude) {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: SetRumble(%u, %.2f, %.2f) — Switch SDK not available",
                   index, lowFreqAmplitude, highFreqAmplitude);
}

// --- Display Mode ---

bool SwitchPlatform::IsDockedMode() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: IsDockedMode() — returning false");
    return false;
}

bool SwitchPlatform::IsHandheldMode() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: IsHandheldMode() — returning true (default)");
    return true;
}

bool SwitchPlatform::IsTabletopMode() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: IsTabletopMode() — returning false");
    return false;
}

void SwitchPlatform::GetCurrentResolution(u32& outWidth, u32& outHeight) const {
    // Default to handheld resolution
    outWidth = 1280;
    outHeight = 720;
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetCurrentResolution() — returning 1280x720 (handheld default)");
}

// --- Touch Screen ---

bool SwitchPlatform::IsTouchScreenAvailable() const {
    // Touch screen is only available in handheld mode on real hardware
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: IsTouchScreenAvailable() — returning false");
    return false;
}

u32 SwitchPlatform::GetTouchPointCount() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetTouchPointCount() — returning 0");
    return 0;
}

SwitchTouchPoint SwitchPlatform::GetTouchPoint(u32 index) const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetTouchPoint(%u) — returning default", index);
    return SwitchTouchPoint{};
}

// --- Performance Mode ---

SwitchPerformanceMode SwitchPlatform::GetPerformanceMode() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetPerformanceMode() — returning Handheld");
    return m_CurrentMode;
}

bool SwitchPlatform::RequestPerformanceMode(SwitchPerformanceMode mode) {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: RequestPerformanceMode(%u) — Switch SDK not available",
                   static_cast<u32>(mode));
    return false;
}

f32 SwitchPlatform::GetGPUClockMHz() const {
    // Return handheld default clock
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetGPUClockMHz() — returning 307.2 (handheld default)");
    return 307.2f;
}

f32 SwitchPlatform::GetCPUClockMHz() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetCPUClockMHz() — returning 1020.0 (default)");
    return 1020.0f;
}

// --- System Info ---

SwitchCapabilities SwitchPlatform::GetCapabilities() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetCapabilities() — returning default Switch 1 capabilities");
    return SwitchCapabilities{};
}

u64 SwitchPlatform::GetAvailableMemory() const {
    // Conservative estimate: ~3 GB usable out of 4 GB total
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetAvailableMemory() — returning estimated 3 GB");
    return 3ULL * 1024 * 1024 * 1024;
}

std::string SwitchPlatform::GetConsoleSerial() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetConsoleSerial() — returning empty string");
    return "";
}

std::string SwitchPlatform::GetFirmwareVersion() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: GetFirmwareVersion() — returning empty string");
    return "";
}

bool SwitchPlatform::IsNetworkAvailable() const {
    ENJIN_LOG_WARN(Core, "SwitchPlatform stub: IsNetworkAvailable() — returning false");
    return false;
}

} // namespace Platform
} // namespace Enjin

#endif // ENJIN_PLATFORM_SWITCH
