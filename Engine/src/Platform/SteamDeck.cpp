#include "Enjin/Platform/SteamDeck.h"
#include "Enjin/Logging/Log.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <numeric>

#ifdef ENJIN_PLATFORM_LINUX
#include <fstream>
#include <filesystem>
#include <sstream>
#endif

namespace Enjin::Platform {

// =============================================================================
// Detection
// =============================================================================

bool SteamDeckSupport::IsRunningOnDeck() {
    // Method 1: Steam sets SteamDeck=1 environment variable
    const char* deckEnv = std::getenv("SteamDeck");
    if (deckEnv && std::strcmp(deckEnv, "1") == 0) {
        return true;
    }

#ifdef ENJIN_PLATFORM_LINUX
    // Method 2: Check DMI product name
    try {
        std::ifstream dmi("/sys/devices/virtual/dmi/id/product_name");
        if (dmi.is_open()) {
            std::string productName;
            std::getline(dmi, productName);
            // "Jupiter" = Steam Deck LCD, "Galileo" = Steam Deck OLED
            if (productName.find("Jupiter") != std::string::npos ||
                productName.find("Galileo") != std::string::npos) {
                return true;
            }
        }
    } catch (...) {
        // Ignore filesystem errors
    }

    // Method 3: Check for gamescope session (less reliable, but catches dev kits)
    const char* session = std::getenv("DESKTOP_SESSION");
    if (session && std::strcmp(session, "gamescope-wayland") == 0) {
        // Could be gamescope on desktop too, so only suggestive
        const char* deckVar = std::getenv("SteamGamepadUI");
        if (deckVar) return true;
    }
#endif

    return false;
}

bool SteamDeckSupport::IsRunningUnderGamescope() {
    const char* gamescope = std::getenv("GAMESCOPE_WAYLAND_DISPLAY");
    return gamescope && gamescope[0] != '\0';
}

std::string SteamDeckSupport::GetHardwareRevision() const {
#ifdef ENJIN_PLATFORM_LINUX
    try {
        std::ifstream dmi("/sys/devices/virtual/dmi/id/product_name");
        if (dmi.is_open()) {
            std::string name;
            std::getline(dmi, name);
            return name;
        }
    } catch (...) {}
#endif
    return "Unknown";
}

// =============================================================================
// Initialization
// =============================================================================

void SteamDeckSupport::Initialize() {
    if (m_Initialized) return;

    m_IsDeck = IsRunningOnDeck();
    m_Initialized = true;

    // Zero out FPS history
    std::memset(m_FPSHistory, 0, sizeof(m_FPSHistory));

    if (m_IsDeck) {
        ENJIN_LOG_INFO(Core, "Steam Deck detected (hardware: %s)", GetHardwareRevision().c_str());
        ENJIN_LOG_INFO(Core, "  Gamescope: %s", IsRunningUnderGamescope() ? "yes" : "no");

        // Initial thermal/battery read
        UpdateThermalState();
        UpdateBatteryInfo();

        if (m_BatteryPercentage >= 0) {
            ENJIN_LOG_INFO(Core, "  Battery: %d%% (%s)",
                           m_BatteryPercentage, m_IsCharging ? "charging" : "discharging");
        }
    } else {
        ENJIN_LOG_INFO(Core, "Steam Deck not detected (SteamDeckSupport available but inactive)");
    }
}

void SteamDeckSupport::Shutdown() {
    m_Initialized = false;
}

// =============================================================================
// Update
// =============================================================================

void SteamDeckSupport::Update(f32 dt) {
    if (!m_Initialized || !m_IsDeck) return;

    // Periodic thermal check
    m_ThermalCheckTimer += dt;
    if (m_ThermalCheckTimer >= THERMAL_CHECK_INTERVAL) {
        m_ThermalCheckTimer = 0.0f;
        UpdateThermalState();
        UpdateBatteryInfo();
    }

    // Track FPS for adaptive quality
    if (dt > 0.0f) {
        f32 currentFPS = 1.0f / dt;
        m_FPSHistory[m_FPSHistoryIndex % 60] = currentFPS;
        m_FPSHistoryIndex++;
    }

    // Periodic quality adjustment
    if (m_Config.adaptiveQuality) {
        m_QualityAdjustTimer += dt;
        if (m_QualityAdjustTimer >= QUALITY_ADJUST_INTERVAL) {
            m_QualityAdjustTimer = 0.0f;
            f32 avgFPS = 0.0f;
            u32 count = std::min(m_FPSHistoryIndex, static_cast<u32>(60));
            if (count > 0) {
                for (u32 i = 0; i < count; ++i) {
                    avgFPS += m_FPSHistory[i];
                }
                avgFPS /= static_cast<f32>(count);
            }
            UpdateRecommendedQuality(avgFPS);
        }
    }
}

// =============================================================================
// Thermal monitoring
// =============================================================================

void SteamDeckSupport::UpdateThermalState() {
#ifdef ENJIN_PLATFORM_LINUX
    // Read GPU temperature from thermal zones
    // Steam Deck typically has several thermal zones; we look for the GPU one
    f32 maxTemp = 0.0f;

    for (int i = 0; i < 20; ++i) {
        std::string zonePath = "/sys/class/thermal/thermal_zone" + std::to_string(i) + "/temp";
        try {
            std::ifstream tempFile(zonePath);
            if (!tempFile.is_open()) break;

            i64 tempMilliC = 0;
            tempFile >> tempMilliC;
            f32 tempC = static_cast<f32>(tempMilliC) / 1000.0f;

            if (tempC > maxTemp) {
                maxTemp = tempC;
            }
        } catch (...) {
            break;
        }
    }

    m_GPUTemp = maxTemp;

    // Classify thermal state
    if (maxTemp < 65.0f) {
        m_ThermalState = ThermalState::Cool;
    } else if (maxTemp < 80.0f) {
        m_ThermalState = ThermalState::Warm;
    } else if (maxTemp < 95.0f) {
        m_ThermalState = ThermalState::Hot;
    } else {
        m_ThermalState = ThermalState::Critical;
    }
#endif
}

void SteamDeckSupport::UpdateBatteryInfo() {
#ifdef ENJIN_PLATFORM_LINUX
    // Read battery info from power_supply
    // Steam Deck battery is typically at /sys/class/power_supply/BAT1/
    const char* batteryPaths[] = {
        "/sys/class/power_supply/BAT1",
        "/sys/class/power_supply/BAT0",
        "/sys/class/power_supply/battery"
    };

    for (const auto& basePath : batteryPaths) {
        std::string capacityPath = std::string(basePath) + "/capacity";
        std::string statusPath = std::string(basePath) + "/status";

        try {
            std::ifstream capFile(capacityPath);
            if (!capFile.is_open()) continue;

            i32 capacity = 0;
            capFile >> capacity;
            m_BatteryPercentage = std::clamp(capacity, 0, 100);

            std::ifstream statusFile(statusPath);
            if (statusFile.is_open()) {
                std::string status;
                std::getline(statusFile, status);
                m_IsCharging = (status == "Charging" || status == "Full");
            }
            return; // Found battery, done
        } catch (...) {
            continue;
        }
    }

    // No battery found (desktop or AC-only)
    m_BatteryPercentage = -1;
    m_IsCharging = false;
#endif
}

// =============================================================================
// Adaptive Quality
// =============================================================================

void SteamDeckSupport::UpdateRecommendedQuality(f32 currentFPS) {
    DeckQualityLevel newLevel = m_RecommendedQuality;
    f32 targetFPS = static_cast<f32>(m_Config.targetFPS);

    // If thermal throttling, always reduce
    if (m_ThermalState == ThermalState::Critical) {
        newLevel = DeckQualityLevel::Low;
    } else if (m_ThermalState == ThermalState::Hot) {
        // Don't go above Medium when hot
        if (newLevel == DeckQualityLevel::High) {
            newLevel = DeckQualityLevel::Medium;
        }
    }

    // FPS-based adjustment
    if (currentFPS > 0.0f) {
        f32 ratio = currentFPS / targetFPS;

        if (ratio < 0.85f) {
            // Significantly below target, reduce quality
            if (newLevel == DeckQualityLevel::High) {
                newLevel = DeckQualityLevel::Medium;
            } else if (newLevel == DeckQualityLevel::Medium) {
                newLevel = DeckQualityLevel::Low;
            }
        } else if (ratio > 1.1f && m_ThermalState <= ThermalState::Warm) {
            // Comfortably above target and cool, try raising quality
            if (newLevel == DeckQualityLevel::Low) {
                newLevel = DeckQualityLevel::Medium;
            } else if (newLevel == DeckQualityLevel::Medium) {
                newLevel = DeckQualityLevel::High;
            }
        }
    }

    // Battery conservation: don't go above Medium when battery is low
    if (m_BatteryPercentage >= 0 && m_BatteryPercentage < 15 && !m_IsCharging) {
        if (newLevel == DeckQualityLevel::High) {
            newLevel = DeckQualityLevel::Medium;
        }
    }

    if (newLevel != m_RecommendedQuality) {
        ENJIN_LOG_INFO(Core, "Steam Deck adaptive quality: %s -> %s (FPS: %.1f, Temp: %.1fC, Battery: %d%%)",
                       newLevel > m_RecommendedQuality ? "increased" : "decreased",
                       newLevel == DeckQualityLevel::High ? "High" :
                       newLevel == DeckQualityLevel::Medium ? "Medium" : "Low",
                       currentFPS, m_GPUTemp, m_BatteryPercentage);
        m_RecommendedQuality = newLevel;
    }
}

u32 SteamDeckSupport::GetRecommendedShadowResolution() const {
    switch (m_RecommendedQuality) {
        case DeckQualityLevel::High:   return 2048;
        case DeckQualityLevel::Medium: return 1024;
        case DeckQualityLevel::Low:    return 512;
    }
    return 1024;
}

f32 SteamDeckSupport::GetRecommendedRenderScale() const {
    switch (m_RecommendedQuality) {
        case DeckQualityLevel::High:   return 1.0f;
        case DeckQualityLevel::Medium: return 0.8f;
        case DeckQualityLevel::Low:    return 0.65f;
    }
    return 1.0f;
}

u32 SteamDeckSupport::GetRecommendedMaxParticles() const {
    switch (m_RecommendedQuality) {
        case DeckQualityLevel::High:   return 8192;
        case DeckQualityLevel::Medium: return 4096;
        case DeckQualityLevel::Low:    return 2048;
    }
    return 4096;
}

// =============================================================================
// Suspend/Resume
// =============================================================================

void SteamDeckSupport::HandleSuspend() {
    if (!m_Initialized) return;

    ENJIN_LOG_INFO(Core, "Steam Deck: suspend event");
    m_WasSuspended = true;

    if (m_SuspendCallback) {
        m_SuspendCallback();
    }
}

void SteamDeckSupport::HandleResume() {
    if (!m_Initialized || !m_WasSuspended) return;

    ENJIN_LOG_INFO(Core, "Steam Deck: resume event");
    m_WasSuspended = false;

    // Re-read thermal state after resume (temps may have changed)
    UpdateThermalState();
    UpdateBatteryInfo();

    // Reset FPS history (stale data from before suspend)
    std::memset(m_FPSHistory, 0, sizeof(m_FPSHistory));
    m_FPSHistoryIndex = 0;

    if (m_ResumeCallback) {
        m_ResumeCallback();
    }
}

// =============================================================================
// Gyroscope
// =============================================================================

void SteamDeckSupport::UpdateGyro(f32 dt) {
    if (!m_Config.gyroEnabled) {
        m_GyroPitch = 0.0f;
        m_GyroYaw = 0.0f;
        m_GyroRoll = 0.0f;
        return;
    }

    // Gyro data comes from SteamInput API (requires ENJIN_STEAM).
    // Without the Steamworks SDK, this is a no-op stub.
    // When ENJIN_STEAM is enabled, SteamInputManager::PollInput() will call
    // SetGyroState() to provide real gyro data.
    (void)dt;
}

} // namespace Enjin::Platform
