#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"

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

// As ResolveCameraZone, and also names the trigger that won, so its Blend Time
// can be read. *outTrigger is INVALID_ENTITY when no zone applies.
ENJIN_API Entity ResolveCameraZone(World* world, Entity player, Entity* outTrigger);

// Where the game camera is this frame, eased when it changed.
//
// A camera zone's Blend Time was saved and never read, so entering one cut
// straight to its camera (SD-27). The camera to render through is now blended
// from wherever the view was to the new camera over the blend time --
// position, rotation and field of view, smoothstep eased -- and a switch in
// the middle of a blend starts from the view as it stood, so nothing jumps.
struct GameCameraPose {
    Entity entity = INVALID_ENTITY;   // the camera being blended toward
    Math::Vector3 position;
    Math::Quaternion rotation;
    f32 fieldOfView = 60.0f;
};
struct GameCameraBlend {
    Entity current = INVALID_ENTITY;
    GameCameraPose last;              // what was rendered last frame
    bool hasLast = false;
    GameCameraPose from;
    f32 elapsed = 0.0f;
    f32 duration = 0.0f;
    f32 lastZoneBlend = 0.0f;         // the zone just left sets the time to leave it
};

// One step toward `target` (any camera entity); blendTime is used only on the
// frame the target changes. The editor calls this with its own camera choice.
ENJIN_API bool BlendGameCamera(World* world, GameCameraBlend& state, Entity target,
                               f32 blendTime, f32 deltaTime, GameCameraPose& out);

// The players' rule: ResolveGameCamera's choice, blended over the entered
// zone's Blend Time, or the time of the zone being left when returning to the
// default camera.
ENJIN_API bool ResolveBlendedGameCamera(World* world, GameCameraBlend& state, f32 deltaTime,
                                        GameCameraPose& out);

} // namespace ECS
} // namespace Enjin
