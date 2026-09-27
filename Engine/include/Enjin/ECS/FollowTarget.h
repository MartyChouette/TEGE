#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS {

class World;

// Moves every FollowTargetComponent toward its target. Called once per
// rendered frame from ControllerSystem::UpdatePresentation.
//
// This read the target, the offset and Move Speed and nothing else (SD-27);
// the distance band, Smooth Time, Match Target Rotation and Use Local Offset
// were inspector fields that did nothing.
//
//   goal       the target's world position plus Offset, the offset turned by
//              the target's rotation when Use Local Offset is on
//   distance   the follower heads for the point Follow Distance short of the
//              goal, on the line from the goal to itself. 0 means the goal
//              itself (a camera on a rigid offset). Closer than Min Distance
//              (never more than Follow Distance) it holds still, and beyond
//              Max Distance it gives up, holding still until the target comes
//              back in range.
//   motion     critically damped toward that point in about Smooth Time
//              seconds, never faster than Move Speed (0 = no cap). Smooth Time 0 snaps.
//   rotation   Match Target Rotation turns it toward the target's world
//              rotation at Rotation Speed degrees per second.
//
// World space throughout; a parented follower is written back in its parent's
// space.
ENJIN_API void UpdateFollowTargets(World* world, f32 deltaTime);

} // namespace ECS
} // namespace Enjin
