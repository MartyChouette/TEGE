#pragma once

// Turns a VoxelVolumeComponent's distance field into geometry.
//
// Same contract as BrushSolidSystem, and for the same reason: the rebuild is
// CPU-side only. It writes MeshComponent vertices and indices, fills the
// MeshColliderComponent, and flags ProceduralMeshComponent. RenderSystem owns
// the GPU work and does it at its own safe point, because destroying a buffer
// mid-frame is how this engine access-violates at submit.
//
// The collider is the same triangles as the mesh. Nothing convex can describe a
// cave -- a convex hull of a cavern is a solid lump -- so a triangle mesh is
// not a fallback here, it is the only correct answer.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace ECS {

class ENJIN_API VoxelVolumeSystem {
public:
    // Rebuild every volume whose field changed. Returns how many were rebuilt.
    // Cheap on a frame that is not an edit.
    static u32 Update(World* world);

    // Rebuild one volume regardless of its dirty flag. Returns false when the
    // entity has no volume, or the field contains no surface.
    static bool Rebuild(World* world, Entity entity);
};

} // namespace ECS
} // namespace Enjin
