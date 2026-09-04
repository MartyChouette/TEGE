#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace ECS {

// ---------------------------------------------------------------------------
// ProceduralMeshComponent
// ---------------------------------------------------------------------------
// One upload path for every system that writes MeshComponent geometry at
// runtime. Before this existed, each generator (terrain, terrain2d, jelly,
// cloth, rope) had its own hardcoded dirty block in RenderSystem, duplicated
// across the Vulkan and WebGPU paths. Adding a sixth generator meant two more
// copies. A system now attaches this component beside the MeshComponent it
// writes and raises a flag; the renderer handles all of them in one loop.
//
// Protocol, identical to the cloth/rope one it generalises:
//   meshDirty      vertex positions changed, index list did not.
//                  The renderer re-uploads into the live vertex buffer.
//   topologyDirty  the index list changed (or the mesh was just created).
//                  The renderer retires the buffers so they rebuild at the
//                  new size, then performs the follow-up vertex upload.
//
// Raise topologyDirty on the first write and whenever the triangle count
// changes. Raise meshDirty when only vertex data moved. The renderer clears
// both. Nothing else should clear them.
struct ENJIN_API ProceduralMeshComponent {
    // What generated this geometry. Inspector display only; the upload path
    // does not branch on it. Kept as an explicit enum so a scene that loads
    // with a stale mesh can say which system is supposed to own it.
    enum class Source : u8 {
        Unknown = 0,
        Metaball,
        CellularAutomata,
        Fourier,
        Projection4D,
        SplineIK,
        Script,
        Count
    };

    Source source = Source::Unknown;
    bool meshDirty = false;
    bool topologyDirty = false;

    // Set false to keep the geometry but stop the owning system regenerating
    // it. Useful once a generated mesh has been dialled in and should freeze.
    bool regenerate = true;
};

inline const char* ProceduralMeshSourceName(ProceduralMeshComponent::Source s) {
    switch (s) {
        case ProceduralMeshComponent::Source::Metaball:         return "Metaball";
        case ProceduralMeshComponent::Source::CellularAutomata: return "Cellular Automata";
        case ProceduralMeshComponent::Source::Fourier:          return "Fourier Mesh";
        case ProceduralMeshComponent::Source::Projection4D:     return "4D Projection";
        case ProceduralMeshComponent::Source::SplineIK:         return "Spline IK";
        case ProceduralMeshComponent::Source::Script:           return "Script";
        default:                                                return "Unknown";
    }
}

} // namespace ECS
} // namespace Enjin
