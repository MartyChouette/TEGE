#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Quaternion.h"

namespace Enjin {
namespace ECS {

// Door (G7) - a hinged door. Put this on the HINGE PIVOT entity: the door
// mesh is a CHILD offset sideways so the pivot sits at the hinge edge, and
// the whole assembly swings by rotating this entity. Give the mesh child a
// kinematic rigidbody + box collider and the physics follows the swing.
//
// Any character controller within interactRadius pressing E toggles it.
// Locked doors refuse (script/MCP can flip `locked`; key items come later
// with the Lock integration).
struct DoorComponent {
    f32 openAngle = 110.0f;      // degrees swung when open (sign flips direction)
    f32 openSpeed = 240.0f;      // degrees per second
    f32 interactRadius = 2.5f;   // E works within this range of the pivot
    f32 autoCloseDelay = 0.0f;   // seconds after opening (0 = stays open)
    bool locked = false;
    bool startOpen = false;

    // --- Runtime (not serialized) ---
    bool open = false;
    f32 currentAngle = 0.0f;
    f32 closeTimer = 0.0f;
    bool initialized = false;             // base rotation captured on first update
    Math::Quaternion baseRotation;
};

} // namespace ECS
} // namespace Enjin
