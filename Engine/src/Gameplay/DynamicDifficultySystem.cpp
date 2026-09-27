#include "Enjin/Gameplay/DynamicDifficultySystem.h"
#include "Enjin/ECS/Components/DynamicDifficulty.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

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
        UpdateDeathsAndHints(entity, *dd, deltaTime);
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

void DynamicDifficultySystem::UpdateDeathsAndHints(ECS::Entity entity, ECS::DynamicDifficultyComponent& dd,
                                                   f32 deltaTime) {
    dd.clock += deltaTime;
    dd.timeSinceHint += deltaTime;

    // recentDeaths is written by Difficulty_RecordDeath and Difficulty_Reset
    // as well as RecordDeath here, so it is reconciled against the timestamps
    // rather than trusted: more deaths than stamps means new ones now, fewer
    // means a reset. Deaths never aged before (SD-27): one bad minute at the
    // start of a level eased the rest of it however well the player did.
    const bool wasReset = dd.recentDeaths < dd.deathTimes.size() && dd.recentDeaths == 0;
    if (dd.recentDeaths > dd.deathTimes.size()) {
        dd.deathTimes.resize(dd.recentDeaths, dd.clock);
    } else if (dd.recentDeaths < dd.deathTimes.size()) {
        dd.deathTimes.erase(dd.deathTimes.begin(), dd.deathTimes.end() - dd.recentDeaths);
    }
    if (dd.deathWindow > 0) {
        const f32 oldest = dd.clock - static_cast<f32>(dd.deathWindow);
        dd.deathTimes.erase(std::remove_if(dd.deathTimes.begin(), dd.deathTimes.end(),
                                           [oldest](f32 t) { return t < oldest; }),
                            dd.deathTimes.end());
    }
    dd.recentDeaths = static_cast<u32>(dd.deathTimes.size());
    // Difficulty_Reset (a new section) zeroes the deaths; the hint count goes with them
    if (wasReset) dd.hintsBeforeSection = 0;

    if (!dd.adjustHintFrequency || dd.deathsBeforeHint == 0) return;
    if (dd.recentDeaths < dd.deathsBeforeHint || dd.timeSinceHint < dd.hintCooldown) return;
    dd.timeSinceHint = 0.0f;
    dd.hintsBeforeSection++;
    if (!m_EventBus) return;
    ECS::EntityEvent ev;
    ev.name = "difficulty_hint";
    ev.sender = entity;
    ev.ints["hint"] = static_cast<i32>(dd.hintsBeforeSection);
    ev.ints["deaths"] = static_cast<i32>(dd.recentDeaths);
    m_EventBus->Send(ev.name, ev);   // dd is not touched after this
}

void DynamicDifficultySystem::RenderOverlay(f32 originX, f32 originY, u32 viewportWidth, u32 viewportHeight) {
    if (!m_Enabled || !m_World || viewportWidth == 0 || viewportHeight == 0) return;
    const ECS::DynamicDifficultyComponent* shown = nullptr;
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::DynamicDifficultyComponent>()) {
        const auto* dd = m_World->GetComponent<ECS::DynamicDifficultyComponent>(entity);
        if (dd && dd->enabled) { shown = dd; break; }
    }
    // Visible To Player was saved and shown in the inspector and nothing drew
    // it (SD-27). The adjustment is how far the smoothed score sits from where
    // the base level alone would put it.
    if (!shown || !shown->visibleToPlayer) return;
    static const char* kLevels[] = {"Easy", "Normal", "Hard", "Nightmare"};
    const u32 base = std::min(shown->baseDifficulty, 3u);
    const f32 baseBias = static_cast<f32>(base) / 3.0f;
    const f32 delta = (shown->smoothedScore - baseBias) * 100.0f;
    const i32 adjust = static_cast<i32>(delta >= 0.0f ? delta + 0.5f : delta - 0.5f);
    char text[64];
    if (adjust == 0) std::snprintf(text, sizeof(text), "Difficulty: %s", kLevels[base]);
    else std::snprintf(text, sizeof(text), "Difficulty: %s  %+d%%", kLevels[base], adjust);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImVec2 size = ImGui::CalcTextSize(text);
    const f32 pad = 6.0f, margin = 12.0f;
    const ImVec2 boxMax(originX + static_cast<f32>(viewportWidth) - margin, originY + margin + size.y + pad * 2.0f);
    const ImVec2 boxMin(boxMax.x - size.x - pad * 2.0f, originY + margin);
    dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 130), 4.0f);
    dl->AddText(ImVec2(boxMin.x + pad, boxMin.y + pad), IM_COL32(255, 255, 255, 230), text);
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
