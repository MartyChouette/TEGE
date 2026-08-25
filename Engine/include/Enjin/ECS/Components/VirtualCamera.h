#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace ECS {

// VirtualCameraComponent — a "vcam" in the Cinemachine sense.
//
// Dead-simple contract: point it at a target to FOLLOW, give it an OFFSET,
// set a PRIORITY. The CameraDirector activates the highest-priority live vcam
// and blends the real camera to it. You never touch the real camera — the
// Director owns it (single-owner invariant). Two vcams + a priority swap is a
// whole camera system (e.g. isometric vs over-the-shoulder).
//
// Body (how position is derived) and Aim (what it looks at) are kept minimal
// here; richer rigs (orbital, dolly, handheld noise — see CineSuite.as) layer
// on top of this same component later without changing the contract.
struct VirtualCameraComponent {
    bool enabled = true;
    i32 priority = 10;              // highest enabled vcam wins

    // Targets
    Entity follow = 0;             // anchor entity (0 = use this entity's own transform)
    Entity lookAt = 0;             // aim entity (0 = look at the follow anchor)

    // Body: camera position = anchor + offset (offset is in world axes for now)
    Math::Vector3 offset = Math::Vector3(0.0f, 6.0f, 10.0f);

    // Aim: look point = (lookAt or anchor) + lookOffset
    Math::Vector3 lookOffset = Math::Vector3(0.0f, 1.0f, 0.0f);

    // Lens
    f32 fov = 55.0f;

    // Feel — seconds to catch up to a moving target (0 = rigid snap). This is the
    // damped-follow term; the Director uses frame-rate-independent smoothing.
    f32 damping = 0.3f;

    // Blend duration used when THIS vcam becomes the live one.
    f32 blendTime = 0.5f;

    // --- runtime (serialized-harmless; the Director maintains these) ---
    bool isLive = false;
};

} // namespace ECS
} // namespace Enjin
