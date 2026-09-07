#pragma once

// Turns BrushSolidComponent brush lists into geometry.
//
// The rebuild is CPU-side only: it writes MeshComponent vertices and indices,
// fills the MeshColliderComponent, and then flags ProceduralMeshComponent the
// same way every other generated-geometry system does. RenderSystem owns the
// GPU work and does it at its own safe point, so nothing here creates or
// destroys a GPU resource. That is deliberate -- destroying a buffer mid-frame
// is how this engine access-violates at submit.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"

namespace Enjin {
namespace ECS {

class ENJIN_API BrushSolidSystem {
public:
    // Rebuild every solid whose list changed. Returns how many were rebuilt.
    // Cheap when nothing is dirty, which is every frame that is not an edit.
    static u32 Update(World* world);

    // Rebuild one solid regardless of its dirty flag. Returns false when the
    // entity has no brush solid, or the brushes produce no geometry.
    static bool Rebuild(World* world, Entity entity);
};

} // namespace ECS
} // namespace Enjin
