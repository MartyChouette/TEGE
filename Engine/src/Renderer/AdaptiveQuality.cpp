#include "Enjin/Renderer/AdaptiveQuality.h"
#include "Enjin/Logging/Log.h"

#include <cstring>
#include <cmath>
#include <algorithm>

namespace Enjin::Renderer {

// =============================================================================
// Lifecycle
// =============================================================================

void AdaptiveQualitySystem::Initialize(const AdaptiveQualityConfig& config) {
    m_Config = config;
    m_CurrentLevel = config.maxLevel;
    m_Initialized = true;

    std::memset(m_FPSHistory, 0, sizeof(m_FPSHistory));
    m_FPSHistoryIndex = 0;
    m_FPSHistoryCount = 0;
    m_AverageFPS = config.targetFPS;
    m_FPSVariance = 0.0f;
    m_Timer = 0.0f;
    m_TimeSinceAdjustment = 0.0f;
    m_UpgradeCooldown = 0.0f;
    m_DowngradeCooldown = 0.0f;
    m_AdjustmentCount = 0;

    ENJIN_LOG_INFO(Renderer, "Adaptive quality initialized (target: %.0f FPS, level: %s)",
                   config.targetFPS, GetLevelName(m_CurrentLevel));
}

void AdaptiveQualitySystem::Reset(QualityLevel level) {
    QualityLevel clamped = static_cast<QualityLevel>(
        std::clamp(static_cast<u8>(level),
                   static_cast<u8>(m_Config.minLevel),
                   static_cast<u8>(m_Config.maxLevel))
    );
    SetLevel(clamped);

    std::memset(m_FPSHistory, 0, sizeof(m_FPSHistory));
    m_FPSHistoryIndex = 0;
    m_FPSHistoryCount = 0;
    m_Timer = 0.0f;
    m_UpgradeCooldown = 0.0f;
    m_DowngradeCooldown = 0.0f;
}

// =============================================================================
// Update
// =============================================================================

void AdaptiveQualitySystem::Update(f32 dt, f32 currentFPS) {
    if (!m_Initialized || !m_Enabled) return;
    if (dt <= 0.0f) return;

    // Record FPS
    m_FPSHistory[m_FPSHistoryIndex % FPS_HISTORY_SIZE] = currentFPS;
    m_FPSHistoryIndex++;
    if (m_FPSHistoryCount < FPS_HISTORY_SIZE) {
        m_FPSHistoryCount++;
    }

    // Compute average and variance
    if (m_FPSHistoryCount > 0) {
        f32 sum = 0.0f;
        for (u32 i = 0; i < m_FPSHistoryCount; ++i) {
            sum += m_FPSHistory[i];
        }
        m_AverageFPS = sum / static_cast<f32>(m_FPSHistoryCount);

        f32 varianceSum = 0.0f;
        for (u32 i = 0; i < m_FPSHistoryCount; ++i) {
            f32 diff = m_FPSHistory[i] - m_AverageFPS;
            varianceSum += diff * diff;
        }
        m_FPSVariance = varianceSum / static_cast<f32>(m_FPSHistoryCount);
    }

    // Update timers
    m_Timer += dt;
    m_TimeSinceAdjustment += dt;
    if (m_UpgradeCooldown > 0.0f) m_UpgradeCooldown -= dt;
    if (m_DowngradeCooldown > 0.0f) m_DowngradeCooldown -= dt;

    // Evaluate quality at the configured interval
    if (m_Timer >= m_Config.adjustInterval) {
        m_Timer = 0.0f;
        EvaluateQuality();
    }

    // Emergency downgrade: if FPS drops critically, don't wait for the interval
    if (currentFPS < m_Config.minFPS && m_DowngradeCooldown <= 0.0f) {
        u8 currentU8 = static_cast<u8>(m_CurrentLevel);
        u8 minU8 = static_cast<u8>(m_Config.minLevel);
        if (currentU8 > minU8) {
            ENJIN_LOG_WARN(Renderer, "Adaptive quality: emergency downgrade (FPS: %.1f < min: %.1f)",
                           currentFPS, m_Config.minFPS);
            SetLevel(static_cast<QualityLevel>(currentU8 - 1));
            m_DowngradeCooldown = DOWNGRADE_COOLDOWN;
            m_UpgradeCooldown = UPGRADE_COOLDOWN; // Reset upgrade cooldown after downgrade
        }
    }
}

void AdaptiveQualitySystem::EvaluateQuality() {
    if (m_FPSHistoryCount < 10) return; // Need enough samples

    f32 target = m_Config.targetFPS;
    f32 avg = m_AverageFPS;
    f32 hysteresis = m_Config.hysteresis;

    u8 currentU8 = static_cast<u8>(m_CurrentLevel);
    u8 minU8 = static_cast<u8>(m_Config.minLevel);
    u8 maxU8 = static_cast<u8>(m_Config.maxLevel);

    // Check if we should downgrade
    if (avg < (target - hysteresis) && m_DowngradeCooldown <= 0.0f) {
        if (currentU8 > minU8) {
            // Check if FPS is consistently low (not just a spike)
            u32 belowCount = 0;
            u32 sampleCount = std::min(m_FPSHistoryCount, static_cast<u32>(30));
            for (u32 i = 0; i < sampleCount; ++i) {
                u32 idx = (m_FPSHistoryIndex - 1 - i) % FPS_HISTORY_SIZE;
                if (m_FPSHistory[idx] < target - hysteresis) {
                    belowCount++;
                }
            }
            // Downgrade if >60% of recent samples are below target
            if (belowCount > sampleCount * 6 / 10) {
                SetLevel(static_cast<QualityLevel>(currentU8 - 1));
                m_DowngradeCooldown = DOWNGRADE_COOLDOWN;
                m_UpgradeCooldown = UPGRADE_COOLDOWN;
            }
        }
    }
    // Check if we should upgrade
    else if (avg > (target + hysteresis) && m_UpgradeCooldown <= 0.0f) {
        if (currentU8 < maxU8) {
            // Only upgrade if FPS is consistently above target (stability check)
            u32 aboveCount = 0;
            u32 sampleCount = std::min(m_FPSHistoryCount, static_cast<u32>(60));
            for (u32 i = 0; i < sampleCount; ++i) {
                u32 idx = (m_FPSHistoryIndex - 1 - i) % FPS_HISTORY_SIZE;
                if (m_FPSHistory[idx] > target + hysteresis / 2.0f) {
                    aboveCount++;
                }
            }
            // Upgrade only if >80% of recent samples are above target
            // (be conservative with upgrades to avoid oscillation)
            if (aboveCount > sampleCount * 8 / 10) {
                SetLevel(static_cast<QualityLevel>(currentU8 + 1));
                m_UpgradeCooldown = UPGRADE_COOLDOWN;
            }
        }
    }
}

void AdaptiveQualitySystem::SetLevel(QualityLevel level) {
    if (level == m_CurrentLevel) return;

    QualityLevel old = m_CurrentLevel;
    m_CurrentLevel = level;
    m_TimeSinceAdjustment = 0.0f;
    m_AdjustmentCount++;

    ENJIN_LOG_INFO(Renderer, "Adaptive quality: %s -> %s (avg FPS: %.1f, target: %.1f)",
                   GetLevelName(old), GetLevelName(level), m_AverageFPS, m_Config.targetFPS);

    if (m_OnQualityChange) {
        m_OnQualityChange(old, level);
    }
}

// =============================================================================
// Quality level name
// =============================================================================

const char* AdaptiveQualitySystem::GetLevelName(QualityLevel level) {
    switch (level) {
        case QualityLevel::VeryLow: return "Very Low";
        case QualityLevel::Low:     return "Low";
        case QualityLevel::Medium:  return "Medium";
        case QualityLevel::High:    return "High";
        case QualityLevel::Ultra:   return "Ultra";
    }
    return "Unknown";
}

// =============================================================================
// Recommended settings
// =============================================================================

u32 AdaptiveQualitySystem::GetRecommendedShadowResolution() const {
    switch (m_CurrentLevel) {
        case QualityLevel::VeryLow: return 256;
        case QualityLevel::Low:     return 512;
        case QualityLevel::Medium:  return 1024;
        case QualityLevel::High:    return 2048;
        case QualityLevel::Ultra:   return 4096;
    }
    return 1024;
}

u32 AdaptiveQualitySystem::GetRecommendedParticleCount() const {
    switch (m_CurrentLevel) {
        case QualityLevel::VeryLow: return 1024;
        case QualityLevel::Low:     return 2048;
        case QualityLevel::Medium:  return 4096;
        case QualityLevel::High:    return 8192;
        case QualityLevel::Ultra:   return 16384;
    }
    return 4096;
}

f32 AdaptiveQualitySystem::GetRecommendedRenderScale() const {
    switch (m_CurrentLevel) {
        case QualityLevel::VeryLow: return 0.5f;
        case QualityLevel::Low:     return 0.65f;
        case QualityLevel::Medium:  return 0.8f;
        case QualityLevel::High:    return 1.0f;
        case QualityLevel::Ultra:   return 1.0f;
    }
    return 1.0f;
}

f32 AdaptiveQualitySystem::GetRecommendedLODBias() const {
    switch (m_CurrentLevel) {
        case QualityLevel::VeryLow: return 0.5f;
        case QualityLevel::Low:     return 0.7f;
        case QualityLevel::Medium:  return 1.0f;
        case QualityLevel::High:    return 1.2f;
        case QualityLevel::Ultra:   return 1.5f;
    }
    return 1.0f;
}

u32 AdaptiveQualitySystem::GetRecommendedShadowCascades() const {
    switch (m_CurrentLevel) {
        case QualityLevel::VeryLow: return 1;
        case QualityLevel::Low:     return 2;
        case QualityLevel::Medium:  return 3;
        case QualityLevel::High:    return 4;
        case QualityLevel::Ultra:   return 4;
    }
    return 4;
}

u32 AdaptiveQualitySystem::GetRecommendedSSAOSamples() const {
    switch (m_CurrentLevel) {
        case QualityLevel::VeryLow: return 0;
        case QualityLevel::Low:     return 8;
        case QualityLevel::Medium:  return 16;
        case QualityLevel::High:    return 32;
        case QualityLevel::Ultra:   return 64;
    }
    return 16;
}

u32 AdaptiveQualitySystem::GetRecommendedBloomPasses() const {
    switch (m_CurrentLevel) {
        case QualityLevel::VeryLow: return 0;
        case QualityLevel::Low:     return 3;
        case QualityLevel::Medium:  return 5;
        case QualityLevel::High:    return 6;
        case QualityLevel::Ultra:   return 8;
    }
    return 5;
}

bool AdaptiveQualitySystem::ShouldEnableFXAA() const {
    return m_CurrentLevel >= QualityLevel::Low;
}

bool AdaptiveQualitySystem::ShouldEnableSSAO() const {
    return m_CurrentLevel >= QualityLevel::Low;
}

bool AdaptiveQualitySystem::ShouldEnableBloom() const {
    return m_CurrentLevel >= QualityLevel::Low;
}

bool AdaptiveQualitySystem::ShouldEnableVolumetrics() const {
    return m_CurrentLevel >= QualityLevel::High;
}

} // namespace Enjin::Renderer
