#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <functional>

namespace Enjin::Renderer {

// Quality level for adaptive rendering
enum class QualityLevel : u8 {
    VeryLow = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    Ultra = 4
};

// Configuration for the adaptive quality system
struct AdaptiveQualityConfig {
    f32 targetFPS = 60.0f;          // Target framerate to maintain
    f32 minFPS = 24.0f;             // Below this, aggressively reduce quality
    f32 maxFPS = 120.0f;            // Max FPS before considering quality increase
    f32 adjustInterval = 3.0f;      // Seconds between quality adjustments
    f32 hysteresis = 5.0f;          // FPS margin to prevent oscillation

    QualityLevel minLevel = QualityLevel::VeryLow;   // Minimum allowed quality
    QualityLevel maxLevel = QualityLevel::Ultra;      // Maximum allowed quality

    bool adjustShadows = true;       // Allow shadow resolution changes
    bool adjustParticles = true;     // Allow particle count changes
    bool adjustRenderScale = true;   // Allow render scale changes (dynamic resolution)
    bool adjustLODBias = true;       // Allow LOD distance bias changes
};

// Callback when quality level changes: (old level, new level)
using QualityChangeCallback = std::function<void(QualityLevel, QualityLevel)>;

// Adaptive quality system that dynamically adjusts rendering parameters
// to maintain a target framerate. Works on all platforms but especially
// useful on constrained hardware like Steam Deck, consoles, and mobile.
class ENJIN_API AdaptiveQualitySystem {
public:
    AdaptiveQualitySystem() = default;
    ~AdaptiveQualitySystem() = default;

    // =============================================================================
    // Lifecycle
    // =============================================================================

    // Initialize with a configuration
    void Initialize(const AdaptiveQualityConfig& config = {});

    // Update the system. Call every frame with delta time and current measured FPS.
    void Update(f32 dt, f32 currentFPS);

    // Reset to a specific quality level
    void Reset(QualityLevel level = QualityLevel::High);

    // =============================================================================
    // Configuration
    // =============================================================================

    const AdaptiveQualityConfig& GetConfig() const { return m_Config; }
    void SetConfig(const AdaptiveQualityConfig& config) { m_Config = config; }

    void SetTargetFPS(f32 fps) { m_Config.targetFPS = fps; }
    f32 GetTargetFPS() const { return m_Config.targetFPS; }

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // =============================================================================
    // Quality queries
    // =============================================================================

    // Get the current quality level
    QualityLevel GetCurrentLevel() const { return m_CurrentLevel; }

    // Get a string name for a quality level
    static const char* GetLevelName(QualityLevel level);

    // =============================================================================
    // Recommended settings for each quality level
    // =============================================================================

    // Shadow map resolution: VeryLow=256, Low=512, Medium=1024, High=2048, Ultra=4096
    u32 GetRecommendedShadowResolution() const;

    // Max particle count: VeryLow=1024, Low=2048, Medium=4096, High=8192, Ultra=16384
    u32 GetRecommendedParticleCount() const;

    // Render scale (dynamic resolution): VeryLow=0.5, Low=0.65, Medium=0.8, High=1.0, Ultra=1.0
    f32 GetRecommendedRenderScale() const;

    // LOD bias multiplier: VeryLow=0.5, Low=0.7, Medium=1.0, High=1.2, Ultra=1.5
    // Higher values = farther LOD transitions = more detail at distance
    f32 GetRecommendedLODBias() const;

    // Shadow cascade count: VeryLow=1, Low=2, Medium=3, High=4, Ultra=4
    u32 GetRecommendedShadowCascades() const;

    // SSAO samples: VeryLow=0 (off), Low=8, Medium=16, High=32, Ultra=64
    u32 GetRecommendedSSAOSamples() const;

    // Bloom quality: VeryLow=0 (off), Low=3, Medium=5, High=6, Ultra=8 (mip levels)
    u32 GetRecommendedBloomPasses() const;

    // Post-processing toggles per level
    bool ShouldEnableFXAA() const;
    bool ShouldEnableSSAO() const;
    bool ShouldEnableBloom() const;
    bool ShouldEnableVolumetrics() const;

    // =============================================================================
    // Callbacks
    // =============================================================================

    // Set callback for quality level changes
    void SetQualityChangeCallback(QualityChangeCallback cb) { m_OnQualityChange = std::move(cb); }

    // =============================================================================
    // Statistics
    // =============================================================================

    // Average FPS over the measurement window
    f32 GetAverageFPS() const { return m_AverageFPS; }

    // Number of quality adjustments since initialization
    u32 GetAdjustmentCount() const { return m_AdjustmentCount; }

    // Time since last quality adjustment
    f32 GetTimeSinceLastAdjustment() const { return m_TimeSinceAdjustment; }

    // FPS stability (0.0 = perfectly stable, higher = more variance)
    f32 GetFPSVariance() const { return m_FPSVariance; }

private:
    // Evaluate whether quality should change
    void EvaluateQuality();

    // Apply a new quality level
    void SetLevel(QualityLevel level);

    AdaptiveQualityConfig m_Config;
    bool m_Enabled = true;
    bool m_Initialized = false;

    QualityLevel m_CurrentLevel = QualityLevel::High;

    // FPS tracking
    static constexpr u32 FPS_HISTORY_SIZE = 120;
    f32 m_FPSHistory[FPS_HISTORY_SIZE]{};
    u32 m_FPSHistoryIndex = 0;
    u32 m_FPSHistoryCount = 0;
    f32 m_AverageFPS = 0.0f;
    f32 m_FPSVariance = 0.0f;

    // Timing
    f32 m_Timer = 0.0f;
    f32 m_TimeSinceAdjustment = 0.0f;

    // Cooldown to prevent rapid oscillation
    f32 m_UpgradeCooldown = 0.0f;
    f32 m_DowngradeCooldown = 0.0f;
    static constexpr f32 UPGRADE_COOLDOWN = 8.0f;   // Wait 8s before upgrading again
    static constexpr f32 DOWNGRADE_COOLDOWN = 2.0f;  // Can downgrade faster (2s)

    // Stats
    u32 m_AdjustmentCount = 0;

    QualityChangeCallback m_OnQualityChange;
};

} // namespace Enjin::Renderer
