#pragma once

// The thing that makes A* reachable.
//
// The engine has a navmesh, a navmesh generator with three generation paths, an
// A* pathfinder, an AISystem that can follow a path, AIControllerComponent's
// `useNavmesh` flag, and seven Navmesh_* script bindings. None of it could run:
// AISystem::SetNavmesh, AISystem::GenerateGridNavmesh and
// AISystem::GenerateNavmeshFromGeometry were never called from anywhere, and
// Scripting::SetBindingsNavmesh -- the function that hands the pathfinder to the
// script bindings -- was defined and never called either.
//
// So Navmesh_HasNavmesh() always answered false, Navmesh_FindPath() always
// answered 0, and an AI agent with "Use Navmesh" ticked walked in a straight line
// through the wall. Every piece existed except the one that says WHERE the
// walkable area is, and without that nothing could start.
//
// This is that piece: a volume you place in the scene, with the settings the
// documentation already describes, and a bake.
//
// The bake RESULT is not serialized, deliberately. It is a function of the scene
// geometry plus these settings, so storing it creates a second source of truth
// that goes stale the moment a wall moves -- and a stale navmesh is worse than no
// navmesh, because agents confidently path through the gap that used to be there.
// It is baked when play starts, and on demand in the editor.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/AI/Navmesh.h"
#include "Enjin/Math/Vector.h"

#include <string>

namespace Enjin {
namespace ECS {

struct NavmeshVolumeComponent {
    // Where the walkable area is built from.
    enum class Source : u8 {
        SceneGeometry = 0,  // triangles of every mesh inside the bounds
        Grid,               // a flat grid at gridHeight, for 2D and top-down
    };
    Source source = Source::SceneGeometry;

    // The volume, in WORLD space and independent of this entity's transform.
    //
    // Independent on purpose, and it is the opposite choice from Water3D (whose
    // position lives in its settings while its transform must stay at the origin).
    // A navmesh volume is a region of the level, not an object in it: parenting it
    // under something that moves would silently re-bake the world's walkable area
    // when a door opened.
    Math::Vector3 boundsMin = Math::Vector3(-50.0f, -2.0f, -50.0f);
    Math::Vector3 boundsMax = Math::Vector3(50.0f, 20.0f, 50.0f);

    // Grid source only.
    f32 gridCellSize = 1.0f;
    f32 gridHeight = 0.0f;

    // Agent shape and limits. These are the fields docs/TUTORIALS.md has always
    // listed under "NavMesh Properties", finally attached to something.
    AI::NavmeshGenSettings settings;

    // Restrict which meshes contribute. Empty includes every mesh in the bounds,
    // which is the useful default: requiring an opt-in tag on every wall in a level
    // is the kind of authoring cost that gets a feature abandoned.
    //
    // A mesh matches a tag through TagComponent or its entity name, the same rule
    // GameplaySystem uses, so the two do not disagree about what "crate" means.
    std::string includeTag;
    std::string excludeTag;

    // Bake when play starts. Off means the level ships with no navigation until
    // something asks for a bake, which is a legitimate choice for a scene whose
    // agents do not path.
    bool bakeOnPlay = true;

    // Draw the baked polygons in the viewport.
    bool debugDraw = false;

    // ---- Runtime state, not serialized --------------------------------------
    //
    // What the last bake actually did. Kept so the inspector can SAY it rather
    // than leaving a bake that produced nothing looking the same as one that was
    // never pressed -- the same reason DataAssetScanResult exists.
    bool baked = false;
    u32 bakedPolygons = 0;
    u32 sourceMeshes = 0;
    u32 sourceTriangles = 0;
    std::string lastBakeMessage;
};

} // namespace ECS
} // namespace Enjin
