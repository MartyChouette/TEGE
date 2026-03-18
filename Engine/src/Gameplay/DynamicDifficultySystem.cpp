#include "Enjin/Gameplay/DynamicDifficultySystem.h"
#include "Enjin/ECS/Components/DynamicDifficulty.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>

namespace Enjin {
namespace Gameplay {

void DynamicDifficultySystem::Update(ECS::World* world, f32 deltaTime) {
    if (!m_Enabled || !world) return;

    // Accumulate elapsed time on tracked entities
    auto entities = world->GetEntitiesWithComponent<ECS::DynamicDifficultyComponent>();
    for (auto entity : entities) {
        auto* dd = world->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (!dd || !dd->enabled) continue;

        // Tick elapsed time for the time-tracking metric
        if (dd->trackTime) {
            dd->elapsedTime += deltaTime;
        }
    }

    // Only recompute scores once per interval
    m_TimeSinceLastUpdate += deltaTime;
    if (m_TimeSinceLastUpdate < m_UpdateInterval) return;
    m_TimeSinceLastUpdate = 0.0f;

    Recompute(world);
}

void DynamicDifficultySystem::Recompute(ECS::World* world) {
    if (!world) return;

    auto entities = world->GetEntitiesWithComponent<ECS::DynamicDifficultyComponent>();
    for (auto entity : entities) {
        auto* dd = world->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (!dd || !dd->enabled) continue;

        // Accumulate weighted scores
        f32 totalWeight = 0.0f;
        f32 weightedSum = 0.0f;

        // --- Death score ---
        if (dd->trackDeaths) {
            // 5+ recent deaths = max struggle (score 1.0)
            f32 deathScore = std::min(static_cast<f32>(dd->recentDeaths) / 5.0f, 1.0f);
            weightedSum += deathScore * dd->deathWeight;
            totalWeight += dd->deathWeight;
        }

        // --- Health score (read from HealthComponent on player entity) ---
        if (dd->trackHealth && dd->playerEntity != ECS::INVALID_ENTITY) {
            auto* health = world->GetComponent<ECS::HealthComponent>(dd->playerEntity);
            if (health && health->maxHealth > 0.0f) {
                f32 healthScore = 1.0f - (health->currentHealth / health->maxHealth);
                healthScore = std::max(0.0f, std::min(healthScore, 1.0f));
                weightedSum += healthScore * dd->healthWeight;
                totalWeight += dd->healthWeight;
            }
        }

        // --- Accuracy score ---
        if (dd->trackAccuracy) {
            f32 accuracyScore = 1.0f;
            if (dd->shotsFired > 0) {
                accuracyScore = 1.0f - (static_cast<f32>(dd->shotsHit) / static_cast<f32>(dd->shotsFired));
            }
            accuracyScore = std::max(0.0f, std::min(accuracyScore, 1.0f));
            weightedSum += accuracyScore * dd->accuracyWeight;
            totalWeight += dd->accuracyWeight;
        }

        // --- Time score ---
        if (dd->trackTime && dd->expectedCompletionTime > 0.0f) {
            f32 timeScore = std::min(dd->elapsedTime / dd->expectedCompletionTime, 2.0f) / 2.0f;
            timeScore = std::max(0.0f, std::min(timeScore, 1.0f));
            weightedSum += timeScore * dd->timeWeight;
            totalWeight += dd->timeWeight;
        }

        // --- Resource score ---
        if (dd->trackResources) {
            f32 resourceScore = 1.0f - std::max(0.0f, std::min(dd->resourceRatio, 1.0f));
            weightedSum += resourceScore * dd->resourceWeight;
            totalWeight += dd->resourceWeight;
        }

        // --- Checkpoint health score ---
        if (dd->trackCheckpointHealth) {
            f32 checkpointScore = 1.0f - std::max(0.0f, std::min(dd->lastCheckpointHealthPercent, 1.0f));
            weightedSum += checkpointScore * dd->checkpointHealthWeight;
            totalWeight += dd->checkpointHealthWeight;
        }

        // Compute raw score (weighted average)
        f32 rawScore = (totalWeight > 0.0f) ? (weightedSum / totalWeight) : 0.5f;
        rawScore = std::max(0.0f, std::min(rawScore, 1.0f));

        // Apply base difficulty offset: map baseDifficulty 0-3 to center bias
        // Easy(0) → bias toward lower score, Hard(2)/Nightmare(3) → bias toward higher
        f32 baseBias = static_cast<f32>(dd->baseDifficulty) / 3.0f; // 0.0 to 1.0
        f32 adjustedScore = rawScore * dd->adjustmentRange + baseBias * (1.0f - dd->adjustmentRange);
        adjustedScore = std::max(0.0f, std::min(adjustedScore, 1.0f));

        dd->difficultyScore = adjustedScore;

        // Exponential moving average for smoothing
        dd->smoothedScore += dd->smoothingRate * (adjustedScore - dd->smoothedScore);
        dd->smoothedScore = std::max(0.0f, std::min(dd->smoothedScore, 1.0f));

        // --- Compute output multipliers ---
        // "struggle factor" = how much the player is struggling (high smoothedScore = doing well,
        // low smoothedScore = struggling). We invert: easeFactor = 1 - smoothedScore.
        f32 easeFactor = 1.0f - dd->smoothedScore;

        // Enemy damage: lower when struggling (easeFactor high → reduce damage)
        if (dd->adjustEnemyDamage) {
            dd->enemyDamageMultiplier = 1.0f - easeFactor * dd->enemyDamageRange;
        }

        // Enemy health: lower when struggling
        if (dd->adjustEnemyHealth) {
            dd->enemyHealthMultiplier = 1.0f - easeFactor * dd->enemyHealthRange;
        }

        // AI aggression: lower when struggling
        if (dd->adjustAIAggression) {
            dd->aiAggressionMultiplier = 1.0f - easeFactor * dd->aiAggressionRange;
        }

        // Resource drops: higher when struggling (inverse)
        if (dd->adjustResourceDrops) {
            dd->resourceDropMultiplier = 1.0f + easeFactor * dd->resourceDropRange;
        }

        // Hint frequency: more hints when struggling
        if (dd->adjustHintFrequency) {
            // Reduce cooldown when struggling: base 30s → as low as 10s
            dd->hintCooldown = 30.0f - easeFactor * 20.0f;
            dd->hintCooldown = std::max(5.0f, dd->hintCooldown);
        }

        // Checkpoint frequency: more frequent when struggling
        if (dd->adjustCheckpointFrequency) {
            dd->checkpointMultiplier = 1.0f - easeFactor * dd->checkpointRange;
        }
    }
}

f32 DynamicDifficultySystem::GetMultiplier(const std::string& which) const {
    if (!m_World) return 1.0f;

    auto entities = m_World->GetEntitiesWithComponent<ECS::DynamicDifficultyComponent>();
    for (auto entity : entities) {
        auto* dd = m_World->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (!dd || !dd->enabled) continue;

        if (which == "enemyDamage") return dd->enemyDamageMultiplier;
        if (which == "enemyHealth") return dd->enemyHealthMultiplier;
        if (which == "aiAggression") return dd->aiAggressionMultiplier;
        if (which == "resourceDrops") return dd->resourceDropMultiplier;
        if (which == "checkpoint") return dd->checkpointMultiplier;
        break; // Only use the first component found
    }
    return 1.0f;
}

f32 DynamicDifficultySystem::GetScore() const {
    if (!m_World) return 0.5f;

    auto entities = m_World->GetEntitiesWithComponent<ECS::DynamicDifficultyComponent>();
    for (auto entity : entities) {
        auto* dd = m_World->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (!dd || !dd->enabled) continue;
        return dd->smoothedScore;
    }
    return 0.5f;
}

void DynamicDifficultySystem::RecordDeath() {
    if (!m_World) return;

    auto entities = m_World->GetEntitiesWithComponent<ECS::DynamicDifficultyComponent>();
    for (auto entity : entities) {
        auto* dd = m_World->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (!dd || !dd->enabled) continue;
        dd->recentDeaths++;
        ENJIN_LOG_INFO(Game, "Dynamic difficulty: death recorded (total recent: %u)", dd->recentDeaths);
        break;
    }
}

void DynamicDifficultySystem::RecordShot() {
    if (!m_World) return;

    auto entities = m_World->GetEntitiesWithComponent<ECS::DynamicDifficultyComponent>();
    for (auto entity : entities) {
        auto* dd = m_World->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (!dd || !dd->enabled) continue;
        dd->shotsFired++;
        break;
    }
}

void DynamicDifficultySystem::RecordHit() {
    if (!m_World) return;

    auto entities = m_World->GetEntitiesWithComponent<ECS::DynamicDifficultyComponent>();
    for (auto entity : entities) {
        auto* dd = m_World->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (!dd || !dd->enabled) continue;
        dd->shotsHit++;
        break;
    }
}

void DynamicDifficultySystem::SetBaseDifficulty(u32 level) {
    if (!m_World) return;

    auto entities = m_World->GetEntitiesWithComponent<ECS::DynamicDifficultyComponent>();
    for (auto entity : entities) {
        auto* dd = m_World->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (!dd) continue;
        dd->baseDifficulty = std::min(level, 3u);
        ENJIN_LOG_INFO(Game, "Dynamic difficulty: base difficulty set to %u", dd->baseDifficulty);
        break;
    }
}

void DynamicDifficultySystem::RecordCheckpointHealth(f32 healthPercent) {
    if (!m_World) return;

    auto entities = m_World->GetEntitiesWithComponent<ECS::DynamicDifficultyComponent>();
    for (auto entity : entities) {
        auto* dd = m_World->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (!dd || !dd->enabled) continue;
        dd->lastCheckpointHealthPercent = std::max(0.0f, std::min(healthPercent, 1.0f));
        break;
    }
}

} // namespace Gameplay
} // namespace Enjin
