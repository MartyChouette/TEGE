#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

namespace Enjin::ECS {

// ParallaxLayerComponent — drop it on any Sprite2D entity to turn that sprite
// into a parallax-scrolling layer. It rides the existing 2D sprite pipeline
// (no new draw path, no new pipeline), so it batches, sorts, and renders exactly
// like any other sprite on every backend, including web. A per-frame system
// offsets the sprite by a fraction of the camera's movement:
//
//   factor 0  = locked to the camera (an infinitely far backdrop; never moves
//               relative to the view)
//   factor 1  = moves fully with the world (a foreground layer; normal)
//   0<factor<1 = the parallax middle ground: distant hills at 0.2, mid trees at
//               0.5, near bushes at 0.8
//
// The sprite's authored position is the anchor. The system captures it on play
// start and writes position = anchor + parallax offset each frame; play-mode
// stop restores the authored position automatically. Optional autoScroll adds a
// constant drift (title-screen skies, endless runners) independent of the camera.
struct ParallaxLayerComponent {
    // Per-axis parallax fraction. 0 = locked to camera, 1 = moves with world.
    Math::Vector2 factor = Math::Vector2(0.5f, 1.0f);

    // Constant drift in world units/second, added on top of camera parallax.
    Math::Vector2 autoScroll = Math::Vector2(0.0f, 0.0f);

    // ── Runtime state (not serialized) ──
    // The authored position, captured once so the offset is always relative to a
    // stable anchor rather than accumulating on the mutated transform.
    Math::Vector3 anchor = Math::Vector3(0.0f);
    // The camera position at capture time, so the offset tracks camera MOVEMENT
    // from there and never jumps if the camera starts away from the origin.
    Math::Vector3 camStart = Math::Vector3(0.0f);
    bool anchorCaptured = false;
    f32 elapsed = 0.0f;   // accumulated time for autoScroll
};

} // namespace Enjin::ECS
