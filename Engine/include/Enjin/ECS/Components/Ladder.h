#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {
namespace ECS {

// Ladder (G1) - a climbable volume. While a character controller overlaps it
// and pushes forward, the controller enters a climb state: gravity off,
// forward input moves up, back input moves down, jump pushes off. Reaching
// the top gives a small boost so the character mantles onto the ledge.
//
// The volume is centered on the entity's transform position with these
// world-space half extents (colliders in this engine are world-space too -
// entity scale does NOT multiply them).
struct LadderComponent {
    Math::Vector3 halfExtents = Math::Vector3(0.5f, 2.0f, 0.5f);
    f32 climbSpeed = 3.0f;      // units/sec up or down
    f32 topBoost = 4.0f;        // upward push when exiting at the top (mantle)
    bool allowJumpOff = true;   // jump exits the climb with a push-off

    // The rest of the climb's feel, which used to be literals in the shared
    // climb step even though everything above it was already authored.
    f32 mantleWindow = 0.2f;    // how far below the top edge counts as "at the top".
                                // Must exceed one climb step (climbSpeed * dt) or a
                                // fast frame tunnels past the top without mantling.
    f32 pushOffScale = 0.7f;    // jump-off strength, as a fraction of the climber's jumpForce
};

} // namespace ECS
} // namespace Enjin
