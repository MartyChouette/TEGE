#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace ECS {

// ============================================================================
// Dynamic Difficulty Component
// ============================================================================
// Opt-in component that tracks player performance metrics and computes
// difficulty adjustment multipliers. Each metric source and adjustment
// target is independently toggleable. Attach to a single entity per scene
// (singleton-style). Game systems query the output multipliers to scale
// enemy damage, health, resource drops, etc.

struct DynamicDifficultyComponent {
    // --- Mode ---
    bool enabled = true;
    bool visibleToPlayer = false;     // Transparent mode: show difficulty indicator in HUD

    // --- Base difficulty (player-chosen) ---
    // 0=Easy, 1=Normal, 2=Hard, 3=Nightmare (or custom labels)
    u32 baseDifficulty = 1;
    f32 adjustmentRange = 0.2f;       // +/-20% auto-adjustment around base

    // --- Current computed state ---
    f32 difficultyScore = 0.5f;       // 0.0=easiest, 1.0=hardest (computed each update)
    f32 smoothedScore = 0.5f;         // Exponential moving average (prevents sudden swings)
    f32 smoothingRate = 0.05f;        // How fast the smoothed score tracks the raw score

    // --- Input Metrics (opt-in toggles) ---
    // Each metric has an enable flag and a weight (importance relative to others)

    bool trackDeaths = true;
    f32 deathWeight = 1.0f;
    u32 recentDeaths = 0;             // Deaths in current session/level
    u32 deathWindow = 300;            // Time window in seconds for "recent" deaths

    bool trackHealth = false;
    f32 healthWeight = 0.5f;
    // Reads from HealthComponent on the player entity (playerEntity field)

    bool trackAccuracy = false;
    f32 accuracyWeight = 0.3f;
    u32 shotsFired = 0;
    u32 shotsHit = 0;

    bool trackTime = false;
    f32 timeWeight = 0.3f;
    f32 expectedCompletionTime = 120.0f; // Seconds expected for current section
    f32 elapsedTime = 0.0f;

    bool trackResources = false;
    f32 resourceWeight = 0.3f;
    f32 resourceRatio = 1.0f;         // Current resources / max resources (updated by game)

    bool trackCheckpointHealth = false;
    f32 checkpointHealthWeight = 0.4f;
    f32 lastCheckpointHealthPercent = 1.0f; // Health % at last checkpoint

    // --- Player entity reference (for reading HealthComponent) ---
    Entity playerEntity = 0;          // Entity with HealthComponent to read from

    // --- Output Multipliers (opt-in toggles) ---
    // Each output can be independently enabled. Multiplier is computed from difficultyScore.

    bool adjustEnemyDamage = true;
    f32 enemyDamageMultiplier = 1.0f;   // Computed: lower when player struggles
    f32 enemyDamageRange = 0.3f;        // +/-30% range

    bool adjustEnemyHealth = true;
    f32 enemyHealthMultiplier = 1.0f;
    f32 enemyHealthRange = 0.3f;

    bool adjustAIAggression = false;
    f32 aiAggressionMultiplier = 1.0f;
    f32 aiAggressionRange = 0.25f;

    bool adjustResourceDrops = false;
    f32 resourceDropMultiplier = 1.0f;
    f32 resourceDropRange = 0.5f;       // +/-50% — bigger swing for resources

    bool adjustHintFrequency = false;
    f32 hintCooldown = 30.0f;           // Seconds between hints (lower = more hints)
    u32 hintsBeforeSection = 0;         // How many hints shown for current section
    u32 deathsBeforeHint = 3;           // Show hint after N deaths on same section

    bool adjustCheckpointFrequency = false;
    f32 checkpointMultiplier = 1.0f;    // <1 = more frequent checkpoints
    f32 checkpointRange = 0.3f;
};

} // namespace ECS
} // namespace Enjin
