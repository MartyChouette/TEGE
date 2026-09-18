#pragma once
// Write entities out as a .glb model.
//
// The engine could read glTF and never write it, so geometry authored here --
// a remeshed model, a spline chain, a group of placed pieces -- could not leave
// the editor at all. That is a one-way door for anyone who wants to touch their
// own work in Blender.
//
// GLB rather than .gltf + .bin: one file, no sidecars to lose, and it is the
// format this engine's own docs call recommended. FBX is not offered because
// Assimp's exporter is compiled out (ASSIMP_NO_EXPORT in Engine/CMakeLists.txt)
// and hand-writing binary FBX is not a thing anyone should do; turning that
// option on is a build decision, not an export feature.
//
// The file is written by hand rather than through a library: a GLB is a JSON
// chunk and a binary chunk with a twelve-byte header, and the alternative was
// enabling an exporter we would then have to keep building on three platforms.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"

#include <string>
#include <vector>

namespace Enjin {
namespace Assets {

struct ENJIN_API GLBExportResult {
    bool success = false;
    std::string error;          // empty on success
    u32 meshesWritten = 0;
    u32 verticesWritten = 0;
    u32 skipped = 0;            // selected entities with nothing to export
};

// Export `entities` (and nothing else) to `path`.
//
// Each entity with a valid MeshComponent becomes one glTF mesh on one node,
// carrying its WORLD transform -- so a selection exports in the arrangement it
// has on screen, and re-importing it puts the pieces back where they were.
//
// What does NOT survive, and is reported rather than silently dropped:
// textures (materials export as base colour, metallic and roughness factors
// only), skinning, animation, and sub-mesh material slots, which collapse to
// the entity's primary material.
ENJIN_API GLBExportResult ExportEntitiesToGLB(ECS::World* world,
                                              const std::vector<ECS::Entity>& entities,
                                              const std::string& path);

} // namespace Assets
} // namespace Enjin
