#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace ECS {

class World;

// Which camera a game renders through.
//
// A CameraTriggerComponent says "while the player is inside this box, use that
// camera". Only the editor's game view ever read it, so a shipped game (desktop
// or web) ignored every camera zone it had (SD-17). The web player also took
// the FIRST camera in the scene, ignoring priority and isActive. These are the
// one rule all three runtimes use now.

// The entity camera zones follow: the first 2D platformer, 2D top-down, 3D
// top-down, third-person or first-person controller, in that order (the order
// the editor always used). INVALID_ENTITY when the scene has none.
ENJIN_API Entity FindCameraZonePlayer(World* world);

// The target camera of the highest-priority zone containing `player`, if that
// camera exists; INVALID_ENTITY otherwise.
ENJIN_API Entity ResolveCameraZone(World* world, Entity player);

// The zone camera when the player stands in a zone, else the highest-priority
// active camera (CameraManager::GetActiveCamera).
ENJIN_API Entity ResolveGameCamera(World* world);

} // namespace ECS
} // namespace Enjin
