#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <string>

namespace Enjin {
namespace ECS { class EntityEventBus; struct DynamicDifficultyComponent; }
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
//
// Deaths age out after Death Window seconds, so a player who stops dying
// drifts back to the base difficulty. With Adjust Hint Frequency on, reaching
// Deaths Before Hint recent deaths sends a "difficulty_hint" event through
// the entity event bus (sender = the difficulty entity, ints "hint" = how many
// this section, "deaths" = recent deaths), at most once per Hint Cooldown.
// Scripts (Events_Listen) and visual scripts (Custom Event) decide what the
// hint says; Difficulty_Reset starts a new section. Visible To Player draws a
// small indicator with the base level and how far it has been adjusted.

class ENJIN_API DynamicDifficultySystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    void SetEventBus(ECS::EntityEventBus* bus) { m_EventBus = bus; }

    // The Visible To Player indicator, beside the other overlays
    void RenderOverlay(f32 originX, f32 originY, u32 viewportWidth, u32 viewportHeight);

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

    // Age deaths out of the window and send a hint when one is due
    void UpdateDeathsAndHints(ECS::Entity entity, ECS::DynamicDifficultyComponent& dd, f32 deltaTime);

    ECS::World* m_World = nullptr;
    ECS::EntityEventBus* m_EventBus = nullptr;
    bool m_Enabled = false;
    f32 m_TimeSinceLastUpdate = 0.0f;
    f32 m_UpdateInterval = 1.0f;      // Recompute every 1 second
};

} // namespace Gameplay
} // namespace Enjin
