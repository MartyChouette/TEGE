#pragma once

#include "Enjin/Sim/Swarm.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Math/Vector.h"
#include <vector>

namespace Enjin {
namespace ECS {

// Authoring + runtime state for a data-oriented crowd.
//
// The heavy per-agent work runs in Sim::Swarm (struct-of-arrays, one
// vectorizable loop). In play mode, SwarmSystem spawns a pool of proxy cube
// entities that share ONE mesh, so RenderSystem's "group by mesh identity"
// path instances them and you can actually see the swarm. This bridges the
// DOD sim to the existing renderer; the true GPU-instanced/compute path is
// Tier-2 (see docs / memory).
struct SwarmComponent {
    // --- Authoring (safe defaults sized for a smooth first-play spawn) ---
    u32 count       = 2000;                              // agents simulated
    u32 renderCap   = 2000;                              // max visible proxies
    f32 spawnRadius = 25.0f;
    f32 maxSpeed    = 12.0f;
    f32 pull        = 1.5f;                              // toward the swarm entity
    f32 damping     = 0.15f;
    f32 cubeSize    = 0.3f;
    Math::Vector3 color = Math::Vector3(0.2f, 0.8f, 1.0f);

    // --- Runtime (rebuilt on play, not authored) ---
    Sim::Swarm sim;
    std::vector<Entity> proxies;
    bool started = false;
};

} // namespace ECS
} // namespace Enjin
