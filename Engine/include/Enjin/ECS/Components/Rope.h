#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS {

// Rope (G3) - a verlet point chain hanging from the entity, rendered as a
// tube mesh rewritten into the entity's MeshComponent every frame (same
// pattern as ClothComponent; ClothSystem simulates both). The top point is
// pinned to the entity's transform, so moving the entity swings the rope.
//
// endAttachName picks a second entity by name and does one of two jobs:
//   pinBottom = false: that entity is DRAGGED to the rope tip each frame -
//                      a dangling lantern, a tire, a hook.
//   pinBottom = true:  that entity is a second ANCHOR - the rope spans
//                      between the two entities and sags (clothesline,
//                      power line, banner rope).
// How the chain renders. The SIMULATION is identical (verlet chain); Chain
// just draws each segment as a rigid link (alternating 90-degree twist, like
// a real chain) instead of a smooth tube. Serialized as int - append only.
enum class RopeStyle : u8 { Tube, Chain };

struct RopeComponent {
    // --- Authoring (serialized) ---
    RopeStyle style = RopeStyle::Tube;
    f32 length = 4.0f;             // world length hanging down from the entity
    i32 segments = 16;             // chain segments (points = segments + 1)
    f32 thickness = 0.05f;         // tube radius (world units)
    i32 iterations = 8;            // constraint solver passes (more = less stretch)
    f32 damping = 0.02f;           // velocity damping per step
    f32 gravityScale = 1.0f;
    Math::Vector3 wind = {0, 0, 0};  // constant world-space wind force
    bool useWeatherWind = true;    // sample live weather wind (zones override)
    f32 weatherWindScale = 1.0f;
    bool collide = true;           // push points out of Box/Sphere/Capsule colliders
    // Act AS a collider for OTHER cloth and ropes: each simulated segment is
    // offered to the collision gather as a capsule, so a sheet can drape over
    // this rope (a garment on a clothesline) instead of falling through it.
    // Off by default -- a rope with 20-30 segments adds that many shapes to
    // every cloth's resolve loop, so it is opt-in per rope.
    // One-way: cloth resting on the rope does NOT weigh the rope down.
    bool collidable = false;
    // Radius used for those capsules, INDEPENDENT of the rendered thickness
    // (0 = use thickness). A clothesline draws thin but must collide fat: cloth
    // is a grid of POINTS, so a point can only be caught if it lands within the
    // capsule radius. With a rope thinner than about half the cloth point
    // spacing the sheet slips between the points and falls straight through
    // (measured: radius 0.05 vs 0.12 spacing = no contact at all; 0.5 catches).
    // Rule of thumb: keep this >= the cloth spacing (width/resX).
    f32 collisionRadius = 0.0f;
    f32 collisionSkin = 0.04f;
    f32 friction = 0.5f;
    f32 endMass = 0.0f;            // >0 hangs a weight on the tip (taut, slow swing)
    std::string endAttachName;     // entity name; see pinBottom above ("" = none)
    bool pinBottom = false;        // endAttach entity anchors the tip instead of dangling

    // --- Runtime state (owned by ClothSystem; not serialized) ---
    bool initialized = false;      // set false to rebuild
    std::vector<Math::Vector3> positions;      // world space
    std::vector<Math::Vector3> prevPositions;  // Verlet
    std::vector<f32> invMass;                  // 0 = pinned
    f32 segmentRest = 0.0f;                    // rest length per segment
    Math::Vector3 frameNormal = {0, 0, 1};     // tube frame carry (anti-twist)
    bool meshDirty = false;        // vertices changed -> re-upload
    bool topologyDirty = false;    // buffers must rebuild from fresh mesh
};

} // namespace ECS
} // namespace Enjin
