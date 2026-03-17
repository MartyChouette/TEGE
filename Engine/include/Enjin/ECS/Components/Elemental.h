#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace ECS {

// Tags an entity as receiving elemental effects (charring, wetness, snow, frost)
struct ENJIN_API ElementalSurfaceComponent {
    Math::Vector4 accumulation = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f); // (fire, water, earth, air)
    f32 flammability = 0.0f;      // 0=fireproof, 1=kindling
    f32 accumulationRate = 1.0f;   // how fast elements deposit
    f32 decayRate = 0.05f;         // how fast elements fade (natural evaporation/cooling)
    f32 maxAccumulation = 1.0f;    // saturation cap per channel

    // Snow specific
    f32 snowDeformation = 0.0f;    // 0-1, how much snow is compressed

    // Derived visual state (computed by system, not serialized)
    f32 charAmount = 0.0f;         // fire accumulation -> char visual
    f32 wetness = 0.0f;            // water accumulation -> wet/dark visual
    f32 snowCoverage = 0.0f;       // cold+water+air -> snow visual
    f32 frostAmount = 0.0f;        // cold+water -> ice visual
};

// Spawns elemental particles continuously (campfire, fountain, vent, lava)
struct ENJIN_API ElementalEmitterComponent {
    Math::Vector4 element = Math::Vector4(1.0f, 0.0f, 0.0f, 0.0f); // default: fire
    f32 emissionRate = 10.0f;      // particles per second
    f32 intensity = 1.0f;          // particle intensity
    f32 lifetime = 2.0f;           // particle lifetime
    f32 spread = 0.3f;             // emission cone angle (radians)
    f32 speed = 2.0f;              // initial particle speed
    Math::Vector3 direction = Math::Vector3(0.0f, 1.0f, 0.0f); // emission direction
    bool active = true;

    // Accumulated time for emission rate
    f32 spawnAccumulator = 0.0f;
};

// Defines a volume where elemental effects are modified (heat zone, cold zone, wind tunnel)
struct ENJIN_API ElementalVolumeComponent {
    Math::Vector3 halfExtents = Math::Vector3(5.0f, 5.0f, 5.0f);
    Math::Vector4 elementBias = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f); // adds to particles passing through
    f32 temperatureBias = 0.0f;    // affects fire/ice behavior
    f32 windMultiplier = 1.0f;     // scales wind inside volume
    bool killOnContact = false;    // destroy certain particles
};

} // namespace ECS
} // namespace Enjin
