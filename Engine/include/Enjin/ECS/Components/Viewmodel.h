#pragma once

#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS {

// First-person viewmodel marker: the entity (typically a weapon/hands mesh
// parented to the camera) renders with the viewport depth range compressed to
// the nearest slice of the depth buffer, so it draws in front of all world
// geometry and never visually clips into walls. Viewmodel entities are also
// excluded from shadow casting (a room-sized gun shadow gives the trick away).
//
// The standard first-person setup: parent the mesh to the camera entity,
// offset it down-right, add this component. Toggling `enabled` at runtime
// (Viewmodel_Set) returns the entity to normal world rendering.
struct ViewmodelComponent {
    bool enabled = true;
};

} // namespace ECS
} // namespace Enjin
