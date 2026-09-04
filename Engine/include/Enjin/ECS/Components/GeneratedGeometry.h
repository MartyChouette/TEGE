#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Effects/CellularAutomataGeometry.h"
#include "Enjin/Effects/Projection4D.h"
#include <string>
#include <vector>

namespace Enjin {
namespace ECS {

// ---------------------------------------------------------------------------
// Generated geometry components
// ---------------------------------------------------------------------------
// Four CPU generators shipped in Engine/src/Effects with full implementations
// and no way to reach them: no component, no inspector, no serializer, no tick.
// These components are the authoring surface. GeneratedGeometrySystem runs the
// generator and writes the result into the entity's MeshComponent, raising the
// ProceduralMeshComponent flags so the renderer uploads it.
//
// Every one of them regenerates on the main thread during the normal system
// tick, so they may add a MeshComponent (structural mutation, ADR-0004).

// --- Metaballs -------------------------------------------------------------
// The blobs themselves are MetaballComponent (Effects/Metaballs.h) on child
// entities. This component marks the entity that RECEIVES the isosurface for
// one group. One surface entity per groupId.
struct ENJIN_API MetaballSurfaceComponent {
    i32 groupId = 0;              // Merges only with MetaballComponents of this group
    i32 gridResolution = 32;      // Voxels per axis, clamped to 16-64 by the system
    f32 gridSize = 10.0f;         // World extent of the evaluation grid
    bool smoothNormals = true;    // Gradient normals instead of face normals
    bool autoCenter = true;       // Centre the grid on the group centroid
    f32 updateRate = 30.0f;       // Rebuild rate in Hz, 0 = every frame
    bool useBlobColors = true;    // Blend MetaballComponent colors into vertex color
};

// --- Cellular automata as geometry -----------------------------------------
struct ENJIN_API CellularAutomataComponent {
    Effects::CARule rule = Effects::CARule::GameOfLife;
    Effects::CAMeshMode meshMode = Effects::CAMeshMode::Voxels;
    u32 width = 32;
    u32 height = 32;
    u32 depth = 1;                    // >1 layers the grid into 3D
    f32 cellSize = 0.25f;
    f32 updateInterval = 0.2f;        // Seconds between automaton steps
    f32 initialFillPercent = 30.0f;
    u32 seed = 1337;
    bool wrapEdges = true;
    f32 isoLevel = 0.5f;              // Marching cubes threshold
    Math::Vector3 liveColor = {0.10f, 1.00f, 0.80f};
    Math::Vector3 dyingColor = {0.80f, 0.30f, 0.10f};

    // Stamp a classic pattern into the grid on reset instead of random fill.
    // "" = random fill. Accepted: "glider", "pulsar", "gospergun".
    std::string stampPattern;

    bool running = true;              // false freezes the current generation
    bool resetRequested = false;      // inspector button / script sets this

    // Runtime readback for the inspector. Not serialised.
    u32 generation = 0;
    u32 liveCells = 0;
};

// --- Stereographic projection of 4D polytopes ------------------------------
struct ENJIN_API Projection4DComponent {
    enum class Polytope : u8 { Tesseract = 0, Cell5, Cell16, Cell24, Cell120, Count };

    Polytope polytope = Polytope::Tesseract;
    Effects::RotationConfig4D rotation;   // Per-plane speeds, radians/second
    f32 lineWidth = 0.02f;                // Tube radius on each projected edge
    f32 projectionDistance = 2.0f;        // Stereographic projection centre
    u32 tubeSegments = 8;                 // Sides per edge tube
    f32 scale = 1.0f;                     // Uniform scale applied after projection
    bool animate = true;

    // Rebuild throttle. The 120-cell is 1200 edges, so a tube mesh per frame is
    // expensive; 0 = every frame.
    f32 updateRate = 60.0f;
};

// --- Fourier decomposition of a 2D contour ---------------------------------
struct ENJIN_API FourierMeshComponent {
    enum class ContourSource : u8 { Circle = 0, Square, Star, Heart, Custom, Count };

    ContourSource source = ContourSource::Star;
    std::vector<Math::Vector2> customContour;  // Used when source == Custom

    i32 coefficients = 64;        // DFT terms computed, 0 = all
    i32 terms = 16;               // Terms used in the reconstruction
    i32 samples = 256;            // Points sampled around the reconstructed curve
    bool animateTerms = true;     // Ramp `terms` up over time for the classic reveal
    f32 animationSeconds = 6.0f;  // Time to reach the full term count
    f32 extrudeDepth = 0.0f;      // 0 = flat triangulated contour, >0 = extruded solid
    f32 scale = 1.0f;

    // Runtime readback. Not serialised.
    f32 elapsed = 0.0f;
    i32 activeTerms = 0;
};

inline const char* Projection4DPolytopeName(Projection4DComponent::Polytope p) {
    switch (p) {
        case Projection4DComponent::Polytope::Tesseract: return "Tesseract (8-cell)";
        case Projection4DComponent::Polytope::Cell5:     return "5-cell (simplex)";
        case Projection4DComponent::Polytope::Cell16:    return "16-cell";
        case Projection4DComponent::Polytope::Cell24:    return "24-cell";
        case Projection4DComponent::Polytope::Cell120:   return "120-cell";
        default:                                         return "Unknown";
    }
}

inline const char* FourierContourSourceName(FourierMeshComponent::ContourSource s) {
    switch (s) {
        case FourierMeshComponent::ContourSource::Circle: return "Circle";
        case FourierMeshComponent::ContourSource::Square: return "Square";
        case FourierMeshComponent::ContourSource::Star:   return "Star";
        case FourierMeshComponent::ContourSource::Heart:  return "Heart";
        case FourierMeshComponent::ContourSource::Custom: return "Custom";
        default:                                          return "Unknown";
    }
}

} // namespace ECS
} // namespace Enjin
