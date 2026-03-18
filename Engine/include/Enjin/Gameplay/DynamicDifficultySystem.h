#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <string>

namespace Enjin {
namespace Gameplay {

// ============================================================================
// Dynamic Difficulty System
// ============================================================================
// Lightweight system that updates once per second (not every frame).
// Finds the entity with DynamicDifficultyComponent, reads enabled input
// metrics, computes a weighted struggle score, applies smoothing, and
// writes output multipliers that game systems can query.
//
// Script API concepts (for AngelScript bindings):
//   float Difficulty_GetMultiplier(string which)
//       — "enemyDamage", "enemyHealth", "aiAggression", "resourceDrops", "checkpoint"
//   void  Difficulty_RecordDeath()
//   void  Difficulty_RecordHit()
//   void  Difficulty_RecordShot()
//   void  Difficulty_SetBaseDifficulty(uint level)
//   float Difficulty_GetScore()
//       — returns current smoothed score

class ENJIN_API DynamicDifficultySystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }

    // Called every frame; internally accumulates time and only recomputes
    // scores once per second to stay lightweight.
    void Update(ECS::World* world, f32 deltaTime);

    // --- Query API (for game systems / scripts) ---

    // Get a named output multiplier. Returns 1.0 if the system is disabled
    // or the component doesn't exist.
    f32 GetMultiplier(const std::string& which) const;

    // Get the current smoothed difficulty score (0=easiest, 1=hardest).
    f32 GetScore() const;

    // --- Event recording API (call from game code / scripts) ---

    void RecordDeath();
    void RecordShot();
    void RecordHit();
    void SetBaseDifficulty(u32 level);

    // Record checkpoint arrival with current health percent (0-1).
    void RecordCheckpointHealth(f32 healthPercent);

    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

private:
    // Recompute raw score from enabled metrics, apply smoothing, compute multipliers.
    void Recompute(ECS::World* world);

    ECS::World* m_World = nullptr;
    bool m_Enabled = false;
    f32 m_TimeSinceLastUpdate = 0.0f;
    f32 m_UpdateInterval = 1.0f;      // Recompute every 1 second
};

} // namespace Gameplay
} // namespace Enjin
