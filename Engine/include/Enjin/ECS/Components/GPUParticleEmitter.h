#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Effects/GPUParticleSystem.h"

namespace Enjin {
namespace ECS {

// Drop this on an entity to emit GPU-compute particles from its position.
// The heavy work (simulate + draw) stays entirely on the GPU; this component
// just tells the emitter system what to spawn, how fast, and where.
//
// Pick a preset for an instant good-looking effect (Smoke/Fire/Sparks/Blood/
// Mist/Spray/Dust/Magic/Snow), or Custom to drive the fields yourself.
struct GPUParticleEmitterComponent {
    Effects::GPUParticlePreset preset = Effects::GPUParticlePreset::Fire;

    bool emitting = true;          // continuous emission on/off
    f32 spawnRate = 200.0f;        // particles per second (continuous)
    Math::Vector3 direction = {0.0f, 1.0f, 0.0f};  // emission direction

    // One-shot burst: set burstCount and flip burstNow (the system fires it
    // once and clears the flag). Great for hits/explosions from script/nodes.
    u32 burstCount = 0;
    bool burstNow = false;

    // Custom overrides (used only when preset == Custom).
    Math::Vector4 customColor = {1, 1, 1, 1};
    f32 customSize = 0.3f;
    f32 customLifetime = 3.0f;
    f32 customSpeed = 2.0f;
    f32 customSpread = 0.5f;
    f32 customGravityScale = 1.0f;
    f32 customDrag = 0.0f;

    // Runtime accumulator for fractional continuous spawns (not serialized).
    f32 accumulator = 0.0f;

    // Resolve the spawn params for this emitter (preset or custom fields).
    Effects::ParticleSpawnParams ResolveParams() const {
        if (preset != Effects::GPUParticlePreset::Custom)
            return Effects::PresetSpawnParams(preset);
        Effects::ParticleSpawnParams p;
        p.color = customColor;
        p.size = customSize;
        p.lifetime = customLifetime;
        p.speed = customSpeed;
        p.spread = customSpread;
        p.gravityScale = customGravityScale;
        p.drag = customDrag;
        return p;
    }
};

} // namespace ECS
} // namespace Enjin
