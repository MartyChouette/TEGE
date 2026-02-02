#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/LOD.h"

namespace Enjin {
namespace Renderer {

// Mesh simplification via quadric error metric edge collapse
class ENJIN_API MeshSimplifier {
public:
    // Simplify a mesh to a target vertex ratio (0.0 - 1.0)
    // ratio = 0.5 means keep ~50% of vertices
    static ECS::MeshComponent Simplify(const ECS::MeshComponent& source, f32 ratio);

    // Auto-generate all LOD levels for a mesh
    // Uses the reduction ratios from the LODComponent
    static void GenerateLODs(const ECS::MeshComponent& sourceMesh, ECS::LODComponent& lod);
};

} // namespace Renderer
} // namespace Enjin
