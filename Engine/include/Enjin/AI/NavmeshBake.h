#pragma once

// One bake, shared by the editor button, the editor's Play, the player and the
// web player.
//
// Written once rather than per-runtime because the failure that hides here is a
// navmesh that differs between the editor and the shipped game: agents path fine
// while you author, and walk through a wall in the build. Every caller goes
// through this function, so there is only one answer to "what is walkable".

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/AI/Navmesh.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/NavmeshVolume.h"

#include <string>

namespace Enjin {
namespace AI {

struct NavmeshBakeResult {
    bool success = false;
    u32 polygons = 0;
    u32 meshes = 0;         // meshes that contributed
    u32 triangles = 0;      // triangles that survived the bounds and tag filters
    std::string message;    // always set, success or not
};

// Bake the volume on `entity` into `generator`, and write what happened back onto
// the component so the inspector can report it.
//
// Returns a result rather than a bare bool: "it failed" and "it succeeded with
// zero polygons" are different situations with different fixes, and a bool makes
// them the same.
ENJIN_API NavmeshBakeResult BakeNavmeshVolume(ECS::World* world, ECS::Entity entity,
                                              NavmeshGenerator& generator);

// Find the volume a scene should use: the first entity with a NavmeshVolumeComponent.
// INVALID_ENTITY when the scene has none.
//
// More than one is a scene-authoring mistake rather than a feature -- the
// pathfinder holds a single navmesh -- so it is reported once rather than silently
// picking one.
ENJIN_API ECS::Entity FindNavmeshVolume(ECS::World* world);

} // namespace AI
} // namespace Enjin
